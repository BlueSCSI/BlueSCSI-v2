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
#include "scsi_cmds.h"
#include "scsi_sense.h"
#include "scsi_status.h"

#ifdef USE_STM32_DMA
#warning "warning USE_STM32_DMA"
#endif

#define DEBUG            0      // 0:No debug information output
                                // 1: Debug information output to USB Serial
                                // 2: Debug information output to LOG.txt (slow)

// SCSI config
#define NUM_SCSIID  7          // Maximum number of supported SCSI-IDs (The minimum is 0)
#define NUM_SCSILUN 2          // Maximum number of LUNs supported     (The minimum is 0)
#define READ_PARITY_CHECK 0    // Perform read parity check (unverified)

// HDD format
#define MAX_BLOCKSIZE 2048     // Maximum BLOCK size

// SDFAT
SdFs SD;

#if DEBUG == 1
#define LOG(XX)     Serial.print(XX)
#define LOGHEX(XX)  Serial.print(XX, HEX)
#define LOGN(XX)    Serial.println(XX)
#define LOGHEXN(XX) Serial.println(XX, HEX)
#elif DEBUG == 2
#define LOG(XX)     LOG_FILE.print(XX); LOG_FILE.sync();
#define LOGHEX(XX)  LOG_FILE.print(XX, HEX); LOG_FILE.sync();
#define LOGN(XX)    LOG_FILE.println(XX); LOG_FILE.sync();
#define LOGHEXN(XX) LOG_FILE.println(XX, HEX); LOG_FILE.sync();
#else
#define LOG(XX)     //Serial.print(XX)
#define LOGHEX(XX)  //Serial.print(XX, HEX)
#define LOGN(XX)    //Serial.println(XX)
#define LOGHEXN(XX) //Serial.println(XX, HEX)
#endif

#define active   1
#define inactive 0
#define high 0
#define low 1

#define isHigh(XX) ((XX) == high)
#define isLow(XX) ((XX) != high)

#define gpio_mode(pin,val) gpio_set_mode(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit, val);
#define gpio_write(pin,val) gpio_write_bit(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit, val)
#define gpio_read(pin) gpio_read_bit(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit)

//#define DB0       PB8     // SCSI:DB0
//#define DB1       PB9     // SCSI:DB1
//#define DB2       PB10    // SCSI:DB2
//#define DB3       PB11    // SCSI:DB3
//#define DB4       PB12    // SCSI:DB4
//#define DB5       PB13    // SCSI:DB5
//#define DB6       PB14    // SCSI:DB6
//#define DB7       PB15    // SCSI:DB7
//#define DBP       PB0     // SCSI:DBP
#define ATN       PA8      // SCSI:ATN
#define BSY       PA9      // SCSI:BSY
#define ACK       PA10     // SCSI:ACK
#define RST       PA15     // SCSI:RST
#define MSG       PB3      // SCSI:MSG
#define SEL       PB4      // SCSI:SEL
#define CD        PB5      // SCSI:C/D
#define REQ       PB6      // SCSI:REQ
#define IO        PB7      // SCSI:I/O
#define LED2      PA0      // External LED

#define SD_CS     PA4      // SDCARD:CS
#define LED       PC13     // LED

// Image Set Selector
#ifdef XCVR
#define IMAGE_SELECT1   PC14
#define IMAGE_SELECT2   PC15
#else
#define IMAGE_SELECT1   PA1
#define IMAGE_SELECT2   PB1
#endif

// GPIO register port
#define PAREG GPIOA->regs
#define PBREG GPIOB->regs

// LED control
#define LED_ON()       gpio_write(LED, high); gpio_write(LED2, low);
#define LED_OFF()      gpio_write(LED, low); gpio_write(LED2, high);

// Virtual pin (Arduio compatibility is slow, so make it MCU-dependent)
#define PA(BIT)       (BIT)
#define PB(BIT)       (BIT+16)
// Virtual pin decoding
#define GPIOREG(VPIN)    ((VPIN)>=16?PBREG:PAREG)
#define BITMASK(VPIN)    (1<<((VPIN)&15))

#define vATN       PA(8)      // SCSI:ATN
#define vBSY       PA(9)      // SCSI:BSY
#define vACK       PA(10)     // SCSI:ACK
#define vRST       PA(15)     // SCSI:RST
#define vMSG       PB(3)      // SCSI:MSG
#define vSEL       PB(4)      // SCSI:SEL
#define vCD        PB(5)      // SCSI:C/D
#define vREQ       PB(6)      // SCSI:REQ
#define vIO        PB(7)      // SCSI:I/O
#define vSD_CS     PA(4)      // SDCARD:CS

// SCSI output pin control: opendrain active LOW (direct pin drive)
#define SCSI_OUT(VPIN,ACTIVE) { GPIOREG(VPIN)->BSRR = BITMASK(VPIN)<<((ACTIVE)?16:0); }

