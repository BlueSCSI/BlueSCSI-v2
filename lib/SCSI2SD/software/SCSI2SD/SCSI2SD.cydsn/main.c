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
// #include "blinky.h"
// #include "loopback.h"
#include "scsi.h"
#include "disk.h"
#include "led.h"

const char* Notice = "Copyright (C) 2013 Michael McMaster <michael@codesrc.com>";

void main()
{
	// scsi2sd_test_blinky(); // Initial test. Will not return.
	// scsi2sd_test_loopback(); // Second test. Will not return.
	ledOff();

	/* Uncomment this line to enable global interrupts. */
	// MM: Try to avoid interrupts completely, as it will screw with our
	// timing.
	 CyGlobalIntEnable;
	 
	// TODO insert any initialisation code here.
	scsiInit(0, 1); // ID 0 is mac boot disk
	scsiDiskInit();

	// Reading jumpers
	// Is SD card detect asserted ?

	// TODO POST ?

	while (1)
	{
		scsiPoll();
		scsiDiskPoll();
	}
}

