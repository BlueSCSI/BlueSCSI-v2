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
#ifndef SD_H
#define SD_H

#define SD_SECTOR_SIZE 512

typedef enum
{
	SD_GO_IDLE_STATE = 0,
	SD_SEND_OP_COND = 1,
	SD_SEND_IF_COND = 8, // SD V2
	SD_SEND_CSD = 9,
	SD_SEND_CID = 10,
	SD_STOP_TRANSMISSION = 12,
	SD_SEND_STATUS = 13,
	SD_SET_BLOCKLEN = 16,
	SD_READ_SINGLE_BLOCK = 17,
	SD_READ_MULTIPLE_BLOCK = 18,
	SD_APP_SET_WR_BLK_ERASE_COUNT = 23,
	SD_WRITE_MULTIPLE_BLOCK = 25,
	SD_APP_SEND_OP_COND = 41,
	SD_APP_CMD = 55,
	SD_READ_OCR = 58,
	SD_CRC_ON_OFF = 59
} SD_CMD;

typedef enum
{
	SD_R1_IDLE = 1,
	SD_R1_ERASE_RESET = 2,
	SD_R1_ILLEGAL = 4,
	SD_R1_CRC = 8,
	SD_R1_ERASE_SEQ = 0x10,
	SD_R1_ADDRESS = 0x20,
	SD_R1_PARAMETER = 0x40
} SD_R1;

typedef struct
{
	int version; // SDHC = version 2.
	int ccs; // Card Capacity Status. 1 = SDHC or SDXC
	uint32 capacity; // in 512 byte blocks

	uint8_t csd[16]; // Unparsed CSD
	uint8_t cid[16]; // Unparsed CID
} SdDevice;

extern SdDevice sdDev;
extern volatile uint8_t sdRxDMAComplete;
extern volatile uint8_t sdTxDMAComplete;

int sdInit(void);

#define sdDMABusy() (!(sdRxDMAComplete && sdTxDMAComplete))

void sdWriteMultiSectorPrep(uint32_t sdLBA, uint32_t sdSectors);
void sdWriteMultiSectorDMA(uint8_t* outputBuffer);
int sdWriteSectorDMAPoll();

void sdReadMultiSectorPrep(uint32_t sdLBA, uint32_t sdSectors);
void sdReadMultiSectorDMA(uint8_t* outputBuffer);
void sdReadSingleSectorDMA(uint32_t lba, uint8_t* outputBuffer);
int sdReadSectorDMAPoll();

void sdCompleteTransfer(void);
void sdCheckPresent();
void sdPoll();

#endif
