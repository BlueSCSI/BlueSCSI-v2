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

#include "stm32f2xx.h"

#include "config.h"
#include "disk.h"
#include "fpga.h"
#include "led.h"
#include "sd.h"
#include "scsi.h"
#include "scsiPhy.h"
#include "time.h"
#include "trace.h"
#include "usb_device/usb_device.h"
#include "usb_device/usbd_composite.h"
#include "usb_device/usbd_msc_storage_sd.h"


const char* Notice = "Copyright (C) 2016 Michael McMaster <michael@codesrc.com>";

void mainEarlyInit()
{
	// USB device is initialised before mainInit is called
	s2s_initUsbDeviceStorage();
}

void mainInit()
{
	traceInit();
	s2s_timeInit();
	s2s_ledInit();
	s2s_fpgaInit();

	scsiPhyInit();

	scsiDiskInit();
	sdInit();
	s2s_configInit(&scsiDev.boardCfg);

	s2s_debugInit();

	scsiInit();

	MX_USB_DEVICE_Init(); // USB lun config now available.

	// Optional bootup delay
	int delaySeconds = 0;
	while (delaySeconds < scsiDev.boardCfg.startupDelay) {
		// Keep the USB connection working, otherwise it's very hard to revert
		// silly extra-long startup delay settings.
		int i;
		for (i = 0; i < 200; i++) {
			s2s_delay_ms(5);
			scsiDev.watchdogTick++;
			s2s_configPoll();
		}
		++delaySeconds;
	}

#if 0
	uint32_t lastSDPoll = getTime_ms();
	sdCheckPresent();
#endif
}

void mainLoop()
{
	scsiDev.watchdogTick++;

	scsiPoll();
	scsiDiskPoll();
	s2s_configPoll();
	s2s_usbDevicePoll();

#if 0
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
#endif
}

