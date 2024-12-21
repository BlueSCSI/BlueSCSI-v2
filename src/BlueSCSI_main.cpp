// Simple wrapper file that diverts boot from main program to bootloader
// when building the bootloader image by build_bootloader.py.
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "usb/usb_descriptors.h"
#include "usb/stdio_tinyusb_cdc.h"
#include "usb/msc_disk.h"
#include "usb/usb_task.h"

#define FORCE_BRIDGE 1

extern "C" void bluescsi_setup(void);
extern "C" void bluescsi_main_loop(void);

extern bool delay_usb_task;
extern bool g_scsi_msc_mode;
extern bool g_disable_usb_cdc;

void bluescsi_main_task(void *param)
{

    bluescsi_setup();

#if FORCE_BRIDGE
    g_scsi_msc_mode = true;
    g_disable_usb_cdc = false;
#endif

    if (!g_scsi_setup_complete){
        printf("WARNING: USB intiailization continuing before SCSI init has completed.\n");
        printf("This is OK if you're doing software development\n");
    }

    if (g_scsi_msc_mode)
    {
        msc_disk_init();
    }

    printf("Welcome to BlueSCSI!\n");

    while (1)
    {
        if (!g_scsi_msc_mode)
        {
            bluescsi_main_loop();
            vTaskDelay((const TickType_t)1);
        }
        else
        {
            // While in MSC mode, we all of the action happens in the
            // USB tasks. So, we'll just sleep here.
            vTaskDelay((const TickType_t)1000);
        }
    }
}
    // #endif
