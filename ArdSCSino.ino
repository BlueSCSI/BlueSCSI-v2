/*
 * SCSI-HD device emulator
 */
#include <SPI.h>
#include "SdFat.h"

//Set ENABLE_EXTENDED_TRANSFER_CLASS to 1
//libraries/SdFat/SdFatConfig.h
SPIClass SPI_2(2);
SdFatEX  SD(&SPI_2);

//#define SPI_SPEED SD_SCK_MHZ(18)
     
#define LOG(XX)     //Serial.print(XX)
#define LOGHEX(XX)  //Serial.print(XX, HEX)
#define LOGN(XX)    //Serial.println(XX)
#define LOGHEXN(XX) //Serial.println(XX, HEX)

#define high 0
#define low 1

#define isHigh(XX) ((XX) == high)
#define isLow(XX) ((XX) != high)

#define gpio_mode(pin,val) gpio_set_mode(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit, val);
#define gpio_write(pin,val) gpio_write_bit(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit, val)
#define gpio_read(pin) gpio_read_bit(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit)

//#define DB0       PA0     // SCSI:DB0
//#define DB1       PA1     // SCSI:DB1
//#define DB2       PA2     // SCSI:DB2
//#define DB3       PA3     // SCSI:DB3
//#define DB4       PA4     // SCSI:DB4
//#define DB5       PA5     // SCSI:DB5
//#define DB6       PA6     // SCSI:DB6
//#define DB7       PA7     // SCSI:DB7
//#define DBP       PA8     // SCSI:DBP

#define ATN       PB0     // SCSI:ATN
#define BSY       PB1     // SCSI:BSY
#define ACK       PB10    // SCSI:ACK
#define RST       PB11    // SCSI:RST
#define MSG       PB5     // SCSI:MSG
#define SEL       PB6     // SCSI:SEL
#define CD        PB7     // SCSI:C/D
#define REQ       PB8     // SCSI:REQ
#define IO        PB9     // SCSI:I/O

#define SD_CS     PB12     // SDCARD:CS
#define LED       PC13     // LED

#define SCSIID    0                 // SCSI-ID 

#define BLOCKSIZE 512               // 1BLOCK size
uint8_t       m_senseKey = 0;       // Sense key
volatile bool m_isBusReset = false; // Bus reset

#define HDIMG_FILE "HD.HDS"         // HD image file name
File          m_file;               // File object
uint32_t      m_fileSize;           // file size
byte          m_buf[BLOCKSIZE];     // General purpose buffer

int           m_msc;
bool          m_msb[256];

/*
 * IO read
 */
inline byte readIO(void)
{
  //GPIO (SCSI BUS) initialization
  //Port setting register (lower level)
  GPIOA->regs->CRL = 0x88888888; // Configure GPIOA[7:0]
  uint32 ret = GPIOA->regs->IDR;
  byte bret =  0x00;
  bret |= ((!bitRead(ret,7)) << 7);
  bret |= ((!bitRead(ret,6)) << 6);
  bret |= ((!bitRead(ret,5)) << 5);
  bret |= ((!bitRead(ret,4)) << 4);
  bret |= ((!bitRead(ret,3)) << 3);
  bret |= ((!bitRead(ret,2)) << 2);
  bret |= ((!bitRead(ret,1)) << 1);
  bret |= ((!bitRead(ret,0)) << 0);
  return bret;
}

/* 
 * IO writing.
 */
