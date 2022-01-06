/*  
 *  AzulSCSI
 *  Copyright (c) 2022 Rabbit Hole Computing
 * 
 * This project is based on BlueSCSI:
 *
 *  BlueSCSI
 *  Copyright (c) 2021  Eric Helgeson, Androda
 *  
 *  This file is free software: you may copy, redistribute and/or modify it  
 *  under the terms of the GNU General Public License as published by the  
 *  Free Software Foundation, either version 2 of the License, or (at your  
 *  option) any later version.  
 *  
 *  This file is distributed in the hope that it will be useful, but  
 *  WITHOUT ANY WARRANTY; without even the implied warranty of  
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 *  General Public License for more details.  
 *  
 *  You should have received a copy of the GNU General Public License  
 *  along with this program.  If not, see https://github.com/erichelgeson/bluescsi.  
 *  
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *  
 *     Copyright (c) 2019 komatsu   
 *  
 *     Permission to use, copy, modify, and/or distribute this software  
 *     for any purpose with or without fee is hereby granted, provided  
 *     that the above copyright notice and this permission notice appear  
 *     in all copies.  
 *  
 *     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL  
 *     WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED  
 *     WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE  
 *     AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR  
 *     CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS  
 *     OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,  
 *     NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN  
 *     CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  
 */

#include <SdFat.h>
#include <minIni.h>
#include <string.h>
#include <ctype.h>
#include "AzulSCSI_config.h"
#include "AzulSCSI_platform.h"
#include "AzulSCSI_log.h"

SdFs SD;

/***********************************/
/* Error reporting by blinking led */
/***********************************/

#define BLINK_ERROR_NO_IMAGES  3 
#define BLINK_ERROR_NO_SD_CARD 5

void blinkStatus(int count)
{
  azdbg("Blinking status code: ", count);
  for (int i = 0; i < count; i++)
  {
    LED_ON();
    delay(250);
    LED_OFF();
    delay(250);
  }
}

/*********************************/
/* Global state for SCSI code    */
/*********************************/

volatile bool g_busreset = false;
static uint8_t g_sensekey = 0;
uint8_t   g_scsi_id_mask;    // Mask list of responding SCSI IDs
uint8_t   g_scsi_id;         // Currently responding SCSI-ID
uint8_t   g_scsi_lun;        // Logical unit number currently responding
uint8_t   g_scsi_sts;        // Status byte


/*********************************/
/* SCSI Drive Vendor information */
/*********************************/

// Quirks for specific SCSI hosts
enum scsi_quirks_t {
  SCSI_QUIRKS_STANDARD = 0,
  SCSI_QUIRKS_SHARP = 1,
  SCSI_QUIRKS_NEC_PC98 = 2
} g_scsi_quirks;

uint8_t SCSI_INFO_BUF[36] = {
  0x00, //device type
  0x00, //RMB = 0
  0x01, //ISO, ECMA, ANSI version
  0x01, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'Q', 'U', 'A', 'N', 'T', 'U', 'M', ' ', // vendor 8
  'F', 'I', 'R', 'E', 'B', 'A', 'L', 'L', '1', ' ', ' ',' ', ' ', ' ', ' ', ' ', // product 16
  '1', '.', '0', ' ' // version 4
};

void readSCSIDeviceConfig()
{
  char tmp[32];

  g_scsi_quirks = (scsi_quirks_t)ini_getl("SCSI", "Quirks", SCSI_QUIRKS_STANDARD, CONFIGFILE);

  const char *default_vendor = DEFAULT_VENDOR;
  const char *default_product = DEFAULT_PRODUCT;
  const char *default_version = DEFAULT_VERSION;

  if (g_scsi_quirks == SCSI_QUIRKS_NEC_PC98)
  {
    default_vendor = "NECITSU ";
    default_product = "ArdSCSino       ";
    default_version = "0010";
  }

  memset(tmp, 0, sizeof(tmp));
  ini_gets("SCSI", "Vendor", default_vendor, tmp, sizeof(tmp), CONFIGFILE);
  memcpy(&(SCSI_INFO_BUF[8]), tmp, 8);

  memset(tmp, 0, sizeof(tmp));
  ini_gets("SCSI", "Product", default_product, tmp, sizeof(tmp), CONFIGFILE);
  memcpy(&(SCSI_INFO_BUF[16]), tmp, 16);

  memset(tmp, 0, sizeof(tmp));
  ini_gets("SCSI", "Version", default_version, tmp, sizeof(tmp), CONFIGFILE);
  memcpy(&(SCSI_INFO_BUF[32]), tmp, 4);
}

