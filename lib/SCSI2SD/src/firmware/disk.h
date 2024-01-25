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
#ifndef DISK_H
#define DISK_H

typedef enum
{
	// DISK_STARTED is stored per-target now as it's controlled by the
	// START STOP UNIT command
	OBSOLETE_DISK_STARTED = 1,
	DISK_PRESENT = 2,     // SD card is physically present
	DISK_INITIALISED = 4, // SD card responded to init sequence
	DISK_WP = 8           // Write-protect.
} DISK_STATE;

typedef enum
{
	TRANSFER_READ,
	TRANSFER_WRITE
} TRANSFER_DIR;

typedef struct
{
	int state;
} BlockDevice;

typedef struct
{
	int multiBlock; // True if we're using a multi-block SPI transfer.
	uint32_t lba;
	uint32_t blocks;

	uint32_t currentBlock;
} Transfer;

extern BlockDevice blockDev;
extern Transfer transfer;

void scsiDiskInit(void);
void scsiDiskReset(void);
void scsiDiskPoll(void);
int scsiDiskCommand(void);
int doTestUnitReady();

#endif
