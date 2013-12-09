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
#include "mode.h"
#include "disk.h"

#include <string.h>

static const uint8 DisconnectReconnectPage[] =
{
0x02, // Page code
0x0E, // Page length
0, // Buffer full ratio
0, // Buffer empty ratio
0x00, 10, // Bus inactivity limit, 100us increments. Allow 1ms.
0x00, 0x00, // Disconnect time limit
0x00, 0x00, // Connect time limit
0x00, 0x00, // Maximum burst size
0x00 ,// DTDC. Not used.
0x00, 0x00, 0x00 // Reserved
};

static const uint8 FormatDevicePage[] =
{
0x03, // Page code 
0x16, // Page length
0x00, 0x00, // Single zone
0x00, 0x00, // No alternate sectors
0x00, 0x00, // No alternate tracks
0x00, 0x00, // No alternate tracks per lun
0x00, SCSI_SECTORS_PER_TRACK, // Sectors per track
SCSI_SECTOR_SIZE >> 8, SCSI_SECTOR_SIZE & 0xFF, // Data bytes per physical sector
0x00, 0x01, // Interleave
0x00, 0x00, // Track skew factor
0x00, 0x00, // Cylinder skew factor
0xC0, // SSEC(set) HSEC(set) RMB SURF
0x00, 0x00, 0x00 // Reserved
};

static const uint8 RigidDiskDriveGeometry[] =
{
0x04, // Page code
0x16, // Page length
0xFF, 0xFF, 0xFF, // Number of cylinders
SCSI_HEADS_PER_CYLINDER, // Number of heads
0xFF, 0xFF, 0xFF, // Starting cylinder-write precompensation
0xFF, 0xFF, 0xFF, // Starting cylinder-reduced write current
0x00, 0x1, // Drive step rate (units of 100ns)
0x00, 0x00, 0x00, // Landing zone cylinder
0x00, // RPL
0x00, // Rotational offset
0x00, // Reserved
5400 >> 8, 5400 & 0xFF, // Medium rotation rate (RPM)
0x00, 0x00 // Reserved
};

static const uint8 CachingPage[] =
{
0x08, // Page Code
0x0A, // Page length
0x01, // Read cache disable
0x00, // No useful rention policy.
0x00, 0x00, // Pre-fetch always disabled
0x00, 0x00, // Minimum pre-fetch
0x00, 0x00, // Maximum pre-fetch
0x00, 0x00, // Maximum pre-fetch ceiling
};

static const uint8 ControlModePage[] =
{
0x0A, // Page code
0x06, // Page length
0x00, // No logging
0x01, // Disable tagged queuing
0x00, // No async event notifications
0x00, // Reserved
0x00, 0x00 // AEN holdoff period.
};

// Allow Apple 68k Drive Setup to format this drive.
// Code
// TODO make this string configurable.
static const uint8 AppleVendorPage[] =
{
0x30, // Page code
28, // Page length
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
'A','P','P','L','E',' ','C','O','M','P','U','T','E','R',',',' ','I','N','C','.'
};

static void pageIn(int pc, int dataIdx, const uint8* pageData, int pageLen)
{
	memcpy(&scsiDev.data[dataIdx], pageData, pageLen);

	if (pc == 0x01) // Mask out (un)changable values
	{
		memset(&scsiDev.data[dataIdx+2], 0, pageLen - 2);
	}
}