inline void writeIO(byte v)
{
  //GPIO (SCSI BUS) initialization
  //Port setting register (lower)
//  GPIOA->regs->CRL = 0x11111111; // Configure GPIOA PP[7:0]10MHz
  GPIOA->regs->CRL = 0x33333333;  // Configure GPIOA PP[7:0]50MHz
  //Port setting register (upper)
  GPIOA->regs->CRH = 0x00000003;  // Configure GPIOA PP[16:8]50MHz
  uint32 retL =  0x00;
  uint32 retH =  0x00;

  if(!parity(v)) {
    bitWrite(retL, 8, 1);
  } else {
    bitWrite(retH, 8, 1);
  }
  if(v & ( 1 << 7 )) {
    bitWrite(retL, 7, 1);
  } else {
    bitWrite(retH, 7, 1);
  }
  if(v & ( 1 << 6 )) {
    bitWrite(retL, 6, 1);
  } else {
    bitWrite(retH, 6, 1);
  }
  if(v & ( 1 << 5 )) {
    bitWrite(retL, 5, 1);
  } else {
    bitWrite(retH, 5, 1);
  }
  if(v & ( 1 << 4 )) {
    bitWrite(retL, 4, 1);
  } else {
    bitWrite(retH, 4, 1);
  }
  if(v & ( 1 << 3 )) {
    bitWrite(retL, 3, 1);
  } else {
    bitWrite(retH, 3, 1);
  }
  if(v & ( 1 << 2 )) {
    bitWrite(retL, 2, 1);
  } else {
    bitWrite(retH, 2, 1);
  }
  if(v & ( 1 << 1 )) {
    bitWrite(retL, 1, 1);
  } else {
    bitWrite(retH, 1, 1);
  }
  if(v & ( 1 << 0 )) {
    bitWrite(retL, 0, 1);
  } else {
    bitWrite(retH, 0, 1);
  }
  // Bit set to LOW
  GPIOA->regs->BRR = retL ;
  // Bit set to HIGH
  GPIOA->regs->BSRR = retH ;
}

/*
 * Initialization.
 *  Parity check
 */
inline int parity(byte val) {
  val ^= val >> 16;
  val ^= val >> 8;
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;

  return val & 0x00000001;
}

/*
 * Initialization.
 * Initialize the bus and set the PIN orientation
 */
void setup()
{
  // PA15 / PB3 / PB4 Cannot be used
  // JTAG Because it is used for debugging.
  disableDebugPorts();

  //Serial initialization
  //Serial.begin(9600);
  //while (!Serial);

  //PIN initialization
  gpio_mode(LED, GPIO_OUTPUT_OD);
  gpio_write(LED, low);

  //GPIO(SCSI BUS)Initialization
  //Port setting register (lower)
  GPIOA->regs->CRL = 0x888888888; // Configure GPIOA[8:0]

  gpio_mode(ATN, GPIO_INPUT_PU);
  gpio_mode(BSY, GPIO_INPUT_PU);
  gpio_mode(ACK, GPIO_INPUT_PU);
  gpio_mode(RST, GPIO_INPUT_PU);
  gpio_mode(SEL, GPIO_INPUT_PU);
  
  gpio_mode(MSG, GPIO_OUTPUT_PP);
  gpio_mode(CD, GPIO_OUTPUT_PP);
  gpio_mode(REQ, GPIO_OUTPUT_PP);
  gpio_mode(IO, GPIO_OUTPUT_PP);

  gpio_write(MSG, low);
  gpio_write(CD, low);
  gpio_write(REQ, low);
  gpio_write(IO, low);

  //Occurs when the RST pin state changes from HIGH to LOW
  attachInterrupt(PIN_MAP[RST].gpio_bit, onBusReset, FALLING);
  
  if(!SD.begin(SD_CS,SPI_FULL_SPEED)) {
    Serial.println("SD initialization failed!");
    onFalseInit();
  }
  //HD image file
  m_file = SD.open(HDIMG_FILE, O_RDWR);
  if(!m_file) {
    Serial.println("Error: open hdimg");
    onFalseInit();
  }
  m_fileSize = m_file.size();
  Serial.println("Found Valid HD Image File.");
  Serial.print(m_fileSize);
  Serial.println("byte");
  Serial.print(m_fileSize / 1024);
  Serial.println("KB");
  Serial.print(m_fileSize / 1024 / 1024);
  Serial.println("MB");
}

/*
 * Initialization failure.
 */
