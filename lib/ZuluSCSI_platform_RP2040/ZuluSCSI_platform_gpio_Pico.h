/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 * 
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

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
#define SCSI_OUT_RST  21
#define SCSI_OUT_BSY  27
#define SCSI_OUT_REQ  17
#define SCSI_OUT_SEL  19

// SCSI input status signals
#define SCSI_IN_SEL  18
#define SCSI_IN_ACK  26
#define SCSI_IN_ATN  28
#define SCSI_IN_BSY  20
#define SCSI_IN_RST  21

// Status line outputs for initiator mode
#define SCSI_OUT_ACK  26
#define SCSI_OUT_ATN  28

// Status line inputs for initiator mode
#define SCSI_IN_IO    22
#define SCSI_IN_CD    18
#define SCSI_IN_MSG   20
#define SCSI_IN_REQ   17

// Status LED pins
#define LED_PIN      16
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

#ifndef ENABLE_AUDIO_OUTPUT
    // No spare pins for I2C
    // IO expander I2C
    // #define GPIO_I2C_SDA 14
    // #define GPIO_I2C_SCL 15
#else
    // IO expander I2C pins being used as SPI for audio
    #define AUDIO_SPI      spi1
    #define GPIO_EXP_SPARE 14
    #define GPIO_EXP_AUDIO 15
#endif

// DIP switch pins
#define HAS_DIP_SWITCHES
#define DIP_INITIATOR 28
#define DIP_DBGLOG 17
#define DIP_TERM 22

// Other pins
#define SWO_PIN 16

// Below are GPIO access definitions that are used from scsiPhy.cpp.

// Write a single SCSI pin.
// Example use: SCSI_OUT(ATN, 1) sets SCSI_ATN to low (active) state.
#define SCSI_OUT(pin, state) \
    *(state ? &sio_hw->gpio_clr : &sio_hw->gpio_set) = 1 << (SCSI_OUT_ ## pin)

// Read a single SCSI pin.
// Example use: SCSI_IN(ATN), returns 1 for active low state.
#define SCSI_IN(pin) \
    ((sio_hw->gpio_in & (1 << (SCSI_IN_ ## pin))) ? 0 : 1)

// Set pin directions for initiator vs. target mode
#define SCSI_ENABLE_INITIATOR() \
    (sio_hw->gpio_oe_set = (1 << SCSI_OUT_ACK) | \
                           (1 << SCSI_OUT_ATN)), \
    (sio_hw->gpio_oe_clr = (1 << SCSI_IN_IO) | \
                           (1 << SCSI_IN_CD) | \
                           (1 << SCSI_IN_MSG) | \
                           (1 << SCSI_IN_REQ))

// Enable driving of shared control pins
#define SCSI_ENABLE_CONTROL_OUT() \
    (sio_hw->gpio_oe_set = (1 << SCSI_OUT_CD) | \
                           (1 << SCSI_OUT_MSG))

// Set SCSI data bus to output
#define SCSI_ENABLE_DATA_OUT() \
    (sio_hw->gpio_clr = (1 << SCSI_DATA_DIR), \
     sio_hw->gpio_oe_set = SCSI_IO_DATA_MASK)

// Write SCSI data bus, also sets REQ to inactive.
#define SCSI_OUT_DATA(data) \
    gpio_put_masked(SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ), \
                    g_scsi_parity_lookup[(uint8_t)(data)] | (1 << SCSI_OUT_REQ)), \
    SCSI_ENABLE_DATA_OUT()

// Release SCSI data bus and REQ signal
#define SCSI_RELEASE_DATA_REQ() \
    (sio_hw->gpio_oe_clr = SCSI_IO_DATA_MASK, \
     sio_hw->gpio_set = (1 << SCSI_DATA_DIR) | (1 << SCSI_OUT_REQ))

// Release all SCSI outputs
#define SCSI_RELEASE_OUTPUTS() \
    SCSI_RELEASE_DATA_REQ(), \
    sio_hw->gpio_oe_clr = (1 << SCSI_OUT_CD) | \
                          (1 << SCSI_OUT_MSG), \
    sio_hw->gpio_set = (1 << SCSI_OUT_IO) | \
                       (1 << SCSI_OUT_CD) | \
                       (1 << SCSI_OUT_MSG) | \
                       (1 << SCSI_OUT_RST) | \
                       (1 << SCSI_OUT_BSY) | \
                       (1 << SCSI_OUT_REQ) | \
                       (1 << SCSI_OUT_SEL)

// Read SCSI data bus
#define SCSI_IN_DATA() \
    (~sio_hw->gpio_in & SCSI_IO_DATA_MASK) >> SCSI_IO_SHIFT

