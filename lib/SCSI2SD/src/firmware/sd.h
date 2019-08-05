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
#ifndef S2S_SD_H
#define S2S_SD_H

#include <stdint.h>

#define SD_SECTOR_SIZE 512

typedef struct
{
	int version; // SDHC = version 2.
	uint32_t capacity; // in 512 byte blocks

	uint8_t csd[16]; // Unparsed CSD
	uint8_t cid[16]; // Unparsed CID
} SdDevice;

extern SdDevice sdDev;

int sdInit(void);

void sdReadDMA(uint32_t lba, uint32_t sectors, uint8_t* outputBuffer);
int sdReadDMAPoll(uint32_t remainingSectors);
void sdCompleteTransfer();


#endif
