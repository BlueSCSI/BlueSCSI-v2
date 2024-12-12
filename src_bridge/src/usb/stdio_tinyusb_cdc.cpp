
// TinyUSB compatible STDIO driver that can be used with the Raspberry Pi
// Pico SDK in conjunction with FreeRTOS.
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

#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "pico/binary_info.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "ringbuffer.h"
#include "tusb.h"
#include "stdio_tinyusb_cdc.h"
#include "bsp/board_api.h"

 // Adjust based on your needs
#define RX_BUFFER_SIZE 512 // We shouldn't be receiving very much data
#define TX_BUFFER_SIZE 4096 // Large buffer for startup (before CDC task starts)

// Create buffers
static RingBuffer<char> *tx_buffer = nullptr; // transmit buffer
static RingBuffer<char> *rx_buffer = nullptr; // receive buffer

static TaskHandle_t cdc_task_handle = nullptr;
volatile bool cdc_task_running = true;

// Define the stdio driver structure
static stdio_driver_t stdio_tinyusb_cdc_driver = {
    .out_chars = NULL,
    .out_flush = NULL,
    .in_chars = NULL,
    .next = NULL,
    .crlf_enabled = true};

// Implementation for writing characters
static void stdio_tinyusb_cdc_out_chars(const char *buf, int length)
{
    if (tx_buffer)
    {
        tx_buffer->write(reinterpret_cast<const char *>(buf), length);
    }
}

// Implementation for flushing output
static void stdio_tinyusb_cdc_out_flush(void)
{
    // Doesn't need to do anything.... periodic TinyUSB thread
    // should push out the data
}

// Implementation for reading characters
static int stdio_tinyusb_cdc_in_chars(char *buf, int length)
{
    if (rx_buffer)
    {
        return rx_buffer->read(reinterpret_cast<char *>(buf), length);
    }
    return 0;
}

// Initialize the tinyusb_cdc stdio
void stdio_tinyusb_cdc_init(void)
{
    // Create buffers if they don't exist
    if (!tx_buffer)
    {
        tx_buffer = new RingBuffer<char>(TX_BUFFER_SIZE, nullptr);
    }
    if (!rx_buffer)
    {
        rx_buffer = new RingBuffer<char>(RX_BUFFER_SIZE, nullptr);
    }

    // Set up function pointers
    stdio_tinyusb_cdc_driver.out_chars = stdio_tinyusb_cdc_out_chars;
    stdio_tinyusb_cdc_driver.out_flush = stdio_tinyusb_cdc_out_flush;
    stdio_tinyusb_cdc_driver.in_chars = stdio_tinyusb_cdc_in_chars;

    if (tx_buffer && rx_buffer)
    {
        // Add your tinyusb_cdc stdio driver to the chain
        stdio_set_driver_enabled(&stdio_tinyusb_cdc_driver, true);
    }
}

void stdio_tinyusb_cdc_deinit(void)
{
    stdio_set_driver_enabled(&stdio_tinyusb_cdc_driver, false);
    delete (tx_buffer);
    delete (rx_buffer);
}

char stdio_tinyusb_cdc_readchar()
{
    char c = EOF;
    if (rx_buffer)
    {
        rx_buffer->read(&c, 1);
    }
    return c;
}

void stdio_tinyusb_cdc_task(void *params)
{
    (void)params; // Unused parameter
    TickType_t wake;
    wake = xTaskGetTickCount();
    while (cdc_task_running)
    {

        static uint32_t stdio_tinyusb_cdc_task_counter = 0;
        static uint32_t stdio_tinyusb_cdc_task_counter2 = 0;
        if (stdio_tinyusb_cdc_task_counter > 1000)
        {
            printf("stdio_tinyusb_cdc_task() running %ld\n", stdio_tinyusb_cdc_task_counter2++);
            stdio_tinyusb_cdc_task_counter = 0;
        }
        stdio_tinyusb_cdc_task_counter++;
        // Check for new USB CDC data
        if (tud_cdc_available())
        {
            char buf[64];
            uint32_t count = tud_cdc_read(buf, sizeof(buf));

            // If rx_buffer write fails, data is lost
            if (rx_buffer)
            {
                rx_buffer->write(buf, count);
            }
        }

        // Check TX buffer overflow
        if (tx_buffer && tx_buffer->checkAndClearOverflow())
        {
            const char overflow_message[] = "CDC TASK: Overflow occured!!!\n";
            tud_cdc_write(overflow_message, sizeof(overflow_message) - 1);
            tud_cdc_write_flush();
        }

        // Process TX buffer
        if (tx_buffer && tud_cdc_connected() && tud_cdc_write_available())
        {
            char buf[64];
            size_t available = tx_buffer->read(buf, sizeof(buf));

            if (available > 0)
            {
                uint32_t written = tud_cdc_write(buf, available);
                if (written > 0)
                {
                    tud_cdc_write_flush();
                }
            }
        }

        xTaskDelayUntil(&wake, 10);
    }

    // We need to kill the CDC USB task
    cdc_task_handle = nullptr;
    vTaskDelete(nullptr);
}

// bool stdio_tinyusb_cdc_start_task(uint8_t priority)
// {
//     if (cdc_task_handle != nullptr)
//     {
//         return false; // Task already running
//     }
//     cdc_task_running = true;
//     auto result = xTaskCreate(stdio_tinyusb_cdc_task,
//                               "USB CDC",
//                               configMINIMAL_STACK_SIZE * 2,
//                               NULL,
//                               priority,
//                               NULL);
//     return (result == pdPASS);
// }

void stdio_tinyusb_cdc_stop_task(void)
{
    if (cdc_task_handle != nullptr)
    {
        cdc_task_running = false;

        // Wait for task to stop (with timeout)
        TickType_t start = xTaskGetTickCount();
        while (cdc_task_handle != nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(1));

            // Timeout after 1 second
            if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(1000))
            {
                // Force delete if task doesn't stop
                vTaskDelete(cdc_task_handle);
                cdc_task_handle = nullptr;
                break;
            }
        }
    }
}

// Invoked when cdc when line state changed e.g connected/disconnected
// Use to reset to DFU when disconnect with 1200 bps
void tud_cdc_line_state_cb(uint8_t instance, bool dtr, bool rts)
{
    (void)rts;

    // DTR = false is counted as disconnected
    if (!dtr)
    {
        // touch1200 only with first CDC instance (Serial)
        if (instance == 0)
        {
            cdc_line_coding_t coding;
            tud_cdc_get_line_coding(&coding);
            if (coding.bit_rate == 1200)
            {
                if (board_reset_to_bootloader)
                {
                    board_reset_to_bootloader();
                }
            }
        }
    }
}