static void doModeSense(
	int sixByteCmd, int dbd, int pc, int pageCode, int allocLength)
{
	// TODO Apple HD SC Drive Setup requests Page 3 (FormatDevicePage) with an
	// allocLength of 0x20. We need 0x24 if we include a block descriptor, and
	// thus return CHECK CONDITION. A block descriptor is optional, so we
	// chose to ignore it.
	// TODO make configurable
	dbd = 1;
	
	if (pc == 0x03) // Saved Values not supported.
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = SAVING_PARAMETERS_NOT_SUPPORTED;
		scsiDev.phase = STATUS;
	}
	else
	{
		////////////// Mode Parameter Header
		////////////////////////////////////

		// Skip the Mode Data Length, we set that last.
		int idx = 1;
		if (!sixByteCmd) ++idx;

		scsiDev.data[idx++] = 0; // Medium type. 0 = default

		// Device-specific parameter. Contains cache bits (0) and
		// a Write-Protect bit.
		scsiDev.data[idx++] = (blockDev.state & DISK_WP) ? 0x80 : 0;

		if (sixByteCmd)
		{
			if (dbd)
			{
				scsiDev.data[idx++] = 0; // No block descriptor
			}
			else
			{
				// One block descriptor of length 8 bytes.
				scsiDev.data[idx++] = 8;
			}
		}
		else
		{
			scsiDev.data[idx++] = 0; // Reserved
			scsiDev.data[idx++] = 0; // Reserved
			if (dbd)
			{
				scsiDev.data[idx++] = 0; // No block descriptor
				scsiDev.data[idx++] = 0; // No block descriptor
			}
			else
			{
				// One block descriptor of length 8 bytes.
				scsiDev.data[idx++] = 0;
				scsiDev.data[idx++] = 8;
			}
		}

		////////////// Block Descriptor
		////////////////////////////////////
		if (!dbd)
		{
			scsiDev.data[idx++] = 0; // Density code. Reserved for direct-access
			// Number of blocks
			// Zero == all remaining blocks shall have the medium
			// characteristics specified.
			scsiDev.data[idx++] = 0;
			scsiDev.data[idx++] = 0;
			scsiDev.data[idx++] = 0;

			scsiDev.data[idx++] = 0; // reserved

			// Block length
			scsiDev.data[idx++] = SCSI_BLOCK_SIZE >> 16;
			scsiDev.data[idx++] = SCSI_BLOCK_SIZE >> 8;
			scsiDev.data[idx++] = SCSI_BLOCK_SIZE & 0xFF;
		}

		int pageFound = 1;

		switch (pageCode)
		{
		case 0x3F:
			// EVERYTHING

		case 0x02:
			pageIn(pc, idx, DisconnectReconnectPage, sizeof(DisconnectReconnectPage));
			idx += sizeof(DisconnectReconnectPage);
			if (pageCode != 0x3f) break;

		case 0x03:
			pageIn(pc, idx, FormatDevicePage, sizeof(FormatDevicePage));
			idx += sizeof(FormatDevicePage);
			if (pageCode != 0x3f) break;

		case 0x04:
		{
			pageIn(pc, idx, RigidDiskDriveGeometry, sizeof(RigidDiskDriveGeometry));

			if (pc != 0x01)
			{
				// Need to fill out the number of cylinders.
				uint32 cyl;
				uint8 head;
				uint32 sector;
				LBA2CHS(blockDev.capacity, &cyl, &head, &sector);

				scsiDev.data[idx+2] = cyl >> 16;
				scsiDev.data[idx+3] = cyl >> 8;
				scsiDev.data[idx+4] = cyl;

				memcpy(&scsiDev.data[idx+6], &scsiDev.data[idx+2], 3);
				memcpy(&scsiDev.data[idx+9], &scsiDev.data[idx+2], 3);
			}

			idx += sizeof(RigidDiskDriveGeometry);
			if (pageCode != 0x3f) break;
		}

		case 0x08:
			pageIn(pc, idx, CachingPage, sizeof(CachingPage));
			idx += sizeof(CachingPage);
			if (pageCode != 0x3f) break;

		case 0x0A:
			pageIn(pc, idx, ControlModePage, sizeof(ControlModePage));
			idx += sizeof(ControlModePage);
			break;

		case 0x30:
			pageIn(pc, idx, AppleVendorPage, sizeof(AppleVendorPage));
			idx += sizeof(AppleVendorPage);
			break;
			
		default:
			// Unknown Page Code
			pageFound = 0;
			scsiDev.status = CHECK_CONDITION;
			scsiDev.sense.code = ILLEGAL_REQUEST;
			scsiDev.sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.phase = STATUS;
		}


		if (idx > allocLength)
		{
			// Initiator may not have space to receive results.
			scsiDev.status = CHECK_CONDITION;
			scsiDev.sense.code = ILLEGAL_REQUEST;
			scsiDev.sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.phase = STATUS;
		}
		else if (pageFound)
		{
			// Go back and fill out the mode data length
			if (sixByteCmd)
			{
				// Cannot currently exceed limits. yay
				scsiDev.data[0] = idx - 1;
			}
			else
			{
				scsiDev.data[0] = ((idx - 2) >> 8);
				scsiDev.data[1] = (idx - 2);
			}

			scsiDev.dataLen = idx;
			scsiDev.phase = DATA_IN;
		}
		else
		{
			// Initiator may not have space to receive results.
			scsiDev.status = CHECK_CONDITION;
			scsiDev.sense.code = ILLEGAL_REQUEST;
			scsiDev.sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.phase = STATUS;
		}
	}
}

int scsiModeCommand()
{
	int commandHandled = 1;

	uint8 command = scsiDev.cdb[0];

	// We don't currently support the setting of any parameters.
	// (ie. no MODE SELECT(6) or MODE SELECT(10) commands)

	if (command == 0x1A)
	{
		// MODE SENSE(6)
		int dbd = scsiDev.cdb[1] & 0x08; // Disable block descriptors
		int pc = scsiDev.cdb[2] >> 6; // Page Control
		int pageCode = scsiDev.cdb[2] & 0x3F;
		int allocLength = scsiDev.cdb[4];
		if (allocLength == 0) allocLength = 256;
		doModeSense(1, dbd, pc, pageCode, allocLength);
	}
	else if (command == 0x5A)
	{
		// MODE SENSE(10)
		int dbd = scsiDev.cdb[1] & 0x08; // Disable block descriptors
		int pc = scsiDev.cdb[2] >> 6; // Page Control
		int pageCode = scsiDev.cdb[2] & 0x3F;
		int allocLength =
			(((uint16) scsiDev.cdb[7]) << 8) +
			scsiDev.cdb[8];
		doModeSense(0, dbd, pc, pageCode, allocLength);
	}
	else if (command == 0x15)
	{
		// MODE SELECT(6)
		int len = scsiDev.cdb[4];
		if (len == 0) len = 256;
		scsiDev.dataLen = len;
		scsiDev.phase = DATA_OUT;
	}
	else if (command == 0x55)
	{
		// MODE SELECT(10)
		int allocLength = (((uint16) scsiDev.cdb[7]) << 8) + scsiDev.cdb[8];
		scsiDev.dataLen = allocLength;
		scsiDev.phase = DATA_OUT;
	}
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
}


