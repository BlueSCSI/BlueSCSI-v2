// Platform-specific definitions for BlueSCSI.
//
// This file is example platform definition that can easily be
// customized for a different board / CPU.

#pragma once

/* Add any platform-specific includes you need here */
#include <stdint.h>
#include <Arduino.h>
#include "BlueSCSI_platform_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These are used in debug output and default SCSI strings */
extern const char *g_bluescsiplatform_name;
#define PLATFORM_NAME "Example"
#define PLATFORM_REVISION "1.0"

// Debug logging function, can be used to print to e.g. serial port.
// May get called from interrupt handlers.
void bluescsiplatform_log(const char *s);

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
void bluescsiplatform_init();

// Initialization for main application, not used for bootloader
void bluescsiplatform_late_init();

// Disable the status LED
void azplatform_disable_led(void);

// Setup soft watchdog if supported
void bluescsiplatform_reset_watchdog();

// Set callback that will be called during data transfer to/from SD card.
// This can be used to implement simultaneous transfer to SCSI bus.
typedef void (*sd_callback_t)(uint32_t bytes_complete);
void bluescsiplatform_set_sd_callback(sd_callback_t func, const uint8_t *buffer);

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
