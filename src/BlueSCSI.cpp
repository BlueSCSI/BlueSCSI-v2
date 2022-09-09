/*  
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

#include <Arduino.h> // For Platform.IO
#include <SdFat.h>
#include <setjmp.h>

#define DEBUG            0      // 0:No debug information output
                                // 1: Debug information output to USB Serial
                                // 2: Debug information output to LOG.txt (slow)

// Log File
#define VERSION "1.1-SNAPSHOT-20220627"
#define LOG_FILENAME "LOG.txt"

#include "BlueSCSI.h"
#include "scsi_cmds.h"
#include "scsi_sense.h"
#include "scsi_status.h"
#include "scsi_mode.h"

#ifdef USE_STM32_DMA
#warning "warning USE_STM32_DMA"
#endif

// SDFAT
SdFs SD;
FsFile LOG_FILE;

volatile bool m_isBusReset = false;   // Bus reset
volatile bool m_resetJmp = false;     // Call longjmp on reset
jmp_buf       m_resetJmpBuf;

byte          scsi_id_mask;              // Mask list of responding SCSI IDs
byte          m_id;                      // Currently responding SCSI-ID
byte          m_lun;                     // Logical unit number currently responding
byte          m_sts;                     // Status byte
byte          m_msg;                     // Message bytes
byte          m_buf[MAX_BLOCKSIZE];      // General purpose buffer
byte          m_scsi_buf[SCSI_BUF_SIZE]; // Buffer for SCSI READ/WRITE Buffer
unsigned      m_scsi_buf_size = 0;
byte          m_msb[256];                // Command storage bytes
SCSI_DEVICE scsi_device_list[NUM_SCSIID][NUM_SCSILUN]; // Maximum number
SCSI_INQUIRY_DATA default_hdd, default_optical;

// function table
byte (*scsi_command_table[MAX_SCSI_COMMAND])(SCSI_DEVICE *dev, const byte *cdb);

// scsi command functions
SCSI_COMMAND_HANDLER(onUnimplemented);
SCSI_COMMAND_HANDLER(onNOP);

SCSI_COMMAND_HANDLER(onRequestSense);
SCSI_COMMAND_HANDLER(onRead6);
SCSI_COMMAND_HANDLER(onRead10);
SCSI_COMMAND_HANDLER(onWrite6);
SCSI_COMMAND_HANDLER(onWrite10);
SCSI_COMMAND_HANDLER(onInquiry);
SCSI_COMMAND_HANDLER(onReadCapacity);
SCSI_COMMAND_HANDLER(onModeSense);
SCSI_COMMAND_HANDLER(onModeSelect);
SCSI_COMMAND_HANDLER(onVerify);
SCSI_COMMAND_HANDLER(onReadBuffer);
SCSI_COMMAND_HANDLER(onWriteBuffer);
SCSI_COMMAND_HANDLER(onReZeroUnit);
SCSI_COMMAND_HANDLER(onSendDiagnostic);
SCSI_COMMAND_HANDLER(onReadDefectData);
SCSI_COMMAND_HANDLER(onReadTOC);
SCSI_COMMAND_HANDLER(onReadDVDStructure);
SCSI_COMMAND_HANDLER(onReadDiscInformation);

static uint32_t MSFtoLBA(const byte *msf);
static void LBAtoMSF(const uint32_t lba, byte *msf);

static void flashError(const unsigned error);
void onBusReset(void);
void initFileLog(int);
void finalizeFileLog(void);
void findDriveImages(FsFile root);

/*
 * IO read.
 */
inline byte readIO(void)
{
  // Port input data register
  uint32_t ret = GPIOB->regs->IDR;
  byte bret = (byte)(~(ret>>8));
#if READ_PARITY_CHECK
  if((db_bsrr[bret]^ret)&1)
    m_sts |= 0x01; // parity error
#endif

  return bret;
}

// If config file exists, read the first three lines and copy the contents.
// File must be well formed or you will get junk in the SCSI Vendor fields.
void readSCSIDeviceConfig(SCSI_DEVICE *dev) {
  FsFile config_file = SD.open("scsi-config.txt", O_RDONLY);
  if (!config_file.isOpen()) {
    return;
  }
  SCSI_INQUIRY_DATA *iq = dev->inquiry_block;

  char vendor[9];
  memset(vendor, 0, sizeof(vendor));
  config_file.readBytes(vendor, sizeof(vendor));
  LOG_FILE.print("SCSI VENDOR: ");
  LOG_FILE.println(vendor);
  memcpy(&iq->vendor, vendor, 8);

  char product[17];
  memset(product, 0, sizeof(product));
  config_file.readBytes(product, sizeof(product));
  LOG_FILE.print("SCSI PRODUCT: ");
  LOG_FILE.println(product);
  memcpy(&iq->product, product, 16);

  char version[5];
  memset(version, 0, sizeof(version));
  config_file.readBytes(version, sizeof(version));
  LOG_FILE.print("SCSI VERSION: ");
  LOG_FILE.println(version);
  memcpy(&iq->revision, version, 4);
  config_file.close();
}

// read SD information and print to logfile
void readSDCardInfo()
{
  cid_t sd_cid;

  if(SD.card()->readCID(&sd_cid))
  {
    LOG_FILE.print("Sd MID:");
    LOG_FILE.print(sd_cid.mid, 16);
    LOG_FILE.print(" OID:");
    LOG_FILE.print(sd_cid.oid[0]);
    LOG_FILE.println(sd_cid.oid[1]);

    LOG_FILE.print("Sd Name:");
    LOG_FILE.print(sd_cid.pnm[0]);
    LOG_FILE.print(sd_cid.pnm[1]);
    LOG_FILE.print(sd_cid.pnm[2]);
    LOG_FILE.print(sd_cid.pnm[3]);
    LOG_FILE.println(sd_cid.pnm[4]);

    LOG_FILE.print("Sd Date:");
    LOG_FILE.print(sd_cid.mdtMonth());
    LOG_FILE.print("/");
    LOG_FILE.println(sd_cid.mdtYear());

    LOG_FILE.print("Sd Serial:");
    LOG_FILE.println(sd_cid.psn());
    LOG_FILE.sync();
  }
}

bool VerifyISOPVD(SCSI_DEVICE *dev, unsigned sector_size, bool mode2)
{ 
  int seek = 16 * sector_size;
  if(sector_size > CDROM_COMMON_SECTORSIZE) seek += 16;
  if(mode2) seek += 8;
  bool ret = false;

  dev->m_file->seekSet(seek);
  dev->m_file->read(m_buf, 2048);

  ret = ((m_buf[0] == 1 && !strncmp((char *)&m_buf[1], "CD001", 5) && m_buf[6] == 1) ||
        (m_buf[8] == 1 && !strncmp((char *)&m_buf[9], "CDROM", 5) && m_buf[14] == 1));

  dev->m_file->rewind();
  return ret;
}

/*
 * Open HDD image file
 */

bool hddimageOpen(SCSI_DEVICE *dev, FsFile *file,int id,int lun,int blocksize)
{
  dev->m_fileSize= 0;
  dev->m_sector_offset = 0;
  dev->m_blocksize = blocksize;
  dev->m_rawblocksize = blocksize;
  dev->m_file = file;
  if(!dev->m_file->isOpen()) { goto failed; }

  dev->m_fileSize = dev->m_file->size();
  
  if(dev->m_fileSize < 1) {
    LOG_FILE.println(" - file is 0 bytes, can not use.");
    goto failed;
  }

  if(dev->m_type == SCSI_DEVICE_OPTICAL) {
    LOG_FILE.print(" CDROM");
    dev->m_blocksize = CDROM_COMMON_SECTORSIZE;

    // Borrowed from PCEM
    if(VerifyISOPVD(dev, CDROM_COMMON_SECTORSIZE, false)) {
      dev->m_rawblocksize = CDROM_COMMON_SECTORSIZE;
      dev->m_mode2 = false;
    } else if(VerifyISOPVD(dev, CDROM_RAW_SECTORSIZE, false)) {
      dev->m_rawblocksize = CDROM_RAW_SECTORSIZE;
      dev->m_mode2 = false;
      dev->m_raw = true;
      dev->m_sector_offset = 16;
    } else if(VerifyISOPVD(dev, 2336, true)) {
      dev->m_rawblocksize = 2336;
      dev->m_mode2 = true;
    } else if(VerifyISOPVD(dev, CDROM_RAW_SECTORSIZE, true)) {
      dev->m_rawblocksize = CDROM_RAW_SECTORSIZE;
      dev->m_mode2 = true;
      dev->m_raw = true;
      dev->m_sector_offset = 24;
    } else {
      // Last ditch effort
      // size must be less than 700MB
      if(dev->m_fileSize > 912579600) {
        goto failed;
      }

      dev->m_raw = true;

      if(!(dev->m_fileSize % CDROM_COMMON_SECTORSIZE)) {
        // try a multiple of 2048
        dev->m_blocksize = CDROM_COMMON_SECTORSIZE;
        dev->m_rawblocksize = CDROM_COMMON_SECTORSIZE;
      } else {
        // I give up!
        LOG_FILE.println(" InvalidISO");
        goto failed;
      }
    }
  } else {
    LOG_FILE.print(" HDD");
  }
  dev->m_blockcount = dev->m_fileSize / dev->m_blocksize;

  // check blocksize dummy file
  LOG_FILE.print(" / ");
  LOG_FILE.print(dev->m_fileSize);
  LOG_FILE.print("bytes / ");
  LOG_FILE.print(dev->m_fileSize / 1024);
  LOG_FILE.print("KiB / ");
  LOG_FILE.print(dev->m_fileSize / 1024 / 1024);
  LOG_FILE.println("MiB");

  if(dev->m_type == SCSI_DEVICE_OPTICAL) {
    LOG_FILE.print(" MODE2:");LOG_FILE.print(dev->m_mode2);
    LOG_FILE.print(" BlockSize:");LOG_FILE.println(dev->m_rawblocksize);
  }
  return true; // File opened

failed:    
  
  dev->m_file->close();
  dev->m_fileSize = dev->m_blocksize = 0; // no file
  delete dev->m_file;
  dev->m_file = NULL;
  return false;
}

