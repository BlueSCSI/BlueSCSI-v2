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
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "device.h"

#include "config.h"
#include "sd.h"

// Max allowed by legacy IBM-PC Bios (6 bits)
#define SCSI_SECTORS_PER_TRACK 63

// MS-DOS up to 7.10 will crash on 256 heads.
#define SCSI_HEADS_PER_CYLINDER 255

typedef enum
{
	ADDRESS_BLOCK = 0,
	ADDRESS_PHYSICAL_BYTE = 4,
	ADDRESS_PHYSICAL_SECTOR = 5
} SCSI_ADDRESS_FORMAT;

static inline int SDSectorsPerSCSISector()
{
	return (config->bytesPerSector + SD_SECTOR_SIZE - 1) / SD_SECTOR_SIZE;
}

uint32_t getScsiCapacity();

uint32_t SCSISector2SD(uint32_t scsiSector);

uint64 CHS2LBA(uint32 c, uint8 h, uint32 s);
void LBA2CHS(uint32 lba, uint32* c, uint8* h, uint32* s);

// Convert an address in the given SCSI_ADDRESS_FORMAT to
// a linear byte address.
// addr must be >= 8 bytes.
uint64 scsiByteAddress(int format, const uint8* addr);
void scsiSaveByteAddress(int format, uint64 byteAddr, uint8* buf);


#endif
