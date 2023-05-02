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

#include "scsi.h"
#include "config.h"
#include "cdrom.h"

#include <string.h>

static const uint8_t SimpleTOC[] =
{
	0x00, // toc length, MSB
	0x12, // toc length, LSB
	0x01, // First track number
	0x01, // Last track number,
	// TRACK 1 Descriptor
	0x00, // reserved
	0x14, // Q sub-channel encodes current position, Digital track
	0x01, // Track 1,
	0x00, // Reserved
	0x00,0x00,0x00,0x00, // Track start sector (LBA)
	0x00, // reserved
	0x14, // Q sub-channel encodes current position, Digital track
	0xAA, // Leadout Track
	0x00, // Reserved
	0x00,0x00,0x00,0x00, // Track start sector (LBA)
};

static const uint8_t LeadoutTOC[] =
{
	0x00, // toc length, MSB
	0x0A, // toc length, LSB
	0x01, // First track number
	0x01, // Last track number,
	0x00, // reserved
	0x14, // Q sub-channel encodes current position, Digital track
	0xAA, // Leadout Track
	0x00, // Reserved
	0x00,0x00,0x00,0x00, // Track start sector (LBA)
};

static const uint8_t SessionTOC[] =
{
	0x00, // toc length, MSB
	0x0A, // toc length, LSB
	0x01, // First session number
	0x01, // Last session number,
	// TRACK 1 Descriptor
	0x00, // reserved
	0x14, // Q sub-channel encodes current position, Digital track
	0x01, // First track number in last complete session
	0x00, // Reserved
	0x00,0x00,0x00,0x00 // LBA of first track in last session
};


static const uint8_t FullTOC[] =
{
	0x00, // toc length, MSB
	0x44, // toc length, LSB
	0x01, // First session number
	0x01, // Last session number,
	// A0 Descriptor
	0x01, // session number
	0x14, // ADR/Control
	0x00, // TNO
	0xA0, // POINT
	0x00, // Min
	0x00, // Sec
	0x00, // Frame
	0x00, // Zero
	0x01, // First Track number.
	0x00, // Disc type 00 = Mode 1
	0x00,  // PFRAME
	// A1
	0x01, // session number
	0x14, // ADR/Control
	0x00, // TNO
	0xA1, // POINT
	0x00, // Min
	0x00, // Sec
	0x00, // Frame
	0x00, // Zero
	0x01, // Last Track number
	0x00, // PSEC
	0x00,  // PFRAME
	// A2
	0x01, // session number
	0x14, // ADR/Control
	0x00, // TNO
	0xA2, // POINT
	0x00, // Min
	0x00, // Sec
	0x00, // Frame
	0x00, // Zero
	0x79, // LEADOUT position BCD
	0x59, // leadout PSEC BCD
	0x74, // leadout PFRAME BCD
	// TRACK 1 Descriptor
	0x01, // session number
	0x14, // ADR/Control
	0x00, // TNO
	0x01, // Point
	0x00, // Min
	0x00, // Sec
	0x00, // Frame
	0x00, // Zero
	0x00, // PMIN
	0x00, // PSEC
	0x00,  // PFRAME
	// b0
	0x01, // session number
	0x54, // ADR/Control
	0x00, // TNO
	0xB1, // POINT
	0x79, // Min BCD
	0x59, // Sec BCD
	0x74, // Frame BCD
	0x00, // Zero
	0x79, // PMIN BCD
	0x59, // PSEC BCD
	0x74,  // PFRAME BCD
	// c0
	0x01, // session number
	0x54, // ADR/Control
	0x00, // TNO
	0xC0, // POINT
	0x00, // Min
	0x00, // Sec
	0x00, // Frame
	0x00, // Zero
	0x00, // PMIN
	0x00, // PSEC
	0x00  // PFRAME
};

static void LBA2MSF(uint32_t LBA, uint8_t* MSF)
{
	MSF[0] = 0; // reserved.
	MSF[3] = LBA % 75; // M
	uint32_t rem = LBA / 75;

	MSF[2] = rem % 60; // S
	MSF[1] = rem / 60;

}

