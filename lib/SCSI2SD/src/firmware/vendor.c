//	Copyright (C) 2016 Michael McMaster <michael@codesrc.com>
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
#include "toolbox.h"

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
		scsiDev.phase = STATUS;
	}
}

int scsiVendorCommand()
{
	int commandHandled = 1;

	uint8_t command = scsiDev.cdb[0];

	if (command == 0xC0)
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
	}   	
	else if (scsiToolboxEnabled() && scsiToolboxCommand())
	{
		// already handled
	}
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
}

void scsiVendorCommandSetLen(uint8_t command, uint8_t* command_length)
{
	if (scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL)
	{
		// Apple CD-ROM with CD audio over the SCSI bus
		if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_APPLE && (command == 0xD8 || command == 0xD9))
		{
			scsiDev.cdbLen =  12;
		}
		// Plextor CD-ROM vendor extensions 0xD8
		if (unlikely(scsiDev.target->cfg->vendorExtensions & VENDOR_EXTENSION_OPTICAL_PLEXTOR) && command == 0xD8)
		{
			scsiDev.cdbLen =  12;
		}
	}

	if (scsiToolboxEnabled())
	{
		// Conflicts with Apple CD-ROM audio over SCSI bus and Plextor CD-ROM D8 extension
		// Will override those commands if enabled
		if (0xD0 <= command && command <= 0xDA)
		{
			*command_length = 10;
		}
	}
}