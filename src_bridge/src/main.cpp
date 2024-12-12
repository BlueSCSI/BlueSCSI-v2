// Copyright (C) 2024 akuker
// Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
// Copyright (c) 2021 Peter Lawrence
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

#include "BlueSCSI_platform.h"

#include "usb/usb_task.h"
#include "cmd_console_task.h"
#include "usb/usb_descriptors.h"
#include "usb/stdio_tinyusb_cdc.h"

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)

void bluescsi_main_task(void *param);
extern "C" void bluescsi_setup(void);

int main(void) 
{

  platform_init();

  usb_descriptors_init();
  stdio_tinyusb_cdc_init();

  printf("Welcome to BlueSCSI Bridge!\n");

  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
  //xTaskCreate(stdio_tinyusb_cdc_task, "cdcd", configMINIMAL_STACK_SIZE * 2, NULL, configMAX_PRIORITIES - 3, NULL);
  stdio_tinyusb_cdc_start_task(configMAX_PRIORITIES - 3);
  xTaskCreate(vCommandConsoleTask, "cli", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
  // This should be the highest priority task
  xTaskCreate(bluescsi_main_task, "scsiusb", configMINIMAL_STACK_SIZE*4, NULL, configMAX_PRIORITIES-1, NULL);
  
  // This should never return.....
  vTaskStartScheduler();

  return 0;
}