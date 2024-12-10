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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/
#ifndef BLUESCSI_TIMINGS_RP2MCU_H
#define BLUESCSI_TIMINGS_RP2MCU_H
#include <stdint.h>
#include <stdbool.h>
#include <BlueSCSI_config.h>

typedef struct
{
    uint32_t clk_hz;
    struct
    {
        // These numbers are for pico-sdk's pll_init() function
        // their values can be obtained using the script:
        // "/src/rp2_common/hardware_clocks/scripts/vcocalc.py" 
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


    // delay0: Data Setup Time - Delay from data write to REQ assertion
    // delay1  Transmit Assertion time from REQ assert to REQ deassert (req pulse) 
    // delay2: Negation period - (total_delay - d0 - d1): total_delay spec is the sync value * 4 in ns width)
    // both values are in clock cycles minus 1 for the pio instruction delay
    // delay0 spec: Ultra(20):  11.5ns  Fast(10): 23ns  SCSI-1(5): 23ns
    // delay1 spec: Ultra(20):  16.5ns  Fast(10): 33ns  SCSI-1(5): 53ns 
    // delay2 spec: Ultra(20):  15ns    Fast(10): 30ns  SCSI-1(5): 80ns 
    // total_delay_adjust is manual adjustment value, when checked with a scope
    // Max sync - the minimum sync period ("max" clock rate) that is supported at this clock rate, the number is 1/4 the actual value in ns
    struct
    {
        uint8_t delay0;
        uint8_t delay1;
        int16_t total_delay_adjust;
        uint8_t max_sync;
    } scsi_20;

    struct
    {
        uint8_t delay0;
        uint8_t delay1;
        int16_t total_delay_adjust;
        uint8_t max_sync;
    } scsi_10;

    struct
    {
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

} bluescsi_timings_t;

extern  bluescsi_timings_t *g_bluescsi_timings;

// Sets timings to the speed_grade, returns false on SPEED_GRADE_DEFAULT and SPEED_GRADE_CUSTOM
bool set_timings(bluescsi_speed_grade_t speed_grade);
#endif // BLUESCSI_TIMINGS_RP2MCU_H