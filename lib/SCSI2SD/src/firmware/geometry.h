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

#include "config.h"
#include "sd.h"

typedef enum
{
	ADDRESS_BLOCK = 0,
	ADDRESS_PHYSICAL_BYTE = 4,
	ADDRESS_PHYSICAL_SECTOR = 5
} SCSI_ADDRESS_FORMAT;

static inline int SDSectorsPerSCSISector(uint16_t bytesPerSector)
{
	return (bytesPerSector + SD_SECTOR_SIZE - 1) / SD_SECTOR_SIZE;
}

uint32_t getScsiCapacity(
	uint32_t sdSectorStart,
	uint16_t bytesPerSector,
	uint32_t scsiSectors);

uint32_t SCSISector2SD(
	uint32_t sdSectorStart,
	uint16_t bytesPerSector,
	uint32_t scsiSector);

uint64_t CHS2LBA(
	uint32_t c,
	uint8_t h,
	uint32_t s,
	uint16_t headsPerCylinder,
	uint16_t sectorsPerTrack);
void LBA2CHS(
	uint32_t lba,
	uint32_t* c,
	uint8_t* h,
	uint32_t* s,
	uint16_t headsPerCylinder,
	uint16_t sectorsPerTrack);

// Convert an address in the given SCSI_ADDRESS_FORMAT to
// a linear byte address.
// addr must be >= 8 bytes.
uint64_t scsiByteAddress(
	uint16_t bytesPerSector,
	uint16_t headsPerCylinder,
	uint16_t sectorsPerTrack,
	int format,
	const uint8_t* addr);
void scsiSaveByteAddress(
	uint16_t bytesPerSector,
	uint16_t headsPerCylinder,
	uint16_t sectorsPerTrack,
	int format,
	uint64_t byteAddr,
	uint8_t* buf);


#endif
