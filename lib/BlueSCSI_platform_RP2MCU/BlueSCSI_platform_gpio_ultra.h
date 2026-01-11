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

// GPIO definitions for RP2350B-based BlueSCSI Ultra
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
#define SCSI_OUT_IO   21
#define SCSI_OUT_CD   23
#define SCSI_OUT_MSG  25
#define SCSI_OUT_RST  29
#define SCSI_OUT_BSY  27
#define SCSI_OUT_REQ  22
#define SCSI_OUT_SEL  24

#define SCSI_ACCEL_SETPINS 0x4001FF
#define SCSI_ACCEL_SETPINS_PRE09A 0x4001FF
//extern uint32_t SCSI_ACCEL_PINMASK;

// SCSI input status signals
#define SCSI_IN_SEL  23
#define SCSI_IN_ACK  26
#define SCSI_IN_ATN  28
#define SCSI_IN_BSY  25
#define SCSI_IN_RST  20

// Status LED pins
#define LED_PIN      30

#define SDIO_DAT_DIR 10
#define SDIO_CMD_DIR 11
#define SDIO_CLK 12
#define SDIO_CMD 13
#define SDIO_D0  14
#define SDIO_D1  15
#define SDIO_D2  16
#define SDIO_D3  17

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

// IO Expander
#define GPIO_I2C_SDA 18
#define GPIO_I2C_SCL 19

// SW1/SW2 buttons on pre-202309a hardware
#define BUTTON_SW1_PRE202309a SCSI_IN_ATN
#define BUTTON_SW2_PRE202309a SCSI_IN_ACK

// Other pins
#define SWO_PIN 31  // UART0

// Status line outputs for initiator mode
#define SCSI_OUT_ACK  26
#define SCSI_OUT_ATN  28

// Status line inputs for initiator mode
#define SCSI_IN_IO    21
#define SCSI_IN_CD    23
#define SCSI_IN_MSG   25
#define SCSI_IN_REQ   22

#define GPIO_RM2_ON   40
#define GPIO_RM2_DATA 41
#define GPIO_RM2_CS   42
#define GPIO_RM2_CLK  43

// Audio GPIO defines
#ifdef ENABLE_AUDIO_OUTPUT_SPDIF
#define AUDIO_SPI      spi0
#define GPIO_EXP_SPARE 38
#define GPIO_EXP_AUDIO 39
#endif

#ifdef ENABLE_AUDIO_OUTPUT_I2S
#define GPIO_I2S_BCLK   37
#define GPIO_I2S_WS     38
#define GPIO_I2S_DOUT   39
#define I2S_DMA_IRQ_NUM DMA_IRQ_2
#endif

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
                           (1 << SCSI_OUT_SEL)), \
    (sio_hw->gpio_oe_clr = (1 << SCSI_IN_IO) | \
                           (1 << SCSI_IN_CD) | \
                           (1 << SCSI_IN_MSG) | \
                           (1 << SCSI_OUT_REQ))

// Used in test mode only
#define SCSI_RELEASE_INITIATOR() \
    (sio_hw->gpio_oe_clr = (1 << SCSI_OUT_ACK) | \
                           (1 << SCSI_OUT_SEL)), \
    (sio_hw->gpio_oe_set = (1 << SCSI_IN_IO) | \
                           (1 << SCSI_IN_CD) | \
                           (1 << SCSI_IN_MSG) | \
                           (1 << SCSI_OUT_REQ))

// Enable driving of shared control pins
#define SCSI_ENABLE_CONTROL_OUT() \
    (sio_hw->gpio_oe_set = (1 << SCSI_OUT_CD) | \
                           (1 << SCSI_OUT_MSG))

// Set SCSI data bus to output
#define SCSI_ENABLE_DATA_OUT() \
    (sio_hw->gpio_set = (1 << SCSI_DATA_DIR), \
     sio_hw->gpio_oe_set = SCSI_IO_DATA_MASK)

// Write SCSI data bus, also sets REQ to inactive.
#define SCSI_OUT_DATA(data) \
    gpio_put_masked(SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ), \
                    g_scsi_parity_lookup[(uint8_t)(data)] | (1 << SCSI_OUT_REQ)), \
    SCSI_ENABLE_DATA_OUT()

// Release SCSI data bus and REQ signal
#define SCSI_RELEASE_DATA_REQ() \
    (sio_hw->gpio_oe_clr = SCSI_IO_DATA_MASK, \
     sio_hw->gpio_clr = (1 << SCSI_DATA_DIR), \
     sio_hw->gpio_set = (1 << SCSI_OUT_REQ))

// Release all SCSI outputs
#define SCSI_RELEASE_OUTPUTS() \
    SCSI_RELEASE_DATA_REQ(), \
    sio_hw->gpio_clr = (1 << SCSI_OUT_RST), \
    sio_hw->gpio_set = (1 << SCSI_OUT_IO) | \
                       (1 << SCSI_OUT_CD) | \
                       (1 << SCSI_OUT_MSG) | \
                       (1 << SCSI_OUT_BSY) | \
                       (1 << SCSI_OUT_REQ) | \
                       (1 << SCSI_OUT_SEL), \
                       platform_delay_ms(1), \
    sio_hw->gpio_oe_clr = (1 << SCSI_OUT_CD) | \
                          (1 << SCSI_OUT_MSG)

// Read SCSI data bus
#define SCSI_IN_DATA() \
    (~sio_hw->gpio_in & SCSI_IO_DATA_MASK) >> SCSI_IO_SHIFT
