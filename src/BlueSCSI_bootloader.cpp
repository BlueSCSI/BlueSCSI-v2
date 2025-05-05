/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
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

// Simple bootloader that loads new firmware from SD card.

#include <BlueSCSI_platform.h>
#include "BlueSCSI_config.h"
#include "BlueSCSI_log.h"
#include <string.h>
#include <SdFat.h>

#ifdef PLATFORM_BOOTLOADER_SIZE

extern SdFs SD;
extern FsFile g_logfile;

bool find_firmware_image(FsFile &file, char name[MAX_FILE_PATH + 1])
{
    FsFile root;
    root.open("/");

    while (file.openNext(&root, O_READ))
    {
        if (file.isDir()) continue;

        int namelen = file.getName(name, MAX_FILE_PATH);

        if (namelen >= sizeof(FIRMWARE_NAME_PREFIX) + 3 &&
            strncasecmp(name, FIRMWARE_NAME_PREFIX, sizeof(FIRMWARE_NAME_PREFIX) - 1) == 0 &&
            strncasecmp(name + namelen - 3, "bin", 3) == 0)
        {
            root.close();
            logmsg("Found firmware file: ", name);
            return true;
        }

        file.close();
    }

    root.close();
    return false;
}

#ifndef PLATFORM_FLASH_SECTOR_ERASE
bool program_firmware(FsFile &file)
{
    uint32_t filesize = file.size();
    uint32_t fwsize = filesize - PLATFORM_BOOTLOADER_SIZE;
    uint32_t num_pages = (fwsize + PLATFORM_FLASH_PAGE_SIZE - 1) / PLATFORM_FLASH_PAGE_SIZE;

    // Make sure the buffer is aligned to word boundary
    static uint32_t buffer32[PLATFORM_FLASH_PAGE_SIZE / 4];
    uint8_t *buffer = (uint8_t*)buffer32;

    if (filesize > PLATFORM_FLASH_TOTAL_SIZE)
    {
        logmsg("Firmware too large: ", (int)filesize, " flash size ", (int)PLATFORM_FLASH_TOTAL_SIZE);
        return false;
    }

    if (!file.seek(PLATFORM_BOOTLOADER_SIZE))
    {
        logmsg("Seek failed");
        return false;
    }

    for (int i = 0; i < num_pages; i++)
    {
        if (i % 2)
            LED_ON();
        else
            LED_OFF();
        
        if (file.read(buffer, PLATFORM_FLASH_PAGE_SIZE) <= 0)
        {
            logmsg("Firmware file read failed on page ", i);
            return false;
        }

        if (!platform_rewrite_flash_page(PLATFORM_BOOTLOADER_SIZE + i * PLATFORM_FLASH_PAGE_SIZE, buffer))
        {
            logmsg("Flash programming failed on page ", i);
            return false;
        }
    }

    return true;
}
#else // PLATFORM_FLASH_SECTOR_ERASE
bool program_firmware(FsFile &file)
{
    if (!platform_firmware_erase(file))
    {
        return false;
    }
    if (!platform_firmware_program(file))
    {
        return false;
    }
    return true;
    
}

#endif // PLATFORM_FLASH_SECTOR_ERASE

static bool mountSDCard()
{
  // Check for the common case, FAT filesystem as first partition
  if (SD.begin(SD_CONFIG))
    return true;

  // Do we have any kind of card?
  if (!SD.card() || SD.sdErrorCode() != 0)
    return false;

  // Try to mount the whole card as FAT (without partition table)
  if (static_cast<FsVolume*>(&SD)->begin(SD.card(), true, 0))
    return true;

  // Bootloader cannot do anything without FAT
  return false;
}

extern "C"
int bootloader_main(void)
{
    platform_init();
    g_log_debug = true;

    logmsg("Bootloader version: " __DATE__ " " __TIME__ " " PLATFORM_NAME);

    if (mountSDCard() || mountSDCard())
    {
        FsFile fwfile;
        char name[MAX_FILE_PATH + 1];
        if (find_firmware_image(fwfile, name))
        {
            if (program_firmware(fwfile))
            {
                logmsg("Firmware update successful!");
                fwfile.close();
                if (!SD.remove(name))
                {
                    logmsg("Failed to remove firmware file");
                }
            }
            else
            {
                logmsg("Firmware update failed!");
                platform_emergency_log_save();
            }
            
        }
    }
    else
    {
        logmsg("Bootloader SD card init failed");
    }

    logmsg("Bootloader continuing to main firmware");
    platform_boot_to_main_firmware();

    return 0;
}

#endif