void onFalseInit(void)
{
  while(true) {
    gpio_write(LED, high);
    delay(500); 
    gpio_write(LED, low);
    delay(500);
  }
}

/*
 * Bus reset interrupt.
 */
void onBusReset(void)
{
  if(isHigh(gpio_read(RST))) {
    delayMicroseconds(20);
    if(isHigh(gpio_read(RST))) {
      LOGN("BusReset!");
      m_isBusReset = true;
    }
  }
}

/*
 * Read by handshake.
 */
byte readHandshake(void)
{
  gpio_write(REQ, high);
  while(isLow(gpio_read(ACK))) {
    if(m_isBusReset) {
      return 0;
    }
  }
  byte r = readIO();
  gpio_write(REQ, low);
  while(isHigh(gpio_read(ACK))) {
    if(m_isBusReset) {
      return 0;
    }
  }
  return r;  
}

/*
 * Write with a handshake.
 */
void writeHandshake(byte d)
{
  writeIO(d);
  gpio_write(REQ, high);
  while(isLow(gpio_read(ACK))) {
    if(m_isBusReset) {
      return;
    }
  }
  gpio_write(REQ, low);
  while(isHigh(gpio_read(ACK))) {
    if(m_isBusReset) {
      return;
    }
  }
}

/*
 * Data in phase.
 *  Send len bytes of data array p.
 */
void writeDataPhase(int len, byte* p)
{
  LOGN("DATAIN PHASE");
  gpio_write(MSG, low);
  gpio_write(CD, low);
  gpio_write(IO, high);
  for (int i = 0; i < len; i++) {
    if(m_isBusReset) {
      return;
    }
    writeHandshake(p[i]);
  }
}

/* 
 * Data in phase.
  * Send len block while reading from SD card.
*/
void writeDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAIN PHASE(SD)");
  uint32_t pos = adds * BLOCKSIZE;
  m_file.seek(pos);
  gpio_write(MSG, low);
  gpio_write(CD, low);
  gpio_write(IO, high);
  for(uint32_t i = 0; i < len; i++) {
    m_file.read(m_buf, BLOCKSIZE);
    for(int j = 0; j < BLOCKSIZE; j++) {
      if(m_isBusReset) {
        return;
      }
      writeHandshake(m_buf[j]);
    }
  }
}

/*
 * Data out phase.
  * Write to SD card while reading len block.
*/
void readDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAOUT PHASE(SD)");
  uint32_t pos = adds * BLOCKSIZE;
  m_file.seek(pos);
  gpio_write(MSG, low);
  gpio_write(CD, low);
  gpio_write(IO, low);
  for(uint32_t i = 0; i < len; i++) {
    for(int j = 0; j < BLOCKSIZE; j++) {
      if(m_isBusReset) {
        return;
      }
      m_buf[j] = readHandshake();
    }
    m_file.write(m_buf, BLOCKSIZE);
  }
  m_file.flush();
}

/*
 * INQUIRY command processing.
 */
void onInquiryCommand(byte len)
{
  byte buf[36] = {
    0x00, //Device type
    0x00, //RMB = 0
    0x01, //ISO,ECMA,ANSI version
    0x01, //Response data format
    35 - 4, //Additional data length
    0, 0, //Reserve
    0x00, //Support function
    'T', 'N', 'B', ' ', ' ', ' ', ' ', ' ',
    'A', 'r', 'd', 'S', 'C', 'S', 'i', 'n', 'o', ' ', ' ',' ', ' ', ' ', ' ', ' ',
    '0', '0', '1', '0',
  };
  writeDataPhase(len < 36 ? len : 36, buf);
}

/*
 * REQUEST SENSE command processing.
 */
void onRequestSenseCommand(byte len)
{
  byte buf[18] = {
    0x70,   //CheckCondition
    0,      //Segment number
    0x00,   //Sense key
    0, 0, 0, 0,  //information
    17 - 7 ,   //Additional data length
    0,
  };
  buf[2] = m_senseKey;
  m_senseKey = 0;
  writeDataPhase(len < 18 ? len : 18, buf);  
}

