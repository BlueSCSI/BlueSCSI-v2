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

// GPIO definitions for RP2350B-based BlueSCSI Ultra Wide
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
#define SCSI_IO_DB8  8
#define SCSI_IO_DB9  9
#define SCSI_IO_DB10 10
#define SCSI_IO_DB11 11
#define SCSI_IO_DB12 12
#define SCSI_IO_DB13 13
#define SCSI_IO_DB14 14
#define SCSI_IO_DB15 15
#define SCSI_IO_DBP  16
#define SCSI_IO_DBP1 17
#define SCSI_IO_SHIFT 0
#define SCSI_IO_DATA_MASK (0x3FFFF << SCSI_IO_SHIFT)

// Data direction control
#define SCSI_DATA_DIR 18
#define SCSI_DATA_DIR_ACTIVE_HIGH 1

// SCSI output status lines
#define SCSI_OUT_IO   30
#define SCSI_OUT_CD   31
#define SCSI_OUT_MSG  33
#define SCSI_OUT_RST  36
#define SCSI_OUT_BSY  34
#define SCSI_OUT_REQ_CURRENT  27
#define SCSI_OUT_REQ_PRE09A   27
extern uint8_t SCSI_OUT_REQ;
#define SCSI_OUT_SEL_CURRENT  32
#define SCSI_OUT_SEL_PRE09A   32
extern uint8_t SCSI_OUT_SEL;

#define SCSI_ACCEL_SETPINS 0x4001FF
#define SCSI_ACCEL_SETPINS_PRE09A 0x4001FF
extern uint32_t SCSI_ACCEL_PINMASK;

// SCSI input status signals
#define SCSI_IN_SEL  31
#define SCSI_IN_ACK  28
#define SCSI_IN_ATN  35
#define SCSI_IN_BSY  33
#define SCSI_IN_RST  29

// Status line outputs for initiator mode
#define SCSI_OUT_ACK  28
#define SCSI_OUT_ATN  35

// Status line inputs for initiator mode
#define SCSI_IN_IO    28
#define SCSI_IN_CD    31
#define SCSI_IN_MSG   33
#define SCSI_IN_REQ   27

// Status LED pins
#define LED_PIN      40

// SD card pins in SDIO mode
#define SDIO_DAT_DIR 19
#define SDIO_CMD_DIR 20
#define SDIO_CLK 21
#define SDIO_CMD 22
#define SDIO_D0  23
#define SDIO_D1  24
#define SDIO_D2  25
#define SDIO_D3  26

// Flags for sdio autoconfig results
#define SDIO_AC_STANDARD_MODE   0b1
#define SDIO_AC_HIGH_SPEED      0b10
#define SDIO_AC_ULTRA_SPEED     0b100
#define SDIO_AC_LOW_VOLTAGE     0b10000
#define SDIO_AC_LV_HIGH_SPEED   0b100000
#define SDIO_AC_LV_ULTRA_SPEED  0b1000000
#define SDIO_AC_LV_HS_MODE_D    0b1000
#define SDIO_AC_LV_US_MODE_D    0b10000000

// Flags for sdio setup
#define SDIO_HS   0b0010
#define SDIO_US   0b0100
#define SDIO_1_8  0b1000
#define SDIO_M_D  0b10000
#define SDIO_LOG  0b100000
#define SDIO_FIN  0b10000000

// TODO: Doesn't exist on this version
// SW1/SW2 buttons on pre-202309a hardware
#define BUTTON_SW1_PRE202309a SCSI_IN_ATN
#define BUTTON_SW2_PRE202309a SCSI_IN_ACK

#ifndef ENABLE_AUDIO_OUTPUT_SPDIF
    // IO expander I2C
    #define GPIO_I2C_SDA 42
    #define GPIO_I2C_SCL 43
#else
    // IO expander I2C pins being used as SPI for audio
    #define AUDIO_SPI      spi1
    #define GPIO_EXP_SPARE 30
    #define GPIO_EXP_AUDIO 31
    #define AUDIO_DMA_IRQ_NUM DMA_IRQ_2
#endif

#define I2S_SCK 37
#define I2S_WS 38
#define I2S_DOUT 39
#ifdef ENABLE_AUDIO_OUTPUT_I2S
    #define GPIO_I2S_BCLK 37
    #define GPIO_I2S_WS   38
    #define GPIO_I2S_DOUT 39
    #define I2S_DMA_IRQ_NUM DMA_IRQ_0
#endif

#define SCSI_DMA_IRQ_IDX 3
#define SCSI_DMA_IRQ_NUM DMA_IRQ_3

// Other pins
#define SWO_PIN 40

// Ejection button
#define GPIO_EJECT_BTN 44

// Parity generation lookup table would be too large for 16-bit bus.
// Instead use CPU-based generation, which is fast enough on RP2350
// thanks to the extended instruction set of Cortex-M33.
#define RP2MCU_USE_CPU_PARITY
#define RP2MCU_CPU_PARITY_CORE1

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
    sio_hw->gpio_oe_set = (1 << SCSI_OUT_ACK), \
    sio_hw->gpio_hi_oe_set = (1 << (SCSI_OUT_ATN - 32)), \
    (sio_hw->gpio_oe_clr = (1 << SCSI_IN_IO) | \
                           (1 << SCSI_IN_CD) | \
                           (1 << SCSI_IN_REQ)), \
    sio_hw->gpio_hi_oe_clr = (1 << (SCSI_IN_MSG - 32))

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
    sio_hw->gpio_oe_clr = (1 << SCSI_OUT_CD), \
    sio_hw->gpio_hi_oe_clr = (1 << (SCSI_OUT_MSG - 32)), \
    sio_hw->gpio_set = ((1 << SCSI_OUT_IO) | \
                       (1 << SCSI_OUT_CD) | \
                       (1 << SCSI_OUT_REQ) | \
                       (1 << SCSI_OUT_ACK)), \
    sio_hw->gpio_hi_set =  ((1 << (SCSI_OUT_MSG - 32) | \
                       (1 << (SCSI_OUT_RST - 32)) | \
                       (1 << (SCSI_OUT_BSY - 32)) | \
                       (1 << (SCSI_OUT_SEL - 32)) | \
                       (1 << (SCSI_OUT_ATN - 32)))) 

// Read SCSI data bus
#define SCSI_IN_DATA() \
    (~sio_hw->gpio_in & SCSI_IO_DATA_MASK) >> SCSI_IO_SHIFT


