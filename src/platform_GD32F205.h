// Platform-specific definitions.
// Can be customized for different microcontrollers, this file is for GD32F205VCT6.

#pragma once

#include <gd32f20x.h>
#include <gd32f20x_gpio.h>

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

// Various SCSI status signals
#define SCSI_ATN_PORT GPIOB // FIXME: Change to 5V-tolerant pin
#define SCSI_ATN_PIN  GPIO_PIN_0
#define SCSI_BSY_PORT GPIOB
#define SCSI_BSY_PIN  GPIO_PIN_10
#define SCSI_SEL_PORT GPIOB
#define SCSI_SEL_PIN  GPIO_PIN_11
#define SCSI_ACK_PORT GPIOB
#define SCSI_ACK_PIN  GPIO_PIN_12
#define SCSI_RST_PORT GPIOB
#define SCSI_RST_PIN  GPIO_PIN_13

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
#define LOG(XX)     //Serial.print(XX)
#define LOGHEX(XX)  //Serial.print(XX, HEX)
#define LOGN(XX)    //Serial.println(XX)
#define LOGHEXN(XX) //Serial.println(XX, HEX)

// Initialize SPI and GPIO configuration
// Clock has already been initialized by system_gd32f20x.c
static void platform_init()
{
    // Enable needed clocks
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);

    // Switch to SWD debug port (disable JTAG) to release PB4 as GPIO
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);    

    // SCSI pins.
    // Initialize open drain outputs to high.
    gpio_bit_set(SCSI_OUT_PORT, SCSI_OUT_MASK);
    gpio_init(SCSI_OUT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_MASK);
    gpio_init(SCSI_IN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_IN_MASK);
    gpio_init(SCSI_ATN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_ATN_PIN);
    gpio_init(SCSI_BSY_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_BSY_PIN);
    gpio_init(SCSI_SEL_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_SEL_PIN);
    gpio_init(SCSI_ACK_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_ACK_PIN);
    gpio_init(SCSI_RST_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_RST_PIN);

    // Terminator enable
    gpio_bit_set(SCSI_TERM_EN_PORT, SCSI_TERM_EN_PIN);
    gpio_init(SCSI_TERM_EN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, SCSI_TERM_EN_PIN);

    // SD card pins
    gpio_init(SD_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SD_CS_PIN);
    gpio_init(SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_CLK_PIN);
    gpio_init(SD_PORT, GPIO_MODE_IN_FLOATING, 0, SD_MISO_PIN);
    gpio_init(SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_MOSI_PIN);

    // DIP switches
    gpio_init(DIP_PORT, GPIO_MODE_IPD, 0, DIPSW1_PIN | DIPSW2_PIN | DIPSW3_PIN);

    // LED pins
    gpio_bit_set(LED_PORT, LED_PINS);
    gpio_init(LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, LED_PINS);

    // SWO trace pin on PB3
    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);
}