/*********************************/
/* Harddisk image file handling  */
/*********************************/

// Information about active HDD image
typedef struct hddimg_struct
{
	FsFile      m_file;                 // File object
	uint64_t    m_fileSize;             // File size
	size_t      m_blocksize;            // SCSI BLOCK size
} HDDIMG;
HDDIMG g_hddimg[NUM_SCSIID][NUM_SCSILUN]; // Allocate storage for all images
static HDDIMG *g_currentimg = NULL; // Pointer to active image based on LUN/ID

bool hddimageOpen(HDDIMG *h,const char *image_name,int id,int lun,int blocksize)
{
  h->m_fileSize = 0;
  h->m_blocksize = blocksize;
  h->m_file = SD.open(image_name, O_RDWR);
  if(h->m_file.isOpen())
  {
    h->m_fileSize = h->m_file.size();
    azlog("Opened image file ", image_name, " fileSize: ", (int)(h->m_fileSize / 1024), " kB");
    
    if(h->m_fileSize > 0)
    {
      return true; // File opened
    }
    else
    {
      h->m_file.close();
      h->m_fileSize = h->m_blocksize = 0; // no file
      azlog("Error: image is empty");
    }
  }
  return false;
}

// Iterate over the root path in the SD card looking for candidate image files.
void findHDDImages()
{
  g_scsi_id_mask = 0x00;

  SdFile root;
  root.open("/");
  SdFile file;
  bool imageReady;
  int usedDefaultId = 0;
  while (1) {
    if (!file.openNext(&root, O_READ)) break;
    char name[MAX_FILE_PATH+1];
    if(!file.isDir()) {
      file.getName(name, MAX_FILE_PATH+1);
      file.close();
      if (tolower(name[0]) == 'h' && tolower(name[1]) == 'd') {
        // Defaults for Hard Disks
        int id  = 1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
        int lun = 0;
        int blk = 512;

        // Positionally read in and coerase the chars to integers.
        // We only require the minimum and read in the next if provided.
        int file_name_length = strlen(name);
        if(file_name_length > 2) { // HD[N]
          int tmp_id = name[HDIMG_ID_POS] - '0';

          if(tmp_id > -1 && tmp_id < 8)
          {
            id = tmp_id; // If valid id, set it, else use default
          }
          else
          {
            id = usedDefaultId++;
          }
        }
        if(file_name_length > 3) { // HD0[N]
          int tmp_lun = name[HDIMG_LUN_POS] - '0';

          if(tmp_lun > -1 && tmp_lun < 2) {
            lun = tmp_lun; // If valid id, set it, else use default
          }
        }
        int blk1 = 0, blk2 = 0, blk3 = 0, blk4 = 0;
        if(file_name_length > 8) { // HD00_[111]
          blk1 = name[HDIMG_BLK_POS] - '0';
          blk2 = name[HDIMG_BLK_POS+1] - '0';
          blk3 = name[HDIMG_BLK_POS+2] - '0';
          if(file_name_length > 9) // HD00_NNN[1]
            blk4 = name[HDIMG_BLK_POS+3] - '0';
        }
        if(blk1 == 2 && blk2 == 5 && blk3 == 6) {
          blk = 256;
        } else if(blk1 == 1 && blk2 == 0 && blk3 == 2 && blk4 == 4) {
          blk = 1024;
        } else if(blk1 == 2 && blk2 == 0 && blk3 == 4 && blk4 == 8) {
          blk  = 2048;
        }

        if(id < NUM_SCSIID && lun < NUM_SCSILUN) {
          HDDIMG *h = &g_hddimg[id][lun];
          azlog("Trying to open ", name, " for id:", id, " lun:", lun);
          imageReady = hddimageOpen(h,name,id,lun,blk);
          if(imageReady) { // Marked as a responsive ID
            g_scsi_id_mask |= 1<<id;
          }
        } else {
          azlog("Invalid lun or id for image ", name);
        }
      } else {
        azlog("Skipping file ", name);
      }
    }
  }

  if(usedDefaultId > 0) {
    azlog("Some images did not specify a SCSI ID. Last file will be used at ID ", usedDefaultId);
  }
  root.close();

  // Error if there are 0 image files
  if(g_scsi_id_mask==0) {
    azlog("ERROR: No valid images found!");
    blinkStatus(BLINK_ERROR_NO_IMAGES);
  }

  // Print SCSI drive map
  azlog("SCSI drive map:");
  azlog_raw("ID");
  for (int lun = 0; lun < NUM_SCSILUN; lun++)
  {
    azlog_raw(":LUN", lun);
  }
  azlog_raw(":\n");

  for (int id = 0; id < NUM_SCSIID; id++)
  {
    azlog_raw(" ", id);
    for (int lun = 0; lun < NUM_SCSILUN; lun++)
    {
      HDDIMG *h = &g_hddimg[id][lun];
      if (h->m_file)
      {
        azlog_raw((h->m_blocksize<1000) ? ": " : ":");
        azlog_raw((int)h->m_blocksize);
      }
      else
      {
        azlog_raw(":----");
      }
    }
    azlog_raw(":\n");
  }
}

