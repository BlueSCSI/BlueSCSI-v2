//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//  Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com>
//  Copyright (C) 2019 Landon Rodgers <g.landon.rodgers@gmail.com>
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
#include "mode.h"
#include "disk.h"
#include "inquiry.h"
#include "BlueSCSI_mode.h"

#include <string.h>

// "Vendor" defined page which was included by Seagate, and required for\r
// Amiga 500 using DKB SpitFire controller.\r
static const uint8_t OperatingPage[] =
{
0x00, // Page code
0x02, // Page length

// Bit 4 = unit attension (0 = on, 1 = off).
// Bit 7 = usage bit, EEPROM life exceeded warning = 1.
0x80, 

// Bit 7 = reserved.
// Bits 0:6: Device type qualifier, as per Inquiry data
0x00
};

static const uint8_t ReadWriteErrorRecoveryPage[] =
{
0x01, // Page code
0x0A, // Page length

// VMS 5.5-2 is very particular regarding the mode page values.
// The required values for a SCSI2/NoTCQ device are:
// AWRE=0 ARRE=0 TB=1 RC=0 EER=? PER=1 DTE=1 DCR=?
// See ftp://www.digiater.nl/openvms/decus/vms94b/net94b/scsi_params_dkdriver.txt
// X-Newsgroups: comp.os.vms
// Subject: Re: VMS 6.1 vs. Seagate Disk Drives
// Message-Id: <32g87h$8q@nntpd.lkg.dec.com>
// From: weber@evms.enet.dec.com (Ralph O. Weber -- OpenVMS AXP)
// Date: 12 Aug 1994 16:32:49 GMT
0x26,

0x00, // Don't try recovery algorithm during reads
0x00, // Correction span 0
0x00, // Head offset count 0,
0x00, // Data strobe offset count 0,
0x00, // Reserved
0x00, // Don't try recovery algorithm during writes
0x00, // Reserved
0x00, 0x00 // Recovery time limit 0 (use default)*/
};

static const uint8_t ReadWriteErrorRecoveryPage_SCSI1[] =
{
0x01, // Page code
0x06, // Page length
0x26,
0x00, // Don't try recovery algorithm during reads
0x00, // Correction span 0
0x00, // Head offset count 0,
0x00, // Data strobe offset count 0,
0xFF // Reserved
};

static const uint8_t DisconnectReconnectPage[] =
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

static const uint8_t DisconnectReconnectPage_SCSI1[] =
{
0x02, // Page code
0x0A, // Page length
0, // Buffer full ratio
0, // Buffer empty ratio
0x00, 10, // Bus inactivity limit, 100us increments. Allow 1ms.
0x00, 0x00, // Disconnect time limit
0x00, 0x00, // Connect time limit
0x00, 0x00 // Maximum burst size
};

static const uint8_t FormatDevicePage[] =
{
0x03 | 0x80, // Page code | PS (persist) bit.
0x16, // Page length
0x00, 0x00, // Single zone
0x00, 0x00, // No alternate sectors
0x00, 0x00, // No alternate tracks
0x00, 0x00, // No alternate tracks per lun
0x00, 0x00, // Sectors per track, configurable
0xFF, 0xFF, // Data bytes per physical sector. Configurable.
0x00, 0x01, // Interleave
0x00, 0x00, // Track skew factor
0x00, 0x00, // Cylinder skew factor
0xC0, // SSEC(set) HSEC(set) RMB SURF
0x00, 0x00, 0x00 // Reserved
};