// SCSI input pin check (inactive=0,avtive=1)
#define SCSI_IN(VPIN) ((~GPIOREG(VPIN)->IDR>>(VPIN&15))&1)

// SCSI phase change as single write to port B
#define SCSIPHASEMASK(MSGACTIVE, CDACTIVE, IOACTIVE) ((BITMASK(vMSG)<<((MSGACTIVE)?16:0)) | (BITMASK(vCD)<<((CDACTIVE)?16:0)) | (BITMASK(vIO)<<((IOACTIVE)?16:0)))

#define SCSI_PHASE_DATAOUT SCSIPHASEMASK(inactive, inactive, inactive)
#define SCSI_PHASE_DATAIN SCSIPHASEMASK(inactive, inactive, active)
#define SCSI_PHASE_COMMAND SCSIPHASEMASK(inactive, active, inactive)
#define SCSI_PHASE_STATUS SCSIPHASEMASK(inactive, active, active)
#define SCSI_PHASE_MESSAGEOUT SCSIPHASEMASK(active, active, inactive)
#define SCSI_PHASE_MESSAGEIN SCSIPHASEMASK(active, active, active)

#define SCSI_PHASE_CHANGE(MASK) { PBREG->BSRR = (MASK); }

#ifdef XCVR
#define TR_TARGET        PA1   // Target Transceiver Control Pin
#define TR_DBP           PA2   // Data Pins Transceiver Control Pin
#define TR_INITIATOR     PA3   // Initiator Transciever Control Pin

#define vTR_TARGET       PA(1) // Target Transceiver Control Pin
#define vTR_DBP          PA(2) // Data Pins Transceiver Control Pin
#define vTR_INITIATOR    PA(3) // Initiator Transciever Control Pin

#define TR_INPUT 1
#define TR_OUTPUT 0

// Transceiver control definitions
#define TRANSCEIVER_IO_SET(VPIN,TR_INPUT) { GPIOREG(VPIN)->BSRR = BITMASK(VPIN) << ((TR_INPUT) ? 16 : 0); }

// Turn on the output only for BSY
#define SCSI_BSY_ACTIVE()      {  gpio_mode(BSY, GPIO_OUTPUT_PP); SCSI_OUT(vBSY, active) }

#define SCSI_TARGET_ACTIVE()   { gpio_mode(REQ, GPIO_OUTPUT_PP); gpio_mode(MSG, GPIO_OUTPUT_PP); gpio_mode(CD, GPIO_OUTPUT_PP); gpio_mode(IO, GPIO_OUTPUT_PP); gpio_mode(BSY, GPIO_OUTPUT_PP);  TRANSCEIVER_IO_SET(vTR_TARGET,TR_OUTPUT);}
// BSY,REQ,MSG,CD,IO Turn off output, BSY is the last input
#define SCSI_TARGET_INACTIVE() { pinMode(REQ, INPUT); pinMode(MSG, INPUT); pinMode(CD, INPUT); pinMode(IO, INPUT); pinMode(BSY, INPUT); TRANSCEIVER_IO_SET(vTR_TARGET,TR_INPUT); }

#define DB_MODE_OUT 1  // push-pull mode
#define DB_MODE_IN  4  // floating inputs

#else

// GPIO mode
// IN , FLOAT      : 4
// IN , PU/PD      : 8
// OUT, PUSH/PULL  : 3
// OUT, OD         : 7
#define DB_MODE_OUT 3
//#define DB_MODE_OUT 7
#define DB_MODE_IN  8

// Turn on the output only for BSY
#define SCSI_BSY_ACTIVE()      { gpio_mode(BSY, GPIO_OUTPUT_OD); SCSI_OUT(vBSY,  active) }
// BSY,REQ,MSG,CD,IO Turn on the output (no change required for OD)
#define SCSI_TARGET_ACTIVE()   { if (DB_MODE_OUT != 7) gpio_mode(REQ, GPIO_OUTPUT_PP);}
// BSY,REQ,MSG,CD,IO Turn off output, BSY is the last input
#define SCSI_TARGET_INACTIVE() { if (DB_MODE_OUT == 7) SCSI_OUT(vREQ,inactive) else { if (DB_MODE_IN == 8) gpio_mode(REQ, GPIO_INPUT_PU) else gpio_mode(REQ, GPIO_INPUT_FLOATING)} SCSI_PHASE_CHANGE(SCSI_PHASE_DATAOUT); gpio_mode(BSY, GPIO_INPUT_PU); }

#endif


// Put DB and DP in output mode
#define SCSI_DB_OUTPUT() { PBREG->CRL=(PBREG->CRL &0xfffffff0)|DB_MODE_OUT; PBREG->CRH = 0x11111111*DB_MODE_OUT; }
// Put DB and DP in input mode
#define SCSI_DB_INPUT()  { PBREG->CRL=(PBREG->CRL &0xfffffff0)|DB_MODE_IN ; PBREG->CRH = 0x11111111*DB_MODE_IN; }