/*
 * Initialization.
 *  Initialize the bus and set the PIN orientation
 */
void setup()
{
  // PA15 / PB3 / PB4 Cannot be used
  // JTAG Because it is used for debugging.
  enableDebugPorts();

  // Setup BSRR table
  for (unsigned i = 0; i <= 255; i++) {
    db_bsrr[i] = DBP(i);
  }

  // Default all SCSI command handlers to onUnimplemented
  for(unsigned i = 0; i < MAX_SCSI_COMMAND; i++)
  {
    scsi_command_table[i] = onUnimplemented;
  }

  // SCSI commands that just need to return ok
  scsi_command_table[SCSI_FORMAT_UNIT4] = onNOP;
  scsi_command_table[SCSI_FORMAT_UNIT6] = onNOP;
  scsi_command_table[SCSI_REASSIGN_BLOCKS] = onNOP;
  scsi_command_table[SCSI_SEEK6] = onNOP;
  scsi_command_table[SCSI_SEEK10] = onNOP;
  scsi_command_table[SCSI_START_STOP_UNIT] = onNOP;
  scsi_command_table[SCSI_PREVENT_ALLOW_REMOVAL] = onNOP;
  scsi_command_table[SCSI_RELEASE] = onNOP;
  scsi_command_table[SCSI_RESERVE] = onNOP;
  scsi_command_table[SCSI_TEST_UNIT_READY] = onNOP;
  
  // SCSI commands that have handlers
  scsi_command_table[SCSI_REZERO_UNIT] = onReZeroUnit;
  scsi_command_table[SCSI_REQUEST_SENSE] = onRequestSense;
  scsi_command_table[SCSI_READ6] = onRead6;
  scsi_command_table[SCSI_READ10] = onRead10;
  scsi_command_table[SCSI_WRITE6] = onWrite6;
  scsi_command_table[SCSI_WRITE10] = onWrite10;
  scsi_command_table[SCSI_INQUIRY] = onInquiry;
  scsi_command_table[SCSI_READ_CAPACITY] = onReadCapacity;
  scsi_command_table[SCSI_MODE_SENSE6] =  onModeSense;
  scsi_command_table[SCSI_MODE_SENSE10] = onModeSense;
  scsi_command_table[SCSI_MODE_SELECT6] = onModeSelect;
  scsi_command_table[SCSI_MODE_SELECT10] = onModeSelect;
  scsi_command_table[SCSI_VERIFY10] = onVerify;
  scsi_command_table[SCSI_READ_BUFFER] = onReadBuffer;
  scsi_command_table[SCSI_WRITE_BUFFER] = onWriteBuffer;
  scsi_command_table[SCSI_SEND_DIAG] = onSendDiagnostic;
  scsi_command_table[SCSI_READ_DEFECT_DATA] = onReadDefectData;
  scsi_command_table[SCSI_READ_TOC] = onReadTOC;
  scsi_command_table[SCSI_READ_DVD_STRUCTURE] = onReadDVDStructure;
  scsi_command_table[SCSI_READ_DISC_INFORMATION] = onReadDiscInformation;

  // clear and initialize default inquiry blocks
  // default SCSI HDD
  memset(&default_hdd, 0, sizeof(default_hdd));
  default_hdd.ansi_version = 1;
  default_hdd.response_format = 1;
  default_hdd.additional_length = 31;
  memcpy(&default_hdd.vendor, "QUANTUM", 7);
  memcpy(&default_hdd.product, "BLUESCSI F1", 11);
  memcpy(&default_hdd.revision, "1.0", 3);

  // default SCSI CDROM
  memset(&default_optical, 0, sizeof(default_optical));
  default_optical.peripheral_device_type = 5;
  default_optical.rmb = 1;
  default_optical.ansi_version = 1;
  default_optical.response_format = 1;
  default_optical.additional_length = 42;
  default_optical.sync = 1;
  memcpy(&default_optical.vendor, "BLUESCSI", 8);
  memcpy(&default_optical.product, "CD-ROM CDU-55S", 14);
  memcpy(&default_optical.revision, "1.9a", 4);
  default_optical.release = 0x20;
  memcpy(&default_optical.revision_date, "1995", 4);

  // Serial initialization
#if DEBUG > 0
  Serial.begin(9600);
  // If using a USB->TTL monitor instead of USB serial monitor - you can uncomment this.
  //while (!Serial);
#endif

  // PIN initialization
  gpio_mode(LED2, GPIO_OUTPUT_PP);
  gpio_mode(LED, GPIO_OUTPUT_OD);

  // Image Set Select Init
  gpio_mode(IMAGE_SELECT1, GPIO_INPUT_PU);
  gpio_mode(IMAGE_SELECT2, GPIO_INPUT_PU);
  pinMode(IMAGE_SELECT1, INPUT);
  pinMode(IMAGE_SELECT2, INPUT);
  int image_file_set = ((digitalRead(IMAGE_SELECT1) == LOW) ? 1 : 0) | ((digitalRead(IMAGE_SELECT2) == LOW) ? 2 : 0);

  LED_OFF();

#ifdef XCVR
  // Transceiver Pin Initialization
  pinMode(TR_TARGET, OUTPUT);
  pinMode(TR_INITIATOR, OUTPUT);
  pinMode(TR_DBP, OUTPUT);
  
  TRANSCEIVER_IO_SET(vTR_INITIATOR,TR_INPUT);
#endif

  //GPIO(SCSI BUS)Initialization
  //Port setting register (lower)
//  GPIOB->regs->CRL |= 0x000000008; // SET INPUT W/ PUPD on PAB-PB0
  //Port setting register (upper)
  //GPIOB->regs->CRH = 0x88888888; // SET INPUT W/ PUPD on PB15-PB8
//  GPIOB->regs->ODR = 0x0000FF00; // SET PULL-UPs on PB15-PB8
  // DB and DP are input modes
  SCSI_DB_INPUT()

#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_DBP,TR_INPUT);
  
  // Initiator port
  pinMode(ATN, INPUT);
  pinMode(BSY, INPUT);
  pinMode(ACK, INPUT);
  pinMode(RST, INPUT);
  pinMode(SEL, INPUT);
  TRANSCEIVER_IO_SET(vTR_INITIATOR,TR_INPUT);

  // Target port
  pinMode(MSG, INPUT);
  pinMode(CD, INPUT);
  pinMode(REQ, INPUT);
  pinMode(IO, INPUT);
  TRANSCEIVER_IO_SET(vTR_TARGET,TR_INPUT);
#else
  // Input port
  gpio_mode(ATN, GPIO_INPUT_PU);
  gpio_mode(BSY, GPIO_INPUT_PU);
  gpio_mode(ACK, GPIO_INPUT_PU);
  gpio_mode(RST, GPIO_INPUT_PU);
  gpio_mode(SEL, GPIO_INPUT_PU);
  // Output port
  gpio_mode(MSG, GPIO_OUTPUT_OD);
  gpio_mode(CD,  GPIO_OUTPUT_OD);
  gpio_mode(REQ, GPIO_OUTPUT_OD);
  gpio_mode(IO,  GPIO_OUTPUT_OD);
  // Turn off the output port
  SCSI_TARGET_INACTIVE()
#endif

  //Occurs when the RST pin state changes from HIGH to LOW
  //attachInterrupt(RST, onBusReset, FALLING);

  // Try Full and half clock speeds.
  LED_ON();
  int mhz = 0;
  if (SD.begin(SdSpiConfig(PA4, DEDICATED_SPI, SD_SCK_MHZ(50), &SPI)))
  {
    mhz = 50;
  }
  else if (SD.begin(SdSpiConfig(PA4, DEDICATED_SPI, SD_SCK_MHZ(25), &SPI)))
  {
    mhz = 25;
  }
  LED_OFF();

  if(mhz == 0) {
#if DEBUG > 0
    Serial.println("SD initialization failed!");
#endif
    flashError(ERROR_NO_SDCARD);
  }
  initFileLog(mhz);
  readSDCardInfo();

  //HD image file open
  scsi_id_mask = 0x00;

  // Iterate over the root path in the SD card looking for candidate image files.
  FsFile root;

  char image_set_dir_name[] = "/ImageSetX/";
  image_set_dir_name[9] = char(image_file_set) + 0x30;
  root.open(image_set_dir_name);
  if (root.isDirectory()) {
    LOG_FILE.print("Looking for images in: ");
    LOG_FILE.println(image_set_dir_name);
    LOG_FILE.sync();
  } else {
    root.close();
    root.open("/");
  }

  findDriveImages(root);
  root.close();

  FsFile images_all_dir;
  images_all_dir.open("/ImageSetAll/");
  if (images_all_dir.isDirectory()) {
    LOG_FILE.println("Looking for images in: /ImageSetAll/");
    LOG_FILE.sync();
    findDriveImages(images_all_dir);
  }
  images_all_dir.close();

  // Error if there are 0 image files
  if(scsi_id_mask==0) {
    LOG_FILE.println("ERROR: No valid images found!");
    flashError(ERROR_FALSE_INIT);
  }

  finalizeFileLog();
  LED_OFF();
  //Occurs when the RST pin state changes from HIGH to LOW
  attachInterrupt(RST, onBusReset, FALLING);
}

