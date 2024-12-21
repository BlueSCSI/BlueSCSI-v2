// FreeRTOS task that will periodically call the TinyUSB stack
//
// Copyright (C) 2024 akuker
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along
// with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "FreeRTOS.h"
#include "task.h"
#include "stdio_tinyusb_cdc.h"
#include <stdio.h>
#include "bsp/board_api.h"
#include "tusb.h"

bool g_scsi_setup_complete = false;
bool g_early_usb_initialization = false;

static void usb_task_debug()
{
    static uint32_t usb_device_task_counter = 0;
    static uint32_t usb_device_task_counter2 = 0;
    if (usb_device_task_counter > 10000)
    {
        printf("usb_device_task() running %ld\n", usb_device_task_counter2++);
        usb_device_task_counter = 0;
    }
    usb_device_task_counter++;
}

void usb_descriptors_init();
void usb_device_task(void *param)
{
    // If we haven't been configured "early" usb initialization, wait for
    // the rest of the initialization (ex: scanning SCSI bus) to happen before
    // we start the USB task.
    while (!g_early_usb_initialization && !g_scsi_setup_complete)
    {
        vTaskDelay(100);
    }

    usb_descriptors_init();
    printf("usb_device_task()\n");
    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);

    TickType_t wake;
    wake = xTaskGetTickCount();

    do
    {

        usb_task_debug();
        tud_task();

        // Go to sleep for up to a tick if nothing to do
        if (!tud_task_event_ready())
            xTaskDelayUntil(&wake, 1);
    } while (1);
}

//-------------------------------------------------------
// Callbacks that are used by TinyUSB...

// Invoked when device is mounted
void tud_mount_cb(void)
{
    // blink_interval_ms = BLINK_MOUNTED;
    printf("tud_mount_cb\n");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    // blink_interval_ms = BLINK_NOT_MOUNTED;
    printf("tud_umount_cb\n");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    // blink_interval_ms = BLINK_SUSPENDED;
    printf("tud_suspend_cb\n");
}