/*
 * READ CAPACITY command processing.
 */
void onReadCapacityCommand(byte pmi)
{
  uint32_t bc = m_fileSize / BLOCKSIZE;
  uint32_t bl = BLOCKSIZE;
  uint8_t buf[8] = {
    bc >> 24, bc >> 16, bc >> 8, bc,
    bl >> 24, bl >> 16, bl >> 8, bl    
  };
  writeDataPhase(8, buf);
}

/*
 * READ6/10 Command processing.
 */
byte onReadCommand(uint32_t adds, uint32_t len)
{
  LOGN("-R");
  LOGHEXN(adds);
  LOGHEXN(len);
  gpio_write(LED, high);
  writeDataPhaseSD(adds, len);
  gpio_write(LED, low);
  return 0; //sts
}

/*
 * WRITE6/10 Command processing.
 */
byte onWriteCommand(uint32_t adds, uint32_t len)
{
  LOGN("-W");
  LOGHEXN(adds);
  LOGHEXN(len);
  gpio_write(LED, high);
  readDataPhaseSD(adds, len);
  gpio_write(LED, low);
  return 0; //sts
}

/*
 * MODE SENSE command processing.
 */
void onModeSenseCommand(byte dbd, int pageCode, uint32_t len)
{
  memset(m_buf, 0, sizeof(m_buf)); 
  int a = 4;
  if(dbd == 0) {
    uint32_t bc = m_fileSize / BLOCKSIZE;
    uint32_t bl = BLOCKSIZE;
    byte c[8] = {
      0,//Dense code
      bc >> 16, bc >> 8, bc,
      0, //Reserve
      bl >> 16, bl >> 8, bl    
    };
    memcpy(&m_buf[4], c, 8);
    a += 8;
    m_buf[3] = 0x08;
  }
  switch(pageCode) {
  case 0x3F:
  case 0x03:  //Drive parameters
    m_buf[a + 0] = 0x03; //Page code
    m_buf[a + 1] = 0x16; //Page length
    m_buf[a + 11] = 0x3F;//number of sectors/track
    a += 24;
    if(pageCode != 0x3F) {
      break;
    }
  case 0x04:  //Drive parameters
    {
      uint32_t bc = m_fileSize / BLOCKSIZE;
      m_buf[a + 0] = 0x04; //Page code
      m_buf[a + 1] = 0x16; // Page length
      m_buf[a + 2] = bc >> 16;// Cylinder length
      m_buf[a + 3] = bc >> 8;
      m_buf[a + 4] = bc;
      m_buf[a + 5] = 1;   //Number of heads
      a += 24;
    }
    if(pageCode != 0x3F) {
      break;
    }
  default:
    break;
  }
  m_buf[0] = a - 1;
  writeDataPhase(len < a ? len : a, m_buf);
}

/*
 * MsgIn2.
 */
void MsgIn2(int msg)
{
  LOGN("MsgIn2");
  gpio_write(MSG, high);
  gpio_write(CD, high);
  gpio_write(IO, high);
  writeHandshake(msg);
}

/*
 * MsgOut2.
 */
void MsgOut2()
{
  LOGN("MsgOut2");
  gpio_write(MSG, high);
  gpio_write(CD, high);
  gpio_write(IO, low);
  m_msb[m_msc] = readHandshake();
  m_msc++;
  m_msc %= 256;
}

/*
 * Main loop.
 */
