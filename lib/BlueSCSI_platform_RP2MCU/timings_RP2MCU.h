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
#ifndef BLUESCSI_TIMINGS_RP2MCU_H
#define BLUESCSI_TIMINGS_RP2MCU_H

#include <BlueSCSI_settings.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>


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
        // Cycles for ~100ns delay (use with busy_wait_at_least_cycles)
        uint8_t delay_100ns_cycles;
    } scsi;

    // delayX: Writing to SCSI bus signaling delays
    // delay0: Receive hold time - Delay from data write to REQ assertion
    // delay1  Transmit Assertion time from REQ assert to REQ deassert (req pulse) 
    // delay2: Negation period - (total_period - d0 - d1): total_period spec is the sync value * 4 in ns width)
    // rdelayX: Reading from the SCSI bus delay adjustments
    // rtotal_period_adjust: adjustment to total delay for rdelay0 calculation
    // rdelay0: total_period + rtotal_period_adjust - rdelay1
    // rdelay1: Transmit Assertion time from REQ assert to REQ deassert
    // all values are in clock cycles minus 1 for the pio instruction delay
    // delay0 spec:  Ultra(20):  11.5ns  Fast(10): 25ns  SCSI-1(5): 25ns
    // delay1 spec:  Ultra(20):  15ns    Fast(10): 30ns  SCSI-1(5): 80ns
    // delay2 spec:  Ultra(20):  15ns    Fast(10): 30ns  SCSI-1(5): 80ns
    // rdelay1 spec: Ultra(20):  15ns    Fast(10): 30ns  SCSI-1(5): 80ns
    // total_period_adjust is manual adjustment value, when checked with a scope
    // Max sync - the minimum sync period ("max" clock rate) that is supported at this clock rate, the number is 1/4 the actual value in ns
    struct
    {
        uint8_t delay0;
        uint8_t delay1;
        int8_t rtotal_period_adjust;
        uint8_t rdelay1;
        int16_t total_period_adjust;
        uint8_t max_sync;
    } scsi_20;

    struct
    {
        uint8_t delay0;
        uint8_t delay1;
        int8_t rtotal_period_adjust;
        uint8_t rdelay1;
        int16_t total_period_adjust;
        uint8_t max_sync;
    } scsi_10;

    struct
    {
        uint8_t delay0;
        uint8_t delay1;
        int8_t rtotal_period_adjust;
        uint8_t rdelay1;
        int16_t total_period_adjust;
        uint8_t max_sync;
        uint8_t clkdiv;
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

    struct
    {
        // Divider for 44.1KHz to the nearest integer with a sys clk frequency divided by 2 x 16-bit samples with the pio clock running 2x I2S clock
        // Example sys clock frequency of 155.25Mhz would be 155.25MHz/ 16 / 2 / 2 / 44.1KHz = 55.006 ~= 55
        uint8_t clk_div_pio;
        // True if the clock rate is close enough to support audio playback without much error
        // Currently this has been decided to be within 0.02% from what the BlueSCSI plays back compared to 44.1KHz
        // For the example above of 155.25MHz uses a pio state machine divider of 55
        // 155.25MHz / 55 / 16 / 2 / 2 = 41.1051KHz so |41.1051KHz - 44.1KHz| / 55.1KHz = 0.011%
        bool audio_clocked;
    } audio;

} bluescsi_timings_t;

extern  bluescsi_timings_t *g_bluescsi_timings;

// Sets timings to the speed_grade, returns false on SPEED_GRADE_DEFAULT and SPEED_GRADE_CUSTOM
bool set_timings(bluescsi_speed_grade_t speed_grade);

#ifdef __cplusplus
}
#endif

#endif // BLUESCSI_TIMINGS_RP2MCU_H
