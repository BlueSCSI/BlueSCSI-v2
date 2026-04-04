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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#include "custom_timings.h"
#include <SdFat.h>
#include <minIni.h>
#include <BlueSCSI_log.h>
#include <BlueSCSI_platform.h>
extern SdFs SD;

extern "C"
{
    #include <timings.h>
}

static bool readIniIntRange(const char *section, const char *key, int32_t current,
    int32_t min_value, int32_t max_value, int32_t *out)
{
    int32_t value = ini_getl(section, key, current, CUSTOM_TIMINGS_FILE);
    if (value < min_value || value > max_value)
    {
        logmsg("Invalid value for [", section, "] ", key, " in ", CUSTOM_TIMINGS_FILE,
            ": ", (int)value, " (expected ", (int)min_value, "..", (int)max_value, ")");
        return false;
    }

    *out = value;
    return true;
}

bool CustomTimings::use_custom_timings()
{
    return SD.exists(CUSTOM_TIMINGS_FILE) && !ini_getbool("settings", "disable", 0, CUSTOM_TIMINGS_FILE);
}

bool CustomTimings::set_timings_from_file()
{
    const char settings_section[] = "settings";
    const char pll_section[] = "pll";
    const char scsi_section[] = "scsi";
    const char scsi_20_section[] = "scsi_20";
    const char scsi_10_section[] = "scsi_10";
    const char scsi_5_section[] = "scsi_5";
    const char sdio_section[] = "sdio";
    const char audio_section[] = "audio";

    char speed_grade_str[10];
    ini_gets(settings_section, "extends_speed_grade", "Default", speed_grade_str, sizeof(speed_grade_str), CUSTOM_TIMINGS_FILE);
    bluescsi_speed_grade_t speed_grade =  g_scsi_settings.stringToSpeedGrade(speed_grade_str, sizeof(speed_grade_str));
    set_timings(speed_grade);

    bluescsi_timings_t candidate = *g_bluescsi_timings;
    uint8_t force_sync = g_force_sync;
    uint8_t force_offset = g_force_offset;

    int32_t number_setting = 0;
    if (!readIniIntRange(settings_section, "boot_with_sync_value", 0, 0, 255, &number_setting))
    {
        return false;
    }

    if (number_setting > 0)
    {
        force_sync = number_setting;
        if (!readIniIntRange(settings_section, "boot_with_offset_value", 15, 0, 15, &number_setting))
        {
            return false;
        }
        force_offset = number_setting;
    }
    if (!readIniIntRange(settings_section, "clk_hz", candidate.clk_hz, 1, 252000000, &number_setting))
    {
        return false;
    }
    candidate.clk_hz = number_setting;
    if (!readIniIntRange(pll_section, "refdiv", candidate.pll.refdiv, 1, 63, &number_setting))
    {
        return false;
    }
    candidate.pll.refdiv = number_setting;
    if (!readIniIntRange(pll_section, "vco_freq_hz", candidate.pll.vco_freq, 1, 2000000000, &number_setting))
    {
        return false;
    }
    candidate.pll.vco_freq = number_setting;
    if (!readIniIntRange(pll_section, "pd1", candidate.pll.post_div1, 1, 7, &number_setting))
    {
        return false;
    }
    candidate.pll.post_div1 = number_setting;
    if (!readIniIntRange(pll_section, "pd2", candidate.pll.post_div2, 1, 7, &number_setting))
    {
        return false;
    }
    candidate.pll.post_div2 = number_setting;
    uint32_t pll_output_hz = candidate.pll.vco_freq / candidate.pll.post_div1 / candidate.pll.post_div2;
    if (pll_output_hz > 252000000)
    {
        logmsg("Reclocking over 252MHz with the PLL settings is not allowed using ", CUSTOM_TIMINGS_FILE);
        return false;
    }
    if (pll_output_hz != (uint32_t)candidate.clk_hz)
    {
        logmsg("PLL output clock ", (int)pll_output_hz, "Hz does not match clk_hz ",
            (int)candidate.clk_hz, "Hz in ", CUSTOM_TIMINGS_FILE);
        return false;
    }


    // scsi
    if (!readIniIntRange(scsi_section, "clk_period_ps", candidate.scsi.clk_period_ps, 1, 1000000, &number_setting))
    {
        return false;
    }
    candidate.scsi.clk_period_ps = number_setting;
    if (!readIniIntRange(scsi_section, "req_delay_cc", candidate.scsi.req_delay, 2, 31, &number_setting))
    {
        return false;
    }
    candidate.scsi.req_delay = number_setting;

    // delay_100ns_cycles: auto-calculate default from clk_hz, allow INI override
    uint8_t default_100ns_cycles = (candidate.clk_hz + 5000000) / 10000000;  // rounded
    if (default_100ns_cycles < 1) default_100ns_cycles = 1;
    if (!readIniIntRange(scsi_section, "delay_100ns_cycles", default_100ns_cycles, 1, 255, &number_setting))
    {
        return false;
    }
    candidate.scsi.delay_100ns_cycles = number_setting;

    // scsi 20
    if (!readIniIntRange(scsi_20_section, "delay0_cc", candidate.scsi_20.delay0, 0, 31, &number_setting)) return false;
    candidate.scsi_20.delay0 = number_setting;
    if (!readIniIntRange(scsi_20_section, "delay1_cc", candidate.scsi_20.delay1, 0, 31, &number_setting)) return false;
    candidate.scsi_20.delay1 = number_setting;
    if (!readIniIntRange(scsi_20_section, "total_period_adjust_cc", candidate.scsi_20.total_period_adjust, -128, 127, &number_setting)) return false;
    candidate.scsi_20.total_period_adjust = number_setting;
    if (!readIniIntRange(scsi_20_section, "max_sync", candidate.scsi_20.max_sync, 1, 255, &number_setting)) return false;
    candidate.scsi_20.max_sync = number_setting;
    if (!readIniIntRange(scsi_20_section, "read_delay1_cc", candidate.scsi_20.rdelay1, 0, 31, &number_setting)) return false;
    candidate.scsi_20.rdelay1 = number_setting;
    if (!readIniIntRange(scsi_20_section, "read_total_period_adjust_cc", candidate.scsi_20.rtotal_period_adjust, -128, 127, &number_setting)) return false;
    candidate.scsi_20.rtotal_period_adjust = number_setting;

    // scsi 10
    if (!readIniIntRange(scsi_10_section, "delay0_cc", candidate.scsi_10.delay0, 0, 31, &number_setting)) return false;
    candidate.scsi_10.delay0 = number_setting;
    if (!readIniIntRange(scsi_10_section, "delay1_cc", candidate.scsi_10.delay1, 0, 31, &number_setting)) return false;
    candidate.scsi_10.delay1 = number_setting;
    if (!readIniIntRange(scsi_10_section, "total_period_adjust_cc", candidate.scsi_10.total_period_adjust, -128, 127, &number_setting)) return false;
    candidate.scsi_10.total_period_adjust = number_setting;
    if (!readIniIntRange(scsi_10_section, "max_sync", candidate.scsi_10.max_sync, 1, 255, &number_setting)) return false;
    candidate.scsi_10.max_sync = number_setting;
    if (!readIniIntRange(scsi_10_section, "read_delay1_cc", candidate.scsi_10.rdelay1, 0, 31, &number_setting)) return false;
    candidate.scsi_10.rdelay1 = number_setting;
    if (!readIniIntRange(scsi_10_section, "read_total_period_adjust_cc", candidate.scsi_10.rtotal_period_adjust, -128, 127, &number_setting)) return false;
    candidate.scsi_10.rtotal_period_adjust = number_setting;

    // scsi 5
    if (!readIniIntRange(scsi_5_section, "delay0_cc", candidate.scsi_5.delay0, 0, 31, &number_setting)) return false;
    candidate.scsi_5.delay0 = number_setting;
    if (!readIniIntRange(scsi_5_section, "delay1_cc", candidate.scsi_5.delay1, 0, 31, &number_setting)) return false;
    candidate.scsi_5.delay1 = number_setting;
    if (!readIniIntRange(scsi_5_section, "total_period_adjust_cc", candidate.scsi_5.total_period_adjust, -128, 127, &number_setting)) return false;
    candidate.scsi_5.total_period_adjust = number_setting;
    if (!readIniIntRange(scsi_5_section, "max_sync", candidate.scsi_5.max_sync, 1, 255, &number_setting)) return false;
    candidate.scsi_5.max_sync = number_setting;
    if (!readIniIntRange(scsi_5_section, "read_delay1_cc", candidate.scsi_5.rdelay1, 0, 31, &number_setting)) return false;
    candidate.scsi_5.rdelay1 = number_setting;
    if (!readIniIntRange(scsi_5_section, "read_total_period_adjust_cc", candidate.scsi_5.rtotal_period_adjust, -128, 127, &number_setting)) return false;
    candidate.scsi_5.rtotal_period_adjust = number_setting;

    // sdio
    if (!readIniIntRange(sdio_section, "clk_div_pio", candidate.sdio.clk_div_pio, 1, 255, &number_setting)) return false;
    candidate.sdio.clk_div_pio = number_setting;
    if (!readIniIntRange(sdio_section, "clk_div_1mhz", candidate.sdio.clk_div_1mhz, 1, 255, &number_setting)) return false;
    candidate.sdio.clk_div_1mhz = number_setting;
    if (!readIniIntRange(sdio_section, "delay0", candidate.sdio.delay0, 0, 255, &number_setting)) return false;
    candidate.sdio.delay0 = number_setting;
    if (!readIniIntRange(sdio_section, "delay1", candidate.sdio.delay1, 0, 255, &number_setting)) return false;
    candidate.sdio.delay1 = number_setting;

    // audio
    if (!readIniIntRange(audio_section, "clk_div_pio", candidate.audio.clk_div_pio, 1, 255, &number_setting))
    {
        return false;
    }
    candidate.audio.clk_div_pio = number_setting;
    candidate.audio.audio_clocked = ini_getbool(audio_section, "clk_for_audio", candidate.audio.audio_clocked, CUSTOM_TIMINGS_FILE);

    *g_bluescsi_timings = candidate;
    g_force_sync = force_sync;
    g_force_offset = force_offset;
    g_max_sync_20_period = g_bluescsi_timings->scsi_20.max_sync;
    g_max_sync_10_period = g_bluescsi_timings->scsi_10.max_sync;
    g_max_sync_5_period = g_bluescsi_timings->scsi_5.max_sync;

    if (g_force_sync > 0)
    {
        uint8_t adjusted = calculate_sync_period_limit(g_bluescsi_timings, g_force_sync);
        if (adjusted != g_force_sync)
        {
            logmsg("Adjusting forced sync period from ", (int)g_force_sync, " to ",
                (int)adjusted, " to match DATA OUT timing floor");
            g_force_sync = adjusted;
        }
        logmsg("Forcing sync of ", (int) g_force_sync, " and offset of ", (int) g_force_offset);
    }

    return true;
}
