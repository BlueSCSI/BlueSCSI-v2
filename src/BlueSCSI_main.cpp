// Simple wrapper file that diverts boot from main program to bootloader
// when building the bootloader image by build_bootloader.py.
#include "FreeRTOS.h"
#include "task.h"
#include "BlueSCSI_usbbridge.h"
#include <stdio.h>
#include "usb/usb_descriptors.h"

#define FORCE_BRIDGE 1

extern "C" void bluescsi_setup(void);
extern "C" void bluescsi_main_loop(void);
void dump_usb_desc_data();
extern "C" void run_usb_desc_tests();

void bluescsi_main(void *param)
{
    bluescsi_setup();

#if FORCE_BRIDGE
    // // USB Descriptor test code....
    // dump_usb_desc_data();
    // run_usb_desc_tests();


    auto bridge = BlueScsiBridge();
    bridge.init();

    bridge.mainLoop();
#endif

    while (1)
    {

        bluescsi_main_loop();
        vTaskDelay((const TickType_t)1);
    }
}

// #endif
