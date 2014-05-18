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
#ifndef Config_H
#define Config_H

#include "device.h"

typedef struct
{
	uint8 scsiId;
	char vendor[8];
	char prodId[16];
	char revision[4];
	uint8 enableParity;
	uint8 enableUnitAttention;
	uint8 reserved1; // Unused. Ensures maxBlocks is aligned.
	uint32 maxSectors;
	uint16 bytesPerSector;

	// Pad to 64 bytes, which is what we can fit into a USB HID packet.
	char reserved[26]; 
} Config;

extern Config* config;

void configInit(void);
void debugInit(void);
void configPoll(void);
void configSave(void);

#endif