// HDDiamge file
#define HDIMG_ID_POS  2                 // Position to embed ID number
#define HDIMG_LUN_POS 3                 // Position to embed LUN numbers
#define HDIMG_BLK_POS 5                 // Position to embed block size numbers
#define MAX_FILE_PATH 32                // Maximum file name length

// HDD image
typedef struct hddimg_struct
{
	FsFile      m_file;                 // File object
	uint64_t    m_fileSize;             // File size
	size_t      m_blocksize;            // SCSI BLOCK size
}HDDIMG;
HDDIMG  img[NUM_SCSIID][NUM_SCSILUN]; // Maximum number

uint8_t       m_senseKey = 0;         // Sense key
uint16_t      m_addition_sense = 0;   // Additional sense information
volatile bool m_isBusReset = false;   // Bus reset
volatile bool m_resetJmp = false;     // Call longjmp on reset
jmp_buf       m_resetJmpBuf;

byte          scsi_id_mask;           // Mask list of responding SCSI IDs
byte          m_id;                   // Currently responding SCSI-ID
byte          m_lun;                  // Logical unit number currently responding
byte          m_sts;                  // Status byte
byte          m_msg;                  // Message bytes
HDDIMG       *m_img;                  // HDD image for current SCSI-ID, LUN
byte          m_buf[MAX_BLOCKSIZE];   // General purpose buffer
byte          m_msb[256];             // Command storage bytes

/*
 *  Data byte to BSRR register setting value and parity table
*/

// Parity bit generation
#define PTY(V)   (1^((V)^((V)>>1)^((V)>>2)^((V)>>3)^((V)>>4)^((V)>>5)^((V)>>6)^((V)>>7))&1)

// Data byte to BSRR register setting value conversion table
// BSRR[31:24] =  DB[7:0]
// BSRR[   16] =  PTY(DB)
// BSRR[15: 8] = ~DB[7:0]
// BSRR[    0] = ~PTY(DB)

// Set DBP, set REQ = inactive
#define DBP(D)    ((((((uint32_t)(D)<<8)|PTY(D))*0x00010001)^0x0000ff01)|BITMASK(vREQ))

// BSRR register control value that simultaneously performs DB set, DP set, and REQ = H (inactrive)
uint32_t db_bsrr[256];

// Parity bit acquisition
#define PARITY(DB) (db_bsrr[DB]&1)

// Macro cleaning
#undef DBP32
#undef DBP8
//#undef DBP
//#undef PTY

// Log File
#define VERSION "1.1-SNAPSHOT-20220407"
#define LOG_FILENAME "LOG.txt"
FsFile LOG_FILE;

// SCSI Drive Vendor information
byte SCSI_INFO_BUF[36] = {
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

void onFalseInit(void);
void noSDCardFound(void);
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
void readSCSIDeviceConfig() {
  FsFile config_file = SD.open("scsi-config.txt", O_RDONLY);
  if (!config_file.isOpen()) {
    return;
  }
  char vendor[9];
  memset(vendor, 0, sizeof(vendor));
  config_file.readBytes(vendor, sizeof(vendor));
  LOG_FILE.print("SCSI VENDOR: ");
  LOG_FILE.println(vendor);
  memcpy(&(SCSI_INFO_BUF[8]), vendor, 8);

  char product[17];
  memset(product, 0, sizeof(product));
  config_file.readBytes(product, sizeof(product));
  LOG_FILE.print("SCSI PRODUCT: ");
  LOG_FILE.println(product);
  memcpy(&(SCSI_INFO_BUF[16]), product, 16);

  char version[5];
  memset(version, 0, sizeof(version));
  config_file.readBytes(version, sizeof(version));
  LOG_FILE.print("SCSI VERSION: ");
  LOG_FILE.println(version);
  memcpy(&(SCSI_INFO_BUF[32]), version, 4);
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
    LOG_FILE.print(sd_cid.mdt_month);
    LOG_FILE.print("/20"); // CID year is 2000 + high/low
    LOG_FILE.print(sd_cid.mdt_year_high);
    LOG_FILE.println(sd_cid.mdt_year_low);

    LOG_FILE.print("Sd Serial:");
    LOG_FILE.println(sd_cid.psn);
    LOG_FILE.sync();
  }
}

/*
 * Open HDD image file
 */

