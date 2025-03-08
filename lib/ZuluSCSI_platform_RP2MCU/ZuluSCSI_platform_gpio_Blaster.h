/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
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
#define SCSI_IO_DB0  12
#define SCSI_IO_DB1  13
#define SCSI_IO_DB2  14
#define SCSI_IO_DB3  15
#define SCSI_IO_DB4  16
#define SCSI_IO_DB5  17
#define SCSI_IO_DB6  18
#define SCSI_IO_DB7  19
#define SCSI_IO_DBP  20
#define SCSI_IO_DATA_MASK 0x1FF000
#define SCSI_IO_SHIFT 12

// Data direction control
#define SCSI_DATA_DIR 22

// SCSI output status lines
#define SCSI_OUT_IO   7
#define SCSI_OUT_CD   23
#define SCSI_OUT_MSG  26
#define SCSI_OUT_RST  47
#define SCSI_OUT_BSY  45
#define SCSI_OUT_REQ  21
#define SCSI_OUT_SEL  44

// SCSI input status signals
#define SCSI_IN_SEL  23
#define SCSI_IN_ACK  27
#define SCSI_IN_ATN  6
#define SCSI_IN_BSY  26
#define SCSI_IN_RST  46

// Status line outputs for initiator mode
#define SCSI_OUT_ACK  27
#define SCSI_OUT_ATN  6

// Status line inputs for initiator mode
#define SCSI_IN_IO    7
#define SCSI_IN_CD    23
#define SCSI_IN_MSG   26
#define SCSI_IN_REQ   21

// Status LED pins
#define LED_PIN      33

// SD card pins in SDIO mode
#define SDIO_CLK 34
#define SDIO_CMD 35
#define SDIO_D0  36
#define SDIO_D1  37
#define SDIO_D2  38
#define SDIO_D3  39

// SD card pins in SPI mode
#define SD_SPI       spi0
#define SD_SPI_SCK   SDIO_CLK
#define SD_SPI_MOSI  SDIO_CMD
#define SD_SPI_MISO  SDIO_D0
#define SD_SPI_CS    SDIO_D3

#ifndef ENABLE_AUDIO_OUTPUT_SPDIF
    // IO expander I2C
    #define GPIO_I2C_SDA 30
    #define GPIO_I2C_SCL 31
#else
    // IO expander I2C pins being used as SPI for audio
    #define AUDIO_SPI      spi1
    #define GPIO_EXP_SPARE 30
    #define GPIO_EXP_AUDIO 31
#endif

#ifdef ENABLE_AUDIO_OUTPUT_I2S
    #define GPIO_I2S_BCLK 8
    #define GPIO_I2S_WS   9
    #define GPIO_I2S_DOUT 10
    #define I2S_DMA_IRQ_NUM DMA_IRQ_2
#endif


// Other pins
#define SWO_PIN 32

// DIP switch pins
#define HAS_DIP_SWITCHES
#define DIP_INITIATOR   SCSI_OUT_ACK
#define DIP_DBGLOG      SWO_PIN
#define DIP_TERM        SCSI_OUT_REQ

// Below are GPIO access definitions that are used from scsiPhy.cpp.

// Write a single SCSI pin.
// Example use: SCSI_OUT(ATN, 1) sets SCSI_ATN to low (active) state.
#define SCSI_OUT(pin, state) \
    ((SCSI_OUT_ ## pin) > 31 ? \
        *(state ? &sio_hw->gpio_hi_clr : &sio_hw->gpio_hi_set) = 1 << ((SCSI_OUT_ ## pin) - 32) \
    : \
        *(state ? &sio_hw->gpio_clr : &sio_hw->gpio_set) = 1 << (SCSI_OUT_ ## pin) \
    )

// Read a single SCSI pin.
// Example use: SCSI_IN(ATN), returns 1 for active low state.
#define SCSI_IN(pin) \
    ((SCSI_IN_ ## pin) > 31 ? \
        ((sio_hw->gpio_hi_in & (1 << ((SCSI_IN_ ## pin) - 32))) ? 0 : 1) \
    : \
        ((sio_hw->gpio_in & (1 << (SCSI_IN_ ## pin))) ? 0 : 1) \
    )

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
                    (g_scsi_parity_lookup[(uint8_t)(data)] << SCSI_IO_SHIFT) | (1 << SCSI_OUT_REQ)), \
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
                       (1 << SCSI_OUT_REQ), \
    sio_hw->gpio_hi_set =   (1 << (SCSI_OUT_RST - 32)) | \
                            (1 << (SCSI_OUT_BSY - 32)) | \
                            (1 << (SCSI_OUT_SEL - 32))

// Read SCSI data bus
#define SCSI_IN_DATA() \
    (~sio_hw->gpio_in & SCSI_IO_DATA_MASK) >> SCSI_IO_SHIFT

