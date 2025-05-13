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


#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
#include "ZuluSCSI_platform.h"
#include <stdio.h>
#include <stdarg.h>

const char *g_log_firmwareversion = ZULU_FW_VERSION " " __DATE__ " " __TIME__;

bool g_log_debug = false;
bool g_log_ignore_busy_free = false;
uint8_t g_scsi_log_mask = 0xFF;

// This memory buffer can be read by debugger and is also saved to zululog.txt
#define LOGBUFMASK (LOGBUFSIZE - 1)

// The log buffer is in special uninitialized RAM section so that it is not reset
// when soft rebooting or jumping from bootloader.
uint32_t g_log_magic;
char g_logbuffer[LOGBUFSIZE + 1];
uint32_t g_logpos;

void log_raw(const char *str)
{
    // Keep log from reboot / bootloader if magic matches expected value
    if (g_log_magic != 0xAA55AA55)
    {
        g_log_magic = 0xAA55AA55;
        g_logpos = 0;
    }

    const char *p = str;
    while (*p)
    {
        g_logbuffer[g_logpos & LOGBUFMASK] = *p++;
        g_logpos++;
    }

    // Keep buffer null-terminated
    g_logbuffer[g_logpos & LOGBUFMASK] = '\0';

    platform_log(str);
}

// Log byte as hex
void log_raw(uint8_t value)
{
    const char *nibble = "0123456789ABCDEF";
    char hexbuf[5] = {
        '0', 'x', 
        nibble[(value >>  4) & 0xF], nibble[(value >>  0) & 0xF],
        0
    };
    log_raw(hexbuf);
}

// Log integer as hex
void log_raw(uint32_t value)
{
    const char *nibble = "0123456789ABCDEF";
    char hexbuf[11] = {
        '0', 'x', 
        nibble[(value >> 28) & 0xF], nibble[(value >> 24) & 0xF],
        nibble[(value >> 20) & 0xF], nibble[(value >> 16) & 0xF],
        nibble[(value >> 12) & 0xF], nibble[(value >>  8) & 0xF],
        nibble[(value >>  4) & 0xF], nibble[(value >>  0) & 0xF],
        0
    };
    log_raw(hexbuf);
}

// Log integer as hex
void log_raw(uint64_t value)
{
    const char *nibble = "0123456789ABCDEF";
    char hexbuf[19] = {
        '0', 'x',
        nibble[(value >> 60) & 0xF], nibble[(value >> 56) & 0xF],
        nibble[(value >> 52) & 0xF], nibble[(value >> 48) & 0xF],
        nibble[(value >> 44) & 0xF], nibble[(value >> 40) & 0xF],
        nibble[(value >> 36) & 0xF], nibble[(value >> 32) & 0xF],
        nibble[(value >> 28) & 0xF], nibble[(value >> 24) & 0xF],
        nibble[(value >> 20) & 0xF], nibble[(value >> 16) & 0xF],
        nibble[(value >> 12) & 0xF], nibble[(value >>  8) & 0xF],
        nibble[(value >>  4) & 0xF], nibble[(value >>  0) & 0xF],
        0
    };
    log_raw(hexbuf);
}

// Log integer as decimal
void log_raw(int value)
{
    char decbuf[16] = {0};
    char *p = &decbuf[14];
    int remainder = (value < 0) ? -value : value;
    do
    {
        *--p = '0' + (remainder % 10);
        remainder /= 10;
    } while (remainder > 0);
    
    if (value < 0)
    {
        *--p = '-';
    }

    log_raw(p);
}

void log_raw(bytearray array)
{
    for (size_t i = 0; i < array.len; i++)
    {
        log_raw(array.data[i]);
        log_raw(" ");
        if (i > 32)
        {
            log_raw("... (total ", (int)array.len, ")");
            break;
        }
    }
}

void logmsg_start()
{
    log_raw("[", (int)millis(), "ms] ");
}

void logmsg_end()
{
    log_raw("\r\n");
}

