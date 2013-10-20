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
#include "config.h"

#include <string.h>

// CYDEV_EEPROM_ROW_SIZE == 16.
static char magic[CYDEV_EEPROM_ROW_SIZE] = "codesrc_00000001";

// Config shadow RAM (copy of EEPROM)
static Config shadow =
{
	0, // SCSI ID
	" codesrc", // vendor  (68k Apple Drive Setup: Set to " SEAGATE")
	"         SCSI2SD", //prodId (68k Apple Drive Setup: Set to "          ST225N")
	"2.0a", // revision (68k Apple Drive Setup: Set to "1.0 ")
	1, // enable parity
	0, // disable unit attention,
	0, // overclock SPI
	0, // Max blocks (0 == disabled)
	"" // reserved
};

// Global
Config* config = NULL;

void configInit()
{
	// We could map cfgPtr directly into the EEPROM memory,
	// but that would waste power. Copy it to RAM then turn off
	// the EEPROM. 	
	CFG_EEPROM_Start();
	CyDelayUs(5); // 5us to start per datasheet.
	
	// Check magic
	int shadowRows = (sizeof(shadow) / CYDEV_EEPROM_ROW_SIZE) + 1;
	int shadowBytes = CYDEV_EEPROM_ROW_SIZE * shadowRows;
	uint8* eeprom = (uint8*)CYDEV_EE_BASE;	
	if (memcmp(eeprom + shadowBytes, magic, sizeof(magic))) 
	{
		CySetTemp();
		int row;
		int status = CYRET_SUCCESS;
		for (row = 0; (row < shadowRows) && (status == CYRET_SUCCESS); ++row)
		{
			CFG_EEPROM_Write(((uint8*)&shadow) + (row * CYDEV_EEPROM_ROW_SIZE), row);
		}
		if (status == CYRET_SUCCESS)
		{
			CFG_EEPROM_Write((uint8*)magic, row);
		}
	}
	else
	{
		memcpy(&shadow, eeprom, sizeof(shadow));
	}
	config = &shadow;
	CFG_EEPROM_Stop();
}