void findDriveImages(FsFile root) {
  bool image_ready;
  FsFile *file = NULL;
  char path_name[MAX_FILE_PATH+1];
  root.getName(path_name, sizeof(path_name));
  SD.chdir(path_name);
  SCSI_DEVICE *dev = NULL;

  while (1) {
    // Directories can not be opened RDWR, so it will fail, but fails the same way with no file/dir, so we need to peek at the file first.
    FsFile file_test = root.openNextFile(O_RDONLY);
    char name[MAX_FILE_PATH+1];
    file_test.getName(name, MAX_FILE_PATH+1);

    // Skip directories and already open files.
    if(file_test.isDir() || strncmp(name, "LOG.txt", 7) == 0) {
      file_test.close();
      continue;
    }
    // If error there is no next file to open.
    if(file_test.getError() > 0) {
      file_test.close();
      break;
    }
    // Valid file, open for reading/writing.
    file = new FsFile(SD.open(name, O_RDWR));
    if(file && file->isFile()) {
      SCSI_DEVICE_TYPE device_type;
      if(tolower(name[1]) != 'd') {
        file->close();
        delete file;
        LOG_FILE.print("Not an image: ");
        LOG_FILE.println(name);
        continue;
      }
      
      switch (tolower(name[0])) {
      case 'h': device_type = SCSI_DEVICE_HDD;
      break;
      case 'c': device_type = SCSI_DEVICE_OPTICAL;
      break;
      default:
        file->close();
        delete file;
        LOG_FILE.print("Not an image: ");
        LOG_FILE.println(name);
        continue;
      }

      // Defaults for Hard Disks
      int id  = 1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
      int lun = 0;
      int blk = 512;

      // Positionally read in and coerase the chars to integers.
      // We only require the minimum and read in the next if provided.
      int file_name_length = strlen(name);
      if(file_name_length > 2) { // HD[N]
        int tmp_id = name[HDIMG_ID_POS] - '0';

        // If valid id, set it, else use default
        if(tmp_id > -1 && tmp_id < 8) {
          id = tmp_id;
        } else {
          LOG_FILE.print(name);
          LOG_FILE.println(" - bad SCSI id in filename, Using default ID 1");
        }
      }

      if(file_name_length > 3) { // HDN[N]
        int tmp_lun = name[HDIMG_LUN_POS] - '0';

        // If valid lun, set it, else use default
        if(tmp_lun == 0 || tmp_lun == 1) {
          lun = tmp_lun;
        } else {
          LOG_FILE.print(name);
          LOG_FILE.println(" - bad SCSI LUN in filename, Using default LUN ID 0");
        }
      }

      int blk1 = 0, blk2, blk3, blk4 = 0;
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
        dev = &scsi_device_list[id][lun];
        LOG_FILE.print(" - ");
        LOG_FILE.print(name);
        dev->m_type = device_type;
        image_ready = hddimageOpen(dev, file, id, lun, blk);
        if(image_ready) { // Marked as a responsive ID
          scsi_id_mask |= 1<<id;
          
          switch(dev->m_type)
          {
             case SCSI_DEVICE_HDD:
              // default SCSI HDD
              dev->inquiry_block = &default_hdd;        
              break;
              
              case SCSI_DEVICE_OPTICAL:
              // default SCSI CDROM
              dev->inquiry_block = &default_optical;
              break;
          }

          readSCSIDeviceConfig(dev);
        }
      }      
    }
    LOG_FILE.sync();
  }
  // cd .. before going back.
  SD.chdir("/");
}

/*
 * Setup initialization logfile
 */
void initFileLog(int success_mhz) {
  LOG_FILE = SD.open(LOG_FILENAME, O_WRONLY | O_CREAT | O_TRUNC);
  LOG_FILE.println("BlueSCSI <-> SD - https://github.com/erichelgeson/BlueSCSI");
  LOG_FILE.print("VER: ");
  LOG_FILE.print(VERSION);
  LOG_FILE.println(BUILD_TAGS);
  LOG_FILE.print("DEBUG:");
  LOG_FILE.print(DEBUG);
  LOG_FILE.print(" SDFAT_FILE_TYPE:");
  LOG_FILE.println(SDFAT_FILE_TYPE);
  LOG_FILE.print("SdFat version: ");
  LOG_FILE.println(SD_FAT_VERSION_STR);
  LOG_FILE.print("Sd Format: ");
  switch(SD.vol()->fatType()) {
    case FAT_TYPE_EXFAT:
    LOG_FILE.println("exFAT");
    break;
    case FAT_TYPE_FAT32:
    LOG_FILE.print("FAT32");
    case FAT_TYPE_FAT16:
    LOG_FILE.print("FAT16");
    default:
    LOG_FILE.println(" - Consider formatting the SD Card with exFAT for improved performance.");
  }
  LOG_FILE.print("SPI speed: ");
  LOG_FILE.print(success_mhz);
  LOG_FILE.println("Mhz");
  if(success_mhz == 25) {
    LOG_FILE.println("SPI running at half speed - read https://github.com/erichelgeson/BlueSCSI/wiki/Slow-SPI");
  }
  LOG_FILE.print("SdFat Max FileName Length: ");
  LOG_FILE.println(MAX_FILE_PATH);
  LOG_FILE.println("Initialized SD Card - let's go!");
  LOG_FILE.sync();
}

/*
 * Finalize initialization logfile
 */
void finalizeFileLog() {
  // View support drive map
  LOG_FILE.print("ID");
  for(int lun=0;lun<NUM_SCSILUN;lun++)
  {
    LOG_FILE.print(":LUN");
    LOG_FILE.print(lun);
  }
  LOG_FILE.println(":");
  //
  for(int id=0;id<NUM_SCSIID;id++)
  {
    LOG_FILE.print(" ");
    LOG_FILE.print(id);
    for(int lun=0;lun<NUM_SCSILUN;lun++)
    {
      SCSI_DEVICE *dev = &scsi_device_list[id][lun];
      if( (lun<NUM_SCSILUN) && (dev->m_file))
      {
        LOG_FILE.print((dev->m_blocksize<1000) ? ": " : ":");
        LOG_FILE.print(dev->m_blocksize);
      }
      else      
        LOG_FILE.print(":----");
    }
    LOG_FILE.println(":");
  }
  LOG_FILE.println("Finished initialization of SCSI Devices - Entering main loop.");
  LOG_FILE.sync();
  #if DEBUG < 2
  LOG_FILE.close();
  #endif
}

static void flashError(const unsigned error)
{
  while(true) {
    for(uint8_t i = 0; i < error; i++) {
      LED_ON();
      delay(250);
      LED_OFF();
      delay(250);
    }
    delay(3000);
  }
}

/*
 * Return from exception and call longjmp
 */
void __attribute__ ((noinline)) longjmpFromInterrupt(jmp_buf jmpb, int retval) __attribute__ ((noreturn));
void longjmpFromInterrupt(jmp_buf jmpb, int retval) {
  // Address of longjmp with the thumb bit cleared
  const uint32_t longjmpaddr = ((uint32_t)longjmp) & 0xfffffffe;
  const uint32_t zero = 0;
  // Default PSR value, function calls don't require any particular value
  const uint32_t PSR = 0x01000000;
  // For documentation on what this is doing, see:
  // https://developer.arm.com/documentation/dui0552/a/the-cortex-m3-processor/exception-model/exception-entry-and-return
  // Stack frame needs to have R0-R3, R12, LR, PC, PSR (from bottom to top)
  // This is being set up to have R0 and R1 contain the parameters passed to longjmp, and PC is the address of the longjmp function.
  // This is using existing stack space, rather than allocating more, as longjmp is just going to unroll the stack even further.
  // 0xfffffff9 is the EXC_RETURN value to return to thread mode.
  asm (
      "str %0, [sp];\
      str %1, [sp, #4];\
      str %2, [sp, #8];\
      str %2, [sp, #12];\
      str %2, [sp, #16];\
      str %2, [sp, #20];\
      str %3, [sp, #24];\
      str %4, [sp, #28];\
      ldr lr, =0xfffffff9;\
      bx lr"
       :: "r"(jmpb),"r"(retval),"r"(zero), "r"(longjmpaddr), "r"(PSR)
  );
}

/*
 * Bus reset interrupt.
 */
