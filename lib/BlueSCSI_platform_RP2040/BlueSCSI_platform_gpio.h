// GPIO definitions for BlueSCSI Pico-based hardware

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

// SCSI control lines
#define SCSI_OUT_IO   22  // Used to be 16
#define SCSI_OUT_REQ  17

#define SCSI_OUT_CD   18  // TODO hardware design
#define SCSI_IN_SEL  18

#define SCSI_OUT_SEL  19

#define SCSI_OUT_MSG  20
#define SCSI_IN_BSY  20  // TODO hardware design

#define SCSI_IN_RST  21
#define SCSI_OUT_RST  22  // Same as IO currently, not initialized or used

#define SCSI_IN_ACK  26
#define SCSI_OUT_BSY  27
#define SCSI_IN_ATN  28

// Status line outputs for initiator mode
#define SCSI_OUT_ACK  10
#define SCSI_OUT_ATN  29

// Status line inputs for initiator mode
#define SCSI_IN_IO    12
#define SCSI_IN_CD    11
#define SCSI_IN_MSG   13
#define SCSI_IN_REQ   9

// Status LED pins
#define LED_PIN      25
#define LED_ON()     sio_hw->gpio_set = 1 << LED_PIN
#define LED_OFF()    sio_hw->gpio_clr = 1 << LED_PIN

// SDIO and SPI block
#define SD_SPI_SCK   10
#define SDIO_CLK 10

#define SD_SPI_MOSI  11
#define SDIO_CMD 11

#define SD_SPI_MISO  12
#define SDIO_D0  12

#define SDIO_D1  13

#define SDIO_D2  14

#define SDIO_D3  15
#define SD_SPI_CS    15

// IO expander I2C
// #define GPIO_I2C_SDA 14
// #define GPIO_I2C_SCL 15

// DIP switch pins
// #define DIP_INITIATOR 10
// #define DIP_DBGLOG 28
// #define DIP_TERM 9

// Other pins
#define SWO_PIN 16
