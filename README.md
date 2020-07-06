# ArdSCSino-stm32

ArdSCSino-stm32 とは たんぼ（TNB製作所）(https://twitter.com/h_koma2) さんが作成した ArdSCSino のSTM32版です<br>
ArdSCSino とは SCSIデバイス（ハードディスク）を arduino で再現するハードウエアです。<br>
許可を頂いて公開することになりました。<br>

# Setup
* Arduino Software (IDE) V1.8.8 を使用しています。<br>

 ツール->ボード->ボードマネージャー->検索のフィルター<br>
 「Arduino SAMボード（32ビットARM Cortex-M3）」の検索とインストール<br>
 ボード「Generic STM32F103Cシリーズ」を選択<br>

 ライブラリとして以下を使用しています。<br>
 
 SDFAT (https://github.com/greiman/SdFat)<br>

 Arduino_STM32(https://github.com/rogerclarkmelbourne/Arduino_STM32/releases/tag/v1.0.0)<br>

 マイクロSDCARDアダプタとして以下を使用しています。<br>

 ArdSCSIno V1<br>
 Arduino SPIマイクロSDアダプター6PINと互換性のあるマイクロSD TFカードメモリシールドモジュール<br>

 ArdSCSIno V2<br>
 Hirose DM3AT-SF-PEJM5<br>
