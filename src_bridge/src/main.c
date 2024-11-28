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
#include <ctype.h>

#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

// #if PICO_SDK_VERSION_MAJOR >= 2
#include "bsp/board_api.h"
// #else
// #include "bsp/board.h"
// #endif
#include "tusb.h"
#include "BlueSCSI_usbbridge.h"

// #include "probe_config.h"
// #include "probe.h"
#include "cdc_uart.h"
#include "get_serial.h"
#include "led.h"
// #include "tusb_edpt_handler.h"
// #include "DAP.h"

// UART0 for debugprobe debug
// UART1 for debugprobe to target device

// static uint8_t TxDataBuffer[CFG_TUD_HID_EP_BUFSIZE];
// static uint8_t RxDataBuffer[CFG_TUD_HID_EP_BUFSIZE];

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// #define THREADED 1

// #define UART_TASK_PRIO (tskIDLE_PRIORITY + 3)
// #define TUD_TASK_PRIO  (tskIDLE_PRIORITY + 2)
// #define DAP_TASK_PRIO  (tskIDLE_PRIORITY + 1)

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#define CDC_STACK_SIZE      configMINIMAL_STACK_SIZE
#define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE

TaskHandle_t dap_taskhandle, tud_taskhandle;
static void local_cdc_task(void);
static void usb_device_task(void *param)
{

  // init device stack on configured roothub port
  // This should be called after scheduler/kernel is started.
  // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
  tud_init(BOARD_TUD_RHPORT);
  
    if (board_init_after_tusb) {
    board_init_after_tusb();
  }
    // TickType_t wake;
    // wake = xTaskGetTickCount();
    do {
        tud_task();
        local_cdc_task();
        // following code only run if tud_task() process at least 1 event
       tud_cdc_write_flush();
// #ifdef PROBE_USB_CONNECTED_LED
//         if (!gpio_get(PROBE_USB_CONNECTED_LED) && tud_ready())
//             gpio_put(PROBE_USB_CONNECTED_LED, 1);
//         else
//             gpio_put(PROBE_USB_CONNECTED_LED, 0);
// #endif
//         // Go to sleep for up to a tick if nothing to do
//         if (!tud_task_event_ready())
//             xTaskDelayUntil(&wake, 1);
    } while (1);
}

// static void led_blinking_task(void);
// static void cdc_task(void);

// /*------------- MAIN -------------*/
// int main(void) {
//   board_init();

//   // init device stack on configured roothub port
//   tud_init(BOARD_TUD_RHPORT);

//   if (board_init_after_tusb) {
//     board_init_after_tusb();
//   }

//   while (1) {
//     tud_task(); // tinyusb device task
//     cdc_task();
//     led_blinking_task();
//   }
// }

