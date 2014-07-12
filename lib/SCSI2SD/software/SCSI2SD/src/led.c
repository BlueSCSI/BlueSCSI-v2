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

#include "led.h"

// External LED support only exists on the 3.5" v4 board.
// The powerbook v4 board ties the pin to ground.
// The v3 boards do not have any such pin.
#ifdef EXTLED_CTL
#define HAVE_EXTLED 1
#endif

#ifdef HAVE_EXTLED
static int enable_EXTLED = 0;
#endif

void ledInit()
{
#ifdef HAVE_EXTLED
	EXTLED_SetDriveMode(EXTLED_DM_DIG_HIZ | EXTLED_DM_RES_UP);
	int val = EXTLED_Read();
	if (val)
	{
		// Pin is not tied to ground, so it's safe to use.
		enable_EXTLED = 1;
		EXTLED_SetDriveMode(LED1_DM_STRONG);
	}
	else
	{
		// Pin is tied to ground. Using it would damage hardware
		// This is the case for the powerbook boards.
		enable_EXTLED = 0;
		EXTLED_SetDriveMode(EXTLED_DM_DIG_HIZ);

	}
#endif
	ledOff();
}

void ledOn()
{
	LED1_Write(0);

#ifdef HAVE_EXTLED
	if (enable_EXTLED)
	{
		EXTLED_Write(1);
	}
#endif
}

void ledOff()
{
	LED1_Write(1);

#ifdef HAVE_EXTLED
	if (enable_EXTLED)
	{
		EXTLED_Write(0);
	}
#endif
}

