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
// Can be customized for different microcontrollers, this file is for GD32F205VCT6.

#pragma once

#include <gd32f4xx.h>
#include <gd32f4xx_gpio.h>
#include <scsi2sd.h>
#include <ZuluSCSI_config.h>

#ifdef __cplusplus
#include <SdFat.h>

extern "C" {
#endif

extern const char *g_platform_name;

// Debug logging functions
void platform_log(const char *s);

// Minimal millis() implementation as GD32F205 does not
// have an Arduino core yet.
unsigned long millis(void);
void delay(unsigned long ms);

// Precise nanosecond delays
// Works in interrupt context also, max delay 500 000 ns, min delay about 500 ns
void delay_ns(unsigned long ns);

static inline void delay_us(unsigned long us)
{
    if (us > 0)
    {
        delay_ns(us * 1000);
    }
}

// Approximate fast delay
static inline void delay_100ns()
{
//    asm volatile ("nop \n nop \n nop \n nop \n nop");
   asm volatile ("nop \n nop \n nop \n nop \n nop");
}

// Initialize SPI and GPIO configuration
void platform_init();

// Initialization for main application, not used for bootloader
void platform_late_init();

// Initialization after the SD Card has been found
void platform_post_sd_card_init();

// Hooks
void platform_end_of_loop_hook(void);

// Disable the status LED
void platform_disable_led(void);

// Setup soft watchdog
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

uint32_t platform_sys_clock_in_hz();

// Attempt to reclock the MCU - unsupported
inline zuluscsi_reclock_status_t platform_reclock(uint32_t clk_in_khz){return ZULUSCSI_RECLOCK_NOT_SUPPORTED;}

// Returns true if reboot was for mass storage - unsupported
inline bool platform_rebooted_into_mass_storage() {return false;}

// Reinitialize SD card connection and save log from interrupt context.
// This can be used in crash handlers.
void platform_emergency_log_save();

// Set callback that will be called during data transfer to/from SD card.
// This can be used to implement simultaneous transfer to SCSI bus.
typedef void (*sd_callback_t)(uint32_t bytes_complete);
void platform_set_sd_callback(sd_callback_t func, const uint8_t *buffer);

// This function is called by scsiPhy.cpp.
// It resets the systick counter to give 1 millisecond of uninterrupted transfer time.
// The total number of skips is kept track of to keep the correct time on average.
void SysTick_Handle_PreEmptively();

// Reprogram firmware in main program area.
#define PLATFORM_BOOTLOADER_SIZE 32768
#define PLATFORM_FLASH_TOTAL_SIZE (512 * 1024)

// must be a factor of each sector map size
#define PLATFORM_FLASH_WRITE_BUFFER_SIZE 2048


void platform_boot_to_main_firmware();

// Configuration customizations based on DIP switch settings
// When DIPSW1 is on, Apple quirks are enabled by default.
void platform_config_hook(S2S_TargetCfg *config);
#define PLATFORM_CONFIG_HOOK(cfg) platform_config_hook(cfg)

// Write a single SCSI pin.
// Example use: SCSI_OUT(ATN, 1) sets SCSI_ATN to low (active) state.
#define SCSI_OUT(pin, state) \
    GPIO_BOP(SCSI_OUT_ ## pin ## _PORT) = (SCSI_OUT_ ## pin ## _PIN) << (state ? 16 : 0)

// Read a single SCSI pin.
// Example use: SCSI_IN(ATN), returns 1 for active low state.
#define SCSI_IN(pin) \
    ((GPIO_ISTAT(SCSI_ ## pin ## _PORT) & (SCSI_ ## pin ## _PIN)) ? 0 : 1)

// Write SCSI data bus, also sets REQ to inactive.
extern const uint32_t g_scsi_out_byte_to_bop[256];
#define SCSI_OUT_DATA(data) \
    GPIO_BOP(SCSI_OUT_PORT) = g_scsi_out_byte_to_bop[(uint8_t)(data)]

// Release SCSI data bus and REQ signal
#define SCSI_RELEASE_DATA_REQ() \
    GPIO_BOP(SCSI_OUT_PORT) = SCSI_OUT_DATA_MASK | SCSI_OUT_REQ

// Release all SCSI outputs
#define SCSI_RELEASE_OUTPUTS() \
    GPIO_BOP(SCSI_OUT_PORT) = SCSI_OUT_DATA_MASK | SCSI_OUT_REQ, \
    GPIO_BOP(SCSI_OUT_IO_PORT)  = SCSI_OUT_IO_PIN, \
    GPIO_BOP(SCSI_OUT_CD_PORT)  = SCSI_OUT_CD_PIN, \
    GPIO_BOP(SCSI_OUT_SEL_PORT) = SCSI_OUT_SEL_PIN, \
    GPIO_BOP(SCSI_OUT_MSG_PORT) = SCSI_OUT_MSG_PIN, \
    GPIO_BOP(SCSI_OUT_RST_PORT) = SCSI_OUT_RST_PIN, \
    GPIO_BOP(SCSI_OUT_BSY_PORT) = SCSI_OUT_BSY_PIN

// Read SCSI data bus
#define SCSI_IN_DATA(data) \
    (((~GPIO_ISTAT(SCSI_IN_PORT)) & SCSI_IN_MASK) >> SCSI_IN_SHIFT)

#ifdef __cplusplus
}

// From GD32F4xx user manual
const uint32_t platform_flash_sector_map[] =
    {
         16 * 1024,
         16 * 1024,
         16 * 1024,
         16 * 1024,
         64 * 1024,
        128 * 1024,
        128 * 1024, 
        128 * 1024
    };

bool platform_firmware_erase(FsFile &file);
bool platform_firmware_program(FsFile &file);
// SD card driver for SdFat

// SDIO interface, ZuluSCSI v1.4
class SdioConfig;
extern SdioConfig g_sd_sdio_config;
extern SdioConfig g_sd_sdio_config_crash;
#define SD_CONFIG g_sd_sdio_config
#define SD_CONFIG_CRASH g_sd_sdio_config_crash

// Check if a DMA request for SD card read has completed.
// This is used to optimize the timing of data transfers on SCSI bus.
bool check_sd_read_done();

#endif
