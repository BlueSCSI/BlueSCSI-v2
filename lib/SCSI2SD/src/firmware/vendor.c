//	Copyright (C) 2016 Michael McMaster <michael@codesrc.com>
//	Copyright (C) 2024 Jokker <jokker@gmail.com>
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
#include "vendor.h"
#include "diagnostic.h"
#include "bluescsi_toolbox.h"
#include <string.h>

// External format functions from BlueSCSI_disk.cpp
extern bool scsiDiskFormatUnit(uint8_t pattern);
extern bool scsiDiskFormatRange(uint32_t startLBA, uint32_t numBlocks, uint8_t pattern);
// Callback after the DATA OUT phase is complete.
static void doAssignDiskParameters(void)
{
	if (scsiDev.status == GOOD)
	{
		scsiDev.phase = STATUS;
	}
}

// XEBEC specific commands
// http://www.bitsavers.org/pdf/xebec/104524C_S1410Man_Aug83.pdf
// WD100x seems to be identical to the Xebec but calls this command "Set Parameters"
// http://www.bitsavers.org/pdf/westernDigital/WD100x/79-000004_WD1002-SHD_OEM_Manual_Aug1984.pdf
static void doXebecInitializeDriveCharacteristics()
{
	if (scsiDev.status == GOOD)
	{
		if (scsiDev.dataLen >= 8)
		{
			int cylinders = ((uint16_t)scsiDev.data[0] << 8) + scsiDev.data[1];
			int heads = scsiDev.data[2];
			int reducedWrite = ((uint16_t)scsiDev.data[3] << 8) + scsiDev.data[4];
			int writePrecomp = ((uint16_t)scsiDev.data[5] << 8) + scsiDev.data[6];
			int eccBurst = scsiDev.data[7];
			
			// Enhanced Xebec parameter validation
			if (cylinders > 0 && cylinders <= 65535 && heads > 0 && heads <= 255)
			{
				// Store parameters in device configuration if needed
				// This mimics what a real Xebec controller would do
				DBGMSG_F("Xebec Initialize Drive Characteristics: cylinders=%d heads=%d reducedWrite=%d writePrecomp=%d eccBurst=%d", 
				        cylinders, heads, reducedWrite, writePrecomp, eccBurst);
				        
				// Acknowledge successful parameter reception
				scsiDev.status = GOOD;
			}
			else
			{
				// Invalid parameters - reject command
				scsiDev.status = CHECK_CONDITION;
				scsiDev.target->sense.code = ILLEGAL_REQUEST;
				scsiDev.target->sense.asc = INVALID_FIELD_IN_PARAMETER_LIST;
			}
		}
		else
		{
			// Insufficient data received
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = ILLEGAL_REQUEST;
			scsiDev.target->sense.asc = PARAMETER_LIST_LENGTH_ERROR;
		}scsiDev.phase = STATUS;
	}
}