void onBusReset(void)
{
  if(isHigh(gpio_read(RST))) {
    delayMicroseconds(20);
    if(isHigh(gpio_read(RST))) {
  // BUS FREE is done in the main process
//      gpio_mode(MSG, GPIO_OUTPUT_OD);
//      gpio_mode(CD,  GPIO_OUTPUT_OD);
//      gpio_mode(REQ, GPIO_OUTPUT_OD);
//      gpio_mode(IO,  GPIO_OUTPUT_OD);
      // Should I enter DB and DBP once?
      SCSI_DB_INPUT()

      LOGN("BusReset!");
      if (m_resetJmp) {
        m_resetJmp = false;
        // Jumping out of the interrupt handler, so need to clear the interupt source.
        uint8 exti = PIN_MAP[RST].gpio_bit;
        EXTI_BASE->PR = (1U << exti);
        longjmpFromInterrupt(m_resetJmpBuf, 1);
      } else {
        m_isBusReset = true;
      }
    }
  }
}
    
/*
 * Enable the reset longjmp, and check if reset fired while it was disabled.
 */
void enableResetJmp(void) {
  m_resetJmp = true;
  if (m_isBusReset) {
    longjmp(m_resetJmpBuf, 1);
  }
}

/*
 * Read by handshake.
 */
inline byte readHandshake(void)
{
  SCSI_OUT(vREQ,active)
  //SCSI_DB_INPUT()
  while( ! SCSI_IN(vACK));
  byte r = readIO();
  SCSI_OUT(vREQ,inactive)
  while( SCSI_IN(vACK));
  return r;  
}

/*
 * Write with a handshake.
 */
inline void writeHandshake(byte d)
{
  // This has a 400ns bus settle delay built in. Not optimal for multi-byte transfers.
  GPIOB->regs->BSRR = db_bsrr[d]; // setup DB,DBP (160ns)
#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_DBP,TR_OUTPUT)
#endif
  SCSI_DB_OUTPUT() // (180ns)
  // ACK.Fall to DB output delay 100ns(MAX)  (DTC-510B)
  SCSI_OUT(vREQ,inactive) // setup wait (30ns)
  SCSI_OUT(vREQ,inactive) // setup wait (30ns)
  SCSI_OUT(vREQ,inactive) // setup wait (30ns)
  SCSI_OUT(vREQ,active)   // (30ns)
  //while(!SCSI_IN(vACK)) { if(m_isBusReset){ SCSI_DB_INPUT() return; }}
  while(!SCSI_IN(vACK));
  // ACK.Fall to REQ.Raise delay 500ns(typ.) (DTC-510B)
  GPIOB->regs->BSRR = DBP(0xff);  // DB=0xFF , SCSI_OUT(vREQ,inactive)
  // REQ.Raise to DB hold time 0ns
  SCSI_DB_INPUT() // (150ns)
#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_DBP,TR_INPUT)
#endif
  while( SCSI_IN(vACK));
}

#pragma GCC push_options
#pragma GCC optimize ("-Os")
/*
 * This loop is tuned to repeat the following pattern:
 * 1) Set REQ
 * 2) 5 cycles of work/delay
 * 3) Wait for ACK
 * Cycle time tunings are for 72MHz STM32F103
 * Alignment matters. For the 3 instruction wait loops,it looks like crossing
 * an 8 byte prefetch buffer can add 2 cycles of wait every branch taken.
 */
void writeDataLoop(uint32_t blocksize, const byte* srcptr) __attribute__ ((aligned(8)));
void writeDataLoop(uint32_t blocksize, const byte* srcptr)
{
#define REQ_ON() (port_b->BRR = req_bit);
#define FETCH_BSRR_DB() (bsrr_val = bsrr_tbl[*srcptr++])
#define REQ_OFF_DB_SET(BSRR_VAL) port_b->BSRR = BSRR_VAL;
#define WAIT_ACK_ACTIVE()   while((*port_a_idr>>(vACK&15)&1))
#define WAIT_ACK_INACTIVE() while(!(*port_a_idr>>(vACK&15)&1))

  register const byte *endptr= srcptr + blocksize;  // End pointer

  register const uint32_t *bsrr_tbl = db_bsrr;      // Table to convert to BSRR
  register uint32_t bsrr_val;                       // BSRR value to output (DB, DBP, REQ = ACTIVE)

  register uint32_t req_bit = BITMASK(vREQ);
  register gpio_reg_map *port_b = PBREG;
  register volatile uint32_t *port_a_idr = &(GPIOA->regs->IDR);

  // Start the first bus cycle.
  FETCH_BSRR_DB();
  REQ_OFF_DB_SET(bsrr_val);
  REQ_ON();
  FETCH_BSRR_DB();
  WAIT_ACK_ACTIVE();
  REQ_OFF_DB_SET(bsrr_val);
  // Align the starts of the do/while and WAIT loops to an 8 byte prefetch.
  asm("nop.w;nop");
  do{
    WAIT_ACK_INACTIVE();
    REQ_ON();
    // 4 cycles of work
    FETCH_BSRR_DB();
    // Extra 1 cycle delay while keeping the loop within an 8 byte prefetch.
    asm("nop");
    WAIT_ACK_ACTIVE();
    REQ_OFF_DB_SET(bsrr_val);
    // Extra 1 cycle delay, plus 4 cycles for the branch taken with prefetch.
    asm("nop");
  }while(srcptr < endptr);
  WAIT_ACK_INACTIVE();
  // Finish the last bus cycle, byte is already on DB.
  REQ_ON();
  WAIT_ACK_ACTIVE();
  REQ_OFF_DB_SET(bsrr_val);
  WAIT_ACK_INACTIVE();
}
#pragma GCC pop_options

/*
 * Data in phase.
 *  Send len bytes of data array p.
 */
void writeDataPhase(int len, const byte* p)
{
  LOG(" DI ");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAIN);
  // Bus settle delay 400ns. Following code was measured at 800ns before REQ asserted. STM32F103.
#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_DBP,TR_OUTPUT)
#endif
  SCSI_DB_OUTPUT()
  writeDataLoop(len, p);
}

/*
 * Data in phase.
 *  Send len block while reading from SD card.
 */
void writeDataPhaseSD(SCSI_DEVICE *dev, uint32_t adds, uint32_t len)
{
  LOG (" DI(SD) ");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAIN);
  //Bus settle delay 400ns, file.seek() measured at over 1000ns.
  uint64_t pos = (uint64_t)adds * dev->m_rawblocksize;
  dev->m_file->seekSet(pos);
#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_DBP,TR_OUTPUT)
#endif
  SCSI_DB_OUTPUT()
  
  for(uint32_t i = 0; i < len; i++) {
      // Asynchronous reads will make it faster ...
    m_resetJmp = false;
    dev->m_file->read(m_buf, dev->m_rawblocksize);
    enableResetJmp();

    writeDataLoop(dev->m_blocksize, &m_buf[dev->m_sector_offset]);
  }
}

#pragma GCC push_options
#pragma GCC optimize ("-Os")
    
/*
 * See writeDataLoop for optimization info.
 */
void readDataLoop(uint32_t blockSize, byte* dstptr) __attribute__ ((aligned(16)));
void readDataLoop(uint32_t blockSize, byte* dstptr)
{
  register byte *endptr= dstptr + blockSize - 1;

#define REQ_ON() (port_b->BRR = req_bit);
#define REQ_OFF() (port_b->BSRR = req_bit);
#define WAIT_ACK_ACTIVE()   while((*port_a_idr>>(vACK&15)&1))
#define WAIT_ACK_INACTIVE() while(!(*port_a_idr>>(vACK&15)&1))
  register uint32_t req_bit = BITMASK(vREQ);
  register gpio_reg_map *port_b = PBREG;
  register volatile uint32_t *port_a_idr = &(GPIOA->regs->IDR);
  REQ_ON();
  // Fastest alignment obtained by trial and error.
  // Wait loop is within an 8 byte prefetch buffer.
  asm("nop");
  do {
    WAIT_ACK_ACTIVE();
    uint32_t ret = port_b->IDR;
    REQ_OFF();
    *dstptr++ = ~(ret >> 8);
    // Move wait loop in to a single 8 byte prefetch buffer
    asm("nop;nop;nop");
    WAIT_ACK_INACTIVE();
    REQ_ON();
    // Extra 1 cycle delay
    asm("nop");
  } while(dstptr<endptr);
  WAIT_ACK_ACTIVE();
  uint32_t ret = GPIOB->regs->IDR;
  REQ_OFF();
  *dstptr = ~(ret >> 8);
  WAIT_ACK_INACTIVE();
}
#pragma GCC pop_options

/*
 * Data out phase.
 *  len block read
 */
void readDataPhase(int len, byte* p)
{
  LOG(" DO ");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAOUT);
  // Bus settle delay 400ns. The following code was measured at 450ns before REQ asserted. STM32F103.
  readDataLoop(len, p);
}

/*
 * Data out phase.
 *  Write to SD card while reading len block.
 */
void readDataPhaseSD(SCSI_DEVICE *dev, uint32_t adds, uint32_t len)
{
  LOG(" DO(SD) ");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAOUT);
  //Bus settle delay 400ns, file.seek() measured at over 1000ns.

  uint64_t pos = (uint64_t)adds * dev->m_blocksize;
  dev->m_file->seekSet(pos);
  for(uint32_t i = 0; i < len; i++) {
    m_resetJmp = true;
    readDataLoop(dev->m_blocksize, m_buf);
    m_resetJmp = false;
    dev->m_file->write(m_buf, dev->m_blocksize);
    // If a reset happened while writing, break and let the flush happen before it is handled.
    if (m_isBusReset) {
      break;
    }
  }
  dev->m_file->flush();
  enableResetJmp();
}

