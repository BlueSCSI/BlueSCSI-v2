/*
 * SCSI-HDデバイスエミュレータ for STM32F103
 */
#include <SdFat.h>

#ifdef USE_STM32_DMA
#warning "warning USE_STM32_DMA"
#endif

#define DEBUG            0      // 0:デバッグ情報出力なし 1:デバッグ情報出力あり 

#define SCSI_SELECT      0      // 0 for STANDARD
                                // 1 for SHARP X1turbo
                                // 2 for NEC PC98
#define READ_SPEED_OPTIMIZE  1 // リードの高速化
#define WRITE_SPEED_OPTIMIZE 1 // ライトの高速化
#define USE_DB2ID_TABLE      1 // SEL-DBからIDの取得にテーブル使用

// SCSI config
#define NUM_SCSIID	7          // サポート最大SCSI-ID数 (最小は0)
#define NUM_SCSILUN	2          // サポート最大LUN数     (最小は0)
#define READ_PARITY_CHECK 0    // リードパリティーチェックを行う（未検証）

// HDD format
#define MAX_BLOCKSIZE 1024     // 最大BLOCKサイズ

// SDFAT
#define SD1_CONFIG SdSpiConfig(PA4, SHARED_SPI, SD_SCK_MHZ(SPI_FULL_SPEED), &SPI) 
SdFs SD;

#if DEBUG 
#define LOG(XX)     Serial.print(XX)
#define LOGHEX(XX)  Serial.print(XX, HEX)
#define LOGN(XX)    Serial.println(XX)
#define LOGHEXN(XX) Serial.println(XX, HEX)
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

#define SD_CS     PA4      // SDCARD:CS
#define LED       PC13     // LED

// GPIOレジスタポート
#define PAREG GPIOA->regs
#define PBREG GPIOB->regs

// LED control
#define LED_ON()       gpio_write(LED, high);
#define LED_OFF()      gpio_write(LED, low);

// 仮想ピン（Arduio互換は遅いのでMCU依存にして）
#define PA(BIT)       (BIT)
#define PB(BIT)       (BIT+16)
// 仮想ピンのデコード
#define GPIOREG(VPIN)    ((VPIN)>=16?PBREG:PAREG)
#define BITMASK(VPIN) (1<<((VPIN)&15))

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

// SCSI 出力ピン制御 : opendrain active LOW (direct pin drive)
#define SCSI_OUT(VPIN,ACTIVE) { GPIOREG(VPIN)->BSRR = BITMASK(VPIN)<<((ACTIVE)?16:0); }

// SCSI 入力ピン確認(inactive=0,avtive=1)
#define SCSI_IN(VPIN) ((~GPIOREG(VPIN)->IDR>>(VPIN&15))&1)

// GPIO mode
// IN , FLOAT      : 4
// IN , PU/PD      : 8
// OUT, PUSH/PULL  : 3
// OUT, OD         : 1
//#define DB_MODE_OUT 3
#define DB_MODE_OUT 1
#define DB_MODE_IN  8

// DB,DPを出力モードにする
#define SCSI_DB_OUTPUT() { PBREG->CRL=(PBREG->CRL &0xfffffff0)|DB_MODE_OUT; PBREG->CRH = 0x11111111*DB_MODE_OUT; }
// DB,DPを入力モードにする
#define SCSI_DB_INPUT()  { PBREG->CRL=(PBREG->CRL &0xfffffff0)|DB_MODE_IN ; PBREG->CRH = 0x11111111*DB_MODE_IN;  }

// BSYだけ出力をON にする
#define SCSI_BSY_ACTIVE()      { gpio_mode(BSY, GPIO_OUTPUT_OD); SCSI_OUT(vBSY,  active) }
// BSY,REQ,MSG,CD,IO 出力をON にする (ODの場合は変更不要）
#define SCSI_TARGET_ACTIVE()   { }
// BSY,REQ,MSG,CD,IO 出力をOFFにする、BSYは最後、入力に
#define SCSI_TARGET_INACTIVE() { SCSI_OUT(vREQ,inactive); SCSI_OUT(vMSG,inactive); SCSI_OUT(vCD,inactive);SCSI_OUT(vIO,inactive); SCSI_OUT(vBSY,inactive); gpio_mode(BSY, GPIO_INPUT_PU); }

// HDDiamge file
#define HDIMG_FILE_256  "HDxx_256.HDS"  // BLOCKSIZE=256  のHDDイメージファイル
#define HDIMG_FILE_512  "HDxx_512.HDS"  // BLOCKSIZE=512  のHDDイメージファイル名ベース
#define HDIMG_FILE_1024 "HDxx_1024.HDS" // BLOCKSIZE=1024 のHDDイメージファイル
#define HDIMG_ID_POS  2                 // ID数字を埋め込む位置
#define HDIMG_LUN_POS 3                 // LUN数字を埋め込む位置
#define MAX_FILE_PATH 32                // 最大ファイル名長