bool hddimageOpen(HDDIMG *h, FsFile file,int id,int lun,int blocksize)
{
  h->m_fileSize = 0;
  h->m_blocksize = blocksize;
  h->m_file = file;
  if(h->m_file.isOpen())
  {
    h->m_fileSize = h->m_file.size();
    if(h->m_fileSize>0)
    {
      // check blocksize dummy file
      LOG_FILE.print(" / ");
      LOG_FILE.print(h->m_fileSize);
      LOG_FILE.print("bytes / ");
      LOG_FILE.print(h->m_fileSize / 1024);
      LOG_FILE.print("KiB / ");
      LOG_FILE.print(h->m_fileSize / 1024 / 1024);
      LOG_FILE.println("MiB");
      return true; // File opened
    }
    else
    {
      LOG_FILE.println(" - file is 0 bytes, can not use.");
      h->m_file.close();
      h->m_fileSize = h->m_blocksize = 0; // no file
    }
  }
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
  disableDebugPorts();

  // Setup BSRR table
  for (unsigned i = 0; i <= 255; i++) {
    db_bsrr[i] = DBP(i);
  }

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

  // Try different clock speeds till we find one that is stable.
  LED_ON();
  int mhz = 50;
  bool sd_ready = false;
  while (mhz >= 32 && !sd_ready) {
    if(SD.begin(SdSpiConfig(PA4, DEDICATED_SPI, SD_SCK_MHZ(mhz), &SPI))) {
      sd_ready = true;
    }
    else {
      mhz--;
    }
  }
  LED_OFF();

  if(!sd_ready) {
#if DEBUG > 0
    Serial.println("SD initialization failed!");
#endif
    noSDCardFound();
  }
  initFileLog(mhz);
  readSCSIDeviceConfig();
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
    onFalseInit();
  }

  finalizeFileLog();
  LED_OFF();
  //Occurs when the RST pin state changes from HIGH to LOW
  attachInterrupt(RST, onBusReset, FALLING);
}

void findDriveImages(FsFile root) {
  bool image_ready;
  FsFile file;
  char path_name[MAX_FILE_PATH+1];
  root.getName(path_name, sizeof(path_name));
  SD.chdir(path_name);

  while (1) {
    // Directories can not be opened RDWR, so it will fail, but fails the same way with no file/dir, so we need to peek at the file first.
    FsFile file_test = root.openNextFile(O_RDONLY);
    char name[MAX_FILE_PATH+1];
    file_test.getName(name, MAX_FILE_PATH+1);
    String file_name = String(name);

    // Skip directories and already open files.
    if(file_test.isDir() || file_name.startsWith("LOG.txt")) {
      file_test.close();
      continue;
    }
    // If error there is no next file to open.
    if(file_test.getError() > 0) {
      file_test.close();
      break;
    }
    // Valid file, open for reading/writing.
    file = SD.open(name, O_RDWR);
    if(file && file.isFile()) {
      file_name.toLowerCase();
      if(file_name.startsWith("hd")) {
        // Defaults for Hard Disks
        int id  = 1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
        int lun = 0;
        int blk = 512;

        // Positionally read in and coerase the chars to integers.
        // We only require the minimum and read in the next if provided.
        int file_name_length = file_name.length();
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
          HDDIMG *h = &img[id][lun];
          LOG_FILE.print(" - ");
          LOG_FILE.print(name);
          image_ready = hddimageOpen(h, file, id, lun, blk);
          if(image_ready) { // Marked as a responsive ID
            scsi_id_mask |= 1<<id;
          }
        }
      }
    } else {
      file.close();
      LOG_FILE.print("Not an image: ");
      LOG_FILE.println(name);
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
  LOG_FILE.print("VERSION: ");
  LOG_FILE.println(VERSION);
  LOG_FILE.print("DEBUG:");
  LOG_FILE.print(DEBUG);
  LOG_FILE.print(" SDFAT_FILE_TYPE:");
  LOG_FILE.println(SDFAT_FILE_TYPE);
  LOG_FILE.print("SdFat version: ");
  LOG_FILE.println(SD_FAT_VERSION_STR);
  LOG_FILE.print("SPI speed: ");
  LOG_FILE.print(success_mhz);
  LOG_FILE.println("Mhz");
  if(success_mhz < 40) {
    LOG_FILE.println("SPI under 40Mhz - read https://github.com/erichelgeson/BlueSCSI/wiki/Slow-SPI");
  }
  LOG_FILE.print("SdFat Max FileName Length: ");
  LOG_FILE.println(MAX_FILE_PATH);
  LOG_FILE.println("Initialized SD Card - lets go!");
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
      HDDIMG *h = &img[id][lun];
      if( (lun<NUM_SCSILUN) && (h->m_file))
      {
        LOG_FILE.print((h->m_blocksize<1000) ? ": " : ":");
        LOG_FILE.print(h->m_blocksize);
      }
      else      
        LOG_FILE.print(":----");
    }
    LOG_FILE.println(":");
  }
  LOG_FILE.println("Finished initialization of SCSI Devices - Entering main loop.");
  LOG_FILE.sync();
  LOG_FILE.close();
}

/*
 * Initialization failed, blink 3x fast
 */