bool dbgmsg_start()
{
    if (g_log_debug)
    {
        // Check if log mask is not the default value, the selection was a success, and the selected ID was not match, then skip logging
        if ( g_scsi_log_mask != 0xFF
            && (SCSI_STS_SELECTION_SUCCEEDED & *SCSI_STS_SELECTED)
            && (0 == (g_scsi_log_mask & (1 << (*SCSI_STS_SELECTED & 7))))
           )
        {
            return false;
        }

        log_raw("[", (int)millis(), "ms] DBG ");

        return true;
    }

    return false;
}

void dbgmsg_end()
{
    log_raw("\r\n");
}

uint32_t log_get_buffer_len()
{
    return g_logpos;
}

const char *log_get_buffer(uint32_t *startpos, uint32_t *available)
{
    uint32_t default_pos = 0;
    if (startpos == NULL)
    {
        startpos = &default_pos;
    }

    // Check oldest data available in buffer
    uint32_t lag = (g_logpos - *startpos);
    if (lag >= LOGBUFSIZE)
    {
        // If we lose data, skip 512 bytes forward to give us time to transmit
        // pending data before new log messages arrive. Also skip to next line
        // break to keep formatting consistent.
        uint32_t oldest = g_logpos - LOGBUFSIZE + 512;
        while (oldest < g_logpos)
        {
            char c = g_logbuffer[oldest & LOGBUFMASK];
            if (c == '\r' || c == '\n') break;
            oldest++;
        }

        if (oldest > g_logpos)
        {
            oldest = g_logpos;
        }

        *startpos = oldest;
    }

    const char *result = &g_logbuffer[*startpos & LOGBUFMASK];

    // Calculate number of bytes available
    uint32_t len;
    if ((g_logpos & LOGBUFMASK) >= (*startpos & LOGBUFMASK))
    {
        // Can read directly to g_logpos
        len = g_logpos - *startpos;
    }
    else
    {
        // Buffer wraps, read to end of buffer now and start from beginning on next call.
        len = LOGBUFSIZE - (*startpos & LOGBUFMASK);
    }

    if (available) { *available = len; }
    *startpos += len;

    return result;
}


#ifdef NETWORK_DEBUG_LOGGING
// TODO write directly global log buffer to save some memory
static char shared_log_buf[1500 * 3];

// core method for variadic printf like logging
static void log_va(bool debug, const char *format, va_list ap)
{
    vsnprintf(shared_log_buf, sizeof(shared_log_buf), format, ap);
    if (debug)
    {
        dbgmsg(shared_log_buf);
    }
    else
    {
        logmsg(shared_log_buf);
    }
}

void logmsg_f(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    log_va(false, format, ap);
    va_end(ap);
}

void dbgmsg_f(const char *format, ...)
{
    if (!g_log_debug)
        return;
    va_list ap;
    va_start(ap, format);
    log_va(true, format, ap);
    va_end(ap);
}

// core method for logging a data buffer into a hex string
void log_hex_buf(const unsigned char *buf, unsigned long size, bool debug)
{
    static char hex[] = "0123456789abcdef";
    int o = 0;

    for (int j = 0; j < size; j++) {
        if (o + 3 >= sizeof(shared_log_buf))
            break;

        if (j != 0)
            shared_log_buf[o++] = ' ';
        shared_log_buf[o++] = hex[(buf[j] >> 4) & 0xf];
        shared_log_buf[o++] = hex[buf[j] & 0xf];
        shared_log_buf[o] = 0;
    }
    if (debug)
        dbgmsg(shared_log_buf);
    else
        logmsg(shared_log_buf);
}

void logmsg_buf(const unsigned char *buf, unsigned long size)
{
    log_hex_buf(buf, size, false);
}

void dbgmsg_buf(const unsigned char *buf, unsigned long size)
{
    if (!g_log_debug)
        return;
    log_hex_buf(buf, size, true);
}
#endif // NETWORK_DEBUG_LOGGING

