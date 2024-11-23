/**
 * RP2040 FreeRTOS Template - App #2
 * Generic I2C functions
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#ifndef I2C_UTILS_HEADER
#define I2C_UTILS_HEADER


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
#include "utils.h"


/*
 * CONSTANTS
 */
constexpr i2c_inst_t* I2C_PORT      = i2c1;
constexpr uint32_t    I2C_FREQUENCY = 400000;
constexpr uint8_t     SDA_GPIO      = 2;
constexpr uint8_t     SCL_GPIO      = 3;


/*
 * PROTOTYPES
 */
namespace I2C {
    void        setup(void);
    void        write_byte(uint8_t address, uint8_t byte);
    void        write_block(uint8_t address, const uint8_t* data, uint8_t count);
    void        read_block(uint8_t address, uint8_t* data, uint8_t count);
    int         read_noblock(uint8_t address, uint8_t* data, uint8_t count);
}


#endif  // I2C_UTILS_HEADER