void onFalseInit(void)
{
  LOG_FILE.sync();
  while(true) {
    for(int i = 0; i < 3; i++) {
      LED_ON();
      delay(250);
      LED_OFF();
      delay(250);
    }
    delay(3000);
  }
}

/*
 * No SC Card found, blink 5x fast
 */
void noSDCardFound(void)
{
  while(true) {
    for(int i = 0; i < 5; i++) {
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
#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_DBP,TR_OUTPUT)
#endif
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
  LOGN("DATAIN PHASE");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAIN);
  // Bus settle delay 400ns. Following code was measured at 800ns before REQ asserted. STM32F103.
  SCSI_DB_OUTPUT()
  writeDataLoop(len, p);
  SCSI_DB_INPUT()
}

/*
 * Data in phase.
 *  Send len block while reading from SD card.
 */
void writeDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAIN PHASE(SD)");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAIN);
  //Bus settle delay 400ns, file.seek() measured at over 1000ns.

  uint64_t pos = (uint64_t)adds * m_img->m_blocksize;
  m_img->m_file.seekSet(pos);

  SCSI_DB_OUTPUT()
  for(uint32_t i = 0; i < len; i++) {
      // Asynchronous reads will make it faster ...
    m_resetJmp = false;
    m_img->m_file.read(m_buf, m_img->m_blocksize);
    enableResetJmp();

    writeDataLoop(m_img->m_blocksize, m_buf);
  }
  SCSI_DB_INPUT()
#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_DBP,TR_INPUT)
#endif
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
  LOGN("DATAOUT PHASE");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAOUT);
  // Bus settle delay 400ns. The following code was measured at 450ns before REQ asserted. STM32F103.
  readDataLoop(len, p);
}

/*
 * Data out phase.
 *  Write to SD card while reading len block.
 */
void readDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAOUT PHASE(SD)");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAOUT);
  //Bus settle delay 400ns, file.seek() measured at over 1000ns.

  uint64_t pos = (uint64_t)adds * m_img->m_blocksize;
  m_img->m_file.seekSet(pos);
  for(uint32_t i = 0; i < len; i++) {
    m_resetJmp = true;
    readDataLoop(m_img->m_blocksize, m_buf);
    m_resetJmp = false;
    m_img->m_file.write(m_buf, m_img->m_blocksize);
    // If a reset happened while writing, break and let the flush happen before it is handled.
    if (m_isBusReset) {
      break;
    }
  }
  m_img->m_file.flush();
  enableResetJmp();
}

/*
 * Data out phase.
 * Compare to SD card while reading len block.
 */
void verifyDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAOUT PHASE(SD)");
  SCSI_PHASE_CHANGE(SCSI_PHASE_DATAOUT);
  //Bus settle delay 400ns, file.seek() measured at over 1000ns.

  uint64_t pos = (uint64_t)adds * m_img->m_blocksize;
  m_img->m_file.seekSet(pos);
  for(uint32_t i = 0; i < len; i++) {
    readDataLoop(m_img->m_blocksize, m_buf);
    // This has just gone through the transfer to make things work, a compare would go here.
  }
}

/*
 * INQUIRY command processing.
 */
byte onInquiryCommand(byte len)
{
  writeDataPhase(len < 36 ? len : 36, SCSI_INFO_BUF);
  return SCSI_STATUS_GOOD;
}

/*
 * REQUEST SENSE command processing.
 */
void onRequestSenseCommand(byte len)
{
  byte buf[18] = {
    0x70,   //CheckCondition
    0,      //Segment number
    m_senseKey,   //Sense key
    0, 0, 0, 0,  //information
    10,   //Additional data length
    0, 0, 0, 0, // command specific information bytes
    (byte)(m_addition_sense >> 8),
    (byte)m_addition_sense,
    0, 0, 0, 0,
  };
  m_senseKey = 0;
  m_addition_sense = 0;
  writeDataPhase(len < 18 ? len : 18, buf);  
}

/*
 * READ CAPACITY command processing.
 */
byte onReadCapacityCommand(byte pmi)
{
  if(!m_img) {
    m_senseKey = SCSI_SENSE_NOT_READY;
    m_addition_sense = SCSI_ASC_LUN_NOT_READY_MANUAL_INTERVENTION_REQUIRED;
    return SCSI_STATUS_CHECK_CONDITION;
  }
  
  uint32_t bl = m_img->m_blocksize;
  uint32_t bc = m_img->m_fileSize / bl - 1; // Points to last LBA
  uint8_t buf[8] = {
    bc >> 24, bc >> 16, bc >> 8, bc,
    bl >> 24, bl >> 16, bl >> 8, bl    
  };
  writeDataPhase(8, buf);
  return SCSI_STATUS_GOOD;
}

/*
 * Check that the image file is present and the block range is valid.
 */
