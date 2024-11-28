// Helpers for log messages.
// Copyright (c) 2022 Rabbit Hole Computing™
// Copyright (c) 2023 Eric Helgeson

#pragma once

#include "scsiPhy.h"
#include <stddef.h>
#include <stdint.h>
#ifdef LIB_FREERTOS_KERNEL
#include "BlueSCSI_platform.h"
#endif

// Get total number of bytes that have been written to log
uint32_t log_get_buffer_len();

// Get log as a string.
// If startpos is given, continues log reading from previous position and updates the position.
// If available is given, number of bytes available is written there.
const char *log_get_buffer(uint32_t *startpos, uint32_t *available = nullptr);

// Whether to enable debug messages
extern "C" bool g_log_debug;
extern "C" uint8_t g_scsi_log_mask;

// Enables output test mode
extern bool g_test_mode;

// Firmware version string
extern const char *g_log_firmwareversion;

// Log string
void log_raw(const char *str);

// Log byte as hex
void log_raw(uint8_t value);

// Log integer as hex
void log_raw(uint32_t value);

// Log integer as hex
void log_raw(uint64_t value);

// Log integer as decimal
void log_raw(int value);

// Log double
void log_raw(double value);

// Log bool
void log_raw(bool value);

// Log array of bytes
struct bytearray {
    bytearray(const uint8_t *data, size_t len): data(data), len(len) {}
    const uint8_t *data;
    size_t len;
};
void log_raw(bytearray array);

inline void log_raw()
{
    // End of template recursion
}

#ifdef __cplusplus
extern "C" {
#endif
void log_buf(const unsigned char *buf, unsigned long size);
// Log formatted string
void log_f(const char *format, ...);
#ifdef __cplusplus
}
#endif

// extern "C" unsigned long millis();

// Variadic template for printing multiple items
template<typename T, typename T2, typename... Rest>
inline void log_raw(T first, T2 second, Rest... rest)
{
    log_raw(first);
    log_raw(second);
    log_raw(rest...);
}

// Format a complete log message
template<typename... Params>
inline void log(Params... params)
{
    if (g_log_debug)
    {
        log_raw("[", (int)millis(), "ms] ");
    }
    log_raw(params...);
    log_raw("\n");
}

// Format a complete debug message
template<typename... Params>
inline void debuglog(Params... params)
{
    if (g_log_debug)
    {
        uint8_t log_for_id = (0x01 << (*SCSI_STS_SELECTED & 7)) & g_scsi_log_mask;
        if(log_for_id == 0)
        {
            return;
        }
        log_raw("[", (int)millis(), "ms] DBG ");
        log_raw(params...);
        log_raw("\n");
    }
}

#ifdef __cplusplus
extern "C" {
#endif
// Log long hex string
void logmsg_buf(const unsigned char *buf, unsigned long size);

// Log long hex string
void dbgmsg_buf(const unsigned char *buf, unsigned long size);

// Log formatted string
void logmsg_f(const char *format, ...);

// Log formatted string
void dbgmsg_f(const char *format, ...);


#ifdef __cplusplus
}
#endif