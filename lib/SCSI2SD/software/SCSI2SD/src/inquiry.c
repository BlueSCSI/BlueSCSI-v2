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
#pragma GCC push_options
#pragma GCC optimize("-flto")

#include "device.h"
#include "scsi.h"
#include "config.h"
#include "inquiry.h"

#include <string.h>

static uint8 StandardResponse[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x00, // device type modifier
0x02, // Complies with ANSI SCSI-2.
0x01, // Response format is compatible with the old CCS format.
0x1f, // standard length.
0, 0, // Reserved
0x08 // Enable linked commands
};
// Vendor set by config 'c','o','d','e','s','r','c',' ',
// prodId set by config'S','C','S','I','2','S','D',' ',' ',' ',' ',' ',' ',' ',' ',' ',
// Revision set by config'2','.','0','a'

/* For reference, here's a dump from an Apple branded 500Mb drive from 1994.
$ sudo sg_inq -H /dev/sdd --len 255
standard INQUIRY:
 00     00 00 02 01 31 00 00 18  51 55 41 4e 54 55 4d 20    ....1...QUANTUM 
 10     4c 50 53 32 37 30 20 20  20 20 20 20 20 20 20 20    LPS270          
 20     30 39 30 30 00 00 00 d9  b0 27 34 01 04 b3 01 1b    0900.....'4.....
 30     07 00 a0 00 00 ff                                   ......
 Vendor identification: QUANTUM 
 Product identification: LPS270          
 Product revision level: 0900
*/


static const uint8 SupportedVitalPages[] =
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

static const uint8 UnitSerialNumber[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x80, // Page Code
0x00, // Reserved
0x10, // Page length
'c','o','d','e','s','r','c','-','1','2','3','4','5','6','7','8'
};

static const uint8 ImpOperatingDefinition[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x81, // Page Code
0x00, // Reserved
0x03, // Page length
0x03, // Current: SCSI-2 operating definition
0x03, // Default: SCSI-2 operating definition
0x03 // Supported (list): SCSI-2 operating definition.
};

static const uint8 AscImpOperatingDefinition[] =
{
0x00, // "Direct-access device". AKA standard hard disk
0x82, // Page Code
0x00, // Reserved
0x07, // Page length
0x06, // Ascii length
'S','C','S','I','-','2'
};

void scsiInquiry()
{
	uint8 evpd = scsiDev.cdb[1] & 1; // enable vital product data.
	uint8 pageCode = scsiDev.cdb[2];
	uint32 allocationLength = scsiDev.cdb[4];

	// SASI standard, X3T9.3_185_RevE  states that 0 == 256 bytes
	if (allocationLength == 0) allocationLength = 256;

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
			const TargetConfig* config = scsiDev.target->cfg;
			memcpy(scsiDev.data, StandardResponse, sizeof(StandardResponse));
			scsiDev.data[1] = scsiDev.target->cfg->deviceTypeModifier;
			memcpy(&scsiDev.data[8], config->vendor, sizeof(config->vendor));
			memcpy(&scsiDev.data[16], config->prodId, sizeof(config->prodId));
			memcpy(&scsiDev.data[32], config->revision, sizeof(config->revision));
			scsiDev.dataLen = sizeof(StandardResponse) +
				sizeof(config->vendor) +
				sizeof(config->prodId) +
				sizeof(config->revision);
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
		switch (scsiDev.target->cfg->deviceType)
		{
		case CONFIG_OPTICAL:
			scsiDev.data[0] = 0x05; // device type
			scsiDev.data[1] |= 0x80; // Removable bit.
			break;

		case CONFIG_SEQUENTIAL:
			scsiDev.data[0] = 0x01; // device type
			scsiDev.data[1] |= 0x80; // Removable bit.
			break;
			
		case CONFIG_MO:
			scsiDev.data[0] = 0x07; // device type
			scsiDev.data[1] |= 0x80; // Removable bit.
			break;

		case CONFIG_FLOPPY_14MB:
		case CONFIG_REMOVEABLE:
			scsiDev.data[1] |= 0x80; // Removable bit.
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

#pragma GCC pop_options