byte checkBlockCommand(uint32_t adds, uint32_t len)
{
  // Check that image file is present
  if(!m_img) {
    m_senseKey = SCSI_SENSE_NOT_READY;
    m_addition_sense = SCSI_ASC_LUN_NOT_READY_MANUAL_INTERVENTION_REQUIRED;
    return SCSI_STATUS_CHECK_CONDITION;
  }
  // Check block range is valid
  uint32_t bc = m_img->m_fileSize / m_img->m_blocksize;
  if (adds >= bc || (adds + len) > bc) {
    m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    m_addition_sense = SCSI_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
    return SCSI_STATUS_CHECK_CONDITION;
  }
  return SCSI_STATUS_GOOD;
}

/*
 * READ6 / 10 Command processing.
 */
byte onReadCommand(uint32_t adds, uint32_t len)
{
  LOGN("-R");
  LOGHEXN(adds);
  LOGHEXN(len);

  byte sts = checkBlockCommand(adds, len);
  if (sts) {
    return sts;
  }
  LED_ON();
  writeDataPhaseSD(adds, len);
  LED_OFF();
  return SCSI_STATUS_GOOD;
}

/*
 * WRITE6 / 10 Command processing.
 */
byte onWriteCommand(uint32_t adds, uint32_t len)
{
  LOGN("-W");
  LOGHEXN(adds);
  LOGHEXN(len);
  
  byte sts = checkBlockCommand(adds, len);
  if (sts) {
    return sts;
  }
  LED_ON();
  readDataPhaseSD(adds, len);
  LED_OFF();
  return SCSI_STATUS_GOOD;
}

/*
 * VERIFY10 Command processing.
 */
    
byte onVerifyCommand(byte flags, uint32_t adds, uint32_t len)
{
  byte sts = checkBlockCommand(adds, len);
  if (sts) {
    return sts;
  }
  int bytchk = (flags >> 1) & 0x03;
  if (bytchk != 0) {
    if (bytchk == 3) {
      // Data-Out buffer is single logical block for repeated verification.
      len = m_img->m_blocksize;
    }
    LED_ON();
    verifyDataPhaseSD(adds, len);
    LED_OFF();
  }
  return SCSI_STATUS_GOOD;
}

/*
 * MODE SENSE command processing.
 */
byte onModeSenseCommand(byte scsi_cmd, byte dbd, byte cmd2, uint32_t len)
{
  if(!m_img) {
    m_senseKey = SCSI_SENSE_NOT_READY;
    m_addition_sense = SCSI_ASC_LUN_NOT_READY_MANUAL_INTERVENTION_REQUIRED;
    return SCSI_STATUS_CHECK_CONDITION;
  }

  uint32_t bl =  m_img->m_blocksize;
  uint32_t bc = m_img->m_fileSize / bl;

  memset(m_buf, 0, sizeof(m_buf));
  int pageCode = cmd2 & 0x3F;
  int pageControl = cmd2 >> 6;
  int a = 4;
  if(scsi_cmd == 0x5A) a = 8;

  if(dbd == 0) {
    byte c[8] = {
      0,//Density code
      bc >> 16, bc >> 8, bc,
      0, //Reserve
      bl >> 16, bl >> 8, bl    
    };
    memcpy(&m_buf[a], c, 8);
    a += 8;
  }
  switch(pageCode) {
  case 0x3F:
  case 0x01: // Read/Write Error Recovery
    m_buf[a + 0] = 0x01;
    m_buf[a + 1] = 0x0A;
    a += 0x0C;
    if(pageCode != 0x3F) break;

  case 0x02: // Disconnect-Reconnect page
    m_buf[a + 0] = 0x02;
    m_buf[a + 1] = 0x0A;
    a += 0x0C;
    if(pageCode != 0x3f) break;

  case 0x03:  //Drive parameters
    m_buf[a + 0] = 0x03; //Page code
    m_buf[a + 1] = 0x16; // Page length
    if(pageControl != 1) {
      m_buf[a + 11] = 0x3F;//Number of sectors / track
      m_buf[a + 12] = (byte)(m_img->m_blocksize >> 8);
      m_buf[a + 13] = (byte)m_img->m_blocksize;
      m_buf[a + 15] = 0x1; // Interleave
    }
    a += 0x18;
    if(pageCode != 0x3F) break;

  case 0x04:  //Drive parameters
    m_buf[a + 0] = 0x04; //Page code
    m_buf[a + 1] = 0x16; // Page length
    if(pageControl != 1) {
      unsigned cylinders = bc / (16 * 63);
      m_buf[a + 2] = (byte)(cylinders >> 16); // Cylinders
      m_buf[a + 3] = (byte)(cylinders >> 8);
      m_buf[a + 4] = (byte)cylinders;
      m_buf[a + 5] = 16;   //Number of heads
    }
    a += 0x18;
    if(pageCode != 0x3F) break;
  case 0x30:
    {
      const byte page30[0x14] = {0x41, 0x50, 0x50, 0x4C, 0x45, 0x20, 0x43, 0x4F, 0x4D, 0x50, 0x55, 0x54, 0x45, 0x52, 0x2C, 0x20, 0x49, 0x4E, 0x43, 0x20};
      m_buf[a + 0] = 0x30; // Page code
      m_buf[a + 1] = sizeof(page30); // Page length
      if(pageControl != 1) {
        memcpy(&m_buf[a + 2], page30, sizeof(page30));
      }
      a += 2 + sizeof(page30);
      if(pageCode != 0x3F) break;
    }
    break; // Don't want 0x3F falling through to error condition

  default:
    m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    m_addition_sense = SCSI_ASC_INVALID_FIELD_IN_CDB;
    return SCSI_STATUS_CHECK_CONDITION;
    break;
  }
  if(scsi_cmd == SCSI_MODE_SENSE10)
  {
    m_buf[1] = a - 2;
    m_buf[7] = 0x08;
  }
  else
  {
    m_buf[0] = a - 1;
    m_buf[3] = 0x08;
  }
  writeDataPhase(len < a ? len : a, m_buf);
  return SCSI_STATUS_GOOD;
}
    