// HDD image
typedef struct hddimg_struct
{
	FsFile      m_file;                 // ファイルオブジェクト
	uint64_t    m_fileSize;             // ファイルサイズ
	size_t      m_blocksize;            // SCSI BLOCKサイズ
}HDDIMG;
HDDIMG	img[NUM_SCSIID][NUM_SCSILUN]; // 最大個数分

uint8_t       m_senseKey = 0;         //センスキー
volatile bool m_isBusReset = false;   //バスリセット

byte          scsi_id_mask;           // 応答するSCSI IDのマスクリスト
byte          m_id;                   // 現在応答中の SCSI-ID
byte          m_lun;                  // 現在応答中のロジカルユニット番号
byte          m_sts;                  // ステータスバイト
byte          m_msg;                  // メッセージバイト
HDDIMG       *m_img;                  // 現在の SCSI-ID,LUNに対するHDD image
byte          m_buf[MAX_BLOCKSIZE+1]; // 汎用バッファ +オーバーランフェッチ
int           m_msc;
bool          m_msb[256];

/*
 *  データバイト to BSRRレジスタ設定値、兼パリティーテーブル
*/

// パリティービット生成
#define PTY(V)   (1^((V)^((V)>>1)^((V)>>2)^((V)>>3)^((V)>>4)^((V)>>5)^((V)>>6)^((V)>>7))&1)

// データバイト to BSRRレジスタ設定値変換テーブル
// BSRR[31:24] =  DB[7:0]
// BSRR[   16] =  PTY(DB)
// BSRR[15: 8] = ~DB[7:0]
// BSRR[    0] = ~PTY(DB)

// DBPのセット、REQ=inactiveにする
#define DBP(D)    ((((((uint32_t)(D)<<8)|PTY(D))*0x00010001)^0x0000ff01)|BITMASK(vREQ))

#define DBP8(D)   DBP(D),DBP(D+1),DBP(D+2),DBP(D+3),DBP(D+4),DBP(D+5),DBP(D+6),DBP(D+7)
#define DBP32(D)  DBP8(D),DBP8(D+8),DBP8(D+16),DBP8(D+24)

// DBのセット,DPのセット,REQ=H(inactrive) を同時に行うBSRRレジスタ制御値
static const uint32_t db_bsrr[256]={
  DBP32(0x00),DBP32(0x20),DBP32(0x40),DBP32(0x60),
  DBP32(0x80),DBP32(0xA0),DBP32(0xC0),DBP32(0xE0)
};
// パリティービット取得
#define PARITY(DB) (db_bsrr[DB]&1)

// マクロの掃除
#undef DBP32
#undef DBP8
//#undef DBP
//#undef PTY

