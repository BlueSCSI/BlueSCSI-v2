/**
 * Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
 * Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com>
 * Copyright (C) 2019 Landon Rodgers <g.landon.rodgers@gmail.com>
 * ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
 *
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 *
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#include <stdint.h>
#include <string.h>

#include "BlueSCSI_cdrom.h"

extern "C" {
#include "BlueSCSI_mode.h"
}

#ifdef ENABLE_AUDIO_OUTPUT
static const uint8_t CDROMCDParametersPage[] =
{
0x0D, // page code
0x06, // page length
0x00, // reserved
0x0D, // reserved, inactivity time 8 min
0x00, 0x3C, // 60 seconds per MSF M unit
0x00, 0x4B  // 75 frames per MSF S unit
};

static const uint8_t CDROMAudioControlParametersPage[] =
{
0x0E, // page code
0x0E, // page length
0x04, // 'Immed' bit set, 'SOTC' bit not set
0x00, // reserved
0x00, // reserved
0x80, // 1 LBAs/sec multip
0x00, 0x4B, // 75 LBAs/sec
0x03, 0xFF, // output port 0 active, max volume
0x03, 0xFF, // output port 1 active, max volume
0x00, 0x00, // output port 2 inactive
0x00, 0x00 // output port 3 inactive
};
#endif

static void pageIn(int pc, int dataIdx, const uint8_t* pageData, int pageLen)
{
    memcpy(&scsiDev.data[dataIdx], pageData, pageLen);

    if (pc == 0x01) // Mask out (un)changable values
    {
        memset(&scsiDev.data[dataIdx+2], 0, pageLen - 2);
    }
}

extern "C"
int modeSenseCDDevicePage(int pc, int idx, int pageCode, int* pageFound)
{
#ifdef ENABLE_AUDIO_OUTPUT
    if ((scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL)
        && (pageCode == 0x0D || pageCode == 0x3F))
    {
        *pageFound = 1;
        pageIn(
            pc,
            idx,
            CDROMCDParametersPage,
            sizeof(CDROMCDParametersPage));
        return sizeof(CDROMCDParametersPage);
    }
    else
    {
        return 0;
    }
#else
    return 0;
#endif
}

extern "C"
int modeSenseCDAudioControlPage(int pc, int idx, int pageCode, int* pageFound)
{
#ifdef ENABLE_AUDIO_OUTPUT
    if ((scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL)
        && (pageCode == 0x0E || pageCode == 0x3F))
    {
        *pageFound = 1;
        pageIn(
            pc,
            idx,
            CDROMAudioControlParametersPage,
            sizeof(CDROMAudioControlParametersPage));
        return sizeof(CDROMAudioControlParametersPage);
    }
    else
    {
        return 0;
    }
#else
    return 0;
#endif
}
