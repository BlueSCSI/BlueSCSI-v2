/**
 * ZuluSCSI™ - Copyright (c) 2025 Rabbit Hole Computing™
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

/************************************/
/* Status reporting by blinking led */
/************************************/
#include <ZuluSCSI_platform.h>
#include "ZuluSCSI_blink.h"

static uint32_t blink_count = 0;
static uint32_t blink_start = 0;
static uint32_t blink_delay = 0;
static uint32_t blink_end_delay= 0;

extern "C" void s2s_ledOn()
{
  LED_ON();
}

extern "C" void s2s_ledOff()
{
  LED_OFF();
}
bool blink_poll()
{
    bool is_blinking = true;

    if (blink_count == 0)
    {
        is_blinking = false;
    }
    else if (blink_count == 1 && ((uint32_t)(millis() - blink_start)) > blink_end_delay )
    {
        LED_OFF_OVERRIDE();
        blink_count = 0;
        is_blinking = false;
    }
    else if (blink_count > 1 && ((uint32_t)(millis() - blink_start)) > blink_delay)
    {
        if (1 & blink_count)
            LED_ON_OVERRIDE();
        else
            LED_OFF_OVERRIDE();
        blink_count--;
        blink_start = millis();
    }

    if (!is_blinking)
        platform_set_blink_status(false);
    return is_blinking;
}

void blink_cancel()
{
    blink_count = 0;
    platform_set_blink_status(false);
}

void blinkStatus(uint32_t times, uint32_t delay, uint32_t end_delay)
{
    if (!blink_poll() && blink_count == 0)
    {
        blink_start = millis();
        blink_count = 2 * times + 1;
        blink_delay = delay / 2;
        blink_end_delay =  end_delay;
        platform_set_blink_status(true);
        LED_OFF_OVERRIDE();
    }
}