/*********************************/
/* SCSI bus communication        */
/*********************************/

#define active   1
#define inactive 0

#define SCSI_WAIT_ACTIVE(pin) \
  if (!SCSI_IN(pin)) { \
    if (!SCSI_IN(pin)) { \
      while(!SCSI_IN(pin) && !g_busreset); \
    } \
  }

#define SCSI_WAIT_INACTIVE(pin) \
  if (SCSI_IN(pin)) { \
    if (SCSI_IN(pin)) { \
      while(SCSI_IN(pin) && !g_busreset); \
    } \
  }

/*
 * Read by handshake.
 */
inline uint8_t readHandshake(void)
{
  SCSI_OUT(REQ,active);
  SCSI_WAIT_ACTIVE(ACK);
  delay_100ns(); // ACK.Fall to DB output delay 100ns(MAX)  (DTC-510B)
  uint8_t r = SCSI_IN_DATA();
  SCSI_OUT(REQ, inactive);
  SCSI_WAIT_INACTIVE(ACK);
  return r;  
}

/*
 * Write with a handshake.
 */
inline void writeHandshake(uint8_t d)
{
  SCSI_OUT_DATA(d);
  delay_100ns(); // DB hold time before REQ (DTC-510B)
  SCSI_OUT(REQ, active);
  SCSI_WAIT_ACTIVE(ACK);
  SCSI_RELEASE_DATA_REQ(); // Release data and REQ
  SCSI_WAIT_INACTIVE(ACK);
}

/*
 * Data in phase.
 *  Send len uint8_ts of data array p.
 */
void writeDataPhase(int len, const uint8_t* p)
{
  SCSI_OUT(MSG,inactive);
  SCSI_OUT(CD ,inactive);
  SCSI_OUT(IO ,  active);
  for (int i = 0; i < len; i++) {
    if (g_busreset) break;
    writeHandshake(p[i]);
  }
}

/* 
 * Data in phase.
 *  Send len blocks while reading from SD card.
 */
void writeDataPhase_FromSD(uint32_t adds, uint32_t len)
{
  uint32_t pos = adds * g_currentimg->m_blocksize;
  g_currentimg->m_file.seek(pos);

  SCSI_OUT(MSG,inactive);
  SCSI_OUT(CD ,inactive);
  SCSI_OUT(IO ,  active);

  uint8_t buf[MAX_BLOCKSIZE];

  for(uint32_t i = 0; i < len; i++)
  {
#if STREAM_SD_TRANSFERS
    azplatform_prepare_stream(buf);
    g_currentimg->m_file.read(buf, g_currentimg->m_blocksize);

    if (g_busreset) return;

    size_t status = azplatform_finish_stream();
    if (status == 0)
    {
      // Streaming did not happen, send data now
      azdbg("Streaming from SD failed, using fallback");
      writeDataPhase(g_currentimg->m_blocksize, buf);
    }
    else if (status != g_currentimg->m_blocksize)
    {
      azlog("Streaming failed halfway, data may be corrupt, aborting!");
      g_scsi_sts |= 2;
      return;
    }
#else
    g_currentimg->m_file.read(buf, g_currentimg->m_blocksize);
    writeDataPhase(g_currentimg->m_blocksize, buf);
#endif
  }
}

/*
 * Data out phase.
 *  len block read
 */
void readDataPhase(int len, uint8_t* p)
{
  SCSI_OUT(MSG,inactive);
  SCSI_OUT(CD ,inactive);
  SCSI_OUT(IO ,inactive);
  for(int i = 0; i < len; i++)
  {
    if (g_busreset) break;
    p[i] = readHandshake();
  }
}

/*
 * Data out phase.
 *  Write to SD card while reading len block.
 */
