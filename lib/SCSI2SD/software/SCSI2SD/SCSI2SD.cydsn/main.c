//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
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
#include "blinky.h"
#include "loopback.h"
#include "scsi.h"
#include "scsiPhy.h"
#include "config.h"
#include "disk.h"
#include "led.h"

const char* Notice = "Copyright (C) 2013 Michael McMaster <michael@codesrc.com>";

int main()
{
	// scsi2sd_test_blink(); // Initial test. Will not return.
	ledOff();

	// Enable global interrupts.
	// Needed for RST and ATN interrupt handlers.
	CyGlobalIntEnable;

	// Set interrupt handlers.
	scsiPhyInit();
	
	// Loopback test requires the interrupt handers.
	// Will not return if uncommented.
	// scsi2sd_test_loopback();
	
	configInit();
	
	scsiInit();
	scsiDiskInit();

	// Reading jumpers
	// Is SD card detect asserted ?

	// TODO POST ?

	while (1)
	{
		scsiPoll();
		scsiDiskPoll();
		configPoll();
	}
	return 0;
}

