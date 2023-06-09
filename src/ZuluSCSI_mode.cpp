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

#ifdef ENABLE_AUDIO_OUTPUT
#include "ZuluSCSI_audio.h"
#endif
#include "ZuluSCSI_cdrom.h"
#include "ZuluSCSI_log.h"

extern "C" {
#include "ZuluSCSI_mode.h"
}

static const uint8_t CDROMCDParametersPage[] =
{
0x0D, // page code
0x06, // page length
0x00, // reserved
0x0D, // reserved, inactivity time 8 min
0x00, 0x3C, // 60 seconds per MSF M unit
0x00, 0x4B  // 75 frames per MSF S unit
};

#ifdef ENABLE_AUDIO_OUTPUT
static const uint8_t CDROMAudioControlParametersPage[] =
{
0x0E, // page code
0x0E, // page length
0x04, // 'Immed' bit set, 'SOTC' bit not set
0x00, // reserved
0x00, // reserved
0x80, // 1 LBAs/sec multip
0x00, 0x4B, // 75 LBAs/sec
0x01, 0xFF, // output port 0 active, max volume
0x02, 0xFF, // output port 1 active, max volume
0x00, 0x00, // output port 2 inactive
0x00, 0x00 // output port 3 inactive
};
#endif

// 0x2A CD-ROM Capabilities and Mechanical Status Page
// This seems to have been standardized in MMC-1 but was de-facto present in
// earlier SCSI-2 drives. The below mirrors one of those earlier SCSI-2
// implementations, being is slightly shorter than the spec format but
// otherwise returning identical information within the same bytes.
static const uint8_t CDROMCapabilitiesPage[] =
{
0x2A, // page code
0x0E, // page length
0x00, // CD-R/RW reading not supported
0x00, // CD-R/RW writing not supported
#ifdef ENABLE_AUDIO_OUTPUT
0x01, // byte 4: audio play supported
#else
0x00, // byte 4: no features supported
#endif
0x03, // byte 5: CD-DA ok with accurate streaming, no other features
0x28, // byte 6: tray loader, ejection ok, but prevent/allow not supported
#ifdef ENABLE_AUDIO_OUTPUT
0x03, // byte 7: separate channel mute and volumes
#else
0x00, // byte 7: no features supported
#endif
0x05, 0x62, // max read speed, state (8X, ~1378KB/s)
#ifdef ENABLE_AUDIO_OUTPUT
0x01, 0x00,  // 256 volume levels supported
#else
0x00, 0x00,  // no volume levels supported
#endif
0x00, 0x40, // read buffer (64KB)
0x05, 0x62, // current read speed, matching max speed
};

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
        if (pc == 0x00)
        {
            // report current port assignments and volume level
            uint16_t chn = audio_get_channel(scsiDev.target->targetId);
            uint16_t vol = audio_get_volume(scsiDev.target->targetId);
            scsiDev.data[idx+8] = chn & 0xFF;
            scsiDev.data[idx+9] = vol & 0xFF;
            scsiDev.data[idx+10] = chn >> 8;
            scsiDev.data[idx+11] = vol >> 8;
        }
        else if (pc == 0x01)
        {
            // report bits that can be set
            scsiDev.data[idx+8] = 0xFF;
            scsiDev.data[idx+9] = 0xFF;
            scsiDev.data[idx+10] = 0xFF;
            scsiDev.data[idx+11] = 0xFF;
        }
        else
        {
            // report defaults for 0x02
            // also report same for 0x03, though we are actually supposed
            // to terminate with CHECK CONDITION and SAVING PARAMETERS NOT SUPPORTED
            scsiDev.data[idx+8] = AUDIO_CHANNEL_ENABLE_MASK & 0xFF;
            scsiDev.data[idx+9] = DEFAULT_VOLUME_LEVEL & 0xFF;
            scsiDev.data[idx+10] = AUDIO_CHANNEL_ENABLE_MASK >> 8;
            scsiDev.data[idx+11] = DEFAULT_VOLUME_LEVEL >> 8;
        }
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

int modeSenseCDCapabilitiesPage(int pc, int idx, int pageCode, int* pageFound)
{
    if ((scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL)
        && (pageCode == 0x2A || pageCode == 0x3F))
    {
        *pageFound = 1;
        pageIn(
            pc,
            idx,
            CDROMCapabilitiesPage,
            sizeof(CDROMCapabilitiesPage));
        return sizeof(CDROMCapabilitiesPage);
    }
    else
    {
        return 0;
    }
}

extern "C"
int modeSelectCDAudioControlPage(int pageLen, int idx)
{
#ifdef ENABLE_AUDIO_OUTPUT
    if (scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL)
    {
        if (pageLen != 0x0E) return 0;
        uint16_t chn = (scsiDev.data[idx+10] << 8) + scsiDev.data[idx+8];
        uint16_t vol = (scsiDev.data[idx+11] << 8) + scsiDev.data[idx+9];
        dbgmsg("------ CD audio control page channels (", chn, "), volume (", vol, ")");
        audio_set_channel(scsiDev.target->targetId, chn);
        audio_set_volume(scsiDev.target->targetId, vol);
        return 1;
    }
    else
    {
        return 0;
    }
#else
    return 0;
#endif
}
