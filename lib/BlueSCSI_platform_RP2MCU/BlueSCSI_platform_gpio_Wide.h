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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// GPIO definitions for BlueSCSI RP2350 Wide (16-bit SCSI)

#pragma once

#include <hardware/gpio.h>

// SCSI data input/output port.
// The data bus uses external bidirectional buffer, with
// direction controlled by DATA_DIR pin.
#define SCSI_IO_DB0  4
#define SCSI_IO_DB1  5
#define SCSI_IO_DB2  6
#define SCSI_IO_DB3  7
#define SCSI_IO_DB4  8
#define SCSI_IO_DB5  9
#define SCSI_IO_DB6  10
#define SCSI_IO_DB7  11
#define SCSI_IO_DB8  12
#define SCSI_IO_DB9  13
#define SCSI_IO_DB10 14
#define SCSI_IO_DB11 15
#define SCSI_IO_DB12 16
#define SCSI_IO_DB13 17
#define SCSI_IO_DB14 18
#define SCSI_IO_DB15 19
#define SCSI_IO_DBP  20
#define SCSI_IO_DBP1 21
#define SCSI_IO_SHIFT 4
#define SCSI_IO_DATA_MASK (0x3FFFF << SCSI_IO_SHIFT)

// Data direction control
#define SCSI_DATA_DIR 23
#define SCSI_DATA_DIR_ACTIVE_HIGH 1

// SCSI output status lines
#define SCSI_OUT_IO   28
#define SCSI_OUT_CD   24
#define SCSI_OUT_MSG  26
#define SCSI_OUT_RST  3
#define SCSI_OUT_BSY  1
#define SCSI_OUT_REQ  22
#define SCSI_OUT_SEL  0

// SCSI input status signals
#define SCSI_IN_SEL  24
#define SCSI_IN_ACK  27
#define SCSI_IN_ATN  25
#define SCSI_IN_BSY  26
#define SCSI_IN_RST  2

// Pin mask for PIO DMA acceleration
#define SCSI_ACCEL_SETPINS ((1 << SCSI_OUT_REQ) | (1 << SCSI_OUT_SEL))

// Pre-2023 board compatibility (not applicable to Wide boards)
// These definitions allow the code to compile but won't be used
#define BUTTON_SW1_PRE202309a 255
#define BUTTON_SW2_PRE202309a 255
#define SCSI_OUT_REQ_PRE09A  SCSI_OUT_REQ
#define SCSI_OUT_SEL_PRE09A  SCSI_OUT_SEL
#define SCSI_ACCEL_SETPINS_PRE09A SCSI_ACCEL_SETPINS

// Status line outputs for initiator mode
#define SCSI_OUT_ACK  27
#define SCSI_OUT_ATN  25

// Status line inputs for initiator mode
#define SCSI_IN_IO    28
#define SCSI_IN_CD    24
#define SCSI_IN_MSG   26
#define SCSI_IN_REQ   22

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

#define FAST_IO_DRIVE_STRENGTH GPIO_DRIVE_STRENGTH_12MA

#ifndef ENABLE_AUDIO_OUTPUT_SPDIF
    // IO expander I2C
    #define GPIO_I2C_INTR 29
    #define GPIO_I2C_SDA 30
    #define GPIO_I2C_SCL 31
#else
    // IO expander I2C pins being used as SPI for audio
    #define AUDIO_SPI      spi1
    #define GPIO_EXP_SPARE 30
    #define GPIO_EXP_AUDIO 31
    #define AUDIO_DMA_IRQ_NUM DMA_IRQ_2
#endif

#ifdef ENABLE_AUDIO_OUTPUT_I2S
    #define GPIO_I2S_BCLK 45
    #define GPIO_I2S_WS   46
    #define GPIO_I2S_DOUT 47
    #define I2S_DMA_IRQ_NUM DMA_IRQ_0
#endif

#define SCSI_DMA_IRQ_IDX 3
#define SCSI_DMA_IRQ_NUM DMA_IRQ_3

// Other pins
#define SWO_PIN 32

// DIP switch pins
#define HAS_DIP_SWITCHES
#define DIP_INITIATOR   SCSI_OUT_ACK
#define DIP_DBGLOG      SWO_PIN
#define DIP_TERM        22

// Ejection button
#define GPIO_EJECT_BTN 44

