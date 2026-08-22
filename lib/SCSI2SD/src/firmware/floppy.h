//	Copyright (c) 2026 Eric Helgeson <eric@bluescsi.com>
//
//	This file is part of BlueSCSI.
//
//	BlueSCSI is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	BlueSCSI is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with BlueSCSI.  If not, see <http://www.gnu.org/licenses/>.
#ifndef S2S_FLOPPY_H
#define S2S_FLOPPY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint32_t blocks;
	uint16_t bytesPerSector;
	uint16_t transferRate;	// SCSI-2 table 159
	uint16_t cylinders;
	uint8_t heads;
	uint8_t sectorsPerTrack;
	uint8_t mediumType;		// SCSI-2 table 153
	const char* name;
} S2S_FloppyFormat;

// Match an image against the known floppy formats by size.
// Returns NULL when the size is not a standard floppy.
const S2S_FloppyFormat* s2s_floppyFormat(uint32_t blocks, uint16_t bytesPerSector);

// Medium type for an image that matched no format, from its head count.
uint8_t s2s_floppyMediumType(uint8_t heads);

#ifdef __cplusplus
}
#endif

#endif
