// Platform-specific definitions for AzulSCSI.
// Can be customized for different microcontrollers, this file is for GD32F205VCT6.

#pragma once

#include <gd32f20x.h>
#include <gd32f20x_gpio.h>
#include <scsi2sd.h>
#include "AzulSCSI_config.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char *g_azplatform_name;

#if defined(AZULSCSI_V1_0)
#   define PLATFORM_NAME "AzulSCSI v1.0"
#   define PLATFORM_REVISION "1.0"
#   define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_ASYNC_50
#   include "AzulSCSI_v1_0_gpio.h"
#elif defined(AZULSCSI_V1_1)
#   define PLATFORM_NAME "AzulSCSI v1.1"
#   define PLATFORM_REVISION "1.1"
#   define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_ASYNC_50
#   define PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE 4096
#   define PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE 65536
#   define PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE 8192
#   include "AzulSCSI_v1_1_gpio.h"
#endif

// Debug logging functions
void azplatform_log(const char *s);

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
    asm volatile ("nop \n nop \n nop \n nop \n nop");
}

// Initialize SPI and GPIO configuration
void azplatform_init();

// Initialization for main application, not used for bootloader
void azplatform_late_init();

// Setup soft watchdog
void azplatform_reset_watchdog();

// Reinitialize SD card connection and save log from interrupt context.
// This can be used in crash handlers.
void azplatform_emergency_log_save();

// Set callback that will be called during data transfer to/from SD card.
// This can be used to implement simultaneous transfer to SCSI bus.
typedef void (*sd_callback_t)(uint32_t bytes_complete);
void azplatform_set_sd_callback(sd_callback_t func, const uint8_t *buffer);

// Reprogram firmware in main program area.
#define AZPLATFORM_BOOTLOADER_SIZE 32768
#define AZPLATFORM_FLASH_TOTAL_SIZE (256 * 1024)
#define AZPLATFORM_FLASH_PAGE_SIZE 2048
bool azplatform_rewrite_flash_page(uint32_t offset, uint8_t buffer[AZPLATFORM_FLASH_PAGE_SIZE]);
void azplatform_boot_to_main_firmware();

// Configuration customizations based on DIP switch settings
// When DIPSW1 is on, Apple quirks are enabled by default.
void azplatform_config_hook(S2S_TargetCfg *config);
#define AZPLATFORM_CONFIG_HOOK(cfg) azplatform_config_hook(cfg)
#define APPLE_DRIVEINFO_FIXED     {"SEAGATE",  "ST225N",            PLATFORM_REVISION, ""}
#define APPLE_DRIVEINFO_REMOVABLE {"AZULSCSI", "APPLE_REMOVABLE",   PLATFORM_REVISION, ""}
#define APPLE_DRIVEINFO_OPTICAL   {"AZULSCSI", "APPLE_CD",          PLATFORM_REVISION, ""}
#define APPLE_DRIVEINFO_FLOPPY    {"AZULSCSI", "APPLE_FLOPPY",      PLATFORM_REVISION, ""}
#define APPLE_DRIVEINFO_MAGOPT    {"AZULSCSI", "APPLE_MO",          PLATFORM_REVISION, ""}
#define APPLE_DRIVEINFO_TAPE      {"AZULSCSI", "APPLE_TAPE",        PLATFORM_REVISION, ""}

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

// SD card driver for SdFat
#ifndef SD_USE_SDIO
// SPI interface, AzulSCSI v1.0
class SdSpiConfig;
extern SdSpiConfig g_sd_spi_config;
#define SD_CONFIG g_sd_spi_config
#define SD_CONFIG_CRASH g_sd_spi_config

#else
// SDIO interface, AzulSCSI v1.1
class SdioConfig;
extern SdioConfig g_sd_sdio_config;
extern SdioConfig g_sd_sdio_config_crash;
#define SD_CONFIG g_sd_sdio_config
#define SD_CONFIG_CRASH g_sd_sdio_config_crash

#endif

#endif