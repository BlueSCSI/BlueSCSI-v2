// GPIO definitions for ZuluSCSI RP2040-based hardware

#pragma once

#include <hardware/gpio.h>

// SCSI data input/output port.
// The data bus uses external bidirectional buffer, with
// direction controlled by DATA_DIR pin.
#define SCSI_IO_DB0  0
#define SCSI_IO_DB1  1
#define SCSI_IO_DB2  2
#define SCSI_IO_DB3  3
#define SCSI_IO_DB4  4
#define SCSI_IO_DB5  5
#define SCSI_IO_DB6  6
#define SCSI_IO_DB7  7
#define SCSI_IO_DBP  8
#define SCSI_IO_DATA_MASK 0x1FF
#define SCSI_IO_SHIFT 0

// Data direction control
#define SCSI_DATA_DIR 17

// SCSI output status lines
#define SCSI_OUT_IO   12
#define SCSI_OUT_CD   11
#define SCSI_OUT_MSG  13
#define SCSI_OUT_RST  28
#define SCSI_OUT_BSY  26
#define SCSI_OUT_REQ  9
#define SCSI_OUT_SEL  24

// SCSI input status signals
#define SCSI_IN_SEL  11
#define SCSI_IN_ACK  10
#define SCSI_IN_ATN  29
#define SCSI_IN_BSY  13
#define SCSI_IN_RST  27

// Status LED pins
#define LED_PIN      25
#define LED_ON()     sio_hw->gpio_set = 1 << LED_PIN
#define LED_OFF()    sio_hw->gpio_clr = 1 << LED_PIN

// SD card pins in SDIO mode
#define SDIO_CLK 18
#define SDIO_CMD 19
#define SDIO_D0  20
#define SDIO_D1  21
#define SDIO_D2  22
#define SDIO_D3  23

// SD card pins in SPI mode
#define SD_SPI       spi0
#define SD_SPI_SCK   18
#define SD_SPI_MOSI  19
#define SD_SPI_MISO  20
#define SD_SPI_CS    23

// IO expander I2C
#define GPIO_I2C_SDA 14
#define GPIO_I2C_SCL 15

// DIP switch pins
#define DIP_INITIATOR 10
#define DIP_DBGLOG 16
#define DIP_TERM 9

// Other pins
#define SWO_PIN 16
