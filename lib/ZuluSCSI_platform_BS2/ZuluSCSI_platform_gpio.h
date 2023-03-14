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
#define SCSI_DATA_DIR 9

// SCSI output status lines
#define SCSI_OUT_IO   22
#define SCSI_OUT_CD   18
#define SCSI_OUT_MSG  20
#define SCSI_OUT_RST  22
#define SCSI_OUT_BSY  27
#define SCSI_OUT_REQ  17
#define SCSI_OUT_SEL  19

// SCSI input status signals
#define SCSI_IN_SEL  18
#define SCSI_IN_ACK  26
#define SCSI_IN_ATN  28
#define SCSI_IN_BSY  20
#define SCSI_IN_RST  21

// Status LED pins
#define LED_PIN      25
#define LED_ON()     sio_hw->gpio_set = 1 << LED_PIN
#define LED_OFF()    sio_hw->gpio_clr = 1 << LED_PIN

// SD card pins in SDIO mode
#define SDIO_CLK 10
#define SDIO_CMD 11
#define SDIO_D0  12
#define SDIO_D1  13
#define SDIO_D2  14
#define SDIO_D3  15

// SD card pins in SPI mode
#define SD_SPI       spi0
#define SD_SPI_SCK   10
#define SD_SPI_MOSI  11
#define SD_SPI_MISO  12
#define SD_SPI_CS    15


// Other pins
#define SWO_PIN 16
