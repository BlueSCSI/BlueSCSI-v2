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

// Platform-specific definitions for ZuluSCSI.
//
// This file is example platform definition that can easily be
// customized for a different board / CPU.

#pragma once

/* Add any platform-specific includes you need here */
#include <stdint.h>
#include <Arduino.h>
#include "ZuluSCSI_platform_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These are used in debug output and default SCSI strings */
extern const char *g_platform_name;
#define PLATFORM_NAME "Example"
#define PLATFORM_REVISION "1.0"

// Debug logging function, can be used to print to e.g. serial port.
// May get called from interrupt handlers.
void platform_log(const char *s);

// Timing and delay functions.
// Arduino platform already provides these
unsigned long millis(void);
void delay(unsigned long ms);

// Short delays, can be called from interrupt mode
static inline void delay_ns(unsigned long ns)
{
    delayMicroseconds(ns / 1000);
}

// Approximate fast delay
static inline void delay_100ns()
{
    asm volatile ("nop \n nop \n nop \n nop \n nop");
}

// Initialize SD card and GPIO configuration
void platform_init();

// Initialization for main application, not used for bootloader
void platform_late_init();

// Initialization after the SD Card has been found
void platform_post_sd_card_init();

// Disable the status LED
void platform_disable_led(void);

// Setup soft watchdog if supported
void platform_reset_watchdog();

// Poll function that is called every few milliseconds.
// The SD card is free to access during this time, and pauses up to
// few milliseconds shouldn't disturb SCSI communication.
void platform_poll();

// Returns the state of any platform-specific buttons.
// The returned value should be a mask for buttons 1-8 in bits 0-7 respectively,
// where '1' is a button pressed and '0' is a button released.
// Debouncing logic is left up to the specific implementation.
// This function should return without significantly delay.
uint8_t platform_get_buttons();

// Set callback that will be called during data transfer to/from SD card.
// This can be used to implement simultaneous transfer to SCSI bus.
typedef void (*sd_callback_t)(uint32_t bytes_complete);
void platform_set_sd_callback(sd_callback_t func, const uint8_t *buffer);

// Below are GPIO access definitions that are used from scsiPhy.cpp.
// The definitions shown will work for STM32 style devices, other platforms
// will need adaptations.

// Write a single SCSI pin.
// Example use: SCSI_OUT(ATN, 1) sets SCSI_ATN to low (active) state.
#define SCSI_OUT(pin, state) \
    (SCSI_OUT_ ## pin ## _PORT)->BSRR = (SCSI_OUT_ ## pin ## _PIN) << (state ? 16 : 0)

// Read a single SCSI pin.
// Example use: SCSI_IN(ATN), returns 1 for active low state.
#define SCSI_IN(pin) \
    (((SCSI_ ## pin ## _PORT)->IDR & (SCSI_ ## pin ## _PIN)) ? 0 : 1)

// Write SCSI data bus, also sets REQ to inactive.
extern const uint32_t g_scsi_out_byte_to_bop[256];
#define SCSI_OUT_DATA(data) \
    (SCSI_OUT_PORT)->BSRR = g_scsi_out_byte_to_bop[(uint8_t)(data)]

// Release SCSI data bus and REQ signal
#define SCSI_RELEASE_DATA_REQ() \
    (SCSI_OUT_PORT)->BSRR = SCSI_OUT_DATA_MASK | SCSI_OUT_REQ

// Release all SCSI outputs
#define SCSI_RELEASE_OUTPUTS() \
    (SCSI_OUT_PORT)->BSRR = SCSI_OUT_DATA_MASK | SCSI_OUT_REQ, \
    (SCSI_OUT_IO_PORT)->BSRR  = SCSI_OUT_IO_PIN, \
    (SCSI_OUT_CD_PORT)->BSRR  = SCSI_OUT_CD_PIN, \
    (SCSI_OUT_SEL_PORT)->BSRR = SCSI_OUT_SEL_PIN, \
    (SCSI_OUT_MSG_PORT)->BSRR = SCSI_OUT_MSG_PIN, \
    (SCSI_OUT_RST_PORT)->BSRR = SCSI_OUT_RST_PIN, \
    (SCSI_OUT_BSY_PORT)->BSRR = SCSI_OUT_BSY_PIN

// Read SCSI data bus
#define SCSI_IN_DATA(data) \
    (((~(SCSI_IN_PORT->IDR)) & SCSI_IN_MASK) >> SCSI_IN_SHIFT)

#ifdef __cplusplus
}

// SD card driver for SdFat
class SdSpiConfig;
extern SdSpiConfig g_sd_spi_config;
#define SD_CONFIG g_sd_spi_config
#define SD_CONFIG_CRASH g_sd_spi_config

#endif