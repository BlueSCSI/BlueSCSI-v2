// TinyUSB compatible STDIO driver that can be used with the Raspberry Pi
// Pico SDK in conjunction with FreeRTOS. It uses a USB Communication Device 
// Class (CDC) interface to send the STDIO information to a USB host.
//
// This is intended to be used with FreeRTOS on a device with a custom
// custom TinyUSB configuration. The main function should be scheduled
// at a fast enough rate to handle user interaction. (10-20Hz is recommended)
//
// The TinyUSB periodic task ALSO needs to be run outside of this task. The
// STDIO and TinyUSB tasks will run asynchronously.
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
// Note: Some portions of this class may have been generated using the Claude.AI LLM

#pragma once
#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CDC_STACK_SIZE configMINIMAL_STACK_SIZE

    void stdio_tinyusb_cdc_init(void);
    bool stdio_tinyusb_cdc_start_task(uint8_t priority);
    void stdio_tinyusb_cdc_task(void *pvParameters);
    void stdio_tinyusb_cdc_deinit();
    char stdio_tinyusb_cdc_readchar();

#ifdef __cplusplus
}
#endif
