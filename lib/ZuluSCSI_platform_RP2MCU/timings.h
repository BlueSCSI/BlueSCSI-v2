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
#include <stdbool.h>
typedef enum
{
    ZULUSCSI_PIO_TARGET_MODE_SIMPLE,
    ZULUSCSI_PIO_TARGET_MODE_EXTRA_DELAY,
} zuluscsi_pio_target_mode_t;

typedef struct
{
    uint32_t clk_hz;
    struct
    {
        uint8_t refdiv;
        uint32_t vco_freq;
        uint8_t post_div1;
        uint8_t post_div2;
    } pll;
    struct
    {
        // Delay from data setup to REQ assertion.
        // deskew delay + cable skew delay = 55 ns minimum
        // One clock cycle is x ns => delay (55 / x) clocks
        uint8_t req_delay;
        // Period of the system clock in pico seconds
        uint32_t clk_period_ps;
    } scsi;


    // delay0: Delay from data write to REQ assertion (data setup)
    // delay1: Delay from REQ assert to REQ deassert (req pulse width)
    // both values are in clock cycles minus 1 for the pio instruction delay
    // total_delay_adjust is manual adjustment value, when checked with a scope
    // Max sync - the max sync period that is supported at this clock rate, the number is 1/4 the actual value in ns
    struct
    {
        zuluscsi_pio_target_mode_t mode;
        uint8_t delay0;
        uint8_t delay1;
        int16_t total_delay_adjust;
        uint8_t max_sync;
    } scsi_20;

    struct
    {
        zuluscsi_pio_target_mode_t mode;
        uint8_t delay0;
        uint8_t delay1;
        int16_t total_delay_adjust;
        uint8_t max_sync;
    } scsi_10;

    struct
    {
        zuluscsi_pio_target_mode_t mode;
        uint8_t delay0;
        uint8_t delay1;
        int16_t total_delay_adjust;
        uint8_t max_sync;
    } scsi_5;


    struct
    {
        // System clock speed in MHz clk / clk_div_pio
        uint8_t clk_div_1mhz;

        // System clock speed / clk_div_pio <= 50MHz
        // At 125Hz, the closest dividers 5 is used for 25 MHz for
        // stability at that clock speed
        // The CPU can apply further divider through state machine
        // registers for the initial handshake.
        uint8_t clk_div_pio;
        // clk_div_pio = (delay0 + 1) + (delay1 + 1)
        // delay1 should be shorter than delay0
        uint8_t delay0; // subtract one for the instruction delay
        uint8_t delay1; // clk_div_pio - delay0 and subtract one for the instruction delay
    } sdio;

} zuluscsi_timings_t;

extern  zuluscsi_timings_t g_zuluscsi_timings;

bool set_timings(uint32_t target_clk_in_khz);
#endif // ZULUSCSI_RP2MCU_TIMINGS_H
