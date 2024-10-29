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
#ifndef ZULUSCSI_RP2MCU_TIMINGS_H
#define ZULUSCSI_RP2MCU_TIMINGS_H
#include <stdint.h>

typedef enum
{
    ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
    ZULUSCSI_PIO_TARGET_MODE_EXTRA_DELAY1,
    ZULUSCSI_PIO_TARGET_MODE_EXTRA_DELAY2,
    ZULUSCSI_PIO_TARGET_MODE_EXTRA_DELAY_1AND2
} zuluscsi_pio_target_mode_t;

typedef struct
{
    uint32_t clk_hz;
    struct
    {
        uint8_t req_delay;
        zuluscsi_pio_target_mode_t mode;
        uint8_t delay0;
        uint8_t delay1;
    } scsi;

    struct 
    {
        uint8_t clk_div_1mhz;
        uint8_t clk_div_pio;
        uint8_t delay0;
        uint8_t delay1;
    } sdio;

} zuluscsi_timings_t;

extern  zuluscsi_timings_t g_zuluscsi_timings;
void set_timings(uint32_t system_clk);
#endif // ZULUSCSI_RP2MCU_TIMINGS_H
