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
        .scsi = 
        {
            .delay0 = 0,
            .delay1 = 0,
            .req_delay = 0,

            .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE
        
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

zuluscsi_timings_t predefined_timings[] =
    {
        {
            .clk_hz = 125000000,
            .scsi = 
            {
                .delay0 = 0,
                .delay1 = 0,
                .req_delay = 0,
                .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE
            
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
            .scsi = 
            {
                .delay0 = 0,
                .delay1 = 0,
                .req_delay = 0,
                .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE
            
            },
            .sdio =
            {
                .clk_div_1mhz = 0,
                .clk_div_pio = 0,
                .delay0 = 0,
                .delay1 = 0
            }
        },
        {
            .clk_hz = 250000000,
            .scsi = 
            {
                .delay0 = 0,
                .delay1 = 0,
                .req_delay = 0,
                .mode = ZULUSCSI_PIO_TARGET_MODE_SIMPLE
            
            },
            .sdio =
            {
                .clk_div_1mhz = 0,
                .clk_div_pio = 0,
                .delay0 = 0,
                .delay1 = 0
            }
        }
    };

void set_timings(uint32_t system_clk)
{
;
}
