// Helpers for log messages.

#pragma once

#include <stdint.h>
#include <stddef.h>

// Get total number of bytes that have been written to log
uint32_t azlog_get_buffer_len();

// Get log as a string.
// If startpos is given, continues log reading from previous position and updates the position.
const char *azlog_get_buffer(uint32_t *startpos);

// Whether to enable debug messages
extern bool g_azlog_debug;

// Firmware version string
extern const char *g_azlog_firmwareversion;

// Log string
void azlog_raw(const char *str);

// Log byte as hex
void azlog_raw(uint8_t value);

// Log integer as hex
void azlog_raw(uint32_t value);

// Log integer as hex
void azlog_raw(uint64_t value);

// Log integer as decimal
void azlog_raw(int value);

// Log array of bytes
struct bytearray {
    bytearray(const uint8_t *data, size_t len): data(data), len(len) {}
    const uint8_t *data;
    size_t len;
};
void azlog_raw(bytearray array);

inline void azlog_raw()
{
    // End of template recursion
}

extern "C" unsigned long millis();

// Variadic template for printing multiple items
template<typename T, typename T2, typename... Rest>
inline void azlog_raw(T first, T2 second, Rest... rest)
{
    azlog_raw(first);
    azlog_raw(second);
    azlog_raw(rest...);
}

// Format a complete log message
template<typename... Params>
inline void azlog(Params... params)
{
    azlog_raw("[", (int)millis(), "ms] ");
    azlog_raw(params...);
    azlog_raw("\n");
}

// Format a complete debug message
template<typename... Params>
inline void azdbg(Params... params)
{
    if (g_azlog_debug)
    {
        azlog_raw("[", (int)millis(), "ms] DBG ");
        azlog_raw(params...);
        azlog_raw("\n");
    }
}