/*
 * Data out phase.
 * Compare to SD card while reading len block.
 */
void verifyDataPhaseSD(SCSI_DEVICE *dev, uint32_t adds, uint32_t len)
{
  LOG(" DO(SD) ");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAOUT);
  //Bus settle delay 400ns, file.seek() measured at over 1000ns.

  uint64_t pos = (uint64_t)adds * dev->m_blocksize;
  dev->m_file->seekSet(pos);
  for(uint32_t i = 0; i < len; i++) {
    readDataLoop(dev->m_blocksize, m_buf);
    // This has just gone through the transfer to make things work, a compare would go here.
  }
}


/*
 * MsgIn2.
 */
void MsgIn2(int msg)
{
  LOG(" MI:"); LOGHEX(msg); LOG(" ");
  SCSI_PHASE_CHANGE(SCSI_PHASE_MESSAGEIN);
  // Bus settle delay 400ns built in to writeHandshake
  writeHandshake(msg);
}

/*
 * Main loop.
 */
void loop() 
{
#ifdef XCVR
  // Reset all DB and Target pins, switch transceivers to input
  // Precaution against bugs or jumps which don't clean up properly
  SCSI_DB_INPUT();
  TRANSCEIVER_IO_SET(vTR_DBP,TR_INPUT)
  SCSI_TARGET_INACTIVE();
  TRANSCEIVER_IO_SET(vTR_INITIATOR,TR_INPUT)
#endif

  //int msg = 0;
  m_msg = 0;
  m_lun = 0xff;
  SCSI_DEVICE *dev = (SCSI_DEVICE *)0; // HDD image for current SCSI-ID, LUN

  do {} while( SCSI_IN(vBSY) || !SCSI_IN(vSEL) || SCSI_IN(vRST));
  //do {} while( !SCSI_IN(vBSY) || SCSI_IN(vRST));
  // We're in ARBITRATION
  //LOG(" A:"); LOGHEX(readIO()); LOG(" ");
  
  //do {} while( SCSI_IN(vBSY) || !SCSI_IN(vSEL) || SCSI_IN(vRST));
  //LOG(" S:"); LOGHEX(readIO()); LOG(" ");
  // We're in SELECTION
  
  byte scsiid = readIO() & scsi_id_mask;
  if(SCSI_IN(vIO) || (scsiid) == 0) {
    delayMicroseconds(1);
    return;
  }
  // We've been selected

  #ifdef XCVR
  // Reconfigure target pins to output mode, after resetting their values
  GPIOB->regs->BSRR = 0x000000E8; // MSG, CD, REQ, IO
  //  GPIOA->regs->BSRR = 0x00000200; // BSY
#endif
  SCSI_TARGET_ACTIVE()  // (BSY), REQ, MSG, CD, IO output turned on

  // Set BSY to-when selected
  SCSI_BSY_ACTIVE();     // Turn only BSY output ON, ACTIVE

  // Wait until SEL becomes inactive
  while(isHigh(gpio_read(SEL))) {}
  
  // Ask for a TARGET-ID to respond
  m_id = 31 - __builtin_clz(scsiid);

  m_isBusReset = false;
  if (setjmp(m_resetJmpBuf) == 1) {
    LOGN("Reset, going to BusFree");
    goto BusFree;
  }
  enableResetJmp();
  
  // In SCSI-2 this is mandatory, but in SCSI-1 it's optional 
  if(isHigh(gpio_read(ATN))) {
    SCSI_PHASE_CHANGE(SCSI_PHASE_MESSAGEOUT);
    // Bus settle delay 400ns. Following code was measured at 350ns before REQ asserted. Added another 50ns. STM32F103.
    SCSI_PHASE_CHANGE(SCSI_PHASE_MESSAGEOUT);// 28ns delay STM32F103
    SCSI_PHASE_CHANGE(SCSI_PHASE_MESSAGEOUT);// 28ns delay STM32F103
    bool syncenable = false;
    int syncperiod = 50;
    int syncoffset = 0;
    int msc = 0;
    while(isHigh(gpio_read(ATN)) && msc < 255) {
      m_msb[msc++] = readHandshake();
    }
    for(int i = 0; i < msc; i++) {
      // ABORT
      if (m_msb[i] == 0x06) {
        goto BusFree;
      }
      // BUS DEVICE RESET
      if (m_msb[i] == 0x0C) {
        syncoffset = 0;
        goto BusFree;
      }
      // IDENTIFY
      if (m_msb[i] >= 0x80) {
        m_lun = m_msb[i] & 0x1f;
      }
      // Extended message
      if (m_msb[i] == 0x01) {
        // Check only when synchronous transfer is possible
        if (!syncenable || m_msb[i + 2] != 0x01) {
          MsgIn2(0x07);
          break;
        }
        // Transfer period factor(50 x 4 = Limited to 200ns)
        syncperiod = m_msb[i + 3];
        if (syncperiod > 50) {
          syncperiod = 50;
        }
        // REQ/ACK offset(Limited to 16)
        syncoffset = m_msb[i + 4];
        if (syncoffset > 16) {
          syncoffset = 16;
        }
        // STDR response message generation
        MsgIn2(0x01);
        MsgIn2(0x03);
        MsgIn2(0x01);
        MsgIn2(syncperiod);
        MsgIn2(syncoffset);
        break;
      }
    }
  }

  LOG("CMD:");
  SCSI_PHASE_CHANGE(SCSI_PHASE_COMMAND);
  // Bus settle delay 400ns. The following code was measured at 20ns before REQ asserted. Added another 380ns. STM32F103.
  asm("nop;nop;nop;nop;nop;nop;nop;nop");// This asm causes some code reodering, which adds 270ns, plus 8 nop cycles for an additional 110ns. STM32F103
  int len;
  byte cmd[20];

  cmd[0] = readHandshake();
  // Atari ST ICD extension support
  // It sends a 0x1F as a indicator there is a 
  // proper full size SCSI command byte to follow
  // so just read it and re-read it again to get the
  // real command byte
  if(cmd[0] == SCSI_ICD_EXTENDED_CMD) { cmd[0] = readHandshake(); }

  LOGHEX(cmd[0]);
  // Command length selection, reception
  static const int cmd_class_len[8]={6,10,10,6,6,12,6,6};
  len = cmd_class_len[cmd[0] >> 5];
  cmd[1] = readHandshake(); LOG(":");LOGHEX(cmd[1]);
  cmd[2] = readHandshake(); LOG(":");LOGHEX(cmd[2]);
  cmd[3] = readHandshake(); LOG(":");LOGHEX(cmd[3]);
  cmd[4] = readHandshake(); LOG(":");LOGHEX(cmd[4]);
  cmd[5] = readHandshake(); LOG(":");LOGHEX(cmd[5]);
  // Receive the remaining commands
  for(int i = 6; i < len; i++ ) {
    cmd[i] = readHandshake();
    LOG(":");
    LOGHEX(cmd[i]);
  }
  // LUN confirmation
  // if it wasn't set in the IDENTIFY then grab it from the CDB
  if(m_lun > MAX_SCSILUN)
  {
      m_lun = (cmd[1] & 0xe0) >> 5;
  }

  LOG(":ID ");
  LOG(m_id);
  LOG(":LUN ");
  LOG(m_lun);
  LOG(" ");

  // HDD Image selection
  if(m_lun >= NUM_SCSILUN)
  {
    m_sts = SCSI_STATUS_GOOD;

    // REQUEST SENSE and INQUIRY are handled different with invalid LUNs
    if(cmd[0] == SCSI_INQUIRY)
    {
      // Special INQUIRY handling for invalid LUNs
      LOGN("onInquiry - InvalidLUN");
      dev = &(scsi_device_list[m_id][0]);

      byte temp = dev->inquiry_block->raw[0];

      // If the LUN is invalid byte 0 of inquiry block needs to be 7fh
      dev->inquiry_block->raw[0] = 0x7f;

      // only write back what was asked for
      writeDataPhase(cmd[4], dev->inquiry_block->raw);

      // return it back to normal if it was altered
      dev->inquiry_block->raw[0] = temp;
    }
    else if(cmd[0] == SCSI_REQUEST_SENSE)
    {
      byte buf[18] = {
        0x70,   //CheckCondition
        0,      //Segment number
        SCSI_SENSE_ILLEGAL_REQUEST,   //Sense key
        0, 0, 0, 0,  //information
        10,   //Additional data length
        0, 0, 0, 0, // command specific information bytes
        (byte)(SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED >> 8),
        (byte)SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED,
        0, 0, 0, 0,
      };
      writeDataPhase(cmd[4] < 18 ? cmd[4] : 18, buf);      
    }
    else
    {    
      m_sts = SCSI_STATUS_CHECK_CONDITION;
    }

    goto Status;
  }

  dev = &(scsi_device_list[m_id][m_lun]);
  if(!dev->m_file)
  {
    dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    dev->m_additional_sense_code = SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED;
    m_sts = SCSI_STATUS_CHECK_CONDITION;
    goto Status;
  }

  LED_ON();
  m_sts = scsi_command_table[cmd[0]](dev, cmd);
  LED_OFF();

Status:
  LOG(" STS:"); LOGHEX(m_sts);
  SCSI_PHASE_CHANGE(SCSI_PHASE_STATUS);
  // Bus settle delay 400ns built in to writeHandshake
  writeHandshake(m_sts);

  LOG(" MI:"); LOGHEX(m_msg);
  SCSI_PHASE_CHANGE(SCSI_PHASE_MESSAGEIN);
  // Bus settle delay 400ns built in to writeHandshake
  writeHandshake(m_msg);

BusFree:
  LOGN(" BF ");
  m_isBusReset = false;
  //SCSI_OUT(vREQ,inactive) // gpio_write(REQ, low);
  //SCSI_OUT(vMSG,inactive) // gpio_write(MSG, low);
  //SCSI_OUT(vCD ,inactive) // gpio_write(CD, low);
  //SCSI_OUT(vIO ,inactive) // gpio_write(IO, low);
  //SCSI_OUT(vBSY,inactive)
  SCSI_TARGET_INACTIVE() // Turn off BSY, REQ, MSG, CD, IO output
#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_TARGET,TR_INPUT);
  // Something in code linked after this function is performing better with a +4 alignment.
  // Adding this nop is causing the next function (_GLOBAL__sub_I_SD) to have an address with a last digit of 0x4.
  // Last digit of 0xc also works.
  // This affects both with and without XCVR, currently without XCVR doesn't need any padding.
  // Until the culprit can be tracked down and fixed, it may be necessary to do manual adjustment.
  asm("nop.w");
#endif
}