int scsiVendorCommand()
{
	int commandHandled = 1;

	uint8_t command = scsiDev.cdb[0];

	// iomega sense command
	if (command == 0x06 && scsiDev.target->cfg->deviceType == S2S_CFG_ZIP100)
	{
		int subcommand = scsiDev.cdb[2];
		uint8_t alloc_length = scsiDev.cdb[4];
		scsiDev.phase = DATA_IN;

		// byte 0 is the page
		scsiDev.data[0] = subcommand;

		if (subcommand == 0x1)
		{
			// page is 86 bytes in length
			scsiDev.dataLen = alloc_length < 0x58 ? alloc_length : 0x58;
			memset(&scsiDev.data[1], 0xff, scsiDev.dataLen);
			// byte 1 is the page length minus pagecode and length
			scsiDev.data[1] = scsiDev.dataLen - 2;

			scsiDev.data[2] = 1;
			scsiDev.data[3] = 0;
			scsiDev.data[4] = 0;
			scsiDev.data[5] = 0;
			scsiDev.data[6] = 0x5;
			scsiDev.data[7] = 0xdc;
			scsiDev.data[8] = 0x6;
			scsiDev.data[9] = 0xc;
			scsiDev.data[10] = 0x5;
			scsiDev.data[11] = 0xdc;
			scsiDev.data[12] = 0x6;
			scsiDev.data[13] = 0xc;
			scsiDev.data[14] = 0;
		}
		else if (subcommand == 0x2) {
			// page is 61 bytes in length
			scsiDev.dataLen = alloc_length < 0x3f ? alloc_length : 0x3f;
			memset(&scsiDev.data[1], 0, scsiDev.dataLen);
			// byte 1 is the page length minus pagecode and length
			scsiDev.data[1] = scsiDev.dataLen - 2;

			scsiDev.data[3] = 2;
			scsiDev.data[6] = 0x2;
			scsiDev.data[7] = 0xff;
			scsiDev.data[8] = 0xff;
			// this has something to do with the format/disk life
			// currently this makes it 100%
			scsiDev.data[14] = 0x7e;
			scsiDev.data[18] = 0x7e;

			// byte 21 is the read/write/password settings
			// 5 = password for R/W
			// 3 = password for W
			// 2 = RO
			// 0 = RW
			scsiDev.data[20] = 0;

			// set a serial number ABCDEFGHIJKLMNO
			// starts at byte 25 and is 15 bytes long
			for(int i = 0; i < 20; i++) {
				scsiDev.data[25 + i] = i + 0x41;
			}

			scsiDev.data[0x3e] = 1;
		}
		else
		{
			// anything else is an illegal command
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = ILLEGAL_REQUEST;
			scsiDev.target->sense.asc = LOGICAL_UNIT_NOT_SUPPORTED;
			scsiDev.phase = STATUS;
		}

	}
	else if (command == 0xC0)
	{
		// Define flexible disk format
		// OMTI-5204 controller
		// http://bitsavers.informatik.uni-stuttgart.de/pdf/sms/OMTI_5x00.pdf
		// Stub. Sectors-per-track should be configured by scsi2sd-util
	}
	else if (command == 0xC2)
	{
		// Assign Disk Parameters command
		// OMTI-5204 controller
		// http://bitsavers.informatik.uni-stuttgart.de/pdf/sms/OMTI_5x00.pdf
		// Stub to read and discard 10 bytes.
		scsiDev.dataLen = 10;
		scsiDev.phase = DATA_OUT;
		scsiDev.postDataOutHook = doAssignDiskParameters;
	}
	else if (command == 0x0C &&
		scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
	{
		// XEBEC S1410: "Initialize Drive Characteristics"
		// WD100x: "Set Parameters"
		scsiDev.dataLen = 8;
		scsiDev.phase = DATA_OUT;
		scsiDev.postDataOutHook = doXebecInitializeDriveCharacteristics;
	}
	else if (command == 0x0F &&
		scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
	{
		// XEBEC S1410, WD100x: "Write Sector Buffer"
		scsiDev.dataLen = scsiDev.target->liveCfg.bytesPerSector;
		scsiDev.phase = DATA_OUT;
		scsiDev.postDataOutHook = doWriteBuffer;
	}
	else if (command == 0x04 &&
		scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
	{
		// XEBEC Format Unit (4)
		// Format entire drive with 0x6C pattern
		DBGMSG_F("Xebec Format Unit - starting full drive format with 0x6C pattern");
		
		if (scsiDiskFormatUnit(0x6C))
		{
			DBGMSG_F("Xebec Format Unit completed successfully");
			scsiDev.status = GOOD;
		}
		else
		{
			DBGMSG_F("Xebec Format Unit failed");
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = MEDIUM_ERROR;
			scsiDev.target->sense.asc = WRITE_ERROR_AUTO_REALLOCATION_FAILED;
		}
		scsiDev.phase = STATUS;
	}
	else if (command == 0x06 &&
		scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
	{
		commandHandled = 1;
		// XEBEC Format Track (6) 
		// Format specific track using CHS addressing with 0x6C pattern
		// CDB format: [0]=0x06, [1]=LUN, [2]=CYADR_H, [3]=CYADR_L, [4]=INTERLEAVE (5 LSB) + 000 (3 MSB), [5]=STEP_OPTION (3 LSB) + control (5 MSB)
		// Logical Address = (CYADR * HDCYL + HDADR) * SETRK + SEADR
		uint16_t cylinder = (((uint16_t)scsiDev.cdb[2]) << 8) | ((uint16_t)scsiDev.cdb[3]);
		uint8_t interleave = scsiDev.cdb[4] & 0x1F; // Extract 5 LSB bits for interleave
		uint8_t stepOption = scsiDev.cdb[5] & 0x07; // Extract 3 LSB bits for step option
		
		// For Xebec Format Track, format all heads on the specified cylinder
		// Get drive geometry
		uint32_t headsPerCylinder = scsiDev.target->cfg->headsPerCylinder;
		uint32_t sectorsPerTrack = scsiDev.target->cfg->sectorsPerTrack;
		
		// Calculate starting logical address for this cylinder
		// Logical Address = (CYADR * HDCYL + HDADR) * SETRK + SEADR
		// Format all tracks on this cylinder (all heads)
		uint32_t startLBA = ((uint32_t)cylinder * headsPerCylinder) * sectorsPerTrack;
		uint32_t blocks = headsPerCylinder * sectorsPerTrack; // Format entire cylinder
		
		DBGMSG_F("Xebec Format Track - CYL=%d INTERLEAVE=%d STEP=%d, LBA=%d + %d blocks with 0x6C pattern", 
		         (int)cylinder, (int)interleave, (int)stepOption, (int)startLBA, (int)blocks);
		
		if (scsiDiskFormatRange(startLBA, blocks, 0x6C))
		{
			DBGMSG_F("Xebec Format Track completed successfully");
			scsiDev.status = GOOD;
		}
		else
		{
			DBGMSG_F("Xebec Format Track failed");
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = MEDIUM_ERROR;
			scsiDev.target->sense.asc = WRITE_ERROR_AUTO_REALLOCATION_FAILED;
		}
		scsiDev.phase = STATUS;
	}
	else if (command == 0xE0 && 
		scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
	{
	  // RAM Diagnostic
	  // XEBEC S1410 controller
	  // http://bitsavers.informatik.uni-stuttgart.de/pdf/xebec/104524C_S1410Man_Aug83.pdf
	  // Stub, return success
	}
	else if (command == 0xE4 && 
		scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
	{
		// Drive Diagnostic
		// XEBEC S1410 controller
		// Stub, return success
		DBGMSG_F("Xebec Drive Diagnostic - completing immediately (diagnostic passed)");
		
		// Debug: show current state
		DBGMSG_F("Before: dataLen=%d dataPtr=%d phase=%d", (int)scsiDev.dataLen, (int)scsiDev.dataPtr, (int)scsiDev.phase);
		
		// For successful diagnostic, complete immediately with no data transfer
		// Reset all data transfer state
		scsiDev.dataLen = 0;
		scsiDev.dataPtr = 0;
		scsiDev.savedDataPtr = 0;
	
	}   	
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
}

void scsiVendorCommandSetLen(uint8_t command, uint8_t* command_length)
{
	if (scsiToolboxEnabled())
	{
		// Conflicts with Apple CD-ROM audio over SCSI bus and Plextor CD-ROM D8 extension
		// Will override those commands if enabled
		if (0xD0 <= command && command <= 0xDA)
		{
			*command_length = 10;
		}
	}
	else if (scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL)
	{
		// Apple CD-ROM with CD audio over the SCSI bus
		if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_APPLE && (command == 0xD8 || command == 0xD9))
		{
			*command_length =  12;
		}
		// Plextor CD-ROM vendor extensions 0xD8
		else if (unlikely(scsiDev.target->cfg->vendorExtensions & VENDOR_EXTENSION_OPTICAL_PLEXTOR) && command == 0xD8)
		{
			*command_length =  12;
		}
	}
}
