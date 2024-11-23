/**
 * RP2040 FreeRTOS Template - App #3
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#ifndef MAIN_H
#define MAIN_H


// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <semphr.h>
// CXX
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
// Pico SDK
#include "pico/stdlib.h"            // Includes `hardware_gpio.h`
#include "pico/binary_info.h"
#include "hardware/i2c.h"
// App
#include "../Common/i2c_utils.h"
#include "../Common/ht16k33.h"
#include "../Common/mcp9808.h"
#include "../Common/utils.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * CONSTANTS
 */
constexpr uint8_t   RED_LED_PIN                 = 20;
constexpr uint8_t   ALERT_LED_PIN               = 26;
constexpr uint8_t   ALERT_SENSE_PIN             = 16;

constexpr uint8_t   SENSOR_TASK_DELAY_TICKS     = 20;
constexpr uint16_t  ALERT_DISPLAY_PERIOD_MS     = 10000;

constexpr uint8_t   LED_ON                      = 1;
constexpr uint8_t   LED_OFF                     = 0;
constexpr uint8_t   LED_ERROR_FLASHES           = 5;

constexpr uint8_t   TEMP_LOWER_LIMIT_C          = 10;
constexpr uint8_t   TEMP_UPPER_LIMIT_C          = 30;
constexpr uint8_t   TEMP_CRIT_LIMIT_C           = 50;


/**
 * PROTOTYPES
 */
void                setup(void);
void                setup_led(void);
void                setup_i2c(void);
void                setup_gpio(void);

void                enable_irq(bool state = true);
void                gpio_isr(uint gpio, uint32_t events);

void                led_on(void);
void                led_off(void);
void                led_set(bool state = true);

void                display_int(int number);
void                display_tmp(double value);

void                timer_fired_callback(TimerHandle_t timer);
void                set_alert_timer(void);


#ifdef __cplusplus
}           // extern "C"
#endif


#endif      // MAIN_H
