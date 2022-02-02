// Platform-specific definitions for AzulSCSI.
// Can be customized for different microcontrollers, this file is for GD32F205VCT6.

#pragma once

#include <gd32f20x.h>
#include <gd32f20x_gpio.h>
#include "AzulSCSI_config.h"

#ifdef __cplusplus
// SD card driver for SdFat
class SdSpiConfig;
extern SdSpiConfig g_sd_spi_config;
#define SD_CONFIG g_sd_spi_config

extern "C" {
#endif

extern const char *g_azplatform_name;

#if defined(AZULSCSI_V1_0)
#   define PLATFORM_NAME "AzulSCSI v1.0"
#   include "AzulSCSI_v1_0_gpio.h"
#elif defined(AZULSCSI_V1_1)
#   define PLATFORM_NAME "AzulSCSI v1.1"
#   include "AzulSCSI_v1_1_gpio.h"
#endif

// Debug logging functions
void azplatform_log(const char *s);

// Minimal millis() implementation as GD32F205 does not
// have an Arduino core yet.
unsigned long millis();
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

// Set callback for when SCSI_RST pin goes low
void azplatform_set_rst_callback(void (*callback)());

// Setup soft watchdog
void azplatform_reset_watchdog(int timeout_ms);

// Reinitialize SD card connection and save log from interrupt context.
// This can be used in crash handlers.
void azplatform_emergency_log_save();

// Direct streaming between SCSI and SD card
// If the SD card driver receives a read request to buffer, it will directly send the data to SCSI bus.
// If the SD card driver receives a write request from buffer, it will directly get the data from SCSI.
void azplatform_prepare_stream(uint8_t *buffer);

// Get status of latest streaming operation.
// Returns number of bytes transferred.
size_t azplatform_finish_stream();

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
#endif