void readDataPhase_ToSD(uint32_t adds, uint32_t len)
{
  uint32_t pos = adds * g_currentimg->m_blocksize;
  g_currentimg->m_file.seek(pos);

  SCSI_OUT(MSG,inactive);
  SCSI_OUT(CD ,inactive);
  SCSI_OUT(IO ,inactive);

  uint8_t buf[MAX_BLOCKSIZE];

  for (uint32_t i = 0; i < len; i++)
  {
#if STREAM_SD_TRANSFERS
  azplatform_prepare_stream(buf);
  g_currentimg->m_file.write(buf, g_currentimg->m_blocksize);

  if (g_busreset) return;

  size_t status = azplatform_finish_stream();

  if (status == 0)
  {
    // Streaming did not happen, rewrite
    azdbg("Streaming to SD failed, using fallback");

    g_currentimg->m_file.seek(pos + i * g_currentimg->m_blocksize);

    readDataPhase(g_currentimg->m_blocksize, buf);
    g_currentimg->m_file.write(buf, g_currentimg->m_blocksize);
  }
  else if (status != g_currentimg->m_blocksize)
  {
    azlog("Streaming to SD failed halfway, data may be corrupt, aborting!");
    g_scsi_sts |= 2;
    return;
  }
#else
    readDataPhase(g_currentimg->m_blocksize, buf);
    g_currentimg->m_file.write(buf, g_currentimg->m_blocksize);
#endif
  }
  g_currentimg->m_file.flush();
}

/*********************************/
/* SCSI commands                 */
/*********************************/

// INQUIRY command processing.
uint8_t onInquiryCommand(uint8_t len)
{
  writeDataPhase(len < 36 ? len : 36, SCSI_INFO_BUF);
  return 0x00;
}

// REQUEST SENSE command processing.
void onRequestSenseCommand(uint8_t len)
{
  uint8_t buf[18] = {
    0x70,   //CheckCondition
    0,      //Segment number
    g_sensekey,   //Sense key
    0, 0, 0, 0,  //information
    17 - 7 ,   //Additional data length
    0,
  };
  g_sensekey = 0;
  writeDataPhase(len < 18 ? len : 18, buf);  
}

// READ CAPACITY command processing.
uint8_t onReadCapacityCommand(uint8_t pmi)
{
  if(!g_currentimg) return 0x02; // Image file absent
  
  uint32_t bl = g_currentimg->m_blocksize;
  uint32_t bc = g_currentimg->m_fileSize / bl;
  uint8_t buf[8] = {
    (uint8_t)(bc >> 24), (uint8_t)(bc >> 16), (uint8_t)(bc >> 8), (uint8_t)(bc),
    (uint8_t)(bl >> 24), (uint8_t)(bl >> 16), (uint8_t)(bl >> 8), (uint8_t)(bl)    
  };
  writeDataPhase(8, buf);
  return 0x00;
}

// READ6 / 10 Command processing.
uint8_t onReadCommand(uint32_t adds, uint32_t len)
{
  azdbg("Read at ", adds, " ", (int)len, " blocks");
  
  if(!g_currentimg) return 0x02; // Image file absent
  
  LED_ON();
  writeDataPhase_FromSD(adds, len);
  LED_OFF();
  return 0x00;
}

// WRITE6 / 10 Command processing.
uint8_t onWriteCommand(uint32_t adds, uint32_t len)
{
  azdbg("Write at ", adds, " ", (int)len, " blocks");
  
  if(!g_currentimg) return 0x02; // Image file absent
  
  LED_ON();
  readDataPhase_ToSD(adds, len);
  LED_OFF();
  return 0;
}

