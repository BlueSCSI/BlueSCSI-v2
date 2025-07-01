/**
 * ZuluSCSI™ - Copyright (c) 2024-2025 Rabbit Hole Computing™
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
#include "timings_RP2MCU.h"
#include <string.h>
#include "timings.h"
#include <hardware/vreg.h>


static bluescsi_timings_t  predefined_timings[]  = {
    // predefined_timings[0] - 125000000
    {
        .clk_hz = 125000000,

        .pll =
        {
            .refdiv = 1,
            .vco_freq = 1500000000,
            .post_div1 = 6,
            .post_div2 = 2
        },

        .scsi =
        {
            .req_delay = 7,
            .clk_period_ps = 8000
        },

        .scsi_20 =
        {
            .delay0 = 3 - 1,
            .delay1 = 4 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 5 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 12,
        },

        .scsi_10 =
        {
            .delay0 = 5 - 1,
            .delay1 = 7 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 6,
            .rtotal_period_adjust = 0,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 10 - 1,
            .delay1 = 15 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 15 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 50,
        },

        .sdio =
        {
            .clk_div_1mhz = 25, // = 125MHz clk / clk_div_pio
            .clk_div_pio = 4,
            .delay0 = 3 - 1, // subtract one for the instruction delay
            .delay1 = 1 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },

        .audio =
        {
            .clk_div_pio = 48,
            .audio_clocked = false,
        }
    },
    // predefined_timings[1] - 133000000
    {
        .clk_hz = 133000000,

        .pll =
        {
            .refdiv = 1,
            .vco_freq = 1596000000,
            .post_div1 = 6,
            .post_div2 = 2
        },

        .scsi =
        {
            .req_delay = 7,
            .clk_period_ps = 7519
        },

        .scsi_20 =
        {
            .delay0 = 3 - 1,
            .delay1 = 4 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 5 - 1,
            .rtotal_period_adjust = 1,
            .max_sync = 12,
        },

        .scsi_10 =
        {
            .delay0 = 5 - 1,
            .delay1 = 7 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 7 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 10 - 1,
            .delay1 = 15 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 15 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 50,
        },

        .sdio =
        {
            .clk_div_1mhz = 25,
            .clk_div_pio = 5,
            .delay0 = 3 - 1, // subtract one for the instruction delay
            .delay1 = 2 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },

        .audio =
        {
            .clk_div_pio = 48,
            .audio_clocked = false,
        }
    },
    // predefined_timings[2] - 135428571 - RP2040 Audio DAC Attack S/PDIF clocks
    {
        .clk_hz = 135428571,

        .pll =
        {
            .refdiv = 1,
            .vco_freq = 948000000,
            .post_div1 = 7,
            .post_div2 = 1
        },

        .scsi =
        {
            .req_delay = 7,
            .clk_period_ps = 7384
        },

        .scsi_20 =
        {
            .delay0 = 3 - 1,
            .delay1 = 4 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 5 - 1,
            .rtotal_period_adjust = 1,
            .max_sync = 12,
        },

        .scsi_10 =
        {
            .delay0 = 5 - 1,
            .delay1 = 7 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 7 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 10 - 1,
            .delay1 = 15 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 15 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 50,
        },

        .sdio =
        {
            .clk_div_1mhz = 27 , // = 135MHz clk / clk_div_pio
            .clk_div_pio = 5,
            .delay0 = 3 - 1, // subtract one for the instruction delay
            .delay1 = 2 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },

        .audio =
        {
            // 44.1KHz to the nearest integer with a sys clk of 135.43MHz and 2 x 16-bit samples with the pio clock running 2x I2S clock
            // 135.43Mhz / 16 / 2 / 2 / 44.1KHz = 47.98 ~= 48
            .clk_div_pio = 48,
            .audio_clocked = true,
        }

    },
    // predefined_timings[3] - 150000000
    {
        .clk_hz = 150000000,

        .pll =
        {
            .refdiv = 1,
            .vco_freq = 1500000000,
            .post_div1 = 5,
            .post_div2 = 2
        },

        .scsi =
        {
            .req_delay = 9,
            .clk_period_ps = 6667
        },

        .scsi_20 =
        {
            .delay0 = 2 - 1,
            .delay1 = 4 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 5 - 1,
            .rtotal_period_adjust = 1,
            .max_sync = 12,

        },

        .scsi_10 =
        {
            .delay0 = 4 - 1,
            .delay1 = 8 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 8 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 25,

        },

        .scsi_5 =
        {
            .delay0 = 5 - 1,
            .delay1 = 8 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 8 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 50,
            .clkdiv = 2,
        },

        .sdio =
        {
            .clk_div_1mhz = 30,// = 150MHz clk / clk_div_pio
            .clk_div_pio = 5,
            .delay0 = 3 - 1, // subtract one for the instruction delay
            .delay1 = 2 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },

        .audio =
        {
            .clk_div_pio = 54,
            .audio_clocked = false,
        }
    },
    // predefined_timings[4] - 250000000
    {
        .clk_hz = 250000000,

        .pll =
        {
            .refdiv = 1,
            .vco_freq = 1500000000,
            .post_div1 = 6,
            .post_div2 = 1
        },

        .scsi =
        {
            .req_delay = 14,
            .clk_period_ps = 4000,
        },

        .scsi_20 =
        {
            .delay0 = 3 - 1,
            .delay1 = 7 - 1,
            .total_period_adjust = 1,
            .rdelay1 = 7 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 12,

        },

        .scsi_10 =
        {
            .delay0 = 6 - 1,
            .delay1 = 12 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 12 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 8 - 1,
            .delay1 = 12 - 1,
            .total_period_adjust = 1,
            .rdelay1 = 12 - 1,
            .rtotal_period_adjust = 1,
            .max_sync = 50,
            .clkdiv = 2,
        },
        .sdio =
        {
            .clk_div_1mhz = 30,// set by trail and error
            .clk_div_pio = 5, // SDIO at 50MHz
            .delay0 = 4 - 1, // subtract one for the instruction delay
            .delay1 = 1 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },
        .audio =
        {
            // Divider for 44.1KHz to the nearest integer with a sys clk divided by 2 x 16-bit samples with the pio clock running 2x I2S clock
            // 200.4Mhz / 16 / 2 / 2 / 44.1KHz = 71.003 ~= 71
            .clk_div_pio = 89,
            .audio_clocked = false,
        }
    },
    // predefined_timings[5] - 155250000 - Default clocks for Blaster I2S Audio
    {
        .clk_hz = 155250000,

        .pll =
        {
            .refdiv = 3,
            .vco_freq = 1242000000,
            .post_div1 = 4,
            .post_div2 = 2,
        },

        .scsi =
        {
            .req_delay = 10,
            .clk_period_ps = 6441
        },

        .scsi_20 =
        {
            .delay0 = 2 - 1,
            .delay1 = 4 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 5 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 12,

        },

        .scsi_10 =
        {
            .delay0 = 4 - 1,
            .delay1 = 7 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 7 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 25,

        },

        .scsi_5 =
        {
            .delay0 = 5 - 1,
            .delay1 = 8 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 8 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 50,
            .clkdiv = 2,
        },

        .sdio =
        {
            .clk_div_1mhz = 26, // = 155.25MHz clk / clk_div_pio
            .clk_div_pio = 6,
            .delay0 = 4 - 1, // subtract one for the instruction delay
            .delay1 = 2 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },

        .audio =
        {
            // 44.1KHz to the nearest integer with a sys clk of 155.25Mhz and 2 x 16-bit samples with the pio clock running 2x I2S clock
            // 155.25Mhz / 16 / 2 / 2 / 44.1KHz = 55.01 ~= 55
            .clk_div_pio = 55,
            .audio_clocked = true,
        }
    },
    // predefined_timings[6] - 175000000 - Alternate clocking for I2S Audio
    {
        // predefined_timings[6] - Clocking for I2S Audio at 175MHz system clock
        .clk_hz = 175000000,

        .pll =
        {
            .refdiv = 2,
            .vco_freq = 1050000000,
            .post_div1 = 6,
            .post_div2 = 1,
        },

        .scsi =
        {
            .req_delay = 10,
            .clk_period_ps = 5714
        },

        .scsi_20 =
        {
            .delay0 = 3 - 1,
            .delay1 = 5 - 1,
            .total_period_adjust = -1,
            .rdelay1 = 5 - 1,
            .rtotal_period_adjust = 1,
            .max_sync = 12,
        },

        .scsi_10 =
        {
            .delay0 = 4 - 1,
            .delay1 = 8 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 8 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 4 - 1,
            .delay1 = 8 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 8 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 50,
            .clkdiv = 2,
        },

        .sdio =
        {
            .clk_div_1mhz = 30,
            .clk_div_pio = 5,
            .delay0 = 4 - 1, // subtract one for the instruction delay
            .delay1 = 1 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },

        .audio =
        {
            // Divider for 44.1KHz to the nearest integer with a sys clk divided by 2 x 16-bit samples with the pio clock running 2x I2S clock
            // 175Mhz / 16 / 2 / 2 / 44.1KHz ~= 62
            .clk_div_pio = 62,
            .audio_clocked = true,
        }
    },
    // predefined_timings[7] - 200400000 - Alternate clocking for I2S Audio
    {
        .clk_hz = 200400000,

        .pll =
        {
            .refdiv = 2,
            .vco_freq = 1002000000,
            .post_div1 = 5,
            .post_div2 = 1,
        },

        .scsi =
        {
            .req_delay = 12,
            .clk_period_ps = 4990
        },

        .scsi_20 =
        {
            .delay0 = 2 - 1,
            .delay1 = 5 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 7 - 1,
            .rtotal_period_adjust = 1,
            .max_sync = 12,

        },

        .scsi_10 =
        {
            .delay0 = 5 - 1,
            .delay1 = 9 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 9 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 25,

        },

        .scsi_5 =
        {
            .delay0 = 5 - 1,
            .delay1 = 10 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 10 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 50,
            .clkdiv = 2,
        },

        .sdio =
        {
            .clk_div_1mhz = 30,
            .clk_div_pio = 5,
            .delay0 = 4 - 1, // subtract one for the instruction delay
            .delay1 = 1 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },

        .audio =
        {
            // Divider for 44.1KHz to the nearest integer with a sys clk divided by 2 x 16-bit samples with the pio clock running 2x I2S clock
            // 200.4Mhz / 16 / 2 / 2 / 44.1KHz = 71.003 ~= 71
            .clk_div_pio = 71,
            .audio_clocked = true,
        }
    },
    // predefined_timings[8] - 251200000 - Alternate clocking for I2S Audio
    {
        .clk_hz = 251200000,

        .pll =
        {
            .refdiv = 3,
            .vco_freq = 1256000000,
            .post_div1 = 5,
            .post_div2 = 1
        },

        .scsi =
        {
            .req_delay = 14,
            .clk_period_ps = 3981,
        },

        .scsi_20 =
        {
            .delay0 = 3 - 1,
            .delay1 = 7 - 1,
            .total_period_adjust = 1,
            .rdelay1 = 7 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 12,

        },

        .scsi_10 =
        {
            .delay0 = 6 - 1,
            .delay1 = 12 - 1,
            .total_period_adjust = 0,
            .rdelay1 = 12 - 1,
            .rtotal_period_adjust = 0,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 8 - 1,
            .delay1 = 12 - 1,
            .total_period_adjust = 1,
            .rdelay1 = 12 -1,
            .rtotal_period_adjust = 1,
            .max_sync = 50,
            .clkdiv = 2,
        },
        .sdio =
        {
            .clk_div_1mhz = 30,// set by trail and error
            .clk_div_pio = 5, // SDIO at 50MHz
            .delay0 = 4 - 1, // subtract one for the instruction delay
            .delay1 = 1 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        },
        .audio =
        {
            // Divider for 44.1KHz to the nearest integer with a sys clk divided by 2 x 16-bit samples with the pio clock running 2x I2S clock
            // 200.4Mhz / 16 / 2 / 2 / 44.1KHz = 71.003 ~= 71
            .clk_div_pio = 89,
            .audio_clocked = true,
        }
    },
    
};

#ifdef ENABLE_AUDIO_OUTPUT_SPDIF
bluescsi_timings_t *g_bluescsi_timings = &predefined_timings[2];
#elif defined(ENABLE_AUDIO_OUTPUT_I2S)
bluescsi_timings_t *g_bluescsi_timings = &predefined_timings[7];
#elif defined(BLUESCSI_MCU_RP23XX)
bluescsi_timings_t *g_bluescsi_timings = &predefined_timings[3];
#elif defined(BLUESCSI_PICO)
bluescsi_timings_t *g_bluescsi_timings = &predefined_timings[1];
#else
bluescsi_timings_t *g_bluescsi_timings = &predefined_timings[0];
#endif


bool set_timings(bluescsi_speed_grade_t speed_grade)
{
    uint8_t timings_index = 0;

    switch (speed_grade)
    {
#ifdef ENABLE_AUDIO_OUTPUT_I2S
    case SPEED_GRADE_MAX:
    case SPEED_GRADE_A:
        timings_index = 8;
        break;
    case SPEED_GRADE_B:
        timings_index = 6;
        break;
    case SPEED_GRADE_C:
        timings_index = 5;
        break;
    case SPEED_GRADE_AUDIO_SPDIF:
        timings_index = 2;
        break;
    case SPEED_GRADE_200MHZ:
    case SPEED_GRADE_AUDIO_I2S:
        timings_index = 7;
        break;
    case SPEED_GRADE_WIFI_RM2:
        timings_index = 5;
#elif defined(BLUESCSI_MCU_RP23XX)
    case SPEED_GRADE_MAX:
    case SPEED_GRADE_A:
        timings_index = 4;
        break;
    case SPEED_GRADE_B:
        timings_index = 7;
        break;
    case SPEED_GRADE_C:
        timings_index  = 6;
        break;
    case SPEED_GRADE_AUDIO_SPDIF:
        timings_index = 2;
        break;
    case SPEED_GRADE_AUDIO_I2S:
        timings_index = 5;
        break;
#else
case SPEED_GRADE_MAX:
    case SPEED_GRADE_A:
        timings_index = 4;
        break;
    case SPEED_GRADE_B:
        timings_index = 3;
        break;
    case SPEED_GRADE_C:
        timings_index  = 1;
        break;
    case SPEED_GRADE_AUDIO_SPDIF:
        timings_index = 2;
        break;
    case SPEED_GRADE_AUDIO_I2S:
        timings_index = 5;
        break;
    case SPEED_GRADE_200MHZ:
        timings_index = 7;
#endif
        default:
            break;
    }
    if (speed_grade != SPEED_GRADE_DEFAULT && speed_grade != SPEED_GRADE_CUSTOM)
    {
        memcpy(g_bluescsi_timings, &predefined_timings[timings_index], sizeof(*g_bluescsi_timings));
        g_max_sync_10_period = g_bluescsi_timings->scsi_10.max_sync;
        g_max_sync_20_period = g_bluescsi_timings->scsi_20.max_sync;
        g_max_sync_5_period = g_bluescsi_timings->scsi_5.max_sync;
        return true;
    }
    return false;
}
