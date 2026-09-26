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
#include "floppy.h"

#include <stddef.h>

// Transfer rates are the SCSI-2 table 159 values, medium types table 153.
// Table 153 has one 90mm code (1Eh), so every 3.5" format shares it.
static const S2S_FloppyFormat FloppyFormats[] =
{
	//  blocks  bps  rate    cyl   h  spt  medium  name
	{      800, 512, 0x00FA,  80,  1, 10, 0x01, "400K" },
	{      720, 512, 0x00FA,  40,  2,  9, 0x12, "360K" },
	{     1280, 512, 0x00FA,  80,  2,  8, 0x1E, "640K" },
	{     1440, 512, 0x00FA,  80,  2,  9, 0x1E, "720K" },
	{     1600, 512, 0x00FA,  80,  2, 10, 0x1E, "800K" },
	{     2400, 512, 0x01F4,  80,  2, 15, 0x1A, "1.2M" },
	{     2880, 512, 0x01F4,  80,  2, 18, 0x1E, "1.44M" },
	{     5760, 512, 0x03E8,  80,  2, 36, 0x1E, "2.88M" },
	// NEC 2HD, used by PC-98 and X68000. Shipped in both 5.25" and 3.5".
	{     1232, 1024, 0x01F4, 77,  2,  8, 0x1E, "1.2M NEC" },
};

const S2S_FloppyFormat* s2s_floppyFormat(uint32_t blocks, uint16_t bytesPerSector)
{
	size_t i;
	for (i = 0; i < sizeof(FloppyFormats) / sizeof(FloppyFormats[0]); i++)
	{
		if (FloppyFormats[i].blocks == blocks &&
			FloppyFormats[i].bytesPerSector == bytesPerSector)
		{
			return &FloppyFormats[i];
		}
	}
	return NULL;
}

uint8_t s2s_floppyMediumType(uint8_t heads)
{
	return (heads > 1) ? 0x02 : 0x01;
}
