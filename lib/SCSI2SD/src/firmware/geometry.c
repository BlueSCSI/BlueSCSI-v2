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

#include "geometry.h"
#include "scsi.h"
#include "sd.h"
#include "config.h"

#include <string.h>

uint32_t getScsiCapacity(
	uint32_t sdSectorStart,
	uint16_t bytesPerSector,
	uint32_t scsiSectors)
{
	uint32_t capacity =
		(sdDev.capacity - sdSectorStart - S2S_CFG_SIZE) /
			SDSectorsPerSCSISector(bytesPerSector);


	if (sdDev.capacity == 0)
	{
		capacity = 0;
	}
	else if (sdSectorStart >= (sdDev.capacity - S2S_CFG_SIZE))
	{
		capacity = 0;
	}
	else if (scsiSectors && (capacity > scsiSectors))
	{
		capacity = scsiSectors;
	}
	return capacity;
}


uint32_t SCSISector2SD(
	uint32_t sdSectorStart,
	uint16_t bytesPerSector,
	uint32_t scsiSector)
{
	return scsiSector * SDSectorsPerSCSISector(bytesPerSector) + sdSectorStart;
}

// Standard mapping according to ECMA-107 and ISO/IEC 9293:1994
// Sector always starts at 1. There is no 0 sector.
uint64_t CHS2LBA(
	uint32_t c,
	uint8_t h,
	uint32_t s,
	uint16_t headsPerCylinder,
	uint16_t sectorsPerTrack)
{
	return (
		(((uint64_t)c) * headsPerCylinder + h) *
			(uint64_t) sectorsPerTrack
		) + (s - 1);
}


void LBA2CHS(
	uint32_t lba,
	uint32_t* c,
	uint8_t* h,
	uint32_t* s,
	uint16_t headsPerCylinder,
	uint16_t sectorsPerTrack)
{
	*c = lba / (((uint32_t) sectorsPerTrack) * headsPerCylinder);
	*h = (lba / sectorsPerTrack) % headsPerCylinder;
	*s = (lba % sectorsPerTrack) + 1;
}

uint64_t scsiByteAddress(
	uint16_t bytesPerSector,
	uint16_t headsPerCylinder,
	uint16_t sectorsPerTrack,
	int format,
	const uint8_t* addr)
{
	uint64_t result;
	switch (format)
	{
	case ADDRESS_BLOCK:
	{
		uint32_t lba =
			(((uint32_t) addr[0]) << 24) +
			(((uint32_t) addr[1]) << 16) +
			(((uint32_t) addr[2]) << 8) +
			addr[3];

		result = (uint64_t) bytesPerSector * lba;
	} break;

	case ADDRESS_PHYSICAL_BYTE:
	{
		uint32_t cyl =
			(((uint32_t) addr[0]) << 16) +
			(((uint32_t) addr[1]) << 8) +
			addr[2];

		uint8_t head = addr[3];

		uint32_t bytes =
			(((uint32_t) addr[4]) << 24) +
			(((uint32_t) addr[5]) << 16) +
			(((uint32_t) addr[6]) << 8) +
			addr[7];

		result = CHS2LBA(cyl, head, 1, headsPerCylinder, sectorsPerTrack) *
			(uint64_t) bytesPerSector + bytes;
	} break;

	case ADDRESS_PHYSICAL_SECTOR:
	{
		uint32_t cyl =
			(((uint32_t) addr[0]) << 16) +
			(((uint32_t) addr[1]) << 8) +
			addr[2];

		uint8_t head = scsiDev.data[3];

		uint32_t sector =
			(((uint32_t) addr[4]) << 24) +
			(((uint32_t) addr[5]) << 16) +
			(((uint32_t) addr[6]) << 8) +
			addr[7];

		result = CHS2LBA(cyl, head, sector, headsPerCylinder, sectorsPerTrack) * (uint64_t) bytesPerSector;
	} break;

	default:
		result = (uint64_t) -1;
	}

	return result;
}


void scsiSaveByteAddress(
	uint16_t bytesPerSector,
	uint16_t headsPerCylinder,
	uint16_t sectorsPerTrack,
	int format,
	uint64_t byteAddr,
	uint8_t* buf)
{
	uint32_t lba = byteAddr / bytesPerSector;
	uint32_t byteOffset = byteAddr % bytesPerSector;

	switch (format)
	{
	case ADDRESS_BLOCK:
	{
		buf[0] = lba >> 24;
		buf[1] = lba >> 16;
		buf[2] = lba >> 8;
		buf[3] = lba;

		buf[4] = 0;
		buf[5] = 0;
		buf[6] = 0;
		buf[7] = 0;
	} break;

	case ADDRESS_PHYSICAL_BYTE:
	{
		uint32_t cyl;
		uint8_t head;
		uint32_t sector;
		uint32_t bytes;

		LBA2CHS(lba, &cyl, &head, &sector, headsPerCylinder, sectorsPerTrack);

		bytes = sector * bytesPerSector + byteOffset;

		buf[0] = cyl >> 16;
		buf[1] = cyl >> 8;
		buf[2] = cyl;

		buf[3] = head;

		buf[4] = bytes >> 24;
		buf[5] = bytes >> 16;
		buf[6] = bytes >> 8;
		buf[7] = bytes;
	} break;

	case ADDRESS_PHYSICAL_SECTOR:
	{
		uint32_t cyl;
		uint8_t head;
		uint32_t sector;

		LBA2CHS(lba, &cyl, &head, &sector, headsPerCylinder, sectorsPerTrack);

		buf[0] = cyl >> 16;
		buf[1] = cyl >> 8;
		buf[2] = cyl;

		buf[3] = head;

		buf[4] = sector >> 24;
		buf[5] = sector >> 16;
		buf[6] = sector >> 8;
		buf[7] = sector;
	} break;

	default:
		memset(buf, 0, 8);
	}

}

