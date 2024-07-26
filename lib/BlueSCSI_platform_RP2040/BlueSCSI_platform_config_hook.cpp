/** 
 * Copyright (C) 2023 Eric Helgeson
 * Portions ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
 * 
 * This file is part of BlueSCSI
 * 
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
#include "BlueSCSI_disk.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_platform_config_hook.h"


static bool isValidMacintoshImage(image_config_t *img)
{
    bool result = true;
    const char apple_magic[2] = {0x45, 0x52};
    const char block_size[2]  = {0x02, 0x00};  // 512 BE == 2
    const char lido_sig[4] = {'C', 'M', 'S', '_' };
    uint8_t tmp[SD_SECTOR_SIZE];
    // Check for Apple Magic
    img->file.seek(0);
    img->file.read(tmp, SD_SECTOR_SIZE);
    if (memcmp(apple_magic, tmp, 2) != 0)
    {
        debuglog("---- Apple magic not found.");
        result = false;
    }
    // Check HFS Block size is 512
    if (memcmp(block_size, &tmp[2], 2) != 0)
    {
        debuglog("---- Block size is ", block_size, ", should be 512.");
        result = false;
    }
    uint8_t *mac_driver = &tmp[MACINTOSH_SCSI_DRIVER_OFFSET];
    uint32_t driver_offset_blocks = mac_driver[0] << 24 |
                                    mac_driver[1] << 16 |
                                    mac_driver[2] << 8  |
                                    mac_driver[3];
    // Find size of SCSI Driver partition
    uint8_t *mac_driver_size = &tmp[MACINTOSH_SCSI_DRIVER_SIZE_OFFSET];
    uint32_t driver_size_blocks = mac_driver_size[0] << 8 | mac_driver_size[1];
    // SCSI Driver sanity checks
    if((driver_size_blocks * MACINTOSH_BLOCK_SIZE) > MACINTOSH_SCSI_DRIVER_MAX_SIZE ||
        (driver_offset_blocks * MACINTOSH_BLOCK_SIZE) > img->file.size())
    {
        debuglog("---- Invalid Macintosh SCSI Driver partition detected.");
        result = false;
    }
    // Contains Lido Driver - driver causes issues on a Mac Plus and is generally slower than the Apple 4.3 or FWB.
    // Also causes compatibility issues with other drivers.
    img->file.seek(driver_offset_blocks * MACINTOSH_BLOCK_SIZE);
    img->file.read(tmp, SD_SECTOR_SIZE);
    uint8_t* lido_driver = &tmp[LIDO_SIG_OFFSET];
    if(memcmp(lido_sig, lido_driver, 4) == 0)
    {
        debuglog("---- WARNING: This drive contains the LIDO driver and may cause issues.");
    }

    return result;
}

// Called from BlueSCSI_disk after image is initialized.
void platformConfigHook(image_config_t *img)
{
    if(ini_getbool("SCSI", "DisableConfigHook", false, CONFIGFILE))
    {
        debuglog("Skipping platformConfigHook due to DisableConfigHook");
        return;
    }
    if (img->quirks == S2S_CFG_QUIRKS_APPLE)
    {
        if(img->deviceType == S2S_CFG_FIXED)
        {
            if(!isValidMacintoshImage(img))
            {
                log("---- WARNING: This image does not appear to be a valid Macintosh Device image. See: https://github.com/BlueSCSI/BlueSCSI-v2/wiki/Disk-Images");
            }
            else
            {
                debuglog("---- Valid Macintosh Device Image detected.");
            }
        }
        // Macintosh hosts reserve ID 7, so warn the user this configuration won't work
        if((img->scsiId & S2S_CFG_TARGET_ID_BITS) == 7)
        {
          log("---- WARNING: Quirks set to Apple so can not use SCSI ID 7!");
        }
    }
}