static const uint8_t RigidDiskDriveGeometry[] =
{
0x04, // Page code
0x16, // Page length
0xFF, 0xFF, 0xFF, // Number of cylinders
0x00, // Number of heads (replaced by configured value)
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

static const uint8_t FlexibleDiskDriveGeometry[] =
{
0x05, // Page code
0x1E, // Page length
0x01, 0xF4, // Transfer Rate (500kbits)
0x01, // heads
18, // sectors per track
0x20,0x00, // bytes per sector
0x00, 80, // Cylinders
0x00, 0x80, // Write-precomp
0x00, 0x80, // reduced current,
0x00, 0x00, // Drive step rate
0x00, // pulse width
0x00, 0x00, // Head settle delay
0x00, // motor on delay
0x00,  // motor off delay
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00
};

static const uint8_t RigidDiskDriveGeometry_SCSI1[] =
{
0x04, // Page code
0x12, // Page length
0xFF, 0xFF, 0xFF, // Number of cylinders
0x00, // Number of heads (replaced by configured value)
0xFF, 0xFF, 0xFF, // Starting cylinder-write precompensation
0xFF, 0xFF, 0xFF, // Starting cylinder-reduced write current
0x00, 0x1, // Drive step rate (units of 100ns)
0x00, 0x00, 0x00, // Landing zone cylinder
0x00, // RPL
0x00, // Rotational offset
0x00 // Reserved
};

static const uint8_t CachingPage[] =
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

// Old CCS SCSI-1 cache page
static const uint8_t CCSCachingPage[] =
{
0x38, // Page Code
0x0E, // Page length
0x00, // Read cache disable
0x00, // Prefetch threshold
0x00, 0x00, // Max threshold / multiplier
0x00, 0x00, // Min threshold / multiplier
0x00, 0x00, // Reserved
0x00, 0x00,
0x00, 0x00,
0x00, 0x00,
};

static const uint8_t ControlModePage[] =
{
0x0A, // Page code
0x06, // Page length
0x00, // No logging
0x01, // Disable tagged queuing
0x00, // No async event notifications
0x00, // Reserved
0x00, 0x00 // AEN holdoff period.
};

static const uint8_t SequentialDeviceConfigPage[] =
{
0x10, // page code
0x0E, // Page length
0x00, // CAP, CAF, Active Format
0x00, // Active partition
0x00, // Write buffer full ratio
0x00, // Read buffer empty ratio
0x00,0x01, // Write delay time, in 100ms units
0x00, // Default gap size
0x10, // auto-generation of default eod (end of data)
0x00,0x00,0x00, // buffer-size at early warning
0x00, // No data compression
0x00 // reserved
};

// Allow Apple 68k Drive Setup to format this drive.
// Code
static const uint8_t AppleVendorPage[] =
{
0x30, // Page code
23, // Page length
'A','P','P','L','E',' ','C','O','M','P','U','T','E','R',',',' ','I','N','C',' ',' ',' ',0x00
};

static void pageIn(int pc, int dataIdx, const uint8_t* pageData, int pageLen)
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
	////////////// Mode Parameter Header
	////////////////////////////////////

	// Skip the Mode Data Length, we set that last.
	int idx = 1;
	if (!sixByteCmd) ++idx;

	uint8_t mediumType = 0;
	uint8_t deviceSpecificParam = 0;
	uint8_t density = 0;
	switch (scsiDev.target->cfg->deviceType)
	{
	case S2S_CFG_FIXED:
	case S2S_CFG_REMOVEABLE:
		mediumType = 0; // We should support various floppy types here!
		// Contains cache bits (0) and a Write-Protect bit.
		deviceSpecificParam =
			(blockDev.state & DISK_WP) ? 0x80 : 0;
		density = 0; // reserved for direct access
		break;

	case S2S_CFG_FLOPPY_14MB:
		mediumType = 0x1E; // 90mm/3.5"
		deviceSpecificParam =
			(blockDev.state & DISK_WP) ? 0x80 : 0;
		density = 0; // reserved for direct access
		break;

	case S2S_CFG_OPTICAL:
		mediumType = 0x02; // 120mm CDROM, data only.
		deviceSpecificParam = 0;
		density = 0x01; // User data only, 2048bytes per sector.
		break;

	case S2S_CFG_SEQUENTIAL:
		mediumType = 0; // reserved
		deviceSpecificParam =
			(blockDev.state & DISK_WP) ? 0x80 : 0;
		density = 0x13; // DAT Data Storage, X3B5/88-185A 
		break;

	case S2S_CFG_MO:
        mediumType = 0x03; // Optical reversible or erasable medium
		deviceSpecificParam =
			(blockDev.state & DISK_WP) ? 0x80 : 0;
		density = 0x00; // Default
		break;

	};

	scsiDev.data[idx++] = mediumType;
	scsiDev.data[idx++] = deviceSpecificParam;

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
		scsiDev.data[idx++] = density;
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

	int pageFound = 0;

	if (pageCode == 0x01 || pageCode == 0x3F)
	{
		pageFound = 1;
		if ((scsiDev.compatMode >= COMPAT_SCSI2))
		{
			pageIn(pc, idx, ReadWriteErrorRecoveryPage, sizeof(ReadWriteErrorRecoveryPage));
			idx += sizeof(ReadWriteErrorRecoveryPage);
		}
		else
		{
			pageIn(pc, idx, ReadWriteErrorRecoveryPage_SCSI1, sizeof(ReadWriteErrorRecoveryPage_SCSI1));
			idx += sizeof(ReadWriteErrorRecoveryPage_SCSI1);
		}
	}

	if (pageCode == 0x02 || pageCode == 0x3F)
	{
		pageFound = 1;
		if ((scsiDev.compatMode >= COMPAT_SCSI2))
		{
			pageIn(pc, idx, DisconnectReconnectPage, sizeof(DisconnectReconnectPage));
			idx += sizeof(DisconnectReconnectPage);
		}
		else
		{
			pageIn(pc, idx, DisconnectReconnectPage_SCSI1, sizeof(DisconnectReconnectPage_SCSI1));
			idx += sizeof(DisconnectReconnectPage_SCSI1);
		}
	}

	if (pageCode == 0x03 || pageCode == 0x3F)
	{
		pageFound = 1;
		pageIn(pc, idx, FormatDevicePage, sizeof(FormatDevicePage));
		if (pc != 0x01)
		{
			uint16_t sectorsPerTrack = scsiDev.target->cfg->sectorsPerTrack;
			scsiDev.data[idx+10] = sectorsPerTrack >> 8;
			scsiDev.data[idx+11] = sectorsPerTrack & 0xFF;

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
	}

	if (pageCode == 0x04 || pageCode == 0x3F)
	{
		pageFound = 1;
		if ((scsiDev.compatMode >= COMPAT_SCSI2))
		{
			pageIn(pc, idx, RigidDiskDriveGeometry, sizeof(RigidDiskDriveGeometry));
		}
		else
		{
			pageIn(pc, idx, RigidDiskDriveGeometry_SCSI1, sizeof(RigidDiskDriveGeometry_SCSI1));
		}

		if (pc != 0x01)
		{
			// Need to fill out the number of cylinders.
			uint32_t cyl;
			uint8_t head;
			uint32_t sector;
			LBA2CHS(
				getScsiCapacity(
					scsiDev.target->cfg->sdSectorStart,
					scsiDev.target->liveCfg.bytesPerSector,
					scsiDev.target->cfg->scsiSectors),
				&cyl,
				&head,
				&sector,
				scsiDev.target->cfg->headsPerCylinder,
				scsiDev.target->cfg->sectorsPerTrack);

			scsiDev.data[idx+2] = cyl >> 16;
			scsiDev.data[idx+3] = cyl >> 8;
			scsiDev.data[idx+4] = cyl;

			memcpy(&scsiDev.data[idx+6], &scsiDev.data[idx+2], 3);
			memcpy(&scsiDev.data[idx+9], &scsiDev.data[idx+2], 3);

			scsiDev.data[idx+5] = scsiDev.target->cfg->headsPerCylinder;
		}

		if ((scsiDev.compatMode >= COMPAT_SCSI2))
		{
			idx += sizeof(RigidDiskDriveGeometry);
		}
		else
		{
			idx += sizeof(RigidDiskDriveGeometry_SCSI1);
		}
	}

	if ((pageCode == 0x05 || pageCode == 0x3F) &&
		(scsiDev.target->cfg->deviceType == S2S_CFG_FLOPPY_14MB))
	{
		pageFound = 1;
		pageIn(pc, idx, FlexibleDiskDriveGeometry, sizeof(FlexibleDiskDriveGeometry));
		idx += sizeof(FlexibleDiskDriveGeometry);
	}

	// DON'T output the following pages for SCSI1 hosts. They get upset when
	// we have more data to send than the allocation length provided.
	// (ie. Try not to output any more pages below this comment)


	if ((scsiDev.compatMode >= COMPAT_SCSI2) &&
		(pageCode == 0x08 || pageCode == 0x3F))
	{
		pageFound = 1;
		pageIn(pc, idx, CachingPage, sizeof(CachingPage));
		idx += sizeof(CachingPage);
	}

	if ((scsiDev.compatMode >= COMPAT_SCSI2)
		&& (pageCode == 0x0A || pageCode == 0x3F))
	{
		pageFound = 1;
		pageIn(pc, idx, ControlModePage, sizeof(ControlModePage));
		idx += sizeof(ControlModePage);
	}

	idx += modeSenseCDDevicePage(pc, idx, pageCode, &pageFound);
	idx += modeSenseCDAudioControlPage(pc, idx, pageCode, &pageFound);

	if ((scsiDev.target->cfg->deviceType == S2S_CFG_SEQUENTIAL) &&
		(pageCode == 0x10 || pageCode == 0x3F))
	{
		pageFound = 1;
		pageIn(
			pc,
			idx,
			SequentialDeviceConfigPage,
			sizeof(SequentialDeviceConfigPage));
		idx += sizeof(SequentialDeviceConfigPage);
	}

	if ((
			(scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_APPLE) ||
			(idx + sizeof(AppleVendorPage) <= allocLength)
		) &&
		(pageCode == 0x30 || pageCode == 0x3F))
	{
		pageFound = 1;
		pageIn(pc, idx, AppleVendorPage, sizeof(AppleVendorPage));
		idx += sizeof(AppleVendorPage);
	}

	if (pageCode == 0x38) // Don't send unless requested
	{
		pageFound = 1;
		pageIn(pc, idx, CCSCachingPage, sizeof(CCSCachingPage));
		idx += sizeof(CCSCachingPage);
	}

	// SCSI 2 standard says page 0 is always last.
	if (pageCode == 0x00 || pageCode == 0x3F)
	{
		pageFound = 1;
		pageIn(pc, idx, OperatingPage, sizeof(OperatingPage));

		// Note inverted logic for the flag.
		scsiDev.data[idx+2] =
			(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_UNIT_ATTENTION) ? 0x80 : 0x90;

		scsiDev.data[idx+3] = getDeviceTypeQualifier();

		idx += sizeof(OperatingPage);
	}

	if (!pageFound)
	{
		// Unknown Page Code
		pageFound = 0;
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}
	else
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

		scsiDev.dataLen = idx > allocLength ? allocLength : idx;
		scsiDev.phase = DATA_IN;
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
					s2s_configSave(scsiDev.target->targetId, bytesPerSector);
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
					s2s_configSave(scsiDev.target->targetId, bytesPerSector);
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

	uint8_t command = scsiDev.cdb[0];

	// We don't currently support the setting of any parameters.
	// (ie. no MODE SELECT(6) or MODE SELECT(10) commands)

	if (command == 0x1A)
	{
		// MODE SENSE(6)
		int dbd = scsiDev.cdb[1] & 0x08; // Disable block descriptors
		int pc = scsiDev.cdb[2] >> 6; // Page Control
		int pageCode = scsiDev.cdb[2] & 0x3F;
		int allocLength = scsiDev.cdb[4];

		// SCSI1 standard: (CCS X3T9.2/86-52)
		// "An Allocation Length of zero indicates that no MODE SENSE data shall
		// be transferred. This condition shall not be considered as an error."
		doModeSense(1, dbd, pc, pageCode, allocLength);
	}
	else if (command == 0x5A)
	{
		// MODE SENSE(10)
		int dbd = scsiDev.cdb[1] & 0x08; // Disable block descriptors
		int pc = scsiDev.cdb[2] >> 6; // Page Control
		int pageCode = scsiDev.cdb[2] & 0x3F;
		int allocLength =
			(((uint16_t) scsiDev.cdb[7]) << 8) +
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
		int allocLength = (((uint16_t) scsiDev.cdb[7]) << 8) + scsiDev.cdb[8];
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

