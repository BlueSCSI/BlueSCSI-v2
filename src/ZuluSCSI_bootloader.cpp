/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
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

#include <ZuluSCSI_platform.h>
#include "ZuluSCSI_config.h"
#include "ZuluSCSI_log.h"
#include <SdFat.h>
#include <string.h>

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

        if (namelen >= 11 &&
            strncasecmp(name, "zuluscsi", 8) == 0 &&
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
    uint32_t fwsize = file.size() - PLATFORM_BOOTLOADER_SIZE;
    uint32_t num_pages = (fwsize + PLATFORM_FLASH_PAGE_SIZE - 1) / PLATFORM_FLASH_PAGE_SIZE;

    // Make sure the buffer is aligned to word boundary
    static uint32_t buffer32[PLATFORM_FLASH_PAGE_SIZE / 4];
    uint8_t *buffer = (uint8_t*)buffer32;

    if (fwsize > PLATFORM_FLASH_TOTAL_SIZE)
    {
        logmsg("Firmware too large: ", (int)fwsize, " flash size ", (int)PLATFORM_FLASH_TOTAL_SIZE);
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
    uint32_t bootloader_sector_index = 0;
    uint32_t bootloader_sector_byte_count = 0;
    const uint32_t map_length = sizeof(platform_flash_sector_map)/sizeof(platform_flash_sector_map[0]);
    // Find at which sector the bootloader ends so it isn't overwritten
    for(;;)
    {
        if (bootloader_sector_index < map_length)
        {
            bootloader_sector_byte_count += platform_flash_sector_map[bootloader_sector_index];
            if (bootloader_sector_byte_count < PLATFORM_BOOTLOADER_SIZE)
            {
                bootloader_sector_index++;
            }
            else
            {
                break;
            }    
        }
        else
        {
            logmsg("Bootloader does not fit in flash");
            return false;
        }
                
    }

    uint32_t fwsize = file.size();
    if (fwsize <=  PLATFORM_BOOTLOADER_SIZE )
    {
        logmsg("Firmware file size too small: ", fwsize, " bootloader fits in the first : ", PLATFORM_BOOTLOADER_SIZE, " bytes");
        return false;
    }
    fwsize -= PLATFORM_BOOTLOADER_SIZE;
    
    // find the last sector the mainline firmware ends
    uint32_t firmware_sector_start = bootloader_sector_index + 1;
    uint32_t last_sector_index = firmware_sector_start;
    uint32_t last_sector_byte_count = 0;
    for(;;)
    {
        if (last_sector_index < map_length)
        {
            last_sector_byte_count += platform_flash_sector_map[last_sector_index];
            if (fwsize > last_sector_byte_count)
            {
                last_sector_index++;
            }
            else
            {
                break;
            }
        }
        else
        {
            logmsg("Firmware too large: ", (int) fwsize, 
                    " space left after the bootloader ",  last_sector_byte_count,
                    " total flash size ", (int)PLATFORM_FLASH_TOTAL_SIZE);
            return false;
        }
    }

    // Make sure the buffer is aligned to word boundary
    static uint32_t buffer32[PLATFORM_FLASH_WRITE_BUFFER_SIZE / 4];
    uint8_t *buffer = (uint8_t*)buffer32;

    if (!file.seek(PLATFORM_BOOTLOADER_SIZE))
    {
        logmsg("Seek failed");
        return false;
    }

    // Erase the sectors the mainline firmware will be written to
    for (int i = firmware_sector_start; i <= last_sector_index; i++)
    {
        LED_ON();
        if (!platform_erase_flash_sector(i))
        {
            logmsg("Flash failed to erase sector ", i);
            return false;
        }
        LED_OFF();
    }

    // write the mainline firmware to flash
    int32_t bytes_read = 0;
    uint32_t address_offset = 0;
    for(;;)
    {
        if (address_offset / PLATFORM_FLASH_WRITE_BUFFER_SIZE % 2)
        {
            LED_ON();
        }
        else
        {
            LED_OFF();
        }
        
        bytes_read = file.read(buffer, PLATFORM_FLASH_WRITE_BUFFER_SIZE);
        if ( bytes_read < 0)
        {
            logmsg("Firmware file read failed, error code ", (int) bytes_read);
            return false;
        }
        if (!platform_write_flash(address_offset, bytes_read, buffer))
        {
            logmsg("Failed to write flash at offset: ", address_offset, " bytes read: ",(int) bytes_read);
            return false;
        }
        if (bytes_read < PLATFORM_FLASH_WRITE_BUFFER_SIZE)
        {
            break;
        }
        address_offset += bytes_read;
        
    }
    LED_OFF();
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