byte onModeSelectCommand(byte scsi_cmd, byte flags, uint32_t len)
{
  if (len > MAX_BLOCKSIZE) {
    m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    m_addition_sense = SCSI_ASC_INVALID_FIELD_IN_CDB;
    return SCSI_STATUS_CHECK_CONDITION;
  }
  readDataPhase(len, m_buf);
  //Apple HD SC Setup sends:
  //0 0 0 8 0 0 0 0 0 0 2 0 0 2 10 0 1 6 24 10 8 0 0 0
  //I believe mode page 0 set to 10 00 is Disable Unit Attention
  //Mode page 1 set to 24 10 08 00 00 00 is TB and PER set, read retry count 16, correction span 8
  for (unsigned i = 0; i < len; i++) {
    LOGHEX(m_buf[i]);LOG(" ");
  }
  LOGN("");
  return SCSI_STATUS_GOOD;
}

/*
 * Test Unit Ready command processing.
*/
byte onTestUnitReady()
{
  // Check that image file is present
  if(!m_img) {
    m_senseKey = SCSI_SENSE_NOT_READY;
    m_addition_sense = SCSI_ASC_MEDIUM_NOT_PRESENT;
    return SCSI_STATUS_CHECK_CONDITION;
  }
  return SCSI_STATUS_GOOD;
}

/*
 * MsgIn2.
 */
