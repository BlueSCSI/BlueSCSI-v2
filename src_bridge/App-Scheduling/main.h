/**
 * RP2040 FreeRTOS Template - App #2
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
constexpr uint8_t   RED_LED_PIN = 20;


/**
 * PROTOTYPES
 */
void                setup(void);
void                setup_led(void);
void                setup_i2c(void);

void                led_on(void);
void                led_off(void);
void                led_set(bool state = true);

[[noreturn]] void   led_task_pico(void* unused_arg);
[[noreturn]] void   led_task_gpio(void* unused_arg);
[[noreturn]] void   sensor_read_task(void* unused_arg);

void                display_int(int number);
void                display_tmp(double value);


#ifdef __cplusplus
}           // extern "C"
#endif


#endif      // MAIN_H