// echo to either Serial0 or Serial1
// with Serial0 as all lower case, Serial1 as all upper case
static void echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count) {
  uint8_t const case_diff = 'a' - 'A';
// uint8_t counter=0;
  for (uint32_t i = 0; i < count; i++) {
    if (itf == 0) {
      // echo back 1st port as lower case
      if (isupper(buf[i])) buf[i] += case_diff;
    } else {
      // echo back 2nd port as upper case
      if (islower(buf[i])) buf[i] -= case_diff;
    }

    tud_cdc_n_write_char(itf, buf[i]);
  }
  tud_cdc_n_write_flush(itf);
}

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
static void local_cdc_task(void) {
  uint8_t itf;
// while(1){
  for (itf = 0; itf < CFG_TUD_CDC; itf++) {
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    // if ( tud_cdc_n_connected(itf) )
    {
      if (tud_cdc_n_available(itf)) {
        uint8_t buf[64];

        uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

        // echo back to both serial ports
        echo_serial_port(0, buf, count);
        echo_serial_port(1, buf, count);
      }
      static uint32_t counter = 0;
    char temp_str[128];
    sprintf(temp_str, "Hello World %ld\n", counter++);
    tud_cdc_n_write_str(itf, temp_str);
    tud_cdc_n_write_flush(itf);

    // }
  }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
// Use to reset to DFU when disconnect with 1200 bps
void tud_cdc_line_state_cb(uint8_t instance, bool dtr, bool rts) {
  (void)rts;

  // DTR = false is counted as disconnected
  if (!dtr) {
    // touch1200 only with first CDC instance (Serial)
    if (instance == 0) {
      cdc_line_coding_t coding;
      tud_cdc_get_line_coding(&coding);
      if (coding.bit_rate == 1200) {
        if (board_reset_to_bootloader) {
          board_reset_to_bootloader();
        }
      }
    }
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void* param) {
  (void) param;
  static uint32_t start_ms = 0;
  static bool led_state = false;

  while (1) {
    // Blink every interval ms
    // vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
    vTaskDelay(blink_interval_ms / 10);
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
}


int main(void) {

    board_init();
    // usb_serial_init();
    // cdc_uart_init();
    // tusb_init();
    // stdio_uart_init();

    // led_init();

  /// This causes the rp2040 to crash for right now............
  /// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    scsiUsbBridgeInit();

    printf("Welcome to BlueSCSI Bridge!\n");

#if configSUPPORT_STATIC_ALLOCATION
  // blinky task
  xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, blinky_stack, &blinky_taskdef);

  // Create a task for tinyusb device stack
  xTaskCreateStatic(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_device_stack, &usb_device_taskdef);

  // Create CDC task
  // xTaskCreateStatic(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, cdc_stack, &cdc_taskdef);
#else
  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
  // xTaskCreate(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
#endif

    // if (THREADED) {
    //     /* UART needs to preempt USB as if we don't, characters get lost */
    //     xTaskCreate(cdc_thread, "UART", configMINIMAL_STACK_SIZE, NULL, UART_TASK_PRIO, &uart_taskhandle);
    //     xTaskCreate(usb_thread, "TUD", configMINIMAL_STACK_SIZE, NULL, TUD_TASK_PRIO, &tud_taskhandle);
    //     /* Lowest priority thread is debug - need to shuffle buffers before we can toggle swd... */
    //     // xTaskCreate(dap_thread, "DAP", configMINIMAL_STACK_SIZE, NULL, DAP_TASK_PRIO, &dap_taskhandle);
    //     vTaskStartScheduler();
    // }

  // skip starting scheduler (and return) for ESP32-S2 or ESP32-S3
#if !TUP_MCU_ESPRESSIF
  vTaskStartScheduler();
#endif

  return 0;

// #if (PROBE_DEBUG_PROTOCOL == PROTO_DAP_V2)
//         if (tud_vendor_available()) {
//             uint32_t resp_len;
//             // tud_vendor_read(RxDataBuffer, sizeof(RxDataBuffer));
//             // // resp_len = DAP_ProcessCommand(RxDataBuffer, TxDataBuffer);
//             // tud_vendor_write(TxDataBuffer, resp_len);
//         }
// #endif
    }

    // return 0;
// }

// // uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
// // {
// //   // TODO not Implemented
// //   (void) itf;
// //   (void) report_id;
// //   (void) report_type;
// //   (void) buffer;
// //   (void) reqlen;

// //   return 0;
// // }

// // void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* RxDataBuffer, uint16_t bufsize)
// // {
// //   // uint32_t response_size = TU_MIN(CFG_TUD_HID_EP_BUFSIZE, bufsize);

// //   // This doesn't use multiple report and report ID
// //   (void) itf;
// //   (void) report_id;
// //   (void) report_type;

// //   // DAP_ProcessCommand(RxDataBuffer, TxDataBuffer);

// //   // tud_hid_report(0, TxDataBuffer, response_size);
// // }

// #if (PROBE_DEBUG_PROTOCOL == PROTO_DAP_V2)
// // extern uint8_t const desc_ms_os_20[];

// // bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
// // {
// //   // nothing to with DATA & ACK stage
// //   if (stage != CONTROL_STAGE_SETUP) return true;

// //   switch (request->bmRequestType_bit.type)
// //   {
// //     case TUSB_REQ_TYPE_VENDOR:
// //       switch (request->bRequest)
// //       {
// //         case 1:
// //           // if ( request->wIndex == 7 )
// //           // {
// //           //   // Get Microsoft OS 2.0 compatible descriptor
// //           //   uint16_t total_len;
// //           //   memcpy(&total_len, desc_ms_os_20+8, 2);

// //           //   return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, total_len);
// //           // }else
// //           // {
// //             return false;
// //           // }

// //         default: break;
// //       }
// //     break;
// //     default: break;
// //   }

// //   // stall unknown request
// //   return false;
// // }
// #endif