#if USE_DB2ID_TABLE
/* DB to SCSI-ID 変換テーブル */
static const byte db2scsiid[256]={
  0xff,
  0,
  1,1,
  2,2,2,2,
  3,3,3,3,3,3,3,3,
  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
#endif

void onFalseInit(void);
void onBusReset(void);

/*
 * IO読み込み.
 */
inline byte readIO(void)
{
  //ポート入力データレジスタ
  uint32_t ret = GPIOB->regs->IDR;
  byte bret = (byte)((~ret)>>8);
#if READ_PARITY_CHECK
  if((db_bsrr[bret]^ret)&1)
    m_sts |= 0x01; // parity error
#endif

  return bret;
}

/*
 * HDDイメージファイルのオープン
 */

bool hddimageOpen(HDDIMG *h,const char *image_name,int id,int lun,int blocksize)
{
  char file_path[MAX_FILE_PATH+1];

  // build file path
  strcpy(file_path,image_name);
  file_path[HDIMG_ID_POS ] = '0'+id;
  file_path[HDIMG_LUN_POS] = '0'+lun;
  h->m_fileSize = 0;
  h->m_blocksize = blocksize;
  h->m_file = SD.open(file_path, O_RDWR);
  if(h->m_file.isOpen())
  {
    h->m_fileSize = h->m_file.size();
#if DEBUG
    Serial.print("Imagefile:");
    Serial.print(h->m_file.name() );
#endif
    if(h->m_fileSize>0)
    {
      // check blocksize dummy file
#if DEBUG
      Serial.print(" / ");
      Serial.print(h->m_fileSize);
      Serial.print("bytes / ");
      Serial.print(h->m_fileSize / 1024);
      Serial.print("KiB / ");
      Serial.print(h->m_fileSize / 1024 / 1024);
      Serial.println("MiB");
#endif
      return true; // ファイルが開けた
    }
    else
    {
      h->m_file.close();
      h->m_fileSize = h->m_blocksize = 0; // no file
#if DEBUG
      Serial.println("FileSizeError");
#endif
    }
  }
  return false;
}

/*
 * 初期化.
 *  バスの初期化、PINの向きの設定を行う
 */
void setup()
{
  // PA15 / PB3 / PB4 が使えない
  // JTAG デバッグ用に使われているからです。
  disableDebugPorts();

  //シリアル初期化
#if DEBUG
  Serial.begin(9600);
  while (!Serial);
#endif

  //PINの初期化
  gpio_mode(LED, GPIO_OUTPUT_OD);
  gpio_write(LED, low);

  //GPIO(SCSI BUS)初期化
  //ポート設定レジスタ（下位）
//  GPIOB->regs->CRL |= 0x000000008; // SET INPUT W/ PUPD on PAB-PB0
  //ポート設定レジスタ（上位）
  //GPIOB->regs->CRH = 0x88888888; // SET INPUT W/ PUPD on PB15-PB8
//  GPIOB->regs->ODR = 0x0000FF00; // SET PULL-UPs on PB15-PB8
  // DB,DPは入力モード
  SCSI_DB_INPUT()

  // 入力ポート
  gpio_mode(ATN, GPIO_INPUT_PU);
  gpio_mode(BSY, GPIO_INPUT_PU);
  gpio_mode(ACK, GPIO_INPUT_PU);
  gpio_mode(RST, GPIO_INPUT_PU);
  gpio_mode(SEL, GPIO_INPUT_PU);
  // 出力ポート
  gpio_mode(MSG, GPIO_OUTPUT_OD);
  gpio_mode(CD,  GPIO_OUTPUT_OD);
  gpio_mode(REQ, GPIO_OUTPUT_OD);
  gpio_mode(IO,  GPIO_OUTPUT_OD);
  // 出力ポートはOFFにする
  SCSI_TARGET_INACTIVE()

  //RSTピンの状態がHIGHからLOWに変わったときに発生
  //attachInterrupt(PIN_MAP[RST].gpio_bit, onBusReset, FALLING);

  LED_ON();

  // clock = 36MHz , about 4Mbytes/sec
  if(!SD.begin(SD1_CONFIG)) {
#if DEBUG
    Serial.println("SD initialization failed!");
#endif
    onFalseInit();
  }

  //セクタデータオーバーランバイトの設定
  m_buf[MAX_BLOCKSIZE] = 0xff; // DB0 all off,DBP off
  //HDイメージファイルオープン
  scsi_id_mask = 0x00;
  for(int id=0;id<NUM_SCSIID;id++)
  {
    for(int lun=0;lun<NUM_SCSILUN;lun++)
    {
      HDDIMG *h = &img[id][lun];
      bool imageReady = false;
      if(!imageReady)
      {
        imageReady = hddimageOpen(h,HDIMG_FILE_256,id,lun,256);
      }
      if(!imageReady)
      {
        imageReady = hddimageOpen(h,HDIMG_FILE_512,id,lun,512);
      }
      if(!imageReady)
      {
        imageReady = hddimageOpen(h,HDIMG_FILE_1024,id,lun,1024);
      }
      if(imageReady)
      {
        // 応答するIDとしてマーキング 
        scsi_id_mask |= 1<<id;
        //totalImage++;
      }
    }
  }
  // イメージファイルが０個ならエラー
  if(scsi_id_mask==0) onFalseInit();
  
  // サポートドライブマップの表示
#if DEBUG
  Serial.print("ID");
  for(int lun=0;lun<NUM_SCSILUN;lun++)
  {
    Serial.print(":LUN");
    Serial.print(lun);
  }
  Serial.println(":");
  //
  for(int id=0;id<NUM_SCSIID;id++)
  {
    Serial.print(" ");
    Serial.print(id);
    for(int lun=0;lun<NUM_SCSILUN;lun++)
    {
      HDDIMG *h = &img[id][lun];
      if( (lun<NUM_SCSILUN) && (h->m_file))
      {
        Serial.print((h->m_blocksize<1000) ? ": " : ":");
        Serial.print(h->m_blocksize);
      }
      else      
        Serial.print(":----");
    }
    Serial.println(":");
  }
#endif
  LED_OFF();
  //RSTピンの状態がHIGHからLOWに変わったときに発生
  attachInterrupt(PIN_MAP[RST].gpio_bit, onBusReset, FALLING);
}

/*
 * 初期化失敗.
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
 * バスリセット割り込み.
 */
void onBusReset(void)
{
#if SCSI_SELECT == 1
  // X1turbo用SASI I/FはRSTパルスがライトサイクル+2クロック ==
  // 1.25us程度しかアクティブにならないのでフィルタを掛けられない
  {{
#else
  if(isHigh(gpio_read(RST))) {
    delayMicroseconds(20);
    if(isHigh(gpio_read(RST))) {
#endif  
	// BUSFREEはメイン処理で行う
//      gpio_mode(MSG, GPIO_OUTPUT_OD);
//      gpio_mode(CD,  GPIO_OUTPUT_OD);
//      gpio_mode(REQ, GPIO_OUTPUT_OD);
//      gpio_mode(IO,  GPIO_OUTPUT_OD);
    	// DB,DBPは一旦入力にしたほうがいい？
    	SCSI_DB_INPUT()
		
      LOGN("BusReset!");
      m_isBusReset = true;
    }
  }
}

/*
 * ハンドシェイクで読み込む.
 */
inline byte readHandshake(void)
{
  SCSI_OUT(vREQ,active)
  //SCSI_DB_INPUT()
  while(!SCSI_IN(vACK)) { if(m_isBusReset) return 0; }
  byte r = readIO();
  SCSI_OUT(vREQ,inactive)
  while( SCSI_IN(vACK)) { if(m_isBusReset) return 0; }
  return r;  
}

/*
 * ハンドシェイクで書込み.
 */
inline void writeHandshake(byte d)
{
  GPIOB->regs->BSRR = db_bsrr[d]; // setup DB,DBP (160ns)
  SCSI_DB_OUTPUT() // (180ns)
  // ACK.Fall to DB output delay 100ns(MAX)  (DTC-510B)
  SCSI_OUT(vREQ,inactive) // setup wait (30ns)
  SCSI_OUT(vREQ,inactive) // setup wait (30ns)
  SCSI_OUT(vREQ,inactive) // setup wait (30ns)
  SCSI_OUT(vREQ,active)   // (30ns)
  //while(!SCSI_IN(vACK)) { if(m_isBusReset){ SCSI_DB_INPUT() return; }}
  while(!m_isBusReset && !SCSI_IN(vACK));
  // ACK.Fall to REQ.Raise delay 500ns(typ.) (DTC-510B)
  GPIOB->regs->BSRR = DBP(0xff);  // DB=0xFF , SCSI_OUT(vREQ,inactive)
  // REQ.Raise to DB hold time 0ns
  SCSI_DB_INPUT() // (150ns)
  while( SCSI_IN(vACK)) { if(m_isBusReset) return; }
}

/*
 * データインフェーズ.
 *  データ配列 p を len バイト送信する。
 */
void writeDataPhase(int len, const byte* p)
{
  LOGN("DATAIN PHASE");
  SCSI_OUT(vMSG,inactive) //  gpio_write(MSG, low);
  SCSI_OUT(vCD ,inactive) //  gpio_write(CD, low);
  SCSI_OUT(vIO ,  active) //  gpio_write(IO, high);
  for (int i = 0; i < len; i++) {
    if(m_isBusReset) {
      return;
    }
    writeHandshake(p[i]);
  }
}

/* 
 * データインフェーズ.
 *  SDカードからの読み込みながら len ブロック送信する。
 */
void writeDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAIN PHASE(SD)");
  uint32_t pos = adds * m_img->m_blocksize;
  m_img->m_file.seek(pos);

  SCSI_OUT(vMSG,inactive) //  gpio_write(MSG, low);
  SCSI_OUT(vCD ,inactive) //  gpio_write(CD, low);
  SCSI_OUT(vIO ,  active) //  gpio_write(IO, high);
	
  for(uint32_t i = 0; i < len; i++) {
      // 非同期リードにすれば速くなるんだけど...
    m_img->m_file.read(m_buf, m_img->m_blocksize);

#if READ_SPEED_OPTIMIZE

//#define REQ_ON() SCSI_OUT(vREQ,active)
#define REQ_ON() (*db_dst = BITMASK(vREQ)<<16)
#define FETCH_SRC()   (src_byte = *srcptr++)
#define FETCH_BSRR_DB() (bsrr_val = bsrr_tbl[src_byte])
#define REQ_OFF_DB_SET(BSRR_VAL) *db_dst = BSRR_VAL
#define WAIT_ACK_ACTIVE()   while(!m_isBusReset && !SCSI_IN(vACK))
#define WAIT_ACK_INACTIVE() do{ if(m_isBusReset) return; }while(SCSI_IN(vACK)) 

    SCSI_DB_OUTPUT()
    register byte *srcptr= m_buf;                 // ソースバッファ
    register byte *endptr= m_buf +  m_img->m_blocksize; // 終了ポインタ
    
    /*register*/ byte src_byte;                       // 送信データバイト
    register const uint32_t *bsrr_tbl = db_bsrr;  // BSRRに変換するテーブル
    register uint32_t bsrr_val;                   // 出力するBSRR値(DB,DBP,REQ=ACTIVE)
    register volatile uint32_t *db_dst = &(GPIOB->regs->BSRR); // 出力ポート

    // prefetch & 1st out
    FETCH_SRC();
    FETCH_BSRR_DB();
    REQ_OFF_DB_SET(bsrr_val);
    // DB.set to REQ.F setup 100ns max (DTC-510B)
    // ここには多少のウェイトがあったほうがいいかも
    //　WAIT_ACK_INACTIVE();
    do{
      // 0
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      // ACK.F  to REQ.R       500ns typ. (DTC-510B)
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 1
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 2
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 3
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 4
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 5
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 6
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 7
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
    }while(srcptr < endptr);
    SCSI_DB_INPUT()
#else
    for(int j = 0; j < BLOCKSIZE; j++) {
      if(m_isBusReset) {
        return;
      }
      writeHandshake(m_buf[j]);
    }
#endif
  }
}

/*
 * データアウトフェーズ.
 *  len ブロック読み込むこむ
 */
void readDataPhase(int len, byte* p)
{
  LOGN("DATAOUT PHASE");
  SCSI_OUT(vMSG,inactive) //  gpio_write(MSG, low);
  SCSI_OUT(vCD ,inactive) //  gpio_write(CD, low);
  SCSI_OUT(vIO ,inactive) //  gpio_write(IO, low);
  for(uint32_t i = 0; i < len; i++)
    p[i] = readHandshake();
}

/*
 * データアウトフェーズ.
 *  len ブロック読み込みながら SDカードへ書き込む。
 */
void readDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAOUT PHASE(SD)");
  uint32_t pos = adds * m_img->m_blocksize;
  m_img->m_file.seek(pos);
  SCSI_OUT(vMSG,inactive) //  gpio_write(MSG, low);
  SCSI_OUT(vCD ,inactive) //  gpio_write(CD, low);
  SCSI_OUT(vIO ,inactive) //  gpio_write(IO, low);
  for(uint32_t i = 0; i < len; i++) {
#if WRITE_SPEED_OPTIMIZE
	register byte *dstptr= m_buf;
	register byte *endptr= m_buf + m_img->m_blocksize;

    for(dstptr=m_buf;dstptr<endptr;dstptr+=8) {
      dstptr[0] = readHandshake();
      dstptr[1] = readHandshake();
      dstptr[2] = readHandshake();
      dstptr[3] = readHandshake();
      dstptr[4] = readHandshake();
      dstptr[5] = readHandshake();
      dstptr[6] = readHandshake();
      dstptr[7] = readHandshake();
      if(m_isBusReset) {
        return;
      }
    }
#else
    for(int j = 0; j <  m_img->m_blocksize; j++) {
      if(m_isBusReset) {
        return;
      }
      m_buf[j] = readHandshake();
    }
#endif	  
    m_img->m_file.write(m_buf, m_img->m_blocksize);
  }
  m_img->m_file.flush();
}

/*
 * INQUIRY コマンド処理.
 */
#if SCSI_SELECT == 2
byte onInquiryCommand(byte len)
{
  byte buf[36] = {
    0x00, //デバイスタイプ
    0x00, //RMB = 0
    0x01, //ISO,ECMA,ANSIバージョン
    0x01, //レスポンスデータ形式
    35 - 4, //追加データ長
    0, 0, //Reserve
    0x00, //サポート機能
    'N', 'E', 'C', 'I', 'T', 'S', 'U', ' ',
    'A', 'r', 'd', 'S', 'C', 'S', 'i', 'n', 'o', ' ', ' ',' ', ' ', ' ', ' ', ' ',
    '0', '0', '1', '0',
  };
  writeDataPhase(len < 36 ? len : 36, buf);
  return 0x00;
}
#else
byte onInquiryCommand(byte len)
{
  byte buf[36] = {
    0x00, //デバイスタイプ
    0x00, //RMB = 0
    0x01, //ISO,ECMA,ANSIバージョン
    0x01, //レスポンスデータ形式
    35 - 4, //追加データ長
    0, 0, //Reserve
    0x00, //サポート機能
    'T', 'N', 'B', ' ', ' ', ' ', ' ', ' ',
    'A', 'r', 'd', 'S', 'C', 'S', 'i', 'n', 'o', ' ', ' ',' ', ' ', ' ', ' ', ' ',
    '0', '0', '1', '0',
  };
  writeDataPhase(len < 36 ? len : 36, buf);
  return 0x00;
}
#endif

/*
 * REQUEST SENSE コマンド処理.
 */
void onRequestSenseCommand(byte len)
{
  byte buf[18] = {
    0x70,   //CheckCondition
    0,      //セグメント番号
    0x00,   //センスキー
    0, 0, 0, 0,  //インフォメーション
    17 - 7 ,   //追加データ長
    0,
  };
  buf[2] = m_senseKey;
  m_senseKey = 0;
  writeDataPhase(len < 18 ? len : 18, buf);  
}

/*
 * READ CAPACITY コマンド処理.
 */
byte onReadCapacityCommand(byte pmi)
{
  if(!m_img) return 0x02; // イメージファイル不在
  
  uint32_t bl = m_img->m_blocksize;
  uint32_t bc = m_img->m_fileSize / bl;
  uint8_t buf[8] = {
    bc >> 24, bc >> 16, bc >> 8, bc,
    bl >> 24, bl >> 16, bl >> 8, bl    
  };
  writeDataPhase(8, buf);
  return 0x00;
}

/*
 * READ6/10 コマンド処理.
 */
byte onReadCommand(uint32_t adds, uint32_t len)
{
  LOGN("-R");
  LOGHEXN(adds);
  LOGHEXN(len);

  if(!m_img) return 0x02; // イメージファイル不在
  
  gpio_write(LED, high);
  writeDataPhaseSD(adds, len);
  gpio_write(LED, low);
  return 0x00; //sts
}

/*
 * WRITE6/10 コマンド処理.
 */
byte onWriteCommand(uint32_t adds, uint32_t len)
{
  LOGN("-W");
  LOGHEXN(adds);
  LOGHEXN(len);
  
  if(!m_img) return 0x02; // イメージファイル不在
  
  gpio_write(LED, high);
  readDataPhaseSD(adds, len);
  gpio_write(LED, low);
  return 0; //sts
}

/*
 * MODE SENSE コマンド処理.
 */
#if SCSI_SELECT == 2
byte onModeSenseCommand(byte dbd, int cmd2, uint32_t len)
{
  if(!m_img) return 0x02; // イメージファイル不在

  int pageCode = cmd2 & 0x3F; 

  // デフォルト設定としてセクタサイズ512,セクタ数25,ヘッド数8を想定
  int size = m_img->m_fileSize;
  int cylinders = (int)(size >> 9);
  cylinders >>= 3;
  cylinders /= 25;
  int sectorsize = 512;
  int sectors = 25;
  int heads = 8;
  // セクタサイズ
 int disksize = 0;
  for(disksize = 16; disksize > 0; --(disksize)) {
    if ((1 << disksize) == sectorsize)
      break;
  }
  // ブロック数
  uint32_t diskblocks = (uint32_t)(size >> disksize);

  memset(m_buf, 0, sizeof(m_buf)); 
  int a = 4;
  if(dbd == 0) {
    uint32_t bl = m_img->m_blocksize;
    uint32_t bc = m_img->m_fileSize / bl;
    byte c[8] = {
      0,//デンシティコード
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
  {
    m_buf[a + 0] = 0x01;
    m_buf[a + 1] = 0x06;
    a += 8;
  }
  case 0x03:  //ドライブパラメータ
  {
    m_buf[a + 0] = 0x80 | 0x03; //ページコード
    m_buf[a + 1] = 0x16; // ページ長
    m_buf[a + 2] = (byte)(heads >> 8);//セクタ数/トラック
    m_buf[a + 3] = (byte)(heads);//セクタ数/トラック
    m_buf[a + 10] = (byte)(sectors >> 8);//セクタ数/トラック
    m_buf[a + 11] = (byte)(sectors);//セクタ数/トラック
    int size = 1 << disksize;
    m_buf[a + 12] = (byte)(size >> 8);//セクタ数/トラック
    m_buf[a + 13] = (byte)(size);//セクタ数/トラック
    a += 24;
    if(pageCode != 0x3F) {
      break;
    }
  }
  case 0x04:  //ドライブパラメータ
  {
      LOGN("AddDrive");
      m_buf[a + 0] = 0x04; //ページコード
      m_buf[a + 1] = 0x12; // ページ長
      m_buf[a + 2] = (cylinders >> 16);// シリンダ長
      m_buf[a + 3] = (cylinders >> 8);
      m_buf[a + 4] = cylinders;
      m_buf[a + 5] = heads;   //ヘッド数
      a += 20;
    if(pageCode != 0x3F) {
      break;
    }
  }
  default:
    break;
  }
  m_buf[0] = a - 1;
  writeDataPhase(len < a ? len : a, m_buf);
  return 0x00;
}
#else
byte onModeSenseCommand(byte dbd, int cmd2, uint32_t len)
{
  if(!m_img) return 0x02; // イメージファイル不在
  
  memset(m_buf, 0, sizeof(m_buf)); 
  int pageCode = cmd2 & 0x3F; 
  int a = 4;
  if(dbd == 0) {
    uint32_t bl =  m_img->m_blocksize;
    uint32_t bc = m_img->m_fileSize / bl;

    byte c[8] = {
      0,//デンシティコード
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
  case 0x03:  //ドライブパラメータ
    m_buf[a + 0] = 0x03; //ページコード
    m_buf[a + 1] = 0x16; // ページ長
    m_buf[a + 11] = 0x3F;//セクタ数/トラック
    a += 24;
    if(pageCode != 0x3F) {
      break;
    }
  case 0x04:  //ドライブパラメータ
    {
      uint32_t bc = m_img->m_fileSize / m_img->m_file;
      m_buf[a + 0] = 0x04; //ページコード
      m_buf[a + 1] = 0x16; // ページ長
      m_buf[a + 2] = bc >> 16;// シリンダ長
      m_buf[a + 3] = bc >> 8;
      m_buf[a + 4] = bc;
      m_buf[a + 5] = 1;   //ヘッド数
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
  return 0x00;
}
#endif
    
#if SCSI_SELECT == 1
/*
 * dtc510b_setDriveparameter
 */
#define PACKED  __attribute__((packed))
typedef struct PACKED dtc500_cmd_c2_param_struct
{
	uint8_t	StepPlusWidth;				// Default is 13.6usec (11)
	uint8_t	StepPeriod;					// Default is  3  msec.(60)
	uint8_t	StepMode;					// Default is  Bufferd (0)
	uint8_t	MaximumHeadAdress;			// Default is 4 heads (3)
	uint8_t	HighCylinderAddressByte;	// Default set to 0   (0)
	uint8_t	LowCylinderAddressByte;		// Default is 153 cylinders (152)
	uint8_t	ReduceWrietCurrent;			// Default is above Cylinder 128 (127)
	uint8_t	DriveType_SeekCompleteOption;// (0)
	uint8_t	Reserved8;					// (0)
	uint8_t	Reserved9;					// (0)
} DTC510_CMD_C2_PARAM;

static void logStrHex(const char *msg,uint32_t num)
{
    LOG(msg);
    LOGHEXN(num);
}

static byte dtc510b_setDriveparameter(void)
{
	DTC510_CMD_C2_PARAM DriveParameter;
	uint16_t maxCylinder;
	uint16_t numLAD;
	//uint32_t stepPulseUsec;
	int StepPeriodMsec;

	// receive paramter 
	writeDataPhase(sizeof(DriveParameter),(byte *)(&DriveParameter));
 
	maxCylinder = 
		(((uint16_t)DriveParameter.HighCylinderAddressByte)<<8) | 
		(DriveParameter.LowCylinderAddressByte);
	numLAD = maxCylinder * (DriveParameter.MaximumHeadAdress+1);
	//stepPulseUsec  = calcStepPulseUsec(DriveParameter.StepPlusWidth);
	StepPeriodMsec = DriveParameter.StepPeriod*50;
	logStrHex	(" StepPlusWidth      : ",DriveParameter.StepPlusWidth);
	logStrHex	(" StepPeriod         : ",DriveParameter.StepPeriod   );
	logStrHex	(" StepMode           : ",DriveParameter.StepMode     );
	logStrHex	(" MaximumHeadAdress  : ",DriveParameter.MaximumHeadAdress);
	logStrHex	(" CylinderAddress    : ",maxCylinder);
	logStrHex	(" ReduceWrietCurrent : ",DriveParameter.ReduceWrietCurrent);
	logStrHex	(" DriveType/SeekCompleteOption : ",DriveParameter.DriveType_SeekCompleteOption);
  logStrHex (" Maximum LAD        : ",numLAD-1);
	return	0; // error result
}
#endif

/*
 * MsgIn2.
 */
void MsgIn2(int msg)
{
  LOGN("MsgIn2");
  SCSI_OUT(vMSG,  active) //  gpio_write(MSG, high);
  SCSI_OUT(vCD ,  active) //  gpio_write(CD, high);
  SCSI_OUT(vIO ,  active) //  gpio_write(IO, high);
  writeHandshake(msg);
}

/*
 * MsgOut2.
 */
void MsgOut2()
{
  LOGN("MsgOut2");
  SCSI_OUT(vMSG,  active) //  gpio_write(MSG, high);
  SCSI_OUT(vCD ,  active) //  gpio_write(CD, high);
  SCSI_OUT(vIO ,inactive) //  gpio_write(IO, low);
  m_msb[m_msc] = readHandshake();
  m_msc++;
  m_msc %= 256;
}

/*
 * メインループ.
 */
void loop() 
{
  //int msg = 0;
  m_msg = 0;

  // RST=H,BSY=H,SEL=L になるまで待つ
  do {} while( SCSI_IN(vBSY) || !SCSI_IN(vSEL) || SCSI_IN(vRST));

	// BSY+ SEL-
  // 応答すべきIDがドライブされていなければ次を待つ 
  //byte db = readIO();
  //byte scsiid = db & scsi_id_mask;
  byte scsiid = readIO() & scsi_id_mask;
  if((scsiid) == 0) {
    return;
  }
  LOGN("Selection");
  m_isBusReset = false;
  // セレクトされたらBSYを-にする
  SCSI_BSY_ACTIVE();     // BSY出力だけON , ACTIVE にする

  // 応答するTARGET-IDを求める
#if USE_DB2ID_TABLE
  m_id = db2scsiid[scsiid];
  //if(m_id==0xff) return;
#else
  for(m_id=7;m_id>=0;m_id--)
    if(scsiid & (1<<m_id)) break;
  //if(m_id<0) return;
#endif

  // SELがinactiveになるまで待つ
  while(isHigh(gpio_read(SEL))) {
    if(m_isBusReset) {
      goto BusFree;
    }
  }
  SCSI_TARGET_ACTIVE()  // (BSY),REQ,MSG,CD,IO 出力をON
  //  
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
      // 拡張メッセージ
      if (m_msb[i] == 0x01) {
        // 同期転送が可能な時だけチェック
        if (!syncenable || m_msb[i + 2] != 0x01) {
          MsgIn2(0x07);
          break;
        }
        // Transfer period factor(50 x 4 = 200nsに制限)
        syncperiod = m_msb[i + 3];
        if (syncperiod > 50) {
          syncoffset = 50;
        }
        // REQ/ACK offset(16に制限)
        syncoffset = m_msb[i + 4];
        if (syncoffset > 16) {
          syncoffset = 16;
        }
        // STDR応答メッセージ生成
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
  SCSI_OUT(vMSG,inactive) // gpio_write(MSG, low);
  SCSI_OUT(vCD ,  active) // gpio_write(CD, high);
  SCSI_OUT(vIO ,inactive) // gpio_write(IO, low);
  
  int len;
  byte cmd[12];
  cmd[0] = readHandshake(); if(m_isBusReset) goto BusFree;
  LOGHEX(cmd[0]);
  // コマンド長選択、受信
  static const int cmd_class_len[8]={6,10,10,6,6,12,6,6};
  len = cmd_class_len[cmd[0] >> 5];
  cmd[1] = readHandshake(); LOG(":");LOGHEX(cmd[1]); if(m_isBusReset) goto BusFree;
  cmd[2] = readHandshake(); LOG(":");LOGHEX(cmd[2]); if(m_isBusReset) goto BusFree;
  cmd[3] = readHandshake(); LOG(":");LOGHEX(cmd[3]); if(m_isBusReset) goto BusFree;
  cmd[4] = readHandshake(); LOG(":");LOGHEX(cmd[4]); if(m_isBusReset) goto BusFree;
  cmd[5] = readHandshake(); LOG(":");LOGHEX(cmd[5]); if(m_isBusReset) goto BusFree;
  // 残りのコマンド受信
  for(int i = 6; i < len; i++ ) {
    cmd[i] = readHandshake();
    LOG(":");
    LOGHEX(cmd[i]);
    if(m_isBusReset) goto BusFree;
  }
  // LUN 確認
  m_lun = m_sts>>5;
  m_sts = cmd[1]&0xe0;      // ステータスバイトにLUNをプリセット
  // HDD Imageの選択
  m_img = (HDDIMG *)0; // 無し
  if( (m_lun <= NUM_SCSILUN) )
  {
    m_img = &(img[m_id][m_lun]); // イメージあり
    if(!(m_img->m_file.isOpen()))
      m_img = (HDDIMG *)0;       // イメージ不在
  }
  // if(!m_img) m_sts |= 0x02;            // LUNに対するイメージファイル不在
  //LOGHEX(((uint32_t)m_img));
  
  LOG(":ID ");
  LOG(m_id);
  LOG(":LUN ");
  LOG(m_lun);

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
    m_sts |= onReadCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
    break;
  case 0x0A:
    LOGN("[Write6]");
    m_sts |= onWriteCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
    break;
  case 0x0B:
    LOGN("[Seek6]");
    break;
  case 0x12:
    LOGN("[Inquiry]");
    m_sts |= onInquiryCommand(cmd[4]);
    break;
  case 0x1A:
    LOGN("[ModeSense6]");
    m_sts |= onModeSenseCommand(cmd[1]&0x80, cmd[2], cmd[4]);
    break;
  case 0x1B:
    LOGN("[StartStopUnit]");
    break;
  case 0x1E:
    LOGN("[PreAllowMed.Removal]");
    break;
  case 0x25:
    LOGN("[ReadCapacity]");
    m_sts |= onReadCapacityCommand(cmd[8]);
    break;
  case 0x28:
    LOGN("[Read10]");
    m_sts |= onReadCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  case 0x2A:
    LOGN("[Write10]");
    m_sts |= onWriteCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
  case 0x2B:
    LOGN("[Seek10]");
    break;
  case 0x5A:
    LOGN("[ModeSense10]");
    onModeSenseCommand(cmd[1] & 0x80, cmd[2], ((uint32_t)cmd[7] << 8) | cmd[8]);
    break;
#if SCSI_SELECT == 1
  case 0xc2:
    LOGN("[DTC510B setDriveParameter]");
    m_sts |= dtc510b_setDriveparameter();
    break;
#endif    
  default:
    LOGN("[*Unknown]");
    m_sts |= 0x02;
    m_senseKey = 5;
    break;
  }
  if(m_isBusReset) {
     goto BusFree;
  }

  LOGN("Sts");
  SCSI_OUT(vMSG,inactive) // gpio_write(MSG, low);
  SCSI_OUT(vCD ,  active) // gpio_write(CD, high);
  SCSI_OUT(vIO ,  active) // gpio_write(IO, high);
  writeHandshake(m_sts);
  if(m_isBusReset) {
     goto BusFree;
  }

  LOGN("MsgIn");
  SCSI_OUT(vMSG,  active) // gpio_write(MSG, high);
  SCSI_OUT(vCD ,  active) // gpio_write(CD, high);
  SCSI_OUT(vIO ,  active) // gpio_write(IO, high);
  writeHandshake(m_msg);

BusFree:
  LOGN("BusFree");
  m_isBusReset = false;
  //SCSI_OUT(vREQ,inactive) // gpio_write(REQ, low);
  //SCSI_OUT(vMSG,inactive) // gpio_write(MSG, low);
  //SCSI_OUT(vCD ,inactive) // gpio_write(CD, low);
  //SCSI_OUT(vIO ,inactive) // gpio_write(IO, low);
  //SCSI_OUT(vBSY,inactive)
  SCSI_TARGET_INACTIVE() // BSY,REQ,MSG,CD,IO 出力をOFFにする
}
