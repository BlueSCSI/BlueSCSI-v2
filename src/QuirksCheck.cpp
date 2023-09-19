/** 
 * Copyright (C) 2023 Eric Helgeson
 * Portions ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
 * 
 * This file is licensed under the GPL version 3 or any later version. 
 * It is derived from BlueSCSI_platform_config_hook.cpp in BluSCSI-v2
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

#include <minIni.h>
#include "ZuluSCSI_disk.h"
#include "ZuluSCSI_log.h"
#include "QuirksCheck.h"
#include <stdint.h>


static bool isValidMacintoshImage(image_config_t *img)
{
    bool result = true;
    const char apple_magic[2] = {0x45, 0x52};
    const char block_size[2]  = {0x02, 0x00};  // 512 BE == 2
    const char lido_sig[4] = {'C', 'M', 'S', '_' };
    uint8_t tmp[4] = {0};

    // Check for Apple Magic
    img->file.seek(0);
    img->file.read(tmp, 2);
    if(memcmp(apple_magic, tmp, 2) != 0)
    {
        dbgmsg("---- Apple magic not found.");
        result = false;
    }
    // Check HFS Block size is 512
    img->file.seek(2);
    img->file.read(tmp, 2);
    if(memcmp(block_size, tmp, 2) != 0)
    {
        dbgmsg("---- Block size not 512", block_size);
        result = false;
    }
    // Find SCSI Driver offset
    img->file.seek(MACINTOSH_SCSI_DRIVER_OFFSET);
    img->file.read(tmp, 4);
    uint64_t driver_offset_blocks = int((unsigned char)(tmp[0]) << 24 | (unsigned char)(tmp[1]) << 16 |
                                        (unsigned char)(tmp[2]) << 8  |  (unsigned char)(tmp[3]));
    // Find size of SCSI Driver partition
    img->file.seek(MACINTOSH_SCSI_DRIVER_SIZE_OFFSET);
    img->file.read(tmp, 2);
    int driver_size_blocks = int((unsigned char)(tmp[0]) << 8 | (unsigned char)(tmp[1]));
    // SCSI Driver sanity checks
    if((driver_size_blocks * MACINTOSH_BLOCK_SIZE) > MACINTOSH_SCSI_DRIVER_MAX_SIZE ||
        (driver_offset_blocks * MACINTOSH_BLOCK_SIZE) > img->file.size())
    {
        dbgmsg("---- Invalid Macintosh SCSI Driver partition detected.");
        result = false;
    }
    // Contains Lido Driver - driver causes issues on a Mac Plus and is generally slower than the Apple 4.3 or FWB.
    // Also causes compatibility issues with other drivers.
    img->file.seek(driver_offset_blocks * MACINTOSH_BLOCK_SIZE + LIDO_SIG_OFFSET);
    img->file.read(tmp, 4);
    if(memcmp(lido_sig, tmp, 4) == 0)
    {
        logmsg("---- WARNING: This drive contains the LIDO driver and may cause issues.");
    }

    return result;
}

// Called from ZuluSCSI_disk after image is initalized.
static void macQuirksSanityCheck(image_config_t *img)
{
    if(ini_getbool("SCSI", "DisableMacSanityCheck", false, CONFIGFILE))
    {
        dbgmsg("---- Skipping Mac sanity check due to DisableMacSanityCheck");
        return;
    }
    if(img->file.isRom() || img->file.isRaw())
    {
        dbgmsg("---- Skipping Mac sanity check on ROM/Raw Drive.");
        return;
    }
    if (img->quirks == S2S_CFG_QUIRKS_APPLE)
    {
        if(img->deviceType == S2S_CFG_FIXED)
        {
            if(!isValidMacintoshImage(img))
            {
                logmsg("---- WARNING: This image does not appear to be a valid Macintosh disk image.");
            }
            else
            {
                dbgmsg("---- Valid Macintosh disk image detected.");
            }
        }
        // Macintosh hosts reserve ID 7, so warn the user this configuration wont work
        if((img->scsiId & S2S_CFG_TARGET_ID_BITS) == 7)
        {
          logmsg("---- WARNING: Quirks set to Apple so can not use SCSI ID 7!");
        }
    }
}

void quirksCheck(image_config_t *img)
{
    macQuirksSanityCheck(img);
}