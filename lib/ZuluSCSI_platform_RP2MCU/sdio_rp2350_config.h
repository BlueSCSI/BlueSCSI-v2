/* Configuration for RP2350_SDIO library */

#pragma once

#include "ZuluSCSI_platform.h"
#include <ZuluSCSI_log.h>

// #define SDIO_BREAKPOINT_ON_ERROR
// #define ZULUSCSI_DEBUG_SDIO

// SDIO error messages are logged only to debug log, because normally
// the problem can be reported through SCSI status.
#ifdef SDIO_BREAKPOINT_ON_ERROR
#define SDIO_ERRMSG(txt, arg1, arg2) do{dbgmsg(txt, " ", (uint32_t)(arg1), " ", (uint32_t)(arg2)); asm("bkpt");} while(0)
#else
#define SDIO_ERRMSG(txt, arg1, arg2) dbgmsg(txt, " ", (uint32_t)(arg1), " ", (uint32_t)(arg2))
#endif

// SDIO debug messages are normally disabled because they are very verbose
#ifdef ZULUSCSI_DEBUG_SDIO
#define SDIO_DBGMSG(txt, arg1, arg2) dbgmsg(txt, " ", (uint32_t)(arg1), " ", (uint32_t)(arg2))
#endif

// PIO block to use
#define SDIO_PIO pio1
#define SDIO_SM  0

// GPIO configuration
#define SDIO_GPIO_FUNC GPIO_FUNC_PIO1
#define SDIO_GPIO_SLEW GPIO_SLEW_RATE_FAST
#define SDIO_GPIO_DRIVE GPIO_DRIVE_STRENGTH_8MA

// DMA channels to use
#define SDIO_DMACH_A 4
#define SDIO_DMACH_B 5
#define SDIO_DMAIRQ_IDX 1
#define SDIO_DMAIRQ DMA_IRQ_1

#define SDIO_DEFAULT_SPEED SDIO_HIGHSPEED_OVERCLOCK
#define SDIO_MAX_CLOCK_RATE_EXCEED_PERCENT 15

// GPIO pins come from platform header
// #define SDIO_CLK 34
// #define SDIO_CMD 35
// #define SDIO_D0  36
// #define SDIO_D1  37
// #define SDIO_D2  38
// #define SDIO_D3  39
// #define SDIO_PIO_IOBASE 16
