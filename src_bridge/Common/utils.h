/**
 * RP2040 FreeRTOS Template - App #2
 * General utility functions
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#ifndef UTILS_HEADER
#define UTILS_HEADER


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
#include "pico/stdlib.h"
#include "pico/binary_info.h"


using std::vector;
using std::string;


/*
 * PROTOTYPES
 */
namespace Utils {
    vector<string>  split_to_lines(string& str, const string& sep = "\r\n");
    string          split_msg(string& msg, uint32_t want_line);
    string          get_sms_number(const string& line);
    string          get_field_value(const string& line, uint32_t field_number);
    string          uppercase(string base);
    uint32_t        bcd(uint32_t base);
    void            log_device_info(void);
    void            log_debug(const string& msg);
}


#endif // UTILS_HEADER