void loop() 
{
  int sts = 0;
  int msg = 0;

  //BSY,SEL + is bus free
  // Selection check
  // Loop between BSY-
  if(isHigh(gpio_read(BSY))) {
    return;
  }
  // Loop while SEL is +
  if(isLow(gpio_read(SEL))) {
    return;
  }
  // BSY+ SEL-
  byte db = readIO();  
  if((db & (1 << SCSIID)) == 0) {
    return;
  }

  LOGN("Selection");
  m_isBusReset = false;
  // Set BSY to-when selected
  gpio_mode(BSY, GPIO_OUTPUT_PP);
  gpio_write(BSY, high);
  while(isHigh(gpio_read(SEL))) {
    if(m_isBusReset) {
      goto BusFree;
    }
  }
  if(isHigh(gpio_read(ATN))) {
    bool syncenable = false;
    int syncperiod = 50;
    int syncoffset = 0;
    m_msc = 0;
    memset(m_msb, 0x00, sizeof(m_msb));
    while(isHigh(gpio_read(ATN))) {
      MsgOut2();
    }
    for(int i = 0; i < m_msc; i++) {
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
          syncoffset = 50;
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

  LOGN("Command");
  gpio_write(MSG, low);
  gpio_write(CD, high);
  gpio_write(IO, low);
  int len;
  byte cmd[12];
  cmd[0] = readHandshake();
  LOGHEX(cmd[0]);
  len = 1;
  switch(cmd[0] >> 5) {
  case 0b000:
    len = 6;
    break;
  case 0b001:
    len = 10;
    break;
  case 0b010:
    len = 10;
    break;
  case 0b101:
    len = 12;
    break;
  default:
    break;
  }
  for(int i = 1; i < len; i++ ) {
    cmd[i] = readHandshake();
    LOGHEX(cmd[i]);
  }
  LOGN("");

  switch(cmd[0]) {
  case 0x00:
    LOGN("[Test Unit]");
    break;
  case 0x01:
    LOGN("[Rezero Unit]");
    break;
  case 0x03:
    LOGN("[RequestSense]");
    onRequestSenseCommand(cmd[4]);
    break;
  case 0x04:
    LOGN("[FormatUnit]");
    break;
  case 0x06:
    LOGN("[FormatUnit]");
    break;
  case 0x07:
    LOGN("[ReassignBlocks]");
    break;
  case 0x08:
    LOGN("[Read6]");
    sts = onReadCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
    break;
  case 0x0A:
    LOGN("[Write6]");
    sts = onWriteCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
    break;
  case 0x0B:
    LOGN("[Seek6]");
    break;
  case 0x12:
    LOGN("[Inquiry]");
    onInquiryCommand(cmd[4]);
    break;
  case 0x1A:
    LOGN("[ModeSense6]");
    onModeSenseCommand(cmd[1]&0x80, cmd[2] & 0x3F, cmd[4]);
    break;
  case 0x1B:
    LOGN("[StartStopUnit]");
    break;
  case 0x1E:
    LOGN("[PreAllowMed.Removal]");
    break;
  case 0x25:
    LOGN("[ReadCapacity]");
    onReadCapacityCommand(cmd[8]);
    break;
  case 0x28:
    LOGN("[Read10]");
    sts = onReadCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  case 0x2A:
    LOGN("[Write10]");
    sts = onWriteCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  case 0x2B:
    LOGN("[Seek10]");
    break;
  case 0x5A:
    LOGN("[ModeSense10]");
    onModeSenseCommand(cmd[1] & 0x80, cmd[2] & 0x3F, ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  default:
    LOGN("[*Unknown]");
    sts = 2;
    m_senseKey = 5;
    break;
  }
  if(m_isBusReset) {
     goto BusFree;
  }

  LOGN("Sts");
  gpio_write(MSG, low);
  gpio_write(CD, high);
  gpio_write(IO, high);
  writeHandshake(sts);
  if(m_isBusReset) {
     goto BusFree;
  }

  LOGN("MsgIn");
  gpio_write(MSG, high);
  gpio_write(CD, high);
  gpio_write(IO, high);
  writeHandshake(msg);

BusFree:
  LOGN("BusFree");
  m_isBusReset = false;
  gpio_write(REQ, low);
  gpio_write(MSG, low);
  gpio_write(CD, low);
  gpio_write(IO, low);
//  gpio_write(BSY, low);
  gpio_mode(BSY, GPIO_INPUT_PU);
}
