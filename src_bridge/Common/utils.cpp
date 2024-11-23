/**
 * RP2040 FreeRTOS Template - App #2
 * General utility functions
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#include "utils.h"

using std::vector;
using std::string;


namespace Utils {

/**
 * @brief Convert a multi-line string into an array of lines,
 *        split at the specified separator string, eg. `\r\n`.
 *
 * @param ml_str:    The multi-line string.
 * @param separator: The line-separator string.
 *
 * @retval The lines as a vector.
 */
vector<string> split_to_lines(string ml_str, const string& separator) {

    vector<string> result;
    while (ml_str.length()) {
        const int index = ml_str.find(separator);
        if (index != string::npos){
            result.push_back(ml_str.substr(0, index));
            ml_str = ml_str.substr(index + separator.length());
        } else {
            result.push_back(ml_str);
            break;
        }
    }

    return result;
}


/**
 * @brief Get a specific line from a multi-line string.
 *
 * @param ml_str:    The multi-line string.
 * @param want_line: The required line (0 indexed).
 *
 * @retval The requested line, otherwise an empty string.
 */
string split_msg(string ml_str, uint32_t want_line) {

    const vector<string> lines = split_to_lines(ml_str);
    for (uint32_t i = 0 ; i < lines.size() ; ++i) {
        if (i == want_line) return lines[i];
    }
    return "";
}


/**
 * @brief Get a number from the end of a +CMTI line from the modem.
 *
 * @param line: The target line.
 *
 * @retval A pointer to the start of the number, or `null`.
 */
string get_sms_number(const string& line) {

    return get_field_value(line, 1);
}


/**
 * @brief A value from a sequence of comma-separated values.
 *
 * @param line:         The source line.
 * @param field_number: The request value.
 *
 * @retval The value as a string, otherwise an empty string.
 */
string get_field_value(const string& line, uint32_t field_number) {

    const vector<string> result = split_to_lines(line, ",");
    if (result.size() > field_number) return result[field_number];
    return "";
}


/**
 * @brief Convert a 16-bit int (to cover decimal range 0-9999) to
 *        its BCD equivalent.
 *
 * @param base: The input integer.
 *
 * @retval The BCD encoding of the input.
 */
uint32_t bcd(uint32_t base) {

    if (base > 9999) base = 9999;
    for (uint32_t i = 0 ; i < 16 ; ++i) {
        base = base << 1;
        if (i == 15) break;
        if ((base & 0x000F0000) > 0x0004FFFF) base += 0x00030000;
        if ((base & 0x00F00000) > 0x004FFFFF) base += 0x00300000;
        if ((base & 0x0F000000) > 0x04FFFFFF) base += 0x03000000;
        if ((base & 0xF0000000) > 0x4FFFFFFF) base += 0x30000000;
    }

    return (base >> 16) & 0xFFFF;
}


/**
 * @brief Convert a string to uppercase.
 *
 * @param base: The input string.
 *
 * @retval An uppercase string.
 */
string uppercase(string base) {

    std::transform(base.begin(), base.end(), base.begin(), ::toupper);
    return base;
}


/**
 * @brief Output basic device info.
 */
void log_device_info(void) {

    printf("App: %s %s (%i)\n", APP_NAME, APP_VERSION, BUILD_NUM);
}


/**
 * @brief Generate and print a debug message from a supplied string.
 *
 * @param msg: The base message to which `[DEBUG]` will be prefixed.
 */
void log_debug(const string& msg) {

    printf("[DEBUG] %s\n", msg.c_str());
}


}   // namespace Utils
