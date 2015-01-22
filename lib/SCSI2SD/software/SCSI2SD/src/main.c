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

		uint32_t now = getTime_ms();
		if (diffTime_ms(lastSDPoll, now) > 200)
		{
			lastSDPoll = now;
			sdPoll();
		}
	}
	return 0;
}

