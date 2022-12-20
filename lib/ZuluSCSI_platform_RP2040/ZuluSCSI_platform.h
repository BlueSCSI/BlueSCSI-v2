// Platform-specific definitions for ZuluSCSI RP2040 hardware.

#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "ZuluSCSI_platform_gpio.h"
#include "scsiHostPhy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These are used in debug output and default SCSI strings */
extern const char *g_azplatform_name;
#define PLATFORM_NAME "ZuluSCSI RP2040"
#define PLATFORM_REVISION "2.0"
#define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_SYNC_10
#define PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE 4096
#define PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE 32768
#define PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE 8192
#define SD_USE_SDIO 1
#define PLATFORM_HAS_INITIATOR_MODE 1

// NOTE: The driver supports synchronous speeds higher than 10MB/s, but this
// has not been tested due to lack of fast enough SCSI adapter.
// #define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_TURBO

// Debug logging function, can be used to print to e.g. serial port.
// May get called from interrupt handlers.
void azplatform_log(const char *s);
void azplatform_emergency_log_save();

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
void azplatform_init();

// Initialization for main application, not used for bootloader
void azplatform_late_init();

// Disable the status LED
void azplatform_disable_led(void);

// Query whether initiator mode is enabled on targets with PLATFORM_HAS_INITIATOR_MODE
bool azplatform_is_initiator_mode_enabled();

// Setup soft watchdog if supported
void azplatform_reset_watchdog();

// Set callback that will be called during data transfer to/from SD card.
// This can be used to implement simultaneous transfer to SCSI bus.
typedef void (*sd_callback_t)(uint32_t bytes_complete);
void azplatform_set_sd_callback(sd_callback_t func, const uint8_t *buffer);

// Reprogram firmware in main program area.
#ifndef RP2040_DISABLE_BOOTLOADER
#define AZPLATFORM_BOOTLOADER_SIZE (128 * 1024)
#define AZPLATFORM_FLASH_TOTAL_SIZE (1024 * 1024)
#define AZPLATFORM_FLASH_PAGE_SIZE 4096
bool azplatform_rewrite_flash_page(uint32_t offset, uint8_t buffer[AZPLATFORM_FLASH_PAGE_SIZE]);
void azplatform_boot_to_main_firmware();
#endif

// ROM drive in the unused external flash area
#ifndef RP2040_DISABLE_ROMDRIVE
#define PLATFORM_HAS_ROM_DRIVE 1
// Check maximum available space for ROM drive in bytes
uint32_t azplatform_get_romdrive_maxsize();

// Read ROM drive area
bool azplatform_read_romdrive(uint8_t *dest, uint32_t start, uint32_t count);

// Reprogram ROM drive area
#define AZPLATFORM_ROMDRIVE_PAGE_SIZE 4096
bool azplatform_write_romdrive(const uint8_t *data, uint32_t start, uint32_t count);
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
extern const uint32_t g_scsi_parity_lookup[256];
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