// MODE SENSE command processing for NEC_PC98
uint8_t onModeSenseCommand_NEC_PC98(uint8_t dbd, int cmd2, uint32_t len)
{
  if(!g_currentimg) return 0x02; // Image file absent

  uint8_t buf[512];

  int pageCode = cmd2 & 0x3F;

  // Assuming sector size 512, number of sectors 25, number of heads 8 as default settings
  int size = g_currentimg->m_fileSize;
  int cylinders = (int)(size >> 9);
  cylinders >>= 3;
  cylinders /= 25;
  int sectorsize = 512;
  int sectors = 25;
  int heads = 8;
  // Sector size
  int disksize = 0;
  for(disksize = 16; disksize > 0; --(disksize)) {
    if ((1 << disksize) == sectorsize)
      break;
  }
  // Number of blocks
  // uint32_t diskblocks = (uint32_t)(size >> disksize);
  memset(buf, 0, sizeof(buf)); 
  uint32_t a = 4;
  if(dbd == 0) {
    uint32_t bl = g_currentimg->m_blocksize;
    uint32_t bc = g_currentimg->m_fileSize / bl;
    uint8_t c[8] = {
      0,// Density code
      (uint8_t)(bc >> 16), (uint8_t)(bc >> 8), (uint8_t)bc,
      0, //Reserve
      (uint8_t)(bl >> 16), (uint8_t)(bl >> 8), (uint8_t)(bl)
    };
    memcpy(&buf[4], c, 8);
    a += 8;
    buf[3] = 0x08;
  }
  switch(pageCode) {
  case 0x3F:
  {
    buf[a + 0] = 0x01;
    buf[a + 1] = 0x06;
    a += 8;
  }
  case 0x03:  // drive parameters
  {
    buf[a + 0] = 0x80 | 0x03; // Page code
    buf[a + 1] = 0x16; // Page length
    buf[a + 2] = (uint8_t)(heads >> 8);// number of sectors / track
    buf[a + 3] = (uint8_t)(heads);// number of sectors / track
    buf[a + 10] = (uint8_t)(sectors >> 8);// number of sectors / track
    buf[a + 11] = (uint8_t)(sectors);// number of sectors / track
    int size = 1 << disksize;
    buf[a + 12] = (uint8_t)(size >> 8);// number of sectors / track
    buf[a + 13] = (uint8_t)(size);// number of sectors / track
    a += 24;
    if(pageCode != 0x3F) {
      break;
    }
  }
  case 0x04:  // drive parameters
  {
      azdbg("onModeSenseCommand_NEC_PC98: AddDrive");
      buf[a + 0] = 0x04; // Page code
      buf[a + 1] = 0x12; // Page length
      buf[a + 2] = (cylinders >> 16);// Cylinder length
      buf[a + 3] = (cylinders >> 8);
      buf[a + 4] = cylinders;
      buf[a + 5] = heads;   // Number of heads
      a += 20;
    if(pageCode != 0x3F) {
      break;
    }
  }
  default:
    break;
  }
  buf[0] = a - 1;
  writeDataPhase(len < a ? len : a, buf);
  return 0x00;
}

uint8_t onModeSenseCommand(uint8_t dbd, int cmd2, uint32_t len)
{
  if (g_scsi_quirks == SCSI_QUIRKS_NEC_PC98)
  {
    return onModeSenseCommand_NEC_PC98(dbd, cmd2, len);
  }

  if(!g_currentimg) return 0x02; // No image file

  uint8_t buf[512];

  memset(buf, 0, sizeof(buf));
  int pageCode = cmd2 & 0x3F;
  uint32_t a = 4;
  if(dbd == 0) {
    uint32_t bl =  g_currentimg->m_blocksize;
    uint32_t bc = g_currentimg->m_fileSize / bl;

    uint8_t c[8] = {
      0,//Density code
      (uint8_t)(bc >> 16), (uint8_t)(bc >> 8), (uint8_t)bc,
      0, //Reserve
      (uint8_t)(bl >> 16), (uint8_t)(bl >> 8), (uint8_t)bl    
    };
    memcpy(&buf[4], c, 8);
    a += 8;
    buf[3] = 0x08;
  }
  switch(pageCode) {
  case 0x3F:
  case 0x03:  //Drive parameters
    buf[a + 0] = 0x03; //Page code
    buf[a + 1] = 0x16; // Page length
    buf[a + 11] = 0x3F;//Number of sectors / track
    a += 24;
    if(pageCode != 0x3F) {
      break;
    }
  case 0x04:  //Drive parameters
    {
      uint32_t bc = g_currentimg->m_fileSize / g_currentimg->m_file;
      buf[a + 0] = 0x04; //Page code
      buf[a + 1] = 0x16; // Page length
      buf[a + 2] = bc >> 16;// Cylinder length
      buf[a + 3] = bc >> 8;
      buf[a + 4] = bc;
      buf[a + 5] = 1;   //Number of heads
      a += 24;
    }
    if(pageCode != 0x3F) {
      break;
    }
  default:
    break;
  }
  buf[0] = a - 1;
  writeDataPhase(len < a ? len : a, buf);
  return 0x00;
}

/*
 * dtc510b_setDriveparameter for SCSI_QUIRKS_SHARP
 */
typedef struct __attribute__((packed)) dtc500_cmd_c2_param_struct
{
  uint8_t StepPlusWidth;        // Default is 13.6usec (11)
  uint8_t StepPeriod;         // Default is  3  msec.(60)
  uint8_t StepMode;         // Default is  Bufferd (0)
  uint8_t MaximumHeadAdress;      // Default is 4 heads (3)
  uint8_t HighCylinderAddressuint8_t;  // Default set to 0   (0)
  uint8_t LowCylinderAddressuint8_t;   // Default is 153 cylinders (152)
  uint8_t ReduceWrietCurrent;     // Default is above Cylinder 128 (127)
  uint8_t DriveType_SeekCompleteOption;// (0)
  uint8_t Reserved8;          // (0)
  uint8_t Reserved9;          // (0)
} DTC510_CMD_C2_PARAM;

static uint8_t dtc510b_setDriveparameter(void)
{
  DTC510_CMD_C2_PARAM DriveParameter;
  uint16_t maxCylinder;
  uint16_t numLAD;
  // int StepPeriodMsec;

  // receive paramter
  writeDataPhase(sizeof(DriveParameter),(uint8_t *)(&DriveParameter));
 
  maxCylinder =
    (((uint16_t)DriveParameter.HighCylinderAddressuint8_t)<<8) |
    (DriveParameter.LowCylinderAddressuint8_t);
  numLAD = maxCylinder * (DriveParameter.MaximumHeadAdress+1);
  // StepPeriodMsec = DriveParameter.StepPeriod*50;
  azdbg(" StepPlusWidth      : ",DriveParameter.StepPlusWidth);
  azdbg(" StepPeriod         : ",DriveParameter.StepPeriod   );
  azdbg(" StepMode           : ",DriveParameter.StepMode     );
  azdbg(" MaximumHeadAdress  : ",DriveParameter.MaximumHeadAdress);
  azdbg(" CylinderAddress    : ",maxCylinder);
  azdbg(" ReduceWriteCurrent : ",DriveParameter.ReduceWrietCurrent);
  azdbg(" DriveType/SeekCompleteOption : ",DriveParameter.DriveType_SeekCompleteOption);
  azdbg(" Maximum LAD        : ",numLAD-1);
  return  0;
}

// Read the command, returns 0 on bus reset.
int readSCSICommand(uint8_t cmd[12])
{
  SCSI_OUT(MSG,inactive);
  SCSI_OUT(CD ,  active);
  SCSI_OUT(IO ,inactive);

  delay_ns(500);

  cmd[0] = readHandshake();
  if (g_busreset) return 0;

  static const int cmd_class_len[8]={6,10,10,6,6,12,6,6};
  int cmdlen = cmd_class_len[cmd[0] >> 5];

  for (int i = 1; i < cmdlen; i++)
  {
    cmd[i] = readHandshake();
    if (g_busreset) return 0;
  }

  azdbg("Got command (", cmdlen, " bytes): ", bytearray(cmd, cmdlen));

  return cmdlen;
}

// ATN message handling, returns false on abort/reset
bool onATNMessage()
{
  bool syncenable = false;
  int syncperiod = 50;
  int syncoffset = 0;

  SCSI_OUT(MSG,  active);
  SCSI_OUT(CD ,  active);
  SCSI_OUT(IO ,inactive);  

  uint8_t msg[256];
  memset(msg, 0x00, sizeof(msg));

  uint8_t response[64];
  size_t responselen = 0;
  memset(response, 0x00, sizeof(response));

  int msg_bytes = 0;
  while(SCSI_IN(ATN) && msg_bytes < (int)sizeof(msg)) {
    if (g_busreset)
    {
      return false;
    }
    
    msg[msg_bytes] = readHandshake();
    msg_bytes++;
  }

  if (msg_bytes == 0)
  {
    return false;
  }

  azdbg("Received MSG ", (int)msg_bytes, " bytes: ", bytearray(msg, msg_bytes));

  for (int i = 0; i < msg_bytes; i++)
  {
    // ABORT
    if (msg[i] == 0x06) {
      azdbg("MSG_ABORT");
      return false;
    }

    // BUS DEVICE RESET
    if (msg[i] == 0x0C) {
      azdbg("MSG_BUS_DEVICE_RESET");
      return false;
    }

    // IDENTIFY
    if (msg[i] >= 0x80) {
      azdbg("MSG_IDENTIFY");
    }

    // Extended message
    if (msg[i] == 0x01) {
      azdbg("MSG_EXTENDED");

      // Check only when synchronous transfer is possible
      if (!syncenable || msg[i + 2] != 0x01) {
        response[0] = 0x07;
        responselen = 1;
        break;
      }
      // Transfer period factor(50 x 4 = Limited to 200ns)
      syncperiod = msg[i + 3];
      if (syncperiod > 50) {
        syncperiod = 50;
      }
      // REQ/ACK offset(Limited to 16)
      syncoffset = msg[i + 4];
      if (syncoffset > 16) {
        syncoffset = 16;
      }
      // STDR response message generation
      response[0] = 0x01;
      response[1] = 0x03;
      response[2] = 0x01;
      response[3] = syncperiod;
      response[4] = syncoffset;
      responselen = 5;
      break;
    }
  }

  if (responselen > 0)
  {
    azdbg("Sending MSG response, ", (int)responselen, " bytes: ", bytearray(response, responselen));
    SCSI_OUT(MSG,  active);
    SCSI_OUT(CD ,  active);
    SCSI_OUT(IO ,  active);

    for (int i = 0; i < responselen; i++)
    {
      writeHandshake(response[i]);
    }
  }

  return true;
}

