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

// Platform-specific definitions for ZuluSCSI RP2040 hardware.

#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "ZuluSCSI_config.h"
#include "ZuluSCSI_platform_network.h"

#ifdef ZULUSCSI_PICO
// ZuluSCSI Pico carrier board variant
#include "ZuluSCSI_platform_gpio_Pico.h"
#elif defined(ZULUSCSI_PICO_2)
// ZuluSCSI Pico 2 carrier board variant
#include "ZuluSCSI_platform_gpio_Pico_2.h"
#elif defined(ZULUSCSI_BS2)
// BS2 hardware variant, using Raspberry Pico board on a carrier PCB
#include "ZuluSCSI_platform_gpio_BS2.h"
#elif defined(ZULUSCSI_RP2350A)
// RP2350A variant, using mcu chip directly
#include "ZuluSCSI_platform_gpio_RP2350A.h"
#else
// Normal RP2040 variant, using RP2040 chip directly
#include "ZuluSCSI_platform_gpio_RP2040.h"
#endif

#include "scsiHostPhy.h"


#ifdef __cplusplus
extern "C" {
#endif

/* These are used in debug output and default SCSI strings */
extern const char *g_platform_name;

// NOTE: The driver supports synchronous speeds higher than 10MB/s, but this
// has not been tested due to lack of fast enough SCSI adapter.
// #define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_20

// Debug logging function, can be used to print to e.g. serial port.
// May get called from interrupt handlers.
void platform_log(const char *s);
void platform_emergency_log_save();

// Timing and delay functions.
// Arduino platform already provides these
unsigned long millis(void);
void delay(unsigned long ms);

// Short delays, can be called from interrupt mode
static inline void delay_ns(unsigned long ns)
{
    delayMicroseconds((ns + 999) / 1000);
}

// Approximate fast delay
static inline void delay_100ns()
{
    asm volatile ("nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop");
}

// Initialize SD card and GPIO configuration
void platform_init();

// Initialization for main application, not used for bootloader
void platform_late_init();

// Initialization after the SD Card has been found
void platform_post_sd_card_init();

// Set the status LED only if it is not in a blinking routine
void platform_write_led(bool state);
#define LED_ON()  platform_write_led(true)
#define LED_OFF() platform_write_led(false)
// Used by the blinking routine
void platform_set_blink_status(bool status);
// LED override will set the status LED regardless of the blinking routine
void platform_write_led_override(bool state);
#define LED_ON_OVERRIDE()  platform_write_led_override(true)
#define LED_OFF_OVERRIDE()  platform_write_led_override(false)

// Disable the status LED
void platform_disable_led(void);

// Specific error code tied to the MCU when the SD card is not detected
uint8_t platform_no_sd_card_on_init_error_code();

// Query whether initiator mode is enabled on targets with PLATFORM_HAS_INITIATOR_MODE
bool platform_is_initiator_mode_enabled();

// Setup soft watchdog if supported
void platform_reset_watchdog();

// Reset MCU
void platform_reset_mcu();


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

uint32_t platform_sys_clock_in_hz();

// Attempt to reclock the MCU
zuluscsi_reclock_status_t platform_reclock(zuluscsi_speed_grade_t speed_grade);

// convert string to speed grade
zuluscsi_speed_grade_t platform_string_to_speed_grade(const char *speed_grade_str, size_t length);

// Returns true if reboot was for mass storage
bool platform_rebooted_into_mass_storage();

// Set callback that will be called during data transfer to/from SD card.
// This can be used to implement simultaneous transfer to SCSI bus.
typedef void (*sd_callback_t)(uint32_t bytes_complete);
void platform_set_sd_callback(sd_callback_t func, const uint8_t *buffer);

// Reprogram firmware in main program area.
#ifndef RP2040_DISABLE_BOOTLOADER
#define PLATFORM_BOOTLOADER_SIZE (128 * 1024)
#define PLATFORM_FLASH_TOTAL_SIZE (1024 * 1024)
#define PLATFORM_FLASH_PAGE_SIZE 4096
bool platform_rewrite_flash_page(uint32_t offset, uint8_t buffer[PLATFORM_FLASH_PAGE_SIZE]);
void platform_boot_to_main_firmware();
#endif

// ROM drive in the unused external flash area
#ifndef RP2040_DISABLE_ROMDRIVE
#define PLATFORM_HAS_ROM_DRIVE 1
// Check maximum available space for ROM drive in bytes
uint32_t platform_get_romdrive_maxsize();

// Read ROM drive area
bool platform_read_romdrive(uint8_t *dest, uint32_t start, uint32_t count);

// Reprogram ROM drive area
#define PLATFORM_ROMDRIVE_PAGE_SIZE 4096
bool platform_write_romdrive(const uint8_t *data, uint32_t start, uint32_t count);
#endif

// Parity lookup tables for write and read from SCSI bus.
// These are used by macros below and the code in scsi_accel_rp2040.cpp
extern const uint16_t g_scsi_parity_lookup[256];
extern const uint16_t g_scsi_parity_check_lookup[512];



#ifdef __cplusplus
}



// SD card driver for SdFat

#ifdef SD_USE_SDIO
class SdioConfig;
extern SdioConfig g_sd_sdio_config;
#define SD_CONFIG g_sd_sdio_config
#define SD_CONFIG_CRASH g_sd_sdio_config
#else
class SdSpiConfig;
extern SdSpiConfig g_sd_spi_config;
#define SD_CONFIG g_sd_spi_config
#define SD_CONFIG_CRASH g_sd_spi_config
#endif

#endif