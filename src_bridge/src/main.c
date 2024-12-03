/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2021 Peter Lawrence
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "stdio_tinyusb_cdc.h"
#include "command_line.h"
#include <ctype.h>

#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "BlueSCSI_usbbridge.h"

#include "cdc_uart.h"
#include "get_serial.h"
#include "led.h"

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)
#define BLINKY_STACK_SIZE configMINIMAL_STACK_SIZE

static void usb_device_task(void *param)
{

  // init device stack on configured roothub port
  // This should be called after scheduler/kernel is started.
  // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
  tud_init(BOARD_TUD_RHPORT);
  if (board_init_after_tusb)
  {
    board_init_after_tusb();
  }
  TickType_t wake;
  wake = xTaskGetTickCount();

  do
  {

    // char temp_str[128];
    static uint32_t counter = 0;
    static uint32_t counter2 = 0;
    if (counter > 4000)
    {
      printf("Hello World %ld\n", counter2++);
      counter = 0;
    }
    else
    {
      counter++;
    }
    tud_task();

    // volatile char rx_char = stdio_tinyusb_cdc_readchar();
    // if (rx_char != (char)EOF) {
    //     printf("You typed: %c\n", rx_char);
    // }

    // Go to sleep for up to a tick if nothing to do
    if (!tud_task_event_ready())
      xTaskDelayUntil(&wake, 1);

  } while (1);
}

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

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void *param)
{
  (void)param;
  static bool led_state = false;
  uint32_t blink_interval_ms = 500;

  while (1)
  {
    // Blink every interval ms
    vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
}

int main(void)
{

  board_init();
  stdio_tinyusb_cdc_init();

  printf("Welcome to BlueSCSI Bridge!\n");

  scsiUsbBridgeInit();

  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
  stdio_tinyusb_cdc_start_task(configMAX_PRIORITIES - 2);
  xTaskCreate(vCommandConsoleTask, "cli", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);

  // This should never return.....
  vTaskStartScheduler();

  return 0;
}