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
#pragma GCC push_options
#pragma GCC optimize("-flto")

#include "device.h"
#include "scsi.h"
#include "scsiPhy.h"
#include "config.h"
#include "disk.h"
#include "led.h"
#include "time.h"

const char* Notice = "Copyright (C) 2014 Michael McMaster <michael@codesrc.com>";

uint8_t testData[512];

int main()
{
	timeInit();
	ledInit();

	// Enable global interrupts.
	// Needed for RST and ATN interrupt handlers.
	CyGlobalIntEnable;

	// Set interrupt handlers.
	scsiPhyInit();

	configInit();
	debugInit();

	scsiInit();
	scsiDiskInit();

	uint32_t lastSDPoll = getTime_ms();
	sdPoll();




	while (1)
	{
		scsiDev.watchdogTick++;

		scsiPoll();
		scsiDiskPoll();
		configPoll();

		if (unlikely(scsiDev.phase == BUS_FREE))
		{
			if (unlikely(elapsedTime_ms(lastSDPoll) > 200))
			{
				lastSDPoll = getTime_ms();
				sdPoll();
			}
			else
			{
				// Wait for our 1ms timer to save some power.
				__WFI();
			}
		}
	}
	return 0;
}

#pragma GCC pop_options