static byte onUnimplemented(SCSI_DEVICE *dev, const byte *cdb)
{
  // does nothing!
  if(Serial)
  {
    Serial.print("Unimplemented SCSI command: ");
    Serial.println(cdb[0], 16);
  }

  dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
  dev->m_additional_sense_code = SCSI_ASC_INVALID_OPERATION_CODE;
  return SCSI_STATUS_CHECK_CONDITION;
}

static byte onNOP(SCSI_DEVICE *dev, const byte *cdb)
{
  dev->m_senseKey = 0;
  dev->m_additional_sense_code = 0;
  return SCSI_STATUS_GOOD;
}

/*
 * INQUIRY command processing.
 */
byte onInquiry(SCSI_DEVICE *dev, const byte *cdb)
{
  writeDataPhase(cdb[4] < 47 ? cdb[4] : 47, dev->inquiry_block->raw);
  return SCSI_STATUS_GOOD;
}

/*
 * REQUEST SENSE command processing.
 */
byte onRequestSense(SCSI_DEVICE *dev, const byte *cdb)
{
  byte buf[18] = {
    0x70,   //CheckCondition
    0,      //Segment number
    dev->m_senseKey,   //Sense key
    0, 0, 0, 0,  //information
    10,   //Additional data length
    0, 0, 0, 0, // command specific information bytes
    (byte)(dev->m_additional_sense_code >> 8),
    (byte)dev->m_additional_sense_code,
    0, 0, 0, 0,
  };
  dev->m_senseKey = 0;
  dev->m_additional_sense_code = 0;
  writeDataPhase(cdb[4] < 18 ? cdb[4] : 18, buf);  

  return SCSI_STATUS_GOOD;
}

/*
 * READ CAPACITY command processing.
 */
byte onReadCapacity(SCSI_DEVICE *dev, const byte *cdb)
{
  uint32_t lastlba = dev->m_blockcount - 1; // Points to last LBA
  uint8_t buf[8] = {
    (byte)(lastlba >> 24),
    (byte)(lastlba >> 16),
    (byte)(lastlba >> 8),
    (byte)(lastlba),
    (byte)(dev->m_blocksize >> 24),
    (byte)(dev->m_blocksize >> 16),
    (byte)(dev->m_blocksize >> 8),
    (byte)(dev->m_blocksize)
  };

  writeDataPhase(sizeof(buf), buf);
  return SCSI_STATUS_GOOD;
}

/*
 * Check that the image file is present and the block range is valid.
 */
byte checkBlockCommand(SCSI_DEVICE *dev, uint32_t adds, uint32_t len)
{
  // Check block range is valid
  if (adds >= dev->m_blockcount || (adds + len) > dev->m_blockcount) {    
    dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    dev->m_additional_sense_code = SCSI_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
    return SCSI_STATUS_CHECK_CONDITION;
  }
  return SCSI_STATUS_GOOD;
}

/*
 * READ6 / 10 Command processing.
 */
static byte onRead6(SCSI_DEVICE *dev, const byte *cdb)
{
  unsigned adds = (((uint32_t)cdb[1] & 0x1F) << 16) | ((uint32_t)cdb[2] << 8) | cdb[3];
  unsigned len = (cdb[4] == 0) ? 0x100 : cdb[4];
  /*
  LOGN("onRead6");
  LOG("-R ");
  LOGHEX(adds);
  LOG(":");
  LOGHEXN(len);
  */
   
  byte sts = checkBlockCommand(dev, adds, len);
  if (sts) {
    return sts;
  }
  
  writeDataPhaseSD(dev, adds, len);
  return SCSI_STATUS_GOOD;
}

static byte onRead10(SCSI_DEVICE *dev, const byte *cdb)
{
  unsigned adds = ((uint32_t)cdb[2] << 24) | ((uint32_t)cdb[3] << 16) | ((uint32_t)cdb[4] << 8) | cdb[5];
  unsigned len = ((uint32_t)cdb[7] << 8) | cdb[8];
  
  LOG (" Read10 ");
  LOG("A:");
  LOGHEX(adds);
  LOG(":");
  LOGHEX(len);
  LOG(" ");
  
  byte sts = checkBlockCommand(dev, adds, len);
  if (sts) {
    return sts;
  }
  
  writeDataPhaseSD(dev, adds, len);
  return SCSI_STATUS_GOOD;
}

/*
 * WRITE6 / 10 Command processing.
 */
static byte onWrite6(SCSI_DEVICE *dev, const byte *cdb)
{
  unsigned adds = (((uint32_t)cdb[1] & 0x1F) << 16) | ((uint32_t)cdb[2] << 8) | cdb[3];
  unsigned len = (cdb[4] == 0) ? 0x100 : cdb[4];
  /*
  LOGN("onWrite6");
  LOG("-W ");
  LOGHEX(adds);
  LOG(":");
  LOGHEXN(len);
  */

  if(dev->m_type == SCSI_DEVICE_OPTICAL)
  {
    dev->m_senseKey = SCSI_SENSE_HARDWARE_ERROR;
    dev->m_additional_sense_code = SCSI_ASC_WRITE_PROTECTED; // Write Protect
    return SCSI_STATUS_CHECK_CONDITION;
  }
  
  byte sts = checkBlockCommand(dev, adds, len);
  if (sts) {
    return sts;
  }
  readDataPhaseSD(dev, adds, len);
  return SCSI_STATUS_GOOD;
}

static byte onWrite10(SCSI_DEVICE *dev, const byte *cdb)
{
  unsigned adds = ((uint32_t)cdb[2] << 24) | ((uint32_t)cdb[3] << 16) | ((uint32_t)cdb[4] << 8) | cdb[5];
  unsigned len = ((uint32_t)cdb[7] << 8) | cdb[8];
  /*
  LOGN("onWrite10");
  LOG("-W ");
  LOGHEX(adds);
  LOG(":");
  LOGHEXN(len);
  */

  if(dev->m_type == SCSI_DEVICE_OPTICAL)
  {
    dev->m_senseKey = SCSI_SENSE_HARDWARE_ERROR;
    dev->m_additional_sense_code = SCSI_ASC_WRITE_PROTECTED; // Write Protect
    return SCSI_STATUS_CHECK_CONDITION;
  }

  byte sts = checkBlockCommand(dev, adds, len);
  if (sts) {
    return sts;
  }

  readDataPhaseSD(dev, adds, len);
  return SCSI_STATUS_GOOD;
}
/*
 * VERIFY10 Command processing.
 */
byte onVerify(SCSI_DEVICE *dev, const byte *cdb)
{
  unsigned adds = ((uint32_t)cdb[2] << 24) | ((uint32_t)cdb[3] << 16) | ((uint32_t)cdb[4] << 8) | cdb[5];
  unsigned len = ((uint32_t)cdb[7] << 8) | cdb[8];

  byte sts = checkBlockCommand(dev, adds, len);
  if (sts) {
    return sts;
  }
  int bytchk = (cdb[1] >> 1) & 0x03;
  if (bytchk != 0) {
    if (bytchk == 3) {
      // Data-Out buffer is single logical block for repeated verification.
      len = dev->m_blocksize;
    }
    verifyDataPhaseSD(dev, adds, len);
  }
  return SCSI_STATUS_GOOD;
}

/*
 * MODE SENSE command processing.
 */
