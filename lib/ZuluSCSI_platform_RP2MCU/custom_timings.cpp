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

#include "custom_timings.h"
#include <SdFat.h>
#include <minIni.h>
#include <ZuluSCSI_log.h>
#include <ZuluSCSI_platform.h>
extern SdFs SD;

extern "C"
{
    #include <timings.h>
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

    // pll
    int32_t vco = ini_getl(pll_section, "vco_freq_hz", g_zuluscsi_timings->pll.vco_freq, CUSTOM_TIMINGS_FILE);
    int32_t post_div1 = ini_getl(pll_section, "pd1", g_zuluscsi_timings->pll.post_div1, CUSTOM_TIMINGS_FILE);
    int32_t post_div2 = ini_getl(pll_section, "pd2", g_zuluscsi_timings->pll.post_div2, CUSTOM_TIMINGS_FILE);

    if (vco > 0 && post_div1 > 0 && post_div2 > 0)
    {
        if (vco / post_div1 / post_div2 > 250000000)
        {
            logmsg("Reclocking over 250MHz with the PLL settings is not allowed using ", CUSTOM_TIMINGS_FILE);
            return false;
        }
    }
    else
    {
        logmsg("Reclocking failed because 0 or negative PLL settings values");
        return false;
    }

    g_zuluscsi_timings->pll.vco_freq = vco;
    g_zuluscsi_timings->pll.post_div1 = post_div1;
    g_zuluscsi_timings->pll.post_div2 = post_div2;
    g_zuluscsi_timings->pll.refdiv =  ini_getl(pll_section, "refdiv", g_zuluscsi_timings->pll.refdiv, CUSTOM_TIMINGS_FILE);

    char speed_grade_str[10];
    ini_gets(settings_section, "extends_speed_grade", "Default", speed_grade_str, sizeof(speed_grade_str), CUSTOM_TIMINGS_FILE);
    zuluscsi_speed_grade_t speed_grade =  g_scsi_settings.stringToSpeedGrade(speed_grade_str, sizeof(speed_grade_str));
    set_timings(speed_grade);

    int32_t number_setting = ini_getl(settings_section, "boot_with_sync_value", 0, CUSTOM_TIMINGS_FILE);

    if (number_setting > 0)
    {
        g_force_sync = number_setting;
        number_setting = ini_getl(settings_section, "boot_with_offset_value", 15, CUSTOM_TIMINGS_FILE);
        g_force_offset = number_setting > 15 ? 15 : number_setting;
        logmsg("Forcing sync of ", (int) g_force_sync, " and offset of ", (int) g_force_offset);
    }
    g_zuluscsi_timings->clk_hz = ini_getl(settings_section, "clk_hz", g_zuluscsi_timings->clk_hz, CUSTOM_TIMINGS_FILE);


    // scsi
    g_zuluscsi_timings->scsi.clk_period_ps = ini_getl(scsi_section, "clk_period_ps", g_zuluscsi_timings->scsi.clk_period_ps, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi.req_delay = ini_getl(scsi_section, "req_delay_cc", g_zuluscsi_timings->scsi.req_delay, CUSTOM_TIMINGS_FILE);

    // scsi 20
    g_zuluscsi_timings->scsi_20.delay0 = ini_getl(scsi_20_section, "delay0_cc", g_zuluscsi_timings->scsi_20.delay0, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_20.delay1 = ini_getl(scsi_20_section, "delay1_cc", g_zuluscsi_timings->scsi_20.delay1, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_20.total_delay_adjust = ini_getl(scsi_20_section, "total_delay_adjust_cc", g_zuluscsi_timings->scsi_20.total_delay_adjust, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_20.max_sync = ini_getl(scsi_20_section, "max_sync", g_zuluscsi_timings->scsi_20.max_sync, CUSTOM_TIMINGS_FILE);

    // scsi 10
    g_zuluscsi_timings->scsi_10.delay0 = ini_getl(scsi_10_section, "delay0_cc", g_zuluscsi_timings->scsi_10.delay0, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_10.delay1 = ini_getl(scsi_10_section, "delay1_cc", g_zuluscsi_timings->scsi_10.delay1, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_10.total_delay_adjust = ini_getl(scsi_10_section, "total_delay_adjust_cc", g_zuluscsi_timings->scsi_10.total_delay_adjust, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_10.max_sync = ini_getl(scsi_10_section, "max_sync", g_zuluscsi_timings->scsi_10.max_sync, CUSTOM_TIMINGS_FILE);

    // scsi 5
    g_zuluscsi_timings->scsi_5.delay0 = ini_getl(scsi_5_section, "delay0_cc", g_zuluscsi_timings->scsi_5.delay0, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_5.delay1 = ini_getl(scsi_5_section, "delay1_cc", g_zuluscsi_timings->scsi_5.delay1, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_5.total_delay_adjust = ini_getl(scsi_5_section, "total_delay_adjust_cc", g_zuluscsi_timings->scsi_5.total_delay_adjust, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->scsi_5.max_sync = ini_getl(scsi_5_section, "max_sync", g_zuluscsi_timings->scsi_5.max_sync, CUSTOM_TIMINGS_FILE);

    // sdio
    g_zuluscsi_timings->sdio.clk_div_pio = ini_getl(sdio_section, "clk_div_pio", g_zuluscsi_timings->sdio.clk_div_pio, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->sdio.clk_div_1mhz = ini_getl(sdio_section, "clk_div_1mhz", g_zuluscsi_timings->sdio.clk_div_1mhz, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->sdio.delay0 = ini_getl(sdio_section, "delay0", g_zuluscsi_timings->sdio.delay0, CUSTOM_TIMINGS_FILE);
    g_zuluscsi_timings->sdio.delay1 = ini_getl(sdio_section, "delay1", g_zuluscsi_timings->sdio.delay1, CUSTOM_TIMINGS_FILE);

    return true;
}
