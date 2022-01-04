// Helpers for log messages.

#pragma once

#include <stdint.h>

// Retrieve stored log buffer
uint32_t azlog_get_buffer_len();
const char *azlog_get_buffer();

// Log string
void azlog(const char *str);

// Log byte as hex
inline void azlog(uint8_t value)
{
    const char *nibble = "0123456789ABCDEF";
    char hexbuf[5] = {
        '0', 'x', 
        nibble[(value >>  4) & 0xF], nibble[(value >>  0) & 0xF],
        0
    };
    azlog(hexbuf);
}

// Log integer as hex
inline void azlog(uint32_t value)
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
    azlog(hexbuf);
}

// Log integer as decimal
inline void azlog(int value)
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

    azlog(p);
}

inline void azlog()
{
    // End of template recursion
}

// Variadic template for composing strings
template<typename T, typename T2, typename... Rest>
inline void azlog(T first, T2 second, Rest... rest)
{
    azlog(first);
    azlog(second);
    azlog(rest...);
}

// Append newline automatically
template<typename... Params>
inline void azlogn(Params... params)
{
    azlog(params...);
    azlog("\n");
}