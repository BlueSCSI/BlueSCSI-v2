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
#include "config.h"
#include "cdrom.h"

uint8_t SimpleTOC[] =
{
	0x00, // toc length, MSB
	0x0A, // toc length, LSB
	0x01, // First track number
	0x01, // Last track number,
	// TRACK 1 Descriptor
	0x00, // reservied
	0x06, // Q sub-channel not supplied, Digital track
	0x01, // Track 1,
	0x00, // Reserved
	0x00,0x00,0x00,0x00 // Track start sector (LBA)
};

void doReadTOC(int MSF, uint8_t track, uint16_t allocationLength)
{
	// We only support track 1.
	// track 0 means "return all tracks"
	if (track > 1)
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}
	else if (MSF)
	{
		// MSF addressing not currently supported.
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}
	else
	{
		uint32_t len = sizeof(SimpleTOC);
		memcpy(scsiDev.data, SimpleTOC, len);
		if (len > allocationLength)
		{
			len = allocationLength;
		}
		scsiDev.dataLen = len;
		scsiDev.phase = DATA_IN;
	}
}

uint8_t SimpleHeader[] =
{
	0x01, // 2048byte user data, L-EC in 288 byte aux field.
	0x00, // reserved
	0x00, // reserved
	0x00, // reserved
	0x00,0x00,0x00,0x00 // Track start sector (LBA)
};

void doReadHeader(int MSF, uint32_t lba, uint16_t allocationLength)
{
	if (MSF)
	{
		// MSF addressing not currently supported.
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}
	else
	{
		uint32_t len = sizeof(SimpleHeader);
		memcpy(scsiDev.data, SimpleHeader, len);
		if (len > allocationLength)
		{
			len = allocationLength;
		}
		scsiDev.dataLen = len;
		scsiDev.phase = DATA_IN;
	}
}


// Handle direct-access scsi device commands
int scsiCDRomCommand()
{
	int commandHandled = 1;

	uint8 command = scsiDev.cdb[0];
	if (scsiDev.target->cfg->deviceType == CONFIG_OPTICAL)
	{
		if (command == 0x43)
		{
			// CD-ROM Read TOC
			int MSF = scsiDev.cdb[1] & 0x02 ? 1 : 0;
			uint8_t track = scsiDev.cdb[6];
			uint16_t allocationLength =
				(((uint32_t) scsiDev.cdb[7]) << 8) +
				scsiDev.cdb[8];

			doReadTOC(MSF, track, allocationLength);
		}
		else if (command == 0x44)
		{
			// CD-ROM Read Header
			int MSF = scsiDev.cdb[1] & 0x02 ? 1 : 0;
			uint32_t lba = 0; // IGNORED for now
			uint16_t allocationLength =
				(((uint32_t) scsiDev.cdb[7]) << 8) +
				scsiDev.cdb[8];
			doReadHeader(MSF, lba, allocationLength);
		}
		else
		{
			commandHandled = 0;
		}
	}
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
}
