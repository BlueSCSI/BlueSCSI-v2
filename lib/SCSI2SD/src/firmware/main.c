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
#include "bsp.h"
#include "disk.h"
#include "fpga.h"
#include "led.h"
#include "sd.h"
#include "scsi.h"
#include "scsiPhy.h"
#include "time.h"
#include "trace.h"
#include "sdio.h"
#include "usb_device/usb_device.h"
#include "usb_device/usbd_composite.h"
#include "usb_device/usbd_msc_storage_sd.h"


const char* Notice = "Copyright (C) 2016 Michael McMaster <michael@codesrc.com>";
uint32_t lastSDPoll;

static int isUsbStarted;

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
	scsiPhyConfig();
	scsiInit();

	MX_USB_DEVICE_Init(); // USB lun config now available.
	isUsbStarted = 1;

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

	lastSDPoll = s2s_getTime_ms();
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
#endif

	if (unlikely(scsiDev.phase == BUS_FREE))
	{
		if (unlikely(s2s_elapsedTime_ms(lastSDPoll) > 200))
		{
			lastSDPoll = s2s_getTime_ms();
			if (sdInit())
			{
				s2s_configInit(&scsiDev.boardCfg);
				scsiPhyConfig();
				scsiInit();

				// Is a USB host connected ?
				if (isUsbStarted)
				{
					USBD_Stop(&hUsbDeviceFS);
					s2s_delay_ms(128);
					USBD_Start(&hUsbDeviceFS);
				}
			}

			// Can we speed up the SD card ?
			// Don't combine with the above block because that won't
			// run if the SD card is present at startup.
			// Don't use VBUS monitoring because that just tells us about
			// power, which could be from a charger
			if ((blockDev.state & DISK_PRESENT) &&
				isUsbStarted &&
				(scsiDev.cmdCount > 0) && // no need for speed without scsi
				!USBD_Composite_IsConfigured(&hUsbDeviceFS))
			{
				if (HAL_SD_HighSpeed(&hsd) == SD_OK)
				{
					USBD_Stop(&hUsbDeviceFS);
					s2s_setFastClock();
					isUsbStarted = 0;
				}
			}

			else if (!(blockDev.state & DISK_PRESENT) && !isUsbStarted)
			{
				// Good time to restart USB.
				s2s_setNormalClock();
				USBD_Start(&hUsbDeviceFS);
				isUsbStarted = 1;
			}
		}
		else
		{
			// TODO this hurts performance significantly! Work out why __WFI()
			// doesn't wake up immediately !
#if 0
			// Wait for our 1ms timer to save some power.
			// There's an interrupt on the SEL signal to ensure we respond
			// quickly to any SCSI commands. The selection abort time is
			// only 250us, and new SCSI-3 controllers time-out very
			// not long after that, so we need to ensure we wake up quickly.
			uint32_t interruptState = __get_PRIMASK();
			__disable_irq();

			if (!*SCSI_STS_SELECTED)
			{
				//__WFI(); // Will wake on interrupt, regardless of mask
			}
			if (!interruptState)
			{
				__enable_irq();
			}
#endif
		}
	}
	else if (scsiDev.phase >= 0)
	{
		// don't waste time scanning SD cards while we're doing disk IO
		lastSDPoll = s2s_getTime_ms();
	}
}

