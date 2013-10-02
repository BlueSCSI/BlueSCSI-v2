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
#include "scsi.h"
#include "scsiPhy.h"
#include "bits.h"

// Spins until the SCSI pin is true, or the reset flag is set.
static void waitForPinTrue(int pin)
{
	while (!scsiDev.resetFlag)
	{
		// TODO put some hardware gates in front of the RST pin, and store
		// the state in a register. The minimum "Reset hold time" is 25us, which
		// we can easily satisfy within this loop, but perhaps hard to satisfy
		// if we don't call this function often.
		scsiDev.resetFlag = SCSI_ReadPin(SCSI_In_RST);

		if (SCSI_ReadPin(pin))
		{
			break;
		}
	}
}

// Spins until the SCSI pin is true, or the reset flag is set.
static void waitForPinFalse(int pin)
{
	while (!scsiDev.resetFlag)
	{
		// TODO put some hardware gates in front of the RST pin, and store
		// the state in a register. The minimum "Reset hold time" is 25us, which
		// we can easily satisfy within this loop, but perhaps hard to satisfy
		// if we don't call this function often.
		scsiDev.resetFlag = SCSI_ReadPin(SCSI_In_RST);

		if (!SCSI_ReadPin(pin))
		{
			break;
		}
	}
}

static void deskewDelay(void)
{
	// Delay for deskew + cable skew. total 55 nanoseconds.
	// Assumes 66MHz.
	CyDelayCycles(4);
}

uint8 scsiRead(void)
{
	SCSI_SetPin(SCSI_Out_REQ);
	waitForPinTrue(SCSI_In_ACK);
	deskewDelay();

	uint8 value = ~SCSI_In_DBx_Read();
	scsiDev.parityError = scsiDev.parityError ||
		(Lookup_OddParity[value] != SCSI_ReadPin(SCSI_In_DBP));

	SCSI_ClearPin(SCSI_Out_REQ);
	waitForPinFalse(SCSI_In_ACK);
	return value;
}

void scsiWrite(uint8 value)
{
	SCSI_Out_DBx_Write(value);
	if (Lookup_OddParity[value])
	{
		SCSI_SetPin(SCSI_Out_DBP);
	}
	deskewDelay();

	SCSI_SetPin(SCSI_Out_REQ);

	// Initiator reads data here.

	waitForPinTrue(SCSI_In_ACK);

	SCSI_ClearPin(SCSI_Out_DBP);
	SCSI_Out_DBx_Write(0);
	SCSI_ClearPin(SCSI_Out_REQ);

	// Wait for ACK to clear.
	waitForPinFalse(SCSI_In_ACK);
}

static void busSettleDelay(void)
{
	// Data Release time (switching IO) = 400ns
	// + Bus Settle time (switching phase) = 400ns.
	CyDelayUs(1); // Close enough.
}

void scsiEnterPhase(int phase)
{
	if (phase > 0)
	{
		if (phase & __scsiphase_msg)
		{
			SCSI_SetPin(SCSI_Out_MSG);
		}
		else
		{
			SCSI_ClearPin(SCSI_Out_MSG);
		}

		if (phase & __scsiphase_cd)
		{
			SCSI_SetPin(SCSI_Out_CD);
		}
		else
		{
			SCSI_ClearPin(SCSI_Out_CD);
		}

		if (phase & __scsiphase_io)
		{
			SCSI_SetPin(SCSI_Out_IO);
		}
		else
		{
			SCSI_ClearPin(SCSI_Out_IO);
		}
	}
	else
	{
		SCSI_ClearPin(SCSI_Out_MSG);
		SCSI_ClearPin(SCSI_Out_CD);
		SCSI_ClearPin(SCSI_Out_IO);
	}
	busSettleDelay();
}

