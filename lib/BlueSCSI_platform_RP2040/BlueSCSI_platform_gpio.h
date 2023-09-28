// GPIO definitions for BlueSCSI Pico-based hardware

#pragma once

#include <hardware/gpio.h>
#include <pico/cyw43_arch.h>

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
#define SCSI_OUT_DIRECTIONPIN 9

// SCSI control lines
#define SCSI_OUT_IO   22
#define SCSI_OUT_REQ  19
#define SCSI_OUT_REQ_BEFORE_2023_09a  17

#define SCSI_OUT_CD   18
#define SCSI_IN_SEL  18

#define SCSI_OUT_SEL  21

#define SCSI_OUT_MSG  20
#define SCSI_IN_BSY  20

#define SCSI_IN_RST  21
#define SCSI_OUT_RST  22  // No RST pin, manual or expander only

#define SCSI_IN_ACK  26
#define SCSI_OUT_BSY  27
#define SCSI_IN_ATN  28

// Status line outputs for initiator mode
#define SCSI_OUT_ACK  26
//#define SCSI_OUT_ATN  29  // ATN output is unused

// Status line inputs for initiator mode
#define SCSI_IN_IO    22
#define SCSI_IN_CD    18
#define SCSI_IN_MSG   28
#define SCSI_IN_REQ   19

// Status LED pins
#define LED_PIN      25
#define LED_ON()     platform_network_supported() ? cyw43_gpio_set(&cyw43_state, 0, true) : sio_hw->gpio_set = 1 << LED_PIN
#define LED_OFF()    platform_network_supported() ? cyw43_gpio_set(&cyw43_state, 0, false) : sio_hw->gpio_clr = 1 << LED_PIN

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
#define GPIO_I2C_SDA 16
#define GPIO_I2C_SCL 17

// Other pins
#define SWO_PIN 16

typedef struct __attribute__((packed))
{
	uint8_t OUT_IO;
	uint8_t OUT_CD;
	uint8_t OUT_REQ;
	uint8_t OUT_SEL;
	uint8_t OUT_MSG;
	uint8_t OUT_RST;
	uint8_t OUT_BSY;
	uint8_t OUT_ACK;

	uint8_t IN_IO;
	uint8_t IN_CD;
	uint8_t IN_MSG;
	uint8_t IN_REQ;
	uint8_t IN_SEL;
	uint8_t IN_BSY;
	uint8_t IN_RST;
	uint8_t IN_ACK;
	uint8_t IN_ATN;

} SCSI_PINS;