void MsgIn2(int msg)
{
  LOGN("MsgIn2");
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

  // Wait until RST = H, BSY = H, SEL = L
  do {} while( SCSI_IN(vBSY) || !SCSI_IN(vSEL) || SCSI_IN(vRST));

  // BSY+ SEL-
  // If the ID to respond is not driven, wait for the next
  //byte db = readIO();
  //byte scsiid = db & scsi_id_mask;
  byte scsiid = readIO() & scsi_id_mask;
  if((scsiid) == 0) {
    delayMicroseconds(1);
    return;
  }
  LOGN("Selection");
  m_isBusReset = false;
  if (setjmp(m_resetJmpBuf) == 1) {
    LOGN("Reset, going to BusFree");
    goto BusFree;
  }
  enableResetJmp();
  
  // Set BSY to-when selected
  SCSI_BSY_ACTIVE();     // Turn only BSY output ON, ACTIVE

  // Ask for a TARGET-ID to respond
  m_id = 31 - __builtin_clz(scsiid);

  // Wait until SEL becomes inactive
  while(isHigh(gpio_read(SEL)) && isLow(gpio_read(BSY))) {
  }
  
#ifdef XCVR
  // Reconfigure target pins to output mode, after resetting their values
  GPIOB->regs->BSRR = 0x000000E8; // MSG, CD, REQ, IO
//  GPIOA->regs->BSRR = 0x00000200; // BSY
#endif
  SCSI_TARGET_ACTIVE()  // (BSY), REQ, MSG, CD, IO output turned on

  //  
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

  LOG("Command:");
  SCSI_PHASE_CHANGE(SCSI_PHASE_COMMAND);
  // Bus settle delay 400ns. The following code was measured at 20ns before REQ asserted. Added another 380ns. STM32F103.
  asm("nop;nop;nop;nop;nop;nop;nop;nop");// This asm causes some code reodering, which adds 270ns, plus 8 nop cycles for an additional 110ns. STM32F103
  int len;
  byte cmd[12];
  cmd[0] = readHandshake();
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
  m_sts = cmd[1]&0xe0;      // Preset LUN in status byte
  m_lun = m_sts>>5;
  // HDD Image selection
  m_img = (HDDIMG *)0; // None
  if( (m_lun <= NUM_SCSILUN) )
  {
    m_img = &(img[m_id][m_lun]); // There is an image
    if(!(m_img->m_file.isOpen()))
      m_img = (HDDIMG *)0;       // Image absent
  }
  // if(!m_img) m_sts |= 0x02;            // Missing image file for LUN
  //LOGHEX(((uint32_t)m_img));
  
  LOG(":ID ");
  LOG(m_id);
  LOG(":LUN ");
  LOG(m_lun);

  LOGN("");
  switch(cmd[0]) {
  case SCSI_TEST_UNIT_READY: // TODO: Implement me!
    LOGN("[Test Unit Ready]");
    m_sts |= onTestUnitReady();
    break;
  case SCSI_REZERO_UNIT: // TODO: Implement me!
    LOGN("[Rezero Unit]");
    break;
  case SCSI_REQUEST_SENSE:
    LOGN("[RequestSense]");
    onRequestSenseCommand(cmd[4]);
    break;
  case SCSI_FORMAT_UNIT4: // TODO: Implement me!
    LOGN("[FormatUnit4]");
    break;
  case SCSI_FORMAT_UNIT6: // TODO: Implement me!
    LOGN("[FormatUnit6]");
    break;
  case SCSI_REASSIGN_BLOCKS: // TODO: Implement me!
    LOGN("[ReassignBlocks]");
    break;
  case SCSI_READ6:
    LOGN("[Read6]");
    m_sts |= onReadCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
    break;
  case SCSI_WRITE6:
    LOGN("[Write6]");
    m_sts |= onWriteCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
    break;
  case SCSI_SEEK6: // TODO: Implement me!
    LOGN("[Seek6]");
    break;
  case SCSI_INQUIRY:
    LOGN("[Inquiry]");
    m_sts |= onInquiryCommand(cmd[4]);
    break;
  case SCSI_MODE_SELECT6:
    LOGN("[ModeSelect6]");
    m_sts |= onModeSelectCommand(cmd[0], cmd[1], cmd[4]);
    break;
  case SCSI_MODE_SENSE6:
    LOGN("[ModeSense6]");
    m_sts |= onModeSenseCommand(cmd[0], cmd[1]&0x80, cmd[2], cmd[4]);
    break;
  case SCSI_START_STOP_UNIT: // TODO: Implement me!
    LOGN("[StartStopUnit]");
    break;
  case SCSI_PREVENT_ALLOW_REMOVAL: // TODO: Implement me!
    LOGN("[PreAllowMed.Removal]");
    break;
  case SCSI_READ_CAPACITY:
    LOGN("[ReadCapacity]");
    m_sts |= onReadCapacityCommand(cmd[8]);
    break;
  case SCSI_READ10:
    LOGN("[Read10]");
    m_sts |= onReadCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  case SCSI_WRITE10:
    LOGN("[Write10]");
    m_sts |= onWriteCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  case SCSI_SEEK10: // TODO: Implement me!
    LOGN("[Seek10]");
    break;
  case SCSI_VERIFY10:
    LOGN("[Verify10]");
    m_sts |= onVerifyCommand(cmd[1], ((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  case SCSI_SYNCHRONIZE_CACHE: // TODO: Implement me!
    LOGN("[SynchronizeCache10]");
    break;
  case SCSI_MODE_SELECT10:
    LOGN("[ModeSelect10");
    m_sts |= onModeSelectCommand(cmd[0], cmd[1], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  case SCSI_MODE_SENSE10:
    LOGN("[ModeSense10]");
    m_sts |= onModeSenseCommand(cmd[0], cmd[1] & 0x80, cmd[2], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  default:
    LOGN("[*Unknown]");
    m_sts |= SCSI_STATUS_CHECK_CONDITION;
    m_senseKey = SCSI_SENSE_ILLEGAL_REQUEST;
    m_addition_sense = SCSI_ASC_INVALID_OPERATION_CODE;
    break;
  }

  LOGN("Sts");
  SCSI_PHASE_CHANGE(SCSI_PHASE_STATUS);
  // Bus settle delay 400ns built in to writeHandshake
  writeHandshake(m_sts);

  LOGN("MsgIn");
  SCSI_PHASE_CHANGE(SCSI_PHASE_MESSAGEIN);
  // Bus settle delay 400ns built in to writeHandshake
  writeHandshake(m_msg);

BusFree:
  LOGN("BusFree");
  m_isBusReset = false;
  //SCSI_OUT(vREQ,inactive) // gpio_write(REQ, low);
  //SCSI_OUT(vMSG,inactive) // gpio_write(MSG, low);
  //SCSI_OUT(vCD ,inactive) // gpio_write(CD, low);
  //SCSI_OUT(vIO ,inactive) // gpio_write(IO, low);
  //SCSI_OUT(vBSY,inactive)
  SCSI_TARGET_INACTIVE() // Turn off BSY, REQ, MSG, CD, IO output
#ifdef XCVR
  TRANSCEIVER_IO_SET(vTR_TARGET,TR_INPUT);
#endif
}