/*********************************/
/* Main SCSI handling loop       */
/*********************************/

void scsi_loop()
{
  SCSI_RELEASE_OUTPUTS();

  if (g_busreset)
  {
    g_busreset = false;
  }

  // Wait until RST = H, BSY = H, SEL = L
  while (SCSI_IN(BSY) || !SCSI_IN(SEL) || SCSI_IN(RST));

  // BSY+ SEL-
  // If the ID to respond is not driven, wait for the next
  uint8_t scsiid = SCSI_IN_DATA() & g_scsi_id_mask;
  if (scsiid == 0) {
    return; // Not for us
  }

  g_busreset = false;
  
  // Set BSY to-when selected
  SCSI_OUT(BSY, active);
  azdbg("SCSI device selected");
  
  // Ask for a TARGET-ID to respond
  g_scsi_id = 0;
  for(int i = 7; i >= 0; i--)
  {
    if (scsiid & (1<<i))
    {
      g_scsi_id = i;
      break;
    }
  }

  // Wait until SEL becomes inactive
  while (SCSI_IN(SEL))
  {
    if (g_busreset)
    {
      SCSI_RELEASE_OUTPUTS();
      return;
    }
  }
  
  if(SCSI_IN(ATN))
  {
    if (!onATNMessage())
    {
      // Abort/reset message
      SCSI_RELEASE_OUTPUTS();
      return;
    }
  }

  uint8_t cmd[12];
  int cmdlen = readSCSICommand(cmd);

  if (cmdlen == 0)
  {
    SCSI_RELEASE_OUTPUTS();
    return;
  }
  
  // LUN confirmation
  g_scsi_sts = cmd[1] & 0xe0;      // Preset LUN in status byte
  g_scsi_lun = g_scsi_sts >> 5;

  // HDD Image selection
  g_currentimg = (HDDIMG *)0; // None
  if( (g_scsi_lun <= NUM_SCSILUN) )
  {
    g_currentimg = &(g_hddimg[g_scsi_id][g_scsi_lun]); // There is an image
    if(!(g_currentimg->m_file.isOpen()))
      g_currentimg = (HDDIMG *)0;       // Image absent
  }
  
  azdbg("CMD ", cmd[0], " (", cmdlen, " bytes): ", "ID", (int)g_scsi_id, ", LUN", (int)g_scsi_lun);
  
  switch(cmd[0]) {
    case 0x00: azdbg("[Test Unit]"); break;
    case 0x01: azdbg("[Rezero Unit]"); break;
    case 0x03:
      azdbg("[RequestSense]");
      onRequestSenseCommand(cmd[4]);
      break;
    case 0x04: azdbg("[FormatUnit]"); break;
    case 0x06: azdbg("[FormatUnit]"); break;
    case 0x07: azdbg("[ReassignBlocks]"); break;
    case 0x08:
      azdbg("[Read6]");
      g_scsi_sts |= onReadCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
      break;
    case 0x0A:
      azdbg("[Write6]");
      g_scsi_sts |= onWriteCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
      break;
    case 0x0B: azdbg("[Seek6]"); break;
    case 0x12:
      azdbg("[Inquiry]");
      g_scsi_sts |= onInquiryCommand(cmd[4]);
      break;
    case 0x1A:
      azdbg("[ModeSense6]");
      g_scsi_sts |= onModeSenseCommand(cmd[1]&0x80, cmd[2], cmd[4]);
      break;
    case 0x1B: azdbg("[StartStopUnit]"); break;
    case 0x1E: azdbg("[PreAllowMed.Removal]"); break;
    case 0x25:
      azdbg("[ReadCapacity]");
      g_scsi_sts |= onReadCapacityCommand(cmd[8]);
      break;
    case 0x28:
      azdbg("[Read10]");
      g_scsi_sts |= onReadCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
      break;
    case 0x2A:
      azdbg("[Write10]");
      g_scsi_sts |= onWriteCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
      break;
    case 0x2B: azdbg("[Seek10]"); break;
    case 0x5A:
      azdbg("[ModeSense10]");
      onModeSenseCommand(cmd[1] & 0x80, cmd[2], ((uint32_t)cmd[7] << 8) | cmd[8]);
      break;
    case 0xc2:
      if (g_scsi_quirks == SCSI_QUIRKS_SHARP)
      {
        azdbg("[DTC510B setDriveParameter]");
        g_scsi_sts |= dtc510b_setDriveparameter();
      }
      else
      {
        g_scsi_sts |= 0x02;
        g_sensekey = 5;
      }
      break;
      
    default:
      azdbg("[Unknown CMD: ", cmd[0], "]");
      g_scsi_sts |= 0x02;
      g_sensekey = 5;
      break;
  }

  if (g_busreset) {
     SCSI_RELEASE_OUTPUTS();
     return;
  }

  azdbg("Status: ", g_scsi_sts);
  SCSI_OUT(MSG,inactive);
  SCSI_OUT(CD ,  active);
  SCSI_OUT(IO ,  active);
  writeHandshake(g_scsi_sts);
  
  if(g_busreset) {
     SCSI_RELEASE_OUTPUTS();
     return;
  }

  SCSI_OUT(MSG,  active);
  SCSI_OUT(CD ,  active);
  SCSI_OUT(IO ,  active);
  writeHandshake(0);

  azdbg("Command complete");

  SCSI_RELEASE_OUTPUTS();
}

