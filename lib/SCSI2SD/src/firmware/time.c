//	Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.

#include "time.h"
#include "stm32f2xx.h"

#include <limits.h>

uint32_t s2s_systickConfig;

void s2s_timeInit()
{
	// Cortex-M3 SYSTICK is already configured by the STM32 HAL_Init function
	// to interrupt every 1ms.
	// See stm32f2xx_hal.c
	// The SysTick_Handler() function is in stm32f2xx_it.c

	// Grab the configured tick period for use in s2s_delay_us
	s2s_systickConfig = HAL_RCC_GetHCLKFreq() / 1000;
}

uint32_t s2s_getTime_ms()
{
	return HAL_GetTick();
}

uint32_t s2s_diffTime_ms(uint32_t start, uint32_t end)
{
	if (end >= start)
	{
		return 	end - start;
	}
	else
	{
		return (UINT_MAX - start) + end;
	}
}

uint32_t s2s_elapsedTime_ms(uint32_t since)
{
	uint32_t now = HAL_GetTick();
	if (now >= since)
	{
		return now - since;
	}
	else
	{
		return (UINT_MAX - since) + now;
	}
}

// Make use of the ARM Cortex M3 SYSTICK timer.
void s2s_delay_clocks(uint32_t delay)
{
	uint32_t start = SysTick->VAL;

	uint32_t diff = 0;
	while (diff < delay)
	{
		uint32_t now = SysTick->VAL;

		diff += (now <= start) ?
			(start - now) :
			(start + s2s_systickConfig - now);

		start = now;
	}
}

