
/*  This file is intended to be used with FreeRTOS on a device using a 
 * custom TinyUSB configuration. This will schedule a new task that runs 
 * at approximately 20 Hz to poll the USB CDC interface for new input and
 * to transmit any data that needs to be sent. It is up to the program
 * main() to schedule this task.
 * 
 * The TinyUSB periodic task also needs to be run outside of this task.
 */


#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "pico/binary_info.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "ringbuffer.h"
#include "tusb.h"
#include "stdio_tinyusb_cdc.h"
#include "bsp/board_api.h"

#define BUFFER_SIZE 1024  // Adjust based on your needs

// Create buffers
static RingBuffer<char>* tx_buffer = nullptr;  // transmit buffer
static RingBuffer<char>* rx_buffer = nullptr; // receive buffer

static TaskHandle_t cdc_task_handle = nullptr;
volatile bool cdc_task_running = false;

// Define the stdio driver structure
static stdio_driver_t stdio_tinyusb_cdc_driver = {
    .out_chars = NULL,
    .out_flush = NULL,
    .in_chars = NULL,
    .next = NULL,
    .crlf_enabled = true
};

// Implementation for writing characters
static void stdio_tinyusb_cdc_out_chars(const char *buf, int length) {
    if (tx_buffer) {
        tx_buffer->write(reinterpret_cast<const char*>(buf), length);
    }
}

// Implementation for flushing output
static void stdio_tinyusb_cdc_out_flush(void) {
    // Doesn't need to do anything.... periodic thread should push out the data
    // Your tinyusb_cdc flush implementation
    // Example:
    // tinyusb_cdc_device_flush();
}

// Implementation for reading characters
static int stdio_tinyusb_cdc_in_chars(char *buf, int length) {
        if (rx_buffer) {
            return rx_buffer->read(reinterpret_cast<char*>(buf), length);
        }
        return 0;
    
}

// Initialize the tinyusb_cdc stdio
void stdio_tinyusb_cdc_init(void) {
    // Create buffers if they don't exist
    if (!tx_buffer) {
        tx_buffer = new RingBuffer<char>(BUFFER_SIZE, nullptr);
    }
    if (!rx_buffer) {
        rx_buffer = new RingBuffer<char>(BUFFER_SIZE, nullptr);
    }

    // Set up function pointers
    stdio_tinyusb_cdc_driver.out_chars = stdio_tinyusb_cdc_out_chars;
    stdio_tinyusb_cdc_driver.out_flush = stdio_tinyusb_cdc_out_flush;
    stdio_tinyusb_cdc_driver.in_chars = stdio_tinyusb_cdc_in_chars;
    
    if(tx_buffer && rx_buffer){
        // Add your tinyusb_cdc stdio driver to the chain
        stdio_set_driver_enabled(&stdio_tinyusb_cdc_driver, true);
    }
}



void stdio_tinyusb_cdc_deinit(void) {
    stdio_set_driver_enabled(&stdio_tinyusb_cdc_driver, false);
    delete(tx_buffer);
    delete(rx_buffer);
}

char stdio_tinyusb_cdc_readchar(){
    char c = EOF;
    if (rx_buffer) {
        rx_buffer->read(&c, 1);
    }
    return c;
}

void stdio_tinyusb_cdc_task(void* params) {
    (void)params;  // Unused parameter
    
    while (cdc_task_running) {
        // Check for new USB CDC data
        if (tud_cdc_available()) {
            char buf[64];
            uint32_t count = tud_cdc_read(buf, sizeof(buf));
            
            // If rx_buffer write fails, data is lost
            if (rx_buffer) {
                rx_buffer->write(buf, count);
            }
        }
        
        // Check TX buffer overflow

        if (tx_buffer && tx_buffer->checkAndClearOverflow()) {
            const char overflow_message[] = "CDC TASK: Overflow occured!!!\n";
            tud_cdc_write(overflow_message, sizeof(overflow_message) - 1);
            tud_cdc_write_flush();
        }
        
        // Process TX buffer
        if (tx_buffer && tud_cdc_connected() && tud_cdc_write_available()) {
            char buf[64];
            size_t available = tx_buffer->read(buf, sizeof(buf));
            
            if (available > 0) {
                uint32_t written = tud_cdc_write(buf, available);
                if (written > 0) {
                    tud_cdc_write_flush();
                }
            }
        }
        
        // Small delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // We need to kill the CDC USB task
    cdc_task_handle = nullptr;
    vTaskDelete(nullptr);
}

bool stdio_tinyusb_cdc_start_task(uint8_t priority) {
        if (cdc_task_handle != nullptr) {
        return false;  // Task already running
    }
    cdc_task_running = true;
    auto result = xTaskCreate(stdio_tinyusb_cdc_task,
                "USB CDC",
                configMINIMAL_STACK_SIZE * 2,
                NULL,
                priority,
                NULL);
    return (result == pdPASS);
}

void stdio_tinyusb_cdc_stop_task(void) {
    if (cdc_task_handle != nullptr) {
        cdc_task_running = false;
        
        // Wait for task to stop (with timeout)
        TickType_t start = xTaskGetTickCount();
        while (cdc_task_handle != nullptr) {
            vTaskDelay(pdMS_TO_TICKS(1));
            
            // Timeout after 1 second
            if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(1000)) {
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