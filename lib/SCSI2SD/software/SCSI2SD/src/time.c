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
#include "limits.h"

static volatile uint32_t counter = 0;

CY_ISR_PROTO(TickISR);
CY_ISR(TickISR)
{
	// Should be atomic at 32bit word size. Limits runtime to 49 days.
	++counter;
}

void timeInit()
{
	// Interrupt 15. SysTick_IRQn is -1.
	// The SysTick timer is integrated into the Arm Cortex M3
	CyIntSetSysVector((SysTick_IRQn + 16), TickISR);

	// Ensure the cycle count is < 24bit.
	// At 50MHz bus clock, counter is 50000.
	SysTick_Config((BCLK__BUS_CLK__HZ + 999u) / 1000u);
}

uint32_t getTime_ms()
{
	return counter;
}

uint32_t diffTime_ms(uint32_t start, uint32_t end)
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
