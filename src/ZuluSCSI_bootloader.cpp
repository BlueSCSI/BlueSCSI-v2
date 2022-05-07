// Simple bootloader that loads new firmware from SD card.

#include <ZuluSCSI_platform.h>
#include "ZuluSCSI_log.h"
#include <SdFat.h>
#include <string.h>

#ifdef AZPLATFORM_BOOTLOADER_SIZE

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
            azlog("Found firmware file: ", name);
            return true;
        }

        file.close();
    }

    root.close();
    return false;
}

bool program_firmware(FsFile &file)
{
    uint32_t fwsize = file.size() - AZPLATFORM_BOOTLOADER_SIZE;
    uint32_t num_pages = (fwsize + AZPLATFORM_FLASH_PAGE_SIZE - 1) / AZPLATFORM_FLASH_PAGE_SIZE;
    static uint8_t buffer[AZPLATFORM_FLASH_PAGE_SIZE];

    if (fwsize > AZPLATFORM_FLASH_TOTAL_SIZE)
    {
        azlog("Firmware too large: ", (int)fwsize, " flash size ", (int)AZPLATFORM_FLASH_TOTAL_SIZE);
        return false;
    }

    if (!file.seek(AZPLATFORM_BOOTLOADER_SIZE))
    {
        azlog("Seek failed");
        return false;
    }

    for (int i = 0; i < num_pages; i++)
    {
        if (i % 2)
            LED_ON();
        else
            LED_OFF();
        
        if (file.read(buffer, AZPLATFORM_FLASH_PAGE_SIZE) <= 0)
        {
            azlog("Firmware file read failed on page ", i);
            return false;
        }

        if (!azplatform_rewrite_flash_page(AZPLATFORM_BOOTLOADER_SIZE + i * AZPLATFORM_FLASH_PAGE_SIZE, buffer))
        {
            azlog("Flash programming failed on page ", i);
            return false;
        }
    }

    return true;
}

extern "C"
int bootloader_main(void)
{
    azplatform_init();
    g_azlog_debug = true;

    azlog("Bootloader version: " __DATE__ " " __TIME__ " " PLATFORM_NAME);

    if (SD.begin(SD_CONFIG) || SD.begin(SD_CONFIG))
    {
        FsFile fwfile;
        char name[MAX_FILE_PATH + 1];
        if (find_firmware_image(fwfile, name))
        {
            if (program_firmware(fwfile))
            {
                azlog("Firmware update successful!");
                fwfile.close();
                if (!SD.remove(name))
                {
                    azlog("Failed to remove firmware file");
                }
            }
            else
            {
                azlog("Firmware update failed!");
                azplatform_emergency_log_save();
            }
            
        }
    }
    else
    {
        azlog("Bootloader SD card init failed");
    }

    azlog("Bootloader continuing to main firmware");
    azplatform_boot_to_main_firmware();

    return 0;
}

#endif
