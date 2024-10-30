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
#include "timings.h"
#include <string.h>
#include "scsi2sd_timings.h"

#ifdef ZULUSCSI_MCU_RP23XX
    zuluscsi_timings_t g_zuluscsi_timings =
    {
        .clk_hz = 150000000,
        .scsi =
        {
            .delay0 = 0,
            .delay1 = 0,
            .req_delay = 0,
            .gpio_ack = 0,
            .gpio_req = 0,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE

        },
        .sdio =
        {
            .clk_div_1mhz = 0,
            .clk_div_pio = 0,
            .delay0 = 0,
            .delay1 = 0
        }
    };
#else
    zuluscsi_timings_t g_zuluscsi_timings =
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
            .clk_period_ps = 5000
        },

        .scsi_20 =
        {
            .delay0 = 4,
            .delay1 = 6,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = -1,
            .max_sync = 25,

        },

        .scsi_10 =
        {
            .delay0 = 4,
            .delay1 = 6,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = -1,
            .max_sync = 25,

        },

        .scsi_5 =
        {
            .delay0 = 7,
            .delay1 = 14,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
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
    };
#endif

static zuluscsi_timings_t predefined_timings[] = {
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
            .clk_period_ps = 5000
        },

        .scsi_20 =
        {
            .delay0 = 4,
            .delay1 = 6,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = -1,
            .max_sync = 25,

        },

        .scsi_10 =
        {
            .delay0 = 4,
            .delay1 = 6,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = -1,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 7,
            .delay1 = 14,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
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
            .delay0 = 3,
            .delay1 = 4,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = 0,
            .max_sync = 18,

        },

        .scsi_10 =
        {
            .delay0 = 4,
            .delay1 = 6,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = 0,
            .max_sync = 25,

        },

        .scsi_5 =
        {
            .delay0 = 7,
            .delay1 = 14,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = 0,
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
            .delay0 = 2,
            .delay1 = 4,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = 1,
            .max_sync = 12,

        },

        .scsi_10 =
        {
            .delay0 = 8,
            .delay1 = 10,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = 1,
            .max_sync = 25,
        },

        .scsi_5 =
        {
            .delay0 = 14,
            .delay1 = 15,
            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
            .total_delay_adjust = 1,
            .max_sync = 50,
        },

        .sdio =
        {
            .clk_div_1mhz = 50, // = 250MHz clk / clk_div_pio
            .clk_div_pio = 5, // SDIO at 50MHz
            .delay0 = 4 - 1, // subtract one for the instruction delay
            .delay1 = 1 - 1  // clk_div_pio - delay0 and subtract one for the instruction delay
        }
    }
};

bool set_timings(uint32_t target_clk_in_khz)
{
    uint32_t number_of_timings = sizeof(predefined_timings)/sizeof( predefined_timings[0]);
    for (uint8_t i = 0; i < number_of_timings; i++)
    {
        if (target_clk_in_khz == predefined_timings[i].clk_hz / 1000)
        {
            memcpy(&g_zuluscsi_timings, &predefined_timings[i], sizeof(g_zuluscsi_timings));
            g_max_sync_10_period = g_zuluscsi_timings.scsi_10.max_sync;
            g_max_sync_20_period = g_zuluscsi_timings.scsi_20.max_sync;
            g_max_sync_5_period = g_zuluscsi_timings.scsi_5.max_sync;
            return true;
        }
    }
    return false;
}
