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
#include "diagnostic.h"

#include <string.h>

static const uint8 SupportedDiagnosticPages[] =
{
0x00, // Page Code
0x00, // Reserved
0x02, // Page length
0x00, // Support "Supported diagnostic page"
0x40  // Support "Translate address page"
};

void scsiSendDiagnostic()
{
	// SEND DIAGNOSTIC
	// Pretend to do self-test. Actual data is returned via the
	// RECEIVE DIAGNOSTIC RESULTS command.
	int selfTest = scsiDev.cdb[1] & 0x04;
	uint32 paramLength =
		(((uint32) scsiDev.cdb[3]) << 8) +
		scsiDev.cdb[4];

	if (!selfTest)
	{
		// Initiator sends us page data.
		scsiDev.dataLen = paramLength;
		scsiDev.phase = DATA_OUT;

		if (scsiDev.dataLen > sizeof (scsiDev.data))
		{
			// Nowhere to store this data!
			// Shouldn't happen - our buffer should be many magnitudes larger
			// than the required size for diagnostic parameters.
			scsiDev.sense.code = ILLEGAL_REQUEST;
			scsiDev.sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.status = CHECK_CONDITION;
			scsiDev.phase = STATUS;
		}
	}
	else
	{
		// Default command result will be a status of GOOD anyway.
	}
}

void scsiReceiveDiagnostic()
{
	// RECEIVE DIAGNOSTIC RESULTS
	// We assume scsiDev.data contains the contents of a previous
	// SEND DIAGNOSTICS command.  We only care about the page-code part
	// of the parameter list.
	uint8 pageCode = scsiDev.data[0];

	int allocLength =
		(((uint16) scsiDev.cdb[3]) << 8) +
		scsiDev.cdb[4];


	if (pageCode == 0x00)
	{
		memcpy(
			scsiDev.data,
			SupportedDiagnosticPages,
			sizeof(SupportedDiagnosticPages));
		scsiDev.dataLen = sizeof(SupportedDiagnosticPages);
		scsiDev.phase = DATA_IN;
	}
	else if (pageCode == 0x40)
	{
		// Translate between logical block address, physical sector address, or
		// physical bytes.
		uint8 suppliedFmt = scsiDev.data[4] & 0x7;
		uint8 translateFmt = scsiDev.data[5] & 0x7;

		// Convert each supplied address back to a simple
		// 64bit linear address, then convert back again.
		uint64 fromByteAddr =
			scsiByteAddress(suppliedFmt, &scsiDev.data[6]);

		scsiSaveByteAddress(translateFmt, fromByteAddr, &scsiDev.data[6]);

		// Fill out the rest of the response.
		// (Clear out any optional bits).
		scsiDev.data[4] = suppliedFmt;
		scsiDev.data[5] = translateFmt;

		scsiDev.dataLen = 14;
		scsiDev.phase = DATA_IN;
	}
	else
	{
		// error.
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}

	if (scsiDev.phase == DATA_IN && scsiDev.dataLen > allocLength)
	{
		// simply truncate the response.
		scsiDev.dataLen = allocLength;
	}

	{
		uint8 lun = scsiDev.cdb[1] >> 5;
		// Set the first byte to indicate LUN presence.
		if (lun) // We only support lun 0
		{
			scsiDev.data[0] = 0x7F;
		}
	}
}

