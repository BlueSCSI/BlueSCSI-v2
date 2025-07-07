/**
 * This file is part of BlueSCSI
 *
 * Copyright (C) 2023-2025 Eric Helgeson
 * Portions ZuluSCSI™ - Copyright (c) 2023-2025 Rabbit Hole Computing™
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
#include "BlueSCSI_disk.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_settings.h"
#include "QuirksCheck.h"
#include <assert.h>


static bool isValidMacintoshImage(image_config_t *img)
{
    // Mac block sizes should be the same size SD sector sizes for raw seeks, and reads to work
    assert(MACINTOSH_BLOCK_SIZE == SD_SECTOR_SIZE);

    constexpr char LIDO_SIGNATURE[] = {'C', 'M', 'S', '_' };
    // Apple Volume Magic
    // constexpr uint8_t APPLE_VOLUME_MAGIC[] {0x45, 0x52};
    // 512 BE == 2
    // constexpr uint8_t HFS_BLOCK_SIZE_HEX[] = {0x02, 0x00};
    // HFS partitions start with BD at offset 1024.
    constexpr uint8_t HFS_VOLUME_MAGIC[] {0x42, 0x44};

    // Check for Raw HFS Volume start magic
    if (2 * MACINTOSH_BLOCK_SIZE > img->file.size()) {
        return false;
    }
    img->file.seek(2 * MACINTOSH_BLOCK_SIZE);
    img->file.read(scsiDev.data, SD_SECTOR_SIZE);

    if (memcmp(HFS_VOLUME_MAGIC, scsiDev.data, sizeof(HFS_VOLUME_MAGIC)) == 0) {
        logmsg("---- ERROR: This is a bare HFS Volume. Use DiskJockey to convert it to a Device image.");
        return false;
    }
    const uint8_t *mac_driver = &scsiDev.data[MACINTOSH_SCSI_DRIVER_OFFSET];
    const uint32_t driver_offset_blocks = mac_driver[0] << 24 |
                                          mac_driver[1] << 16 |
                                          mac_driver[2] << 8  |
                                          mac_driver[3];
    // Find the size of SCSI Driver partition
    const uint8_t *mac_driver_size = &scsiDev.data[MACINTOSH_SCSI_DRIVER_SIZE_OFFSET];
    const uint32_t driver_size_blocks = mac_driver_size[0] << 8 | mac_driver_size[1];
    // SCSI Driver sanity checks
    if(!((driver_size_blocks * MACINTOSH_BLOCK_SIZE) > MACINTOSH_SCSI_DRIVER_MAX_SIZE ||
        (driver_offset_blocks * MACINTOSH_BLOCK_SIZE) > img->file.size()))
    {
        // Check if Lido Driver - driver causes issues on a Mac Plus and is generally slower than the Apple 4.3 or FWB.
        // Also causes compatibility issues with other drivers.
        img->file.seek(driver_offset_blocks * MACINTOSH_BLOCK_SIZE);
        img->file.read(scsiDev.data, SD_SECTOR_SIZE);
        uint8_t* lido_driver = &scsiDev.data[LIDO_SIG_OFFSET];
        if(memcmp(LIDO_SIGNATURE, lido_driver, sizeof(LIDO_SIGNATURE)) == 0)
        {
            logmsg("---- WARNING: This drive contains the LIDO driver and may cause issues. Use DiskJockey to replace it.");
            return false;
        }
    }
    return true;
}

// Called from BlueSCSI_disk after image is initalized.
static void macQuirksSanityCheck(image_config_t *img)
{

    if(g_scsi_settings.getDevice(img->scsiId & S2S_CFG_TARGET_ID_BITS)->disableMacSanityCheck)
    {
        dbgmsg("---- Skipping Mac sanity check due to DisableMacSanityCheck");
        return;
    }

    if(img->deviceType == S2S_CFG_FIXED)
    {
        isValidMacintoshImage(img);
    }
    
    // Macintosh hosts reserve ID 7, so warn the user this configuration won't work
    if((img->scsiId & S2S_CFG_TARGET_ID_BITS) == S2S_CFG_TARGET_ID_BITS)
    {
        logmsg("---- WARNING: Quirks set to Apple so can not use SCSI ID 7!");
    }
}

void quirksCheck(image_config_t *img)
{
    if (img->quirks == S2S_CFG_QUIRKS_APPLE)
    {
        macQuirksSanityCheck(img);
    }
}