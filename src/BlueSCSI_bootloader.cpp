// Simple bootloader that loads new firmware from SD card.
//
// Copyright (c) 2022 Rabbit Hole Computing™

#include <BlueSCSI_platform.h>
#include "BlueSCSI_config.h"
#include "BlueSCSI_log.h"
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
            strncasecmp(name, "bluescsi", 8) == 0 &&
            strncasecmp(name + namelen - 3, "bin", 3) == 0)
        {
            root.close();
            log("Found firmware file: ", name);
            return true;
        }

        file.close();
    }

    root.close();
    return false;
}

bool program_firmware(FsFile &file)
{
    uint32_t fwsize = file.size() - PLATFORM_BOOTLOADER_SIZE;
    uint32_t num_pages = (fwsize + PLATFORM_FLASH_PAGE_SIZE - 1) / PLATFORM_FLASH_PAGE_SIZE;

    // Make sure the buffer is aligned to word boundary
    static uint32_t buffer32[PLATFORM_FLASH_PAGE_SIZE / 4];
    uint8_t *buffer = (uint8_t*)buffer32;

    if (fwsize > PLATFORM_FLASH_TOTAL_SIZE)
    {
        log("Firmware too large: ", (int)fwsize, " flash size ", (int)PLATFORM_FLASH_TOTAL_SIZE);
        return false;
    }

    if (!file.seek(PLATFORM_BOOTLOADER_SIZE))
    {
        log("Seek failed");
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
            log("Firmware file read failed on page ", i);
            return false;
        }

        if (!platform_rewrite_flash_page(PLATFORM_BOOTLOADER_SIZE + i * PLATFORM_FLASH_PAGE_SIZE, buffer))
        {
            log("Flash programming failed on page ", i);
            return false;
        }
    }

    return true;
}

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

    log("Bootloader version: " __DATE__ " " __TIME__ " " PLATFORM_NAME);

    if (mountSDCard() || mountSDCard())
    {
        FsFile fwfile;
        char name[MAX_FILE_PATH + 1];
        if (find_firmware_image(fwfile, name))
        {
            if (program_firmware(fwfile))
            {
                log("Firmware update successful!");
                fwfile.close();
                if (!SD.remove(name))
                {
                    log("Failed to remove firmware file");
                }
            }
            else
            {
                log("Firmware update failed!");
                platform_emergency_log_save();
            }

        }
    }
    else
    {
        log("Bootloader SD card init failed");
    }

    log("Bootloader continuing to main firmware");
    platform_boot_to_main_firmware();

    return 0;
}

#endif
