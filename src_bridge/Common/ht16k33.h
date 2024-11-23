/**
 * RP2040 FreeRTOS Template - App #2
 * HT16K33-based I2C 4-digit, 7-segment LED display driver
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#ifndef HT16K33_HEADER
#define HT16K33_HEADER


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
#include "i2c_utils.h"
#include "utils.h"


/*
 * CONSTANTS
 */
constexpr uint8_t HT16K33_GENERIC_DISPLAY_ON        = 0x81;
constexpr uint8_t HT16K33_GENERIC_DISPLAY_OFF       = 0x80;
constexpr uint8_t HT16K33_GENERIC_SYSTEM_ON         = 0x21;
constexpr uint8_t HT16K33_GENERIC_SYSTEM_OFF        = 0x20;
constexpr uint8_t HT16K33_GENERIC_DISPLAY_ADDRESS   = 0x00;
constexpr uint8_t HT16K33_GENERIC_CMD_BRIGHTNESS    = 0xE0;
constexpr uint8_t HT16K33_GENERIC_CMD_BLINK         = 0x81;
constexpr uint8_t HT16K33_ADDRESS                   = 0x70;

constexpr uint8_t HT16K33_SEGMENT_COLON_ROW         = 0x04;
constexpr uint8_t HT16K33_SEGMENT_MINUS_CHAR        = 0x10;
constexpr uint8_t HT16K33_SEGMENT_DEGREE_CHAR       = 0x11;
constexpr uint8_t HT16K33_SEGMENT_SPACE_CHAR        = 0x00;


/**
    A basic driver for I2C-connected HT16K33-based four-digit, seven-segment displays.
 */
class HT16K33_Segment {

    public:
        explicit HT16K33_Segment(uint8_t address = HT16K33_ADDRESS);

        void                init(void);
        void                power_on(bool turn_on = true) const;

        void                set_brightness(uint8_t brightness = 15) const;
        HT16K33_Segment&    set_colon(bool is_set = false);
        HT16K33_Segment&    set_glyph(uint8_t glyph, uint8_t digit, bool has_dot = false);
        HT16K33_Segment&    set_number(uint8_t number, uint8_t digit, bool has_dot = false);
        HT16K33_Segment&    set_alpha(char chr, uint8_t digit, bool has_dot = false);

        HT16K33_Segment&    clear(void);
        void                draw(void) const;

    private:
        uint8_t             buffer[16];
        uint8_t             i2c_addr;
};


#endif  // HT16K33_HEADER
