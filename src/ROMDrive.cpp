/**
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * Function romDriveClear() Copyright (c) 2023 Eric Helgeson
 *
 * This file is licensed under the GPL version 3 or any later version. 
 * It is derived from disk.c in SCSI2SD V6
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

#include "ROMDrive.h"
#include <SdFat.h>
#include <ZuluSCSI_platform.h>
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
#include <strings.h>
#include <string.h>

extern "C" {
#include <scsi.h>
}

extern SdFs SD;

#ifndef PLATFORM_HAS_ROM_DRIVE

bool romDriveCheckPresent(romdrive_hdr_t *hdr)
{
    return false;
}

bool romDriveClear()
{
    logmsg("---- Platform does not support ROM drive");
    return false;
}

bool scsiDiskProgramRomDrive(const char *filename, int scsi_id, int blocksize, S2S_CFG_TYPE type)
{
    logmsg("---- Platform does not support ROM drive");
    return false;
}

bool romDriveRead(uint8_t *buf, uint32_t start, uint32_t count)
{
    return false;
}

#else

// Check if the romdrive is present
bool romDriveCheckPresent(romdrive_hdr_t *hdr)
{
    romdrive_hdr_t tmp;
    if (!hdr) hdr = &tmp;

    if (!platform_read_romdrive((uint8_t*)hdr, 0, sizeof(romdrive_hdr_t)))
    {
        return false;
    }

    if (memcmp(hdr->magic, "ROMDRIVE", 8) != 0)
    {
        return false;
    }

    if (hdr->imagesize <= 0 || hdr->scsi_id < 0 || hdr->scsi_id > 8)
    {
        return false;
    }

    return true;
}

// Clear the drive metadata header
bool romDriveClear()
{
    romdrive_hdr_t hdr = {0x0};

    if (!platform_write_romdrive((const uint8_t*)&hdr, 0, PLATFORM_ROMDRIVE_PAGE_SIZE))
    {
        logmsg("-- Failed to clear ROM drive");
        return false;
    }
    logmsg("-- Cleared ROM drive");
    SD.remove("CLEAR_ROM");
    return true;
}

// Load an image file to romdrive
bool scsiDiskProgramRomDrive(const char *filename, int scsi_id, int blocksize, S2S_CFG_TYPE type)
{
    FsFile file = SD.open(filename, O_RDONLY);
    if (!file.isOpen())
    {
        logmsg("---- Failed to open: ", filename);
        return false;
    }

    uint64_t filesize = file.size();
    uint32_t maxsize = platform_get_romdrive_maxsize() - PLATFORM_ROMDRIVE_PAGE_SIZE;

    logmsg("---- SCSI ID: ", scsi_id, " blocksize ", blocksize, " type ", (int)type);
    logmsg("---- ROM drive maximum size is ", (int)maxsize,
          " bytes, image file is ", (int)filesize, " bytes");

    if (filesize > maxsize)
    {
        logmsg("---- Image size exceeds ROM space, not loading");
        file.close();
        return false;
    }

    romdrive_hdr_t hdr = {};
    memcpy(hdr.magic, "ROMDRIVE", 8);
    hdr.scsi_id = scsi_id;
    hdr.imagesize = filesize;
    hdr.blocksize = blocksize;
    hdr.drivetype = type;

    // Program the drive metadata header
    if (!platform_write_romdrive((const uint8_t*)&hdr, 0, PLATFORM_ROMDRIVE_PAGE_SIZE))
    {
        logmsg("---- Failed to program ROM drive header");
        file.close();
        return false;
    }

    // Program the drive contents
    uint32_t pages = (filesize + PLATFORM_ROMDRIVE_PAGE_SIZE - 1) / PLATFORM_ROMDRIVE_PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++)
    {
        if (i % 2)
            LED_ON();
        else
            LED_OFF();

        if (file.read(scsiDev.data, PLATFORM_ROMDRIVE_PAGE_SIZE) <= 0 ||
            !platform_write_romdrive(scsiDev.data, (i + 1) * PLATFORM_ROMDRIVE_PAGE_SIZE, PLATFORM_ROMDRIVE_PAGE_SIZE))
        {
            logmsg("---- Failed to program ROM drive page ", (int)i);
            file.close();
            return false;
        }
    }

    LED_OFF();

    file.close();

    char newname[MAX_FILE_PATH * 2] = "";
    strlcat(newname, filename, sizeof(newname));
    strlcat(newname, "_loaded", sizeof(newname));
    SD.rename(filename, newname);
    logmsg("---- ROM drive programming successful, image file renamed to ", newname);

    return true;
}

bool romDriveRead(uint8_t *buf, uint32_t start, uint32_t count)
{
    return platform_read_romdrive(buf, start + PLATFORM_ROMDRIVE_PAGE_SIZE, count);
}

#endif