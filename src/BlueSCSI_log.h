/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * Copyright (c) 2023 joshua stein <jcs@jcs.org>
 * Copyright (c) 2024 Eric Helgeson <erichelgeson@gmail.com>
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 * 
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// Helpers for log messages.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <scsiPhy.h>

// Get total number of bytes that have been written to log
uint32_t log_get_buffer_len();

// Get log as a string.
// If startpos is given, continues log reading from previous position and updates the position.
// If available is given, number of bytes available is written there.
const char *log_get_buffer(uint32_t *startpos, uint32_t *available = nullptr);

// Whether to enable debug messages
extern "C" bool g_log_debug;
extern "C" bool g_log_ignore_busy_free;
extern "C" uint8_t g_scsi_log_mask;

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

// Log integer as decimal
void log_raw(double value);

// Log boolean
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

// These functions contain the common prefix and end of each
// log message, and check if debug messages are enabled.
// Having them in separate functions avoids inlining the code
// into every call.
void logmsg_start();
void logmsg_end();
bool dbgmsg_start();
void dbgmsg_end();

extern "C" unsigned long millis();

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
inline void logmsg(Params... params)
{
    if (g_log_debug)
        log_raw("[", (int)millis(), "ms] ");
    log_raw(params...);
    logmsg_end();
}

// Format a complete debug message
template<typename... Params>
inline void dbgmsg(Params... params)
{
    if (g_log_debug && dbgmsg_start())
    {
        log_raw(params...);
        dbgmsg_end();
    }
}

#ifdef NETWORK_DEBUG_LOGGING
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
#endif
