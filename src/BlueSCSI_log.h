// Helpers for log messages.

#pragma once

#include <stdint.h>
#include <stddef.h>

// Get total number of bytes that have been written to log
uint32_t bluelog_get_buffer_len();

// Get log as a string.
// If startpos is given, continues log reading from previous position and updates the position.
const char *bluelog_get_buffer(uint32_t *startpos);

// Whether to enable debug messages
extern bool g_bluelog_debug;

// Firmware version string
extern const char *g_bluelog_firmwareversion;

// Log string
void bluelog_raw(const char *str);

// Log byte as hex
void bluelog_raw(uint8_t value);

// Log integer as hex
void bluelog_raw(uint32_t value);

// Log integer as hex
void bluelog_raw(uint64_t value);

// Log integer as decimal
void bluelog_raw(int value);

// Log array of bytes
struct bytearray {
    bytearray(const uint8_t *data, size_t len): data(data), len(len) {}
    const uint8_t *data;
    size_t len;
};
void bluelog_raw(bytearray array);

inline void bluelog_raw()
{
    // End of template recursion
}

extern "C" unsigned long millis();

// Variadic template for printing multiple items
template<typename T, typename T2, typename... Rest>
inline void bluelog_raw(T first, T2 second, Rest... rest)
{
    bluelog_raw(first);
    bluelog_raw(second);
    bluelog_raw(rest...);
}

// Format a complete log message
template<typename... Params>
inline void bluelog(Params... params)
{
    bluelog_raw("[", (int)millis(), "ms] ");
    bluelog_raw(params...);
    bluelog_raw("\n");
}

// Format a complete debug message
template<typename... Params>
inline void bluedbg(Params... params)
{
    if (g_bluelog_debug)
    {
        bluelog_raw("[", (int)millis(), "ms] DBG ");
        bluelog_raw(params...);
        bluelog_raw("\n");
    }
}
