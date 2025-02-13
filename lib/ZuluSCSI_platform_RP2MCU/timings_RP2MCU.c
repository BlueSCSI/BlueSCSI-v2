/**
 * ZuluSCSI™ - Copyright (c) 2024 Rabbit Hole Computing™
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


static zuluscsi_timings_t  predefined_timings[]  = {
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
            .delay0 = 4,
            .delay1 = 6,
            .total_delay_adjust = -1,
            .max_sync = 25,

        },

        .scsi_10 =
        {
            .delay0 = 4,
            .delay1 = 6,
            .total_delay_adjust = -1,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 10 - 1,
            .delay1 = 15 - 1,
            .total_delay_adjust = -1,
            .max_sync = 50,
        },

        .sdio =
        {
            .clk_div_1mhz = 25, // = 125MHz clk / clk_div_pio
            .clk_div_pio = 5,
            .delay0 = 3 - 1, // subtract one for the instruction delay
            .delay1 = 2 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        }
    },
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
            .delay0 = 4,
            .delay1 = 6,
            .total_delay_adjust = -1,
            .max_sync = 25,

        },

        .scsi_10 =
        {
            .delay0 = 4,
            .delay1 = 6,
            .total_delay_adjust = -1,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 10 - 1,
            .delay1 = 15 - 1,
            .total_delay_adjust = -1,
            .max_sync = 50,
        },

        .sdio =
        {
            .clk_div_1mhz = 25,
            .clk_div_pio = 5,
            .delay0 = 3 - 1, // subtract one for the instruction delay
            .delay1 = 2 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        }
    },
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
            .delay0 = 4,
            .delay1 = 6,
            .total_delay_adjust = -1,
            .max_sync = 25,

        },

        .scsi_10 =
        {
            .delay0 = 4,
            .delay1 = 6,
            .total_delay_adjust = -1,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 10 - 1,
            .delay1 = 15 - 1,
            .total_delay_adjust = -1,
            .max_sync = 50,
        },

        .sdio =
        {
            .clk_div_1mhz = 27 , // = 135MHz clk / clk_div_pio
            .clk_div_pio = 5,
            .delay0 = 3 - 1, // subtract one for the instruction delay
            .delay1 = 2 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        }
    },
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
            .delay0 = 3 - 1,
            .delay1 = 4 - 1,
            .total_delay_adjust = 0,
            .max_sync = 18,

        },

        .scsi_10 =
        {
            .delay0 = 4 - 1,
            .delay1 = 5 - 1,
            .total_delay_adjust = 0,
            .max_sync = 25,

        },

        .scsi_5 =
        {
            .delay0 = 10 - 1,
            .delay1 = 15, // should be 18 - 1 but max currently is 15
            .total_delay_adjust = 0,
            .max_sync = 50,

        },

        .sdio =
        {
            .clk_div_1mhz = 30, // = 150MHz clk / clk_div_pio
            .clk_div_pio = 5,
            .delay0 = 3 - 1, // subtract one for the instruction delay
            .delay1 = 2 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        }
    },
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
            .delay1 = 5 - 1,
            .total_delay_adjust = 1,
            .max_sync = 12,

        },

        .scsi_10 =
        {
            .delay0 = 6 - 1,
            .delay1 = 9 - 1,
            .total_delay_adjust = 1,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 15, // maxed out should be 16
            .delay1 = 15, // maxed out should be 30
            .total_delay_adjust = 1,
            .max_sync = 50,
        },
        .sdio =
        {
            .clk_div_1mhz = 30, // set by trail and error
            .clk_div_pio = 5, // SDIO at 50MHz
            .delay0 = 4 - 1, // subtract one for the instruction delay
            .delay1 = 1 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        }
    },
};
    zuluscsi_timings_t  current_timings;

#ifdef ENABLE_AUDIO_OUTPUT
    zuluscsi_timings_t *g_zuluscsi_timings = &predefined_timings[2];
#elif defined(ZULUSCSI_MCU_RP23XX)
    zuluscsi_timings_t *g_zuluscsi_timings = &predefined_timings[3];
#elif defined(ZULUSCSI_PICO)
    zuluscsi_timings_t *g_zuluscsi_timings = &predefined_timings[1];
#else
    zuluscsi_timings_t *g_zuluscsi_timings = &predefined_timings[0];
#endif


bool set_timings(zuluscsi_speed_grade_t speed_grade)
{
    uint8_t timings_index = 0;

    switch (speed_grade)
    {
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
        case SPEED_GRADE_AUDIO:
            timings_index = 2;
            break;
    }   
    if (speed_grade != SPEED_GRADE_DEFAULT && speed_grade != SPEED_GRADE_CUSTOM)
    {
        g_zuluscsi_timings = &current_timings;
        memcpy(g_zuluscsi_timings, &predefined_timings[timings_index], sizeof(current_timings));
        g_max_sync_10_period = g_zuluscsi_timings->scsi_10.max_sync;
        g_max_sync_20_period = g_zuluscsi_timings->scsi_20.max_sync;
        g_max_sync_5_period = g_zuluscsi_timings->scsi_5.max_sync;
        return true;
    }
    return false;
}