// Parity generation lookup table would be too large for 16-bit bus.
// Instead use CPU-based generation, which is fast enough on RP2350
// thanks to the extended instruction set of Cortex-M33.
// Note: RP2MCU_USE_CPU_PARITY is defined in platformio.ini build flags
#define RP2MCU_CPU_PARITY_CORE1

// Dummy parity lookup tables for compatibility with code that references them
// These should never actually be accessed due to RP2MCU_USE_CPU_PARITY
#ifdef __cplusplus
extern "C" {
#endif
extern const uint32_t g_scsi_parity_lookup[256];
extern const uint32_t g_scsi_parity_check_lookup[256];
#ifdef __cplusplus
}
#endif

// Generate parity for bytes or halfwords.
// This is only used for slow control & command transfers.
// Returns the GPIO value without SCSI_IO_SHIFT.
static inline uint32_t scsi_generate_parity(uint16_t w)
{
    uint32_t w2 = (w << 4) ^ w;
    uint32_t w3 = (w2 << 2) ^ w2;
    uint32_t w4 = (w3 << 1) ^ w3;

    return (w ^ 0xFFFF) | ((w4 & 0x80) << 9) | ((w4 & 0x8000) << 2);
}

// Check parity of a 8-bit received word.
// Argument is the 18-bit word read from IO pins, inverted
// Returns true if parity is valid.
static inline bool scsi_check_parity(uint32_t w)
{
    // Calculate parity bit
    uint32_t w2 = (w >> 4) ^ w;
    uint32_t w3 = (w2 >> 2) ^ w2;
    uint32_t w4 = (w3 >> 1) ^ w3;

    // Move DBP to same bit position as it is in w4.
    uint32_t p2 = (w >> 16);

    // Compare parity bit states (SCSI has odd parity, so they should differ)
    return ((w4 ^ p2) & 0x0001);
}

// Check parity of a 16-bit received word.
// Argument is the 18-bit word read from IO pins, inverted
// Returns true if parity is valid.
static inline bool scsi_check_parity_16bit(uint32_t w)
{
    // Calculate parity bit in parallel for the 2 bytes in the word
    uint32_t w2 = (w >> 4) ^ w;
    uint32_t w3 = (w2 >> 2) ^ w2;
    uint32_t w4 = (w3 >> 1) ^ w3;

    // Distribute the parity bits to same places as they are in the word
    uint32_t p2 = ((uint64_t)(w & 0x30000) * 0x810000) >> 32;

    // Compare parity bit states (SCSI has odd parity, so they should differ)
    return ((w4 ^ p2) & 0x0101) == 0x0101;
}

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
    (sio_hw->gpio_set = (1 << SCSI_DATA_DIR), \
     sio_hw->gpio_oe_set = SCSI_IO_DATA_MASK)

// Write SCSI data bus, also sets REQ to inactive.
// Data can be 8 or 16-bit.
#define SCSI_OUT_DATA(data) \
    gpio_put_masked(SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ), \
                    (scsi_generate_parity(data) << SCSI_IO_SHIFT) | (1 << SCSI_OUT_REQ)), \
    SCSI_ENABLE_DATA_OUT()

// Release SCSI data bus and REQ signal
#define SCSI_RELEASE_DATA_REQ() \
    (sio_hw->gpio_oe_clr = SCSI_IO_DATA_MASK, \
     sio_hw->gpio_clr = (1 << SCSI_DATA_DIR), \
     sio_hw->gpio_set = (1 << SCSI_OUT_REQ))

// Release all SCSI outputs
#define SCSI_RELEASE_OUTPUTS() \
    SCSI_RELEASE_DATA_REQ(), \
    sio_hw->gpio_oe_clr = (1 << SCSI_OUT_CD) | \
                          (1 << SCSI_OUT_MSG), \
    sio_hw->gpio_set = (1 << SCSI_OUT_IO) | \
                       (1 << SCSI_OUT_CD) | \
                       (1 << SCSI_OUT_MSG) | \
                       (1 << SCSI_OUT_REQ) | \
                       (1 << SCSI_OUT_RST) | \
                       (1 << SCSI_OUT_BSY) | \
                       (1 << SCSI_OUT_SEL) | \
                       (1 << SCSI_OUT_ACK) | \
                       (1 << SCSI_OUT_ATN)

// Read SCSI data bus
#define SCSI_IN_DATA() \
    (~sio_hw->gpio_in & SCSI_IO_DATA_MASK) >> SCSI_IO_SHIFT