void onBusReset(void)
{
  int filterlen = 100;

  if (g_scsi_quirks == SCSI_QUIRKS_SHARP)
  {
    // SASI I / F for X1 turbo has RST pulse write cycle +2 clock
    // Active about 1.25 us
    filterlen = 2;
  }

  while (filterlen > 0)
  {
    delay_100ns();
    if (!SCSI_IN(RST)) return;
    filterlen--;
  }

  SCSI_RELEASE_OUTPUTS();
  azdbg("BUSRESET");
  g_busreset = true;
}

// Check for any new data in log buffer and save it to file
void saveLog(FsFile &logfile)
{
  static uint32_t prev_log_pos = 0;
  static uint32_t prev_log_len = 0;
  uint32_t loglen = azlog_get_buffer_len();

  if (loglen != prev_log_len)
  {
    // Hold down BSY during the write to avoid timeouts on host
    if (SCSI_IN(BSY))
    {
      return;
    }

    SCSI_OUT(BSY, active);

    logfile.write(azlog_get_buffer(&prev_log_pos));
    logfile.flush();
    
    SCSI_OUT(BSY, inactive);

    prev_log_len = loglen;
  }
}

int main(void)
{
  azplatform_init();
  azplatform_set_rst_callback(&onBusReset);

  if(!SD.begin(SD_CONFIG))
  {
    azlog("SD card init failed, sdErrorCode: ", (int)SD.sdErrorCode(),
           " sdErrorData: ", (int)SD.sdErrorData());
    
    do
    {
      blinkStatus(BLINK_ERROR_NO_SD_CARD);
      delay(5000);
    } while (!SD.begin(SD_CONFIG));
    azlog("SD card init succeeded after retry");
  }

  uint64_t size = (uint64_t)SD.vol()->clusterCount() * SD.vol()->bytesPerCluster();
  azlog("SD card init succeeded, FAT", (int)SD.vol()->fatType(),
          " volume size: ", (int)(size / 1024 / 1024), " MB");

  readSCSIDeviceConfig();
  findHDDImages();

  azlog("Initialization complete!");
  azlog("Platform: ", g_azplatform_name);
  azlog("FW Version: ", g_azlog_firmwareversion);

  FsFile logfile;
  logfile = SD.open(LOGFILE, O_WRONLY | O_CREAT | O_TRUNC);
  saveLog(logfile);

  uint32_t prev_log_save = millis();

  while (1)
  {
    scsi_loop();

    // Save log once a second if there are new log messages
    if ((uint32_t)(millis() - prev_log_save) > 1000)
    {
      saveLog(logfile);
      prev_log_save = millis();
    }
  }
}
