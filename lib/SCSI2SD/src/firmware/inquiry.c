//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//	Copyright (C) 2019 Landon Rodgers  <g.landon.rodgers@gmail.com>
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
//
// This work incorporates work from the following
//  Copyright (c) 2023 joshua stein <jcs@jcs.org>

#include "scsi.h"
#include "config.h"
#include "inquiry.h"

#include <string.h>

static uint8_t StandardResponse[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x00, // device type modifier
0x02, // Complies with ANSI SCSI-2.
0x01, // Response format is compatible with the old CCS format.
0x1f, // standard length.
0, 0, // Reserved
0x18 // Enable sync and linked commands
};
// Vendor set by config 'c','o','d','e','s','r','c',' ',
// prodId set by config'S','C','S','I','2','S','D',' ',' ',' ',' ',' ',' ',' ',' ',' ',
// Revision set by config'2','.','0','a'


static const uint8_t SupportedVitalPages[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x00, // Page Code
0x00, // Reserved
0x04, // Page length
0x00, // Support "Supported vital product data pages"
0x80, // Support "Unit serial number page"
0x81, // Support "Implemented operating definition page"
0x82 // Support "ASCII Implemented operating definition page"
};

static const uint8_t UnitSerialNumber[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x80, // Page Code
0x00, // Reserved
0x10, // Page length
'c','o','d','e','s','r','c','-','1','2','3','4','5','6','7','8'
};

static const uint8_t ImpOperatingDefinition[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x81, // Page Code
0x00, // Reserved
0x03, // Page length
0x03, // Current: SCSI-2 operating definition
0x03, // Default: SCSI-2 operating definition
0x03 // Supported (list): SCSI-2 operating definition.
};

static const uint8_t AscImpOperatingDefinition[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x82, // Page Code
0x00, // Reserved
0x07, // Page length
0x06, // Ascii length
'S','C','S','I','-','2'
};

