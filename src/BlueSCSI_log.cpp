#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include "BlueSCSI_platform.h"

const char *g_bluelog_firmwareversion = BLUESCSI_FW_VERSION " " __DATE__ " " __TIME__;
bool g_bluelog_debug = true;

// This memory buffer can be read by debugger and is also saved to log.txt
#define LOGBUFMASK (LOGBUFSIZE - 1)

// The log buffer is in special uninitialized RAM section so that it is not reset
// when soft rebooting or jumping from bootloader.
uint32_t g_log_magic;
char g_logbuffer[LOGBUFSIZE + 1];
uint32_t g_logpos;

void bluelog_raw(const char *str)
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
void bluelog_raw(uint8_t value)
{
    const char *nibble = "0123456789ABCDEF";
    char hexbuf[5] = {
        '0', 'x',
        nibble[(value >>  4) & 0xF], nibble[(value >>  0) & 0xF],
        0
    };
    bluelog_raw(hexbuf);
}

// Log integer as hex
void bluelog_raw(uint32_t value)
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
    bluelog_raw(hexbuf);
}

// Log integer as hex
void bluelog_raw(uint64_t value)
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
    bluelog_raw(hexbuf);
}

// Log integer as decimal
void bluelog_raw(int value)
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

    bluelog_raw(p);
}

void bluelog_raw(bytearray array)
{
    for (size_t i = 0; i < array.len; i++)
    {
        bluelog_raw(array.data[i]);
        bluelog_raw(" ");
        if (i > 32)
        {
            bluelog_raw("... (total ", (int)array.len, ")");
            break;
        }
    }
}

uint32_t bluelog_get_buffer_len()
{
    return g_logpos;
}

const char *bluelog_get_buffer(uint32_t *startpos)
{
    uint32_t default_pos = 0;
    if (startpos == NULL)
    {
        startpos = &default_pos;
    }

    // Check oldest data available in buffer
    uint32_t margin = 16;
    if (g_logpos + margin > LOGBUFSIZE)
    {
        uint32_t oldest = g_logpos + margin - LOGBUFSIZE;
        if (*startpos < oldest)
        {
            *startpos = oldest;
        }
    }

    const char *result = &g_logbuffer[*startpos & LOGBUFMASK];
    if ((g_logpos & LOGBUFMASK) >= (*startpos & LOGBUFMASK))
    {
        // Ok, everything has been read now
        *startpos = g_logpos;
    }
    else
    {
        // Buffer wraps, read to end of buffer now and start from beginning on next call.
        *startpos = g_logpos & (~LOGBUFMASK);
    }

    return result;
}

