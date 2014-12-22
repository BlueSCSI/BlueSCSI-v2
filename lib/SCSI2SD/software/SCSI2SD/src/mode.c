//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//  Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com>
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

static const uint8 ReadWriteErrorRecoveryPage[] =
{
0x01, // Page code
0x0A, // Page length
0x00, // No error recovery options for now
0x00, // Don't try recovery algorithm during reads
0x00, // Correction span 0
0x00, // Head offset count 0,
0x00, // Data strobe offset count 0,
0x00, // Reserved
0x00, // Don't try recovery algorithm during writes
0x00, // Reserved
0x00, 0x00 // Recovery time limit 0 (use default)*/
};

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
0x03 | 0x80, // Page code | PS (persist) bit.
0x16, // Page length
0x00, 0x00, // Single zone
0x00, 0x00, // No alternate sectors
0x00, 0x00, // No alternate tracks
0x00, 0x00, // No alternate tracks per lun
0x00, SCSI_SECTORS_PER_TRACK, // Sectors per track
0xFF, 0xFF, // Data bytes per physical sector. Configurable.
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
	if (pc == 0x03) // Saved Values not supported.
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = SAVING_PARAMETERS_NOT_SUPPORTED;
		scsiDev.phase = STATUS;
	}
	else
	{
		int pageFound = 1;

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
			uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
			scsiDev.data[idx++] = bytesPerSector >> 16;
			scsiDev.data[idx++] = bytesPerSector >> 8;
			scsiDev.data[idx++] = bytesPerSector & 0xFF;
		}

		switch (pageCode)
		{
		case 0x3F:
			// EVERYTHING

		case 0x01:
			pageIn(pc, idx, ReadWriteErrorRecoveryPage, sizeof(ReadWriteErrorRecoveryPage));
			idx += sizeof(ReadWriteErrorRecoveryPage);
			if (pageCode != 0x3f) break;

		case 0x02:
			pageIn(pc, idx, DisconnectReconnectPage, sizeof(DisconnectReconnectPage));
			idx += sizeof(DisconnectReconnectPage);
			if (pageCode != 0x3f) break;

		case 0x03:
			pageIn(pc, idx, FormatDevicePage, sizeof(FormatDevicePage));
			if (pc != 0x01)
			{
				// Fill out the configured bytes-per-sector
				uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
				scsiDev.data[idx+12] = bytesPerSector >> 8;
				scsiDev.data[idx+13] = bytesPerSector & 0xFF;
			}
			else
			{
				// Set a mask for the changeable values.
				scsiDev.data[idx+12] = 0xFF;
				scsiDev.data[idx+13] = 0xFF;
			}

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
				LBA2CHS(
					getScsiCapacity(
						scsiDev.target->cfg->sdSectorStart,
						scsiDev.target->liveCfg.bytesPerSector,
						scsiDev.target->cfg->scsiSectors),
					&cyl,
					&head,
					&sector);

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
			scsiDev.target->sense.code = ILLEGAL_REQUEST;
			scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.phase = STATUS;
		}


		if (idx > allocLength)
		{
			// Chop the reply off early if shorter length is requested
			idx = allocLength;
		}

		if (pageFound)
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
			// Page not found
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = ILLEGAL_REQUEST;
			scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.phase = STATUS;
		}
	}
}

// Callback after the DATA OUT phase is complete.
static void doModeSelect(void)
{
	if (scsiDev.status == GOOD) // skip if we've already encountered an error
	{
		// scsiDev.dataLen bytes are in scsiDev.data

		int idx;
		int blockDescLen;
		if (scsiDev.cdb[0] == 0x55)
		{
			blockDescLen =
				(((uint16_t)scsiDev.data[6]) << 8) |scsiDev.data[7];
			idx = 8;
		}
		else
		{
			blockDescLen = scsiDev.data[3];
			idx = 4;
		}

		// The unwritten rule.  Blocksizes are normally set using the
		// block descriptor value, not by changing page 0x03.
		if (blockDescLen >= 8)
		{
			uint32_t bytesPerSector =
				(((uint32_t)scsiDev.data[idx+5]) << 16) |
				(((uint32_t)scsiDev.data[idx+6]) << 8) |
				scsiDev.data[idx+7];
			if ((bytesPerSector < MIN_SECTOR_SIZE) ||
				(bytesPerSector > MAX_SECTOR_SIZE))
			{
				goto bad;
			}
			else
			{
				scsiDev.target->liveCfg.bytesPerSector = bytesPerSector;
				if (bytesPerSector != scsiDev.target->cfg->bytesPerSector)
				{
					configSave(scsiDev.target->targetId, bytesPerSector);
				}
			}
		}
		idx += blockDescLen;

		while (idx < scsiDev.dataLen)
		{
			int pageLen = scsiDev.data[idx + 1];
			if (idx + 2 + pageLen > scsiDev.dataLen) goto bad;

			int pageCode = scsiDev.data[idx] & 0x3F;
			switch (pageCode)
			{
			case 0x03: // Format Device Page
			{
				if (pageLen != 0x16) goto bad;

				// Fill out the configured bytes-per-sector
				uint16_t bytesPerSector =
					(((uint16_t)scsiDev.data[idx+12]) << 8) |
					scsiDev.data[idx+13];

				// Sane values only, ok ?
				if ((bytesPerSector < MIN_SECTOR_SIZE) ||
					(bytesPerSector > MAX_SECTOR_SIZE))
				{
					goto bad;
				}

				scsiDev.target->liveCfg.bytesPerSector = bytesPerSector;
				if (scsiDev.cdb[1] & 1) // SP Save Pages flag
				{
					configSave(scsiDev.target->targetId, bytesPerSector);
				}
			}
			break;
			//default:

				// Easiest to just ignore for now. We'll get here when changing
				// the SCSI block size via the descriptor header.
			}
			idx += 2 + pageLen;
		}
	}

	goto out;
bad:
	scsiDev.status = CHECK_CONDITION;
	scsiDev.target->sense.code = ILLEGAL_REQUEST;
	scsiDev.target->sense.asc = INVALID_FIELD_IN_PARAMETER_LIST;

out:
	scsiDev.phase = STATUS;
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
		if (len == 0)
		{
			// If len == 0, then transfer no data. From the SCSI 2 standard:
			//      A parameter list length of zero indicates that no data shall
			//      be transferred. This condition shall not be considered as an
			//		error.
			scsiDev.phase = STATUS;
		}
		else
		{
			scsiDev.dataLen = len;
			scsiDev.phase = DATA_OUT;
			scsiDev.postDataOutHook = doModeSelect;
		}
	}
	else if (command == 0x55)
	{
		// MODE SELECT(10)
		int allocLength = (((uint16) scsiDev.cdb[7]) << 8) + scsiDev.cdb[8];
		if (allocLength == 0)
		{
			// If len == 0, then transfer no data. From the SCSI 2 standard:
			//      A parameter list length of zero indicates that no data shall
			//      be transferred. This condition shall not be considered as an
			//		error.
			scsiDev.phase = STATUS;
		}
		else
		{
			scsiDev.dataLen = allocLength;
			scsiDev.phase = DATA_OUT;
			scsiDev.postDataOutHook = doModeSelect;
		}
	}
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
}