void s2s_scsiInquiry()
{
	uint8_t evpd = scsiDev.cdb[1] & 1; // enable vital product data.
	uint8_t pageCode = scsiDev.cdb[2];
	uint32_t allocationLength = scsiDev.cdb[4];

	// SASI standard, X3T9.3_185_RevE  states that 0 == 256 bytes
	// BUT SCSI 2 standard says 0 == 0.
	if (scsiDev.compatMode <= COMPAT_SCSI1) // excludes COMPAT_SCSI2_DISABLED
	{
		if (allocationLength == 0) allocationLength = 256;
	}

	if (!evpd)
	{
		if (pageCode)
		{
			// error.
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = ILLEGAL_REQUEST;
			scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.phase = STATUS;
		}
		else
		{
			const S2S_TargetCfg* config = scsiDev.target->cfg;
			scsiDev.dataLen =
				s2s_getStandardInquiry(
					config,
					scsiDev.data,
					sizeof(scsiDev.data));
			scsiDev.phase = DATA_IN;
		}
	}
	else if (pageCode == 0x00)
	{
		memcpy(scsiDev.data, SupportedVitalPages, sizeof(SupportedVitalPages));
		scsiDev.dataLen = sizeof(SupportedVitalPages);
		scsiDev.phase = DATA_IN;
	}
	else if (pageCode == 0x80)
	{
		memcpy(scsiDev.data, UnitSerialNumber, sizeof(UnitSerialNumber));
		scsiDev.dataLen = sizeof(UnitSerialNumber);
        const S2S_TargetCfg* config = scsiDev.target->cfg;
        memcpy(&scsiDev.data[4], config->serial, sizeof(config->serial));
		scsiDev.phase = DATA_IN;
	}
	else if (pageCode == 0x81)
	{
		memcpy(
			scsiDev.data,
			ImpOperatingDefinition,
			sizeof(ImpOperatingDefinition));
		scsiDev.dataLen = sizeof(ImpOperatingDefinition);
		scsiDev.phase = DATA_IN;
	}
	else if (pageCode == 0x82)
	{
		memcpy(
			scsiDev.data,
			AscImpOperatingDefinition,
			sizeof(AscImpOperatingDefinition));
		scsiDev.dataLen = sizeof(AscImpOperatingDefinition);
		scsiDev.phase = DATA_IN;
	}
	else
	{
		// error.
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}


	if (scsiDev.phase == DATA_IN)
	{
		// VAX workaround
		if (allocationLength == 255 &&
			(scsiDev.target->cfg->quirks & S2S_CFG_QUIRKS_VMS))
		{
			allocationLength = 254;
		}

		// "real" hard drives send back exactly allocationLenth bytes, padded
		// with zeroes. This only seems to happen for Inquiry responses, and not
		// other commands that also supply an allocation length such as Mode Sense or
		// Request Sense.
		// (See below for exception to this rule when 0 allocation length)
		if (scsiDev.dataLen < allocationLength)
		{
			memset(
				&scsiDev.data[scsiDev.dataLen],
				0,
				allocationLength - scsiDev.dataLen);
		}
		// Spec 8.2.5 requires us to simply truncate the response if it's
		// too big.
		scsiDev.dataLen = allocationLength;

		// Set the device type as needed.
		scsiDev.data[0] = getDeviceTypeQualifier();

		switch (scsiDev.target->cfg->deviceType)
		{
		case S2S_CFG_OPTICAL:
			scsiDev.data[1] |= 0x80; // Removable bit.
			break;

		case S2S_CFG_SEQUENTIAL:
			scsiDev.data[1] |= 0x80; // Removable bit.
			break;

		case S2S_CFG_MO:
			scsiDev.data[1] |= 0x80; // Removable bit.
			break;

		case S2S_CFG_FLOPPY_14MB:
		case S2S_CFG_REMOVEABLE:
			scsiDev.data[1] |= 0x80; // Removable bit.
			break;

		case S2S_CFG_NETWORK:
			scsiDev.data[2] = 0x01;  // Page code.
			break;
		
		default:
			// Accept defaults for a fixed disk.
			break;
		}
	}

	// Set the first byte to indicate LUN presence.
	if (scsiDev.lun) // We only support lun 0
	{
		scsiDev.data[0] = 0x7F;
	}
}

uint32_t s2s_getStandardInquiry(
	const S2S_TargetCfg* cfg, uint8_t* out, uint32_t maxlen
	)
{
	uint32_t buflen = sizeof(StandardResponse);
	if (buflen > maxlen) buflen = maxlen;

	memcpy(out, StandardResponse, buflen);
	out[1] = cfg->deviceTypeModifier;

	if (!(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_SCSI2))
	{
		out[2] = 1; // Report only SCSI 1 compliance version
	}

	if (scsiDev.compatMode >= COMPAT_SCSI2)
	{
		out[3] = 2; // SCSI 2 response format.
	}
	memcpy(&out[8], cfg->vendor, sizeof(cfg->vendor));
	memcpy(&out[16], cfg->prodId, sizeof(cfg->prodId));
	memcpy(&out[32], cfg->revision, sizeof(cfg->revision));
	return sizeof(StandardResponse) +
		sizeof(cfg->vendor) +
		sizeof(cfg->prodId) +
		sizeof(cfg->revision);
}

uint8_t getDeviceTypeQualifier()
{
	// Set the device type as needed.
	switch (scsiDev.target->cfg->deviceType)
	{
	case S2S_CFG_OPTICAL:
		return 0x05;
		break;

	case S2S_CFG_SEQUENTIAL:
		return 0x01;
		break;

	case S2S_CFG_MO:
		return 0x07;
		break;

	case S2S_CFG_FLOPPY_14MB:
	case S2S_CFG_REMOVEABLE:
		return 0;
		break;

	case S2S_CFG_NETWORK:
		// processor device
		return 0x03;
		break;

	default:
		// Accept defaults for a fixed disk.
		return 0;
	}
}