byte onModeSense(SCSI_DEVICE *dev, const byte *cdb)
{
  const byte apple_magic[] = "APPLE COMPUTER, INC   ";
  int pageCode = cdb[2] & 0x3F;
  int pageControl = cdb[2] >> 6;
  byte dbd = cdb[1] & 0x8;
  byte block_descriptor_length = 8;

  // saving parameters is not allowed...yet!
  if(pageControl == 3)
  {
    dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    dev->m_additional_sense_code = SCSI_ASC_SAVING_PARAMETERS_NOT_SUPPORTED;
    return SCSI_STATUS_CHECK_CONDITION;
  }

  // SCSI_MODE_SENSE6
  int a = 4;
  int length = cdb[4];

  if(cdb[0] == SCSI_MODE_SENSE10) {
    a = 8;
    length = cdb[7];
    length <<= 8;
    length |= cdb[8];
    if(length > 0x800) { length = 0x800; }; 
  } 
  
  memset(m_buf, 0, length);
  
  if(!dbd) {
    byte c[8] = {
      0,//Density code
      (byte)(dev->m_blockcount >> 16),
      (byte)(dev->m_blockcount >> 8),
      (byte)(dev->m_blockcount),
      0, //Reserve
      (byte)(dev->m_blocksize >> 16),
      (byte)(dev->m_blocksize >> 8),
      (byte)(dev->m_blocksize),
    };
    memcpy(&m_buf[a], c, 8);
    a += 8;
  }

  // HDD supports page codes 0x1 (Read/Write), 0x2, 0x3, 0x4
  // CDROM supports page codes 0x1 (Read Only), 0x2, 0xD, 0xE, 0x30
  if(dev->m_type == SCSI_DEVICE_HDD) {
    switch(pageCode) {
    case SCSI_SENSE_MODE_ALL:
    case SCSI_SENSE_MODE_READ_WRITE_ERROR_RECOVERY:
      m_buf[a + 0] = SCSI_SENSE_MODE_READ_WRITE_ERROR_RECOVERY;
      m_buf[a + 1] = 0x0A;
      a += 0x0C;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;

    case SCSI_SENSE_MODE_DISCONNECT_RECONNECT:
      m_buf[a + 0] = SCSI_SENSE_MODE_DISCONNECT_RECONNECT;
      m_buf[a + 1] = 0x0A;
      a += 0x0C;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;

    case SCSI_SENSE_MODE_FORMAT_DEVICE:  //Drive parameters
      m_buf[a + 0] = SCSI_SENSE_MODE_FORMAT_DEVICE; //Page code
      m_buf[a + 1] = 0x16; // Page length
      if(pageControl != 1) {
        m_buf[a + 11] = 0x3F;//Number of sectors / track
        m_buf[a + 12] = (byte)(dev->m_blocksize >> 8);
        m_buf[a + 13] = (byte)dev->m_blocksize;
        m_buf[a + 15] = 0x1; // Interleave
      }
      a += 0x18;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;

    case SCSI_SENSE_MODE_DISK_GEOMETRY:  //Drive parameters
      m_buf[a + 0] = SCSI_SENSE_MODE_DISK_GEOMETRY; //Page code
      m_buf[a + 1] = 0x16; // Page length
      if(pageControl != 1) {
        unsigned cylinders = dev->m_blockcount / (16 * 63);
        m_buf[a + 2] = (byte)(cylinders >> 16); // Cylinders
        m_buf[a + 3] = (byte)(cylinders >> 8);
        m_buf[a + 4] = (byte)cylinders;
        m_buf[a + 5] = 16;   //Number of heads
      } else {
        m_buf[a + 2] = 0xFF; // Cylinder length
        m_buf[a + 3] = 0xFF;
        m_buf[a + 4] = 0xFF;
        m_buf[a + 5] = 16;   //Number of heads
      }
      a += 0x18;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;
    case SCSI_SENSE_MODE_FLEXABLE_GEOMETRY:
      m_buf[a + 0] = SCSI_SENSE_MODE_FLEXABLE_GEOMETRY;
      m_buf[a + 1] = 0x1E;  // Page length
      if(pageControl != 1) {
        m_buf[a + 2] = 0x03; 
        m_buf[a + 3] = 0xE8; // Transfer rate 1 mbit/s
        m_buf[a + 4] = 16; // Number of heads
        m_buf[a + 5] = 63; // Sectors per track
        m_buf[a + 6] = (byte)dev->m_blocksize >> 8;
        m_buf[a + 7] = (byte)dev->m_blocksize & 0xff;  // Data bytes per sector
      }
      a += 0x20;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;
    case SCSI_SENSE_MODE_CACHING:
      m_buf[a + 0] = SCSI_SENSE_MODE_CACHING;
      m_buf[a + 1] = 0x0A;  // Page length
      if(pageControl != 1) {
        m_buf[a + 2] = 0x01; // Disalbe Read Cache so no one asks for Cache Stats page.
      }
      a += 0x0C;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;
    case SCSI_SENSE_MODE_VENDOR_APPLE:
      {
        if(pageControl != 1) {
          m_buf[a + 0] = SCSI_SENSE_MODE_VENDOR_APPLE;
          m_buf[a + 1] = sizeof(apple_magic); // Page length
          memcpy(&m_buf[a + 2], apple_magic, sizeof(apple_magic));
          a += sizeof(apple_magic) + 2;
        }
        if(pageCode != SCSI_SENSE_MODE_ALL) break;
      }
      break; // Don't want SCSI_SENSE_MODE_ALL falling through to error condition

    default:
      dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
      dev->m_additional_sense_code = SCSI_ASC_INVALID_FIELD_IN_CDB;
      return SCSI_STATUS_CHECK_CONDITION;
      break;
    }
  } else {
    // OPTICAL
    if(cdb[0] == SCSI_MODE_SENSE6) {
      m_buf[2] = 1 << 7; // WP bit
    } else {
      m_buf[3] = 1 << 7; // WP bit
    }

    switch(pageCode) {
    case SCSI_SENSE_MODE_ALL:
    case SCSI_SENSE_MODE_READ_WRITE_ERROR_RECOVERY:
      m_buf[a + 0] = SCSI_SENSE_MODE_READ_WRITE_ERROR_RECOVERY;
      m_buf[a + 1] = 0x06;
      m_buf[a + 3] = 0x01; // Retry Count
      a += 0x08;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;

    case SCSI_SENSE_MODE_DISCONNECT_RECONNECT:
      m_buf[a + 0] = SCSI_SENSE_MODE_DISCONNECT_RECONNECT;
      m_buf[a + 1] = 0x0A;
      a += 0x0C;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;
     
    case SCSI_SENSE_MODE_CDROM:
      m_buf[a + 0] = SCSI_SENSE_MODE_CDROM;
      m_buf[a + 1] = 0x06;
      if(pageControl != 1)
      {
        // 2 seconds for inactive timer
        m_buf[a + 3] = 0x05;
        // MSF multiples are 60 and 75
        m_buf[a + 5] = 60;
        m_buf[a + 7] = 75;
      }
      a += 0x8;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;

    case SCSI_SENSE_MODE_CDROM_AUDIO_CONTROL:
      m_buf[a + 0] = SCSI_SENSE_MODE_CDROM_AUDIO_CONTROL;
      m_buf[a + 1] = 0x0E;

      a += 0x10;
      if(pageCode != SCSI_SENSE_MODE_ALL) break;

    case SCSI_SENSE_MODE_VENDOR_APPLE:
      {
        if(pageControl != 1) {
          m_buf[a + 0] = SCSI_SENSE_MODE_VENDOR_APPLE;
          m_buf[a + 1] = sizeof(apple_magic); // Page length
          memcpy(&m_buf[a + 2], apple_magic, sizeof(apple_magic));
          a += sizeof(apple_magic) + 2;
        }
        if(pageCode != SCSI_SENSE_MODE_ALL) break;
      }
      break; // Don't want SCSI_SENSE_MODE_ALL falling through to error condition

    default:
      dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
      dev->m_additional_sense_code = SCSI_ASC_INVALID_FIELD_IN_CDB;
      return SCSI_STATUS_CHECK_CONDITION;
      break;
    }
  }

  if(cdb[0] == SCSI_MODE_SENSE10)
  {
    m_buf[1] = a - 2;
    m_buf[7] = block_descriptor_length; // block descriptor length
  }
  else
  {
    m_buf[0] = a - 1;
    m_buf[3] = block_descriptor_length; // block descriptor length
  }

  writeDataPhase(length < a ? length : a, m_buf);
  return SCSI_STATUS_GOOD;
}

void setBlockLength(SCSI_DEVICE *dev, uint32_t length)
{
  dev->m_blocksize = dev->m_rawblocksize = length;
  dev->m_blockcount = dev->m_fileSize / dev->m_blocksize;
}
    
