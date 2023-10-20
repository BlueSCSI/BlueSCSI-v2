/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
 * Copyright (c) 2023 joshua stein <jcs@jcs.org>
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
bool g_log_debug = true;

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

void logmsg_f(const char *format, ...)
{
    static char out[2048];

    va_list ap;
    va_start(ap, format);
    vsnprintf(out, sizeof(out), format, ap);
    va_end(ap);

    logmsg(out);
}

void logmsg_buf(const unsigned char *buf, unsigned long size)
{
    static char tmp[1500 * 3];
    static char hex[] = "0123456789abcdef";
    int o = 0;

    for (int j = 0; j < size; j++) {
        if (o + 3 >= sizeof(tmp))
            break;

        if (j != 0)
            tmp[o++] = ' ';
        tmp[o++] = hex[(buf[j] >> 4) & 0xf];
        tmp[o++] = hex[buf[j] & 0xf];
        tmp[o] = 0;
    }

    logmsg_f("%s", tmp);
}

