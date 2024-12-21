// Simple wrapper file that diverts boot from main program to bootloader
// when building the bootloader image by build_bootloader.py.
#ifndef LIB_FREERTOS_KERNEL
// This file is broken up into two "halves". The first half is the original
// Arduino functionality. The second half is the FreeRTOS functionality.
// The Arduino functionality should be dumped at some point.

#ifdef BlueSCSI_BOOTLOADER_MAIN

extern "C" int bootloader_main(void);

#ifdef USE_ARDUINO
extern "C" void setup(void)
{
    bootloader_main();
}
extern "C" void loop(void)
{
}
#else
int main(void)
{
    return bootloader_main();
}
#endif

#else

extern "C" void bluescsi_setup(void);
extern "C" void bluescsi_main_loop(void);

#ifdef USE_ARDUINO
extern "C" void setup(void)
{
    bluescsi_setup();
}

extern "C" void loop(void)
{
    bluescsi_main_loop();
}
#else
int main(void)
{
    bluescsi_setup();
    while (1)
    {
        bluescsi_main_loop();
    }
}
#endif

#endif

#else // LIB_FREERTOS_KERNEL

// This file is broken up into two "halves". The first half is the original
// Arduino functionality. The second half is the FreeRTOS functionality.
// The Arduino functionality should be dumped at some point.
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "usb/usb_descriptors.h"
#include "usb/stdio_tinyusb_cdc.h"
#include "usb/msc_disk.h"
#include "usb/usb_task.h"

// ONLY ENABLE THIS FOR TESTING!!! SHOULDN"T BE NEEDEED
#define FORCE_BRIDGE 0

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
#endif // LIB_FREERTOS_KERNEL