byte onModeSelect(SCSI_DEVICE *dev, const byte *cdb)
{
  unsigned length = 0;
  LOGN("onModeSelect");

  // saving mode pages isn't supported yet
  if(cdb[1] & 0x01)
  {
    dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    dev->m_additional_sense_code = SCSI_ASC_INVALID_FIELD_IN_CDB;
    return SCSI_STATUS_CHECK_CONDITION;
  }

  if(cdb[0] == SCSI_MODE_SELECT6)
  {
    length = cdb[4];
  }
  else /* SCSI_MODE_SELECT10 */
  {
    length = cdb[7] << 8;
    length |= cdb[8];
    if(length > 0x800) { length = 0x800; }
  }

  if(length == 0)
  {
    return SCSI_STATUS_GOOD;
  }

  memset(m_buf, 0, length);
  readDataPhase(length, m_buf);
  //Apple HD SC Setup sends:
  //0 0 0 8 0 0 0 0 0 0 2 0 0 2 10 0 1 6 24 10 8 0 0 0
  //I believe mode page 0 set to 10 00 is Disable Unit Attention
  //Mode page 1 set to 24 10 08 00 00 00 is TB and PER set, read retry count 16, correction span 8
  
  if(dev->m_type == SCSI_DEVICE_OPTICAL)
  {
    // check for a block descriptor
    if(m_buf[3] == 8)
    {
      // Requested change of blocksize
      // Only supporting 512 or 2048 for optical devices
      uint32_t new_block_size =  ((uint32_t)m_buf[8] << 16) | ((uint32_t)m_buf[10] << 8) | m_buf[9];
      switch(new_block_size)
      {
        case 512: setBlockLength(dev, 512);
        break;

        case 2048: setBlockLength(dev, 2048);
        break;

        default: LOG("Err BlockSize:"); LOG(new_block_size); LOG(" ");
      }
    }
  }
  
  #if DEBUG > 0
  for (unsigned i = 0; i < length; i++) {
    LOGHEX(m_buf[i]);LOG(" ");
  }
  LOGN("");
  #endif
  return SCSI_STATUS_GOOD;
}

/*
 * ReZero Unit - Move to Logical Block Zero in file.
 */
byte onReZeroUnit(SCSI_DEVICE *dev, const byte *cdb) {
  LOGN("-ReZeroUnit");
  // Make sure we have an image with atleast a first byte.
  // Actually seeking to the position wont do anything, so dont.
  return checkBlockCommand(dev, 0, 0);
}

/*
 * WriteBuffer - Used for testing buffer, no change to medium
 */
byte onWriteBuffer(SCSI_DEVICE *dev, const byte *cdb)
{
  byte mode = cdb[1] & 7; 
  uint32_t allocLength = ((uint32_t)cdb[6] << 16) | ((uint32_t)cdb[7] << 8) | cdb[8];

  LOGN("-WriteBuffer");
  LOGHEXN(mode);
  LOGHEXN(allocLength);

  if (mode == MODE_COMBINED_HEADER_DATA && (allocLength - 4) <= SCSI_BUF_SIZE)
  {
    byte tmp[allocLength];
    readDataPhase(allocLength, tmp);
    // Drop header
    memcpy(m_scsi_buf, (&tmp[4]), allocLength - 4);
    #if DEBUG > 0
    for (unsigned i = 0; i < allocLength; i++) {
      LOGHEX(tmp[i]);LOG(" ");
    }
    LOGN("");
    #endif
    return SCSI_STATUS_GOOD;
  }
  else if ( mode == MODE_DATA && allocLength <= SCSI_BUF_SIZE)
  {
    readDataPhase(allocLength, m_scsi_buf);
    #if DEBUG > 0
    for (unsigned i = 0; i < allocLength; i++) {
      LOGHEX(m_scsi_buf[i]);LOG(" ");
    }
    LOGN("");
    #endif
    return SCSI_STATUS_GOOD;
  }
  else
  {
    dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    dev->m_additional_sense_code = SCSI_ASC_INVALID_FIELD_IN_CDB;
    return SCSI_STATUS_CHECK_CONDITION;
  }
}

/*
 * ReadBuffer - Used for testing buffer, no change to medium
 */
byte onReadBuffer(SCSI_DEVICE *dev, const byte *cdb)
{
  byte mode = cdb[1] & 7; 
  uint32_t allocLength = ((uint32_t)cdb[6] << 16) | ((uint32_t)cdb[7] << 8) | cdb[8];
  
  LOGN("-ReadBuffer");
  LOGHEXN(mode);
  LOGHEXN(allocLength);

  if (mode == MODE_COMBINED_HEADER_DATA)
  {
    memset(m_buf, 0, 4 + m_scsi_buf_size);
    // four byte read buffer header
    m_buf[0] = 0;
    m_buf[1] = (SCSI_BUF_SIZE >> 16) & 0xff;
    m_buf[2] = (SCSI_BUF_SIZE >> 8) & 0xff;
    m_buf[3] = SCSI_BUF_SIZE & 0xff;
    // actual data
    memcpy((&m_buf[4]), m_scsi_buf, m_scsi_buf_size);

    writeDataPhase(4 + m_scsi_buf_size, m_buf);

    #if DEBUG > 0
    for (unsigned i = 0; i < allocLength; i++) {
      LOGHEX(m_scsi_buf[i]);LOG(" ");
    }
    LOGN("");
    #endif
    return SCSI_STATUS_GOOD;
  }
  else if (mode == MODE_DATA)
  {
    writeDataPhase(m_scsi_buf_size, m_scsi_buf);
    #if DEBUG > 0
    for (unsigned i = 0; i < allocLength; i++) {
      LOGHEX(m_scsi_buf[i]);LOG(" ");
    }
    LOGN("");
    #endif
    return SCSI_STATUS_GOOD;
  }
  else
  {
    dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    dev->m_additional_sense_code = SCSI_ASC_INVALID_FIELD_IN_CDB;
    return SCSI_STATUS_CHECK_CONDITION;
  }
}

/*
 * On Send Diagnostic
 */
byte onSendDiagnostic(SCSI_DEVICE *dev, const byte *cdb)
{
  int self_test = cdb[1] & 0x4;
  LOGN("-SendDiagnostic");
  LOGHEXN(cdb[1]);

  if(self_test)
  {
    // Don't actually do a test, we're good.
    return SCSI_STATUS_GOOD;
  }
  else
  {
    dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    dev->m_additional_sense_code = SCSI_ASC_INVALID_FIELD_IN_CDB;
    return SCSI_STATUS_CHECK_CONDITION;
  }
}

/*
 * Read Defect Data
 */
byte onReadDefectData(SCSI_DEVICE *dev, const byte *cdb)
{
  byte response[4] = {
    0x0, // Reserved
    cdb[2], // echo back Reserved, Plist, Glist, Defect list format
    cdb[7], cdb[8] // echo back defect list length
  };
  writeDataPhase(4, response);
  return SCSI_STATUS_GOOD;
}

static byte onReadTOC(SCSI_DEVICE *dev, const byte *cdb)
{
  uint8_t msf = cdb[1] & 0x02;
  uint8_t track = cdb[6];
  unsigned len = ((uint32_t)cdb[7] << 8) | cdb[8];
  memset(m_buf, 0, len);

  // Doing just the error seemed to make MacOS unhappy
#if 0
  dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
  dev->m_additional_sense_code = SCSI_ASC_INVALID_FIELD_IN_CDB;
  return SCSI_STATUS_CHECK_CONDITION;
#endif
    
  if(track > 1 || cdb[2] != 0)
  {
    dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    dev->m_additional_sense_code = SCSI_ASC_INVALID_FIELD_IN_CDB;
    return SCSI_STATUS_CHECK_CONDITION;
  }
  
  m_buf[1] = 18; // TOC length LSB
  m_buf[2] = 1; // First Track
  m_buf[3] = 1; // Last Track
  
  // first track
  m_buf[5] = 0x14; // data track
  m_buf[6] = 1; 
  
  // leadout track 
  m_buf[13] = 0x14; // data track
  m_buf[14] = 0xaa; // leadout track
  if(msf)
  {
    LBAtoMSF(dev->m_blockcount, &m_buf[16]);
  }
  else
  {
    m_buf[16] = (byte)(dev->m_blockcount >> 24);
    m_buf[17] = (byte)(dev->m_blockcount >> 16);
    m_buf[18] = (byte)(dev->m_blockcount >> 8);
    m_buf[20] = (byte)(dev->m_blockcount);
  }
  
  writeDataPhase(SCSI_TOC_LENGTH > len ? len : SCSI_TOC_LENGTH, m_buf);
  return SCSI_STATUS_GOOD;
}

static byte onReadDiscInformation(SCSI_DEVICE *dev, const byte *cdb)
{
  writeDataPhase((cdb[7] >> 8) | cdb[8], m_buf);
  return SCSI_STATUS_GOOD;
}

static byte onReadDVDStructure(SCSI_DEVICE *dev, const byte *cdb)
{
  dev->m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
  dev->m_additional_sense_code = SCSI_ASC_CANNOT_READ_MEDIUM_INCOMPATIBLE_FORMAT;
  return SCSI_STATUS_CHECK_CONDITION;
}

// Thanks RaSCSI :D
//	LBA→MSF Conversion
static inline void LBAtoMSF(const uint32_t lba, byte *msf)
{
	uint32_t m, s, f;

	// 75 and 75*60 get the remainder
	m = lba / (75 * 60);
	s = lba % (75 * 60);
	f = s % 75;
	s /= 75;

	// The base point is M=0, S=2, F=0
	s += 2;
	if (s >= 60) {
		s -= 60;
		m++;
	}

	// Store
	msf[0] = 0x00;
	msf[1] = (byte)m;
	msf[2] = (byte)s;
	msf[3] = (byte)f;
}

static inline uint32_t MSFtoLBA(const byte *msf)
{
	uint32_t lba;

	// 1, 75, add up in multiples of 75*60
	lba = msf[1];
	lba *= 60;
	lba += msf[2];
	lba *= 75;
	lba += msf[3];

	// Since the base point is M=0, S=2, F=0, subtract 150
	lba -= 150;

	return lba;
}