/* The exact-width types SSH OS's sources expect, in place of its own
 * kernel/types.h (whose "long is 32 bits" holds on the 68000 only). */
#ifndef SSHOS_TYPES_H
#define SSHOS_TYPES_H
#include <stddef.h>
#include <stdint.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
#endif
