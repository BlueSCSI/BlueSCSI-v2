// Platform-specific definitions for AzulSCSI.
// Can be customized for different microcontrollers, this file is for GD32F205VCT6.

#pragma once

#include <gd32f20x.h>
#include <gd32f20x_gpio.h>

#ifdef __cplusplus
// SD card driver for SdFat
class SdSpiConfig;
extern SdSpiConfig g_sd_spi_config;
#define SD_CONFIG g_sd_spi_config

extern "C" {
#endif

extern const char *g_azplatform_name;

// GPIO definitions

// SCSI output port.
// This is written using BSRR mechanism, so all output pins must be on same GPIO port.
// The output pins are open-drain in hardware, using separate buffer chips for driving.
#define SCSI_OUT_PORT GPIOD
#define SCSI_OUT_DB7  GPIO_PIN_0
#define SCSI_OUT_DB6  GPIO_PIN_1
#define SCSI_OUT_DB5  GPIO_PIN_2
#define SCSI_OUT_DB4  GPIO_PIN_3
#define SCSI_OUT_DB3  GPIO_PIN_4
#define SCSI_OUT_DB2  GPIO_PIN_5
#define SCSI_OUT_DB1  GPIO_PIN_6
#define SCSI_OUT_DB0  GPIO_PIN_7
#define SCSI_OUT_IO   GPIO_PIN_8
#define SCSI_OUT_REQ  GPIO_PIN_9
#define SCSI_OUT_CD   GPIO_PIN_10
#define SCSI_OUT_SEL  GPIO_PIN_11
#define SCSI_OUT_MSG  GPIO_PIN_12
#define SCSI_OUT_RST  GPIO_PIN_13
#define SCSI_OUT_BSY  GPIO_PIN_14
#define SCSI_OUT_DBP  GPIO_PIN_15
#define SCSI_OUT_DATA_MASK (0x00FF | SCSI_OUT_DBP)
#define SCSI_OUT_MASK 0xFFFF

// SCSI input port
#define SCSI_IN_PORT  GPIOE
#define SCSI_IN_DB7   GPIO_PIN_15
#define SCSI_IN_DB6   GPIO_PIN_14
#define SCSI_IN_DB5   GPIO_PIN_13
#define SCSI_IN_DB4   GPIO_PIN_12
#define SCSI_IN_DB3   GPIO_PIN_11
#define SCSI_IN_DB2   GPIO_PIN_10
#define SCSI_IN_DB1   GPIO_PIN_9
#define SCSI_IN_DB0   GPIO_PIN_8
#define SCSI_IN_DBP   GPIO_PIN_7
#define SCSI_IN_MASK  (SCSI_IN_DB7|SCSI_IN_DB6|SCSI_IN_DB5|SCSI_IN_DB4|SCSI_IN_DB3|SCSI_IN_DB2|SCSI_IN_DB1|SCSI_IN_DB0|SCSI_IN_DBP)
#define SCSI_IN_SHIFT 8

// Various SCSI status signals
#define SCSI_ATN_PORT GPIOB // FIXME: Change to 5V-tolerant pin
#define SCSI_ATN_PIN  GPIO_PIN_0
#define SCSI_BSY_PORT GPIOB
#define SCSI_BSY_PIN  GPIO_PIN_10
#define SCSI_SEL_PORT GPIOB
#define SCSI_SEL_PIN  GPIO_PIN_11
#define SCSI_ACK_PORT GPIOB
#define SCSI_ACK_PIN  GPIO_PIN_12

// RST pin uses EXTI interrupt
#define SCSI_RST_PORT GPIOB
#define SCSI_RST_PIN  GPIO_PIN_13
#define SCSI_RST_EXTI EXTI_13
#define SCSI_RST_EXTI_SOURCE_PORT GPIO_PORT_SOURCE_GPIOB
#define SCSI_RST_EXTI_SOURCE_PIN GPIO_PIN_SOURCE_13
#define SCSI_RST_IRQ  EXTI10_15_IRQHandler
#define SCSI_RST_IRQn EXTI10_15_IRQn

// Terminator enable/disable config, active low
#define SCSI_TERM_EN_PORT GPIOC
#define SCSI_TERM_EN_PIN  GPIO_PIN_8

// SD card pins
#define SD_PORT      GPIOA
#define SD_CS_PIN    GPIO_PIN_4
#define SD_CLK_PIN   GPIO_PIN_5
#define SD_MISO_PIN  GPIO_PIN_6
#define SD_MOSI_PIN  GPIO_PIN_7

// DIP switches
#define DIP_PORT     GPIOB
#define DIPSW1_PIN   GPIO_PIN_4
#define DIPSW2_PIN   GPIO_PIN_5
#define DIPSW3_PIN   GPIO_PIN_6

// Status LED pins
#define LED_PORT     GPIOC
#define LED_I_PIN    GPIO_PIN_4
#define LED_E_PIN    GPIO_PIN_5
#define LED_PINS     (LED_I_PIN | LED_E_PIN)
#define LED_ON()     gpio_bit_reset(LED_PORT, LED_PINS)
#define LED_OFF()    gpio_bit_set(LED_PORT, LED_PINS)

// Debug logging functions
void azplatform_log(const char *s);

// Minimal millis() implementation as GD32F205 does not
// have an Arduino core yet.
unsigned long millis();
void delay(unsigned long ms);

// Precise nanosecond delays
static inline void delay_100ns()
{
#if HXTAL_VALUE==8000000
    asm("nop"); asm("nop"); asm("nop");
#else
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
#endif
}

// Initialize SPI and GPIO configuration
void azplatform_init();

// Set callback for when SCSI_RST pin goes low
void azplatform_set_rst_callback(void (*callback)());

// Reinitialize SD card connection and save log from interrupt context.
// This can be used in crash handlers.
void azplatform_emergency_log_save();

// Write a single SCSI pin.
// Example use: SCSI_OUT(ATN, 1) sets SCSI_ATN to low (active) state.
#define SCSI_OUT(pin, state) \
    GPIO_BOP(SCSI_OUT_PORT) = (SCSI_OUT_ ## pin) << (state ? 16 : 0)

// Read a single SCSI pin.
// Example use: SCSI_IN(ATN), returns 1 for active low state.
#define SCSI_IN(pin) \
    ((GPIO_ISTAT(SCSI_ ## pin ## _PORT) & (SCSI_ ## pin ## _PIN)) ? 0 : 1)

// Write SCSI data bus
extern const uint32_t g_scsi_out_byte_to_bop[256];
#define SCSI_OUT_DATA(data) \
    GPIO_BOP(SCSI_OUT_PORT) = g_scsi_out_byte_to_bop[(uint8_t)(data)]

// Release SCSI data bus
#define SCSI_RELEASE_DATA() \
    GPIO_BOP(SCSI_OUT_PORT) = SCSI_OUT_DATA_MASK

// Release all SCSI outputs
#define SCSI_RELEASE_OUTPUTS() \
    GPIO_BOP(SCSI_OUT_PORT) = SCSI_OUT_MASK

// Read SCSI data bus
#define SCSI_IN_DATA(data) \
    (((~GPIO_ISTAT(SCSI_IN_PORT)) & SCSI_IN_MASK) >> SCSI_IN_SHIFT)

#ifdef __cplusplus
}
#endif