static void doReadTOC(int MSF, uint8_t track, uint16_t allocationLength)
{
	if (track == 0xAA)
	{
		// 0xAA requests only lead-out track information (reports capacity)
		uint32_t len = sizeof(LeadoutTOC);
		memcpy(scsiDev.data, LeadoutTOC, len);

		uint32_t capacity = getScsiCapacity(
			scsiDev.target->cfg->sdSectorStart,
			scsiDev.target->liveCfg.bytesPerSector,
			scsiDev.target->cfg->scsiSectors);

		// Replace start of leadout track
		if (MSF)
		{
			LBA2MSF(capacity, scsiDev.data + 8);
		}
		else
		{
			scsiDev.data[8] = capacity >> 24;
			scsiDev.data[9] = capacity >> 16;
			scsiDev.data[10] = capacity >> 8;
			scsiDev.data[11] = capacity;
		}

		if (len > allocationLength)
		{
			len = allocationLength;
		}
		scsiDev.dataLen = len;
		scsiDev.phase = DATA_IN;
	}
	else if (track <= 1)
	{
		// We only support track 1.
		// track 0 means "return all tracks"
		uint32_t len = sizeof(SimpleTOC);
		memcpy(scsiDev.data, SimpleTOC, len);

		uint32_t capacity = getScsiCapacity(
			scsiDev.target->cfg->sdSectorStart,
			scsiDev.target->liveCfg.bytesPerSector,
			scsiDev.target->cfg->scsiSectors);

		// Replace start of leadout track
		if (MSF)
		{
			LBA2MSF(capacity, scsiDev.data + 0x10);
		}
		else
		{
			scsiDev.data[0x10] = capacity >> 24;
			scsiDev.data[0x11] = capacity >> 16;
			scsiDev.data[0x12] = capacity >> 8;
			scsiDev.data[0x13] = capacity;
		}

		if (len > allocationLength)
		{
			len = allocationLength;
		}
		scsiDev.dataLen = len;
		scsiDev.phase = DATA_IN;
	}
	else
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}
}

static void doReadSessionInfo(uint8_t session, uint16_t allocationLength)
{
	uint32_t len = sizeof(SessionTOC);
	memcpy(scsiDev.data, SessionTOC, len);

	if (len > allocationLength)
	{
		len = allocationLength;
	}
	scsiDev.dataLen = len;
	scsiDev.phase = DATA_IN;
}

static inline uint8_t
fromBCD(uint8_t val)
{
	return ((val >> 4) * 10) + (val & 0xF);
}

static void doReadFullTOC(int convertBCD, uint8_t session, uint16_t allocationLength)
{
	// We only support session 1.
	if (session > 1)
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}
	else
	{
		uint32_t len = sizeof(FullTOC);
		memcpy(scsiDev.data, FullTOC, len);

		if (convertBCD)
		{
			int descriptor = 4;
			while (descriptor < len)
			{
				int i;
				for (i = 0; i < 7; ++i)
				{
					scsiDev.data[descriptor + i] =
						fromBCD(scsiDev.data[descriptor + 4 + i]);
				}
				descriptor += 11;
			}

		}

		if (len > allocationLength)
		{
			len = allocationLength;
		}
		scsiDev.dataLen = len;
		scsiDev.phase = DATA_IN;
	}
}

static uint8_t SimpleHeader[] =
{
	0x01, // 2048byte user data, L-EC in 288 byte aux field.
	0x00, // reserved
	0x00, // reserved
	0x00, // reserved
	0x00,0x00,0x00,0x00 // Track start sector (LBA or MSF)
};

void doReadHeader(int MSF, uint32_t lba, uint16_t allocationLength)
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


// Handle direct-access scsi device commands
int scsiCDRomCommand()
{
	int commandHandled = 1;

	uint8_t command = scsiDev.cdb[0];
	if (command == 0x43)
	{
		// CD-ROM Read TOC
		int MSF = scsiDev.cdb[1] & 0x02 ? 1 : 0;
		uint8_t track = scsiDev.cdb[6];
		uint16_t allocationLength =
			(((uint32_t) scsiDev.cdb[7]) << 8) +
			scsiDev.cdb[8];

		// Reject MMC commands for now, otherwise the TOC data format
		// won't be understood.
		// The "format" field is reserved for SCSI-2
		uint8_t format = scsiDev.cdb[2] & 0x0F;
		switch (format)
		{
			case 0: doReadTOC(MSF, track, allocationLength); break; // SCSI-2
			case 1: doReadSessionInfo(MSF, allocationLength); break; // MMC2
			case 2: doReadFullTOC(0, track, allocationLength); break; // MMC2
			case 3: doReadFullTOC(1, track, allocationLength); break; // MMC2
			default:
			{
				scsiDev.status = CHECK_CONDITION;
				scsiDev.target->sense.code = ILLEGAL_REQUEST;
				scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
				scsiDev.phase = STATUS;
			}
		}
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

	return commandHandled;
}

