// Timing functions for SCSI2SD.
// This file is derived from time.h in SCSI2SD-V6.

#pragma once

#include <stdint.h>
#include "ZuluSCSI_platform.h"

#define s2s_getTime_ms() millis()
#define s2s_elapsedTime_ms(since) ((uint32_t)(millis() - (since)))
#define s2s_delay_ms(x) delay_ns(x * 1000000)
#define s2s_delay_us(x) delay_ns(x * 1000)
#define s2s_delay_ns(x) delay_ns(x)
