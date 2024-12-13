
#include "FreeRTOS.h"
#include "task.h"
#include "stdio_tinyusb_cdc.h"
#include <stdio.h>
#include "bsp/board_api.h"
#include "tusb.h"

bool g_delay_usb_task = true;

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
    // Wait for SCSI Scan to complete
    while (g_delay_usb_task)
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
