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
#include "trace.h"

const char* Notice = "Copyright (C) 2015 Michael McMaster <michael@codesrc.com>";

int main()
{
	timeInit();
	ledInit();
	traceInit();

	// Enable global interrupts.
	// Needed for RST and ATN interrupt handlers.
	CyGlobalIntEnable;

	// Set interrupt handlers.
	scsiPhyInit();

	configInit(&scsiDev.boardCfg);
	debugInit();

	scsiInit();
	scsiDiskInit();

	// Optional bootup delay
	int delaySeconds = 0;
	while (delaySeconds < scsiDev.boardCfg.startupDelay) {
		CyDelay(1000);
		++delaySeconds;
	}

	uint32_t lastSDPoll = getTime_ms();
	sdCheckPresent();


	while (1)
	{
		scsiDev.watchdogTick++;

		scsiPoll();
		scsiDiskPoll();
		configPoll();
		sdPoll();

		if (unlikely(scsiDev.phase == BUS_FREE))
		{
			if (unlikely(elapsedTime_ms(lastSDPoll) > 200))
			{
				lastSDPoll = getTime_ms();
				sdCheckPresent();
			}
			else
			{
				// Wait for our 1ms timer to save some power.
				// There's an interrupt on the SEL signal to ensure we respond
				// quickly to any SCSI commands. The selection abort time is
				// only 250us, and new SCSI-3 controllers time-out very
				// not long after that, so we need to ensure we wake up quickly.
				uint8_t interruptState = CyEnterCriticalSection();
				if (!SCSI_ReadFilt(SCSI_Filt_SEL))
				{
					__WFI(); // Will wake on interrupt, regardless of mask
				}
				CyExitCriticalSection(interruptState);
			}
		}
		else if (scsiDev.phase >= 0)
		{
			// don't waste time scanning SD cards while we're doing disk IO
			lastSDPoll = getTime_ms();
		}
	}
	return 0;
}

#pragma GCC pop_options
