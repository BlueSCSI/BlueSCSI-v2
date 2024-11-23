/**
 * RP2040 FreeRTOS Template - App #2
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#include "main.h"

using std::string;
using std::vector;
using std::stringstream;


/*
 * GLOBALS
 */

// This is the inter-task queue
volatile QueueHandle_t queue = nullptr;

// Task handles
TaskHandle_t pico_task_handle;
TaskHandle_t gpio_task_handle;
TaskHandle_t sens_task_handle;

// The 4-digit display
HT16K33_Segment display;

// The sensor
MCP9808 sensor;
volatile double read_temp = 0.0;


/*
 * LED FUNCTIONS
 */

/**
 * @brief Configure the on-board LED.
 */
void setup_led(void) {

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    led_off();
}


/**
 * @brief Turn the on-board LED on.
 */
void led_on(void) {

    led_set();
}


/**
 * @brief Turn the on-board LED off.
 */
void led_off(void) {

    led_set(false);
}


/**
 * @brief Set the on-board LED's state.
 */
void led_set(bool state) {

    gpio_put(PICO_DEFAULT_LED_PIN, state);
}


/*
 * I2C FUNCTIONS
 */

/**
 * @brief Set up I2C and the devices that use it.
 */
void setup_i2c(void) {

    // Initialise the I2C bus for the display and sensor
    I2C::setup();

    // Initialise the display
    display = HT16K33_Segment();
    display.init();

    // Initialise the sensor
    sensor = MCP9808();
}


/*
 * GENERAL SETUP
 */

/**
 * @brief Umbrella hardware setup routine.
 */
void setup(void) {

    setup_i2c();
    setup_led();
}


/*
 * TASKS
 */

/**
 * @brief Repeatedly flash the Pico's built-in LED.
 */
[[noreturn]] void led_task_pico(void* unused_arg) {

    // Store the Pico LED state
    uint8_t pico_led_state = 0;

    int count = -1;
    bool state = true;
    TickType_t then = 0;

    // Start the task loop
    while (true) {
        // Turn Pico LED on an add the LED state
        // to the FreeRTOS xQUEUE
        TickType_t now = xTaskGetTickCount();
        if (now - then >= 500) {
            then = now;

            if (state) {
#ifdef DEBUG
                Utils::log_debug("PICO LED FLASH");
#endif
                led_on();
                pico_led_state = 1;
                xQueueSendToBack(queue, &pico_led_state, 0);
                display_int(++count);
            } else {
                // Turn Pico LED off an add the LED state
                // to the FreeRTOS xQUEUE
                led_off();
                pico_led_state = 0;
                xQueueSendToBack(queue, &pico_led_state, 0);
                display_tmp(read_temp);
            }

            state = !state;
            if (count > 9998) count = 0;
        }

        // Yield -- uncomment the next line to enable,
        // See BLOG POST https://blog.smittytone.net/2022/03/04/further-fun-with-freertos-scheduling/
        //vTaskDelay(0);
    }
}


/**
 * @brief Repeatedly flash an LED connected to GPIO pin 20
 *        based on the value passed via the inter-task queue
 */
[[noreturn]] void led_task_gpio(void* unused_arg) {

    // This variable will take a copy of the value
    // added to the FreeRTOS xQueue
    uint8_t passed_value_buffer = 0;

    // Configure the GPIO LED
    gpio_init(RED_LED_PIN);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);

    while (true) {
        // Check for an item in the FreeRTOS xQueue
        if (xQueueReceive(queue, &passed_value_buffer, portMAX_DELAY) == pdPASS) {
            // Received a value so flash the GPIO LED accordingly
            // (NOT the sent value)
#ifdef DEBUG
            if (passed_value_buffer) Utils::log_debug("GPIO LED FLASH");
#endif
            gpio_put(RED_LED_PIN, passed_value_buffer == 1 ? false : true);
        }

        // Yield -- uncomment the next line to enable,
        // See BLOG POST https://blog.smittytone.net/2022/03/04/further-fun-with-freertos-scheduling/
        //vTaskDelay(0);
    }
}


[[noreturn]] void sensor_read_task(void* unused_arg) {

    while (true) {
        // Just read the sensor and yield
        read_temp = sensor.read_temp();
        vTaskDelay(20);
    }
}


/**
 * @brief Display a four-digit decimal value on the 4-digit display.
 *
 * @param number: The value to show.
 */
void display_int(int number) {

    // Convert the temperature value (a float) to a string value
    // fixed to two decimal places
    if (number < 0 || number > 9999) number = 9999;
    const uint32_t bcd_val = Utils::bcd(number);

    display.clear();
    display.set_number((bcd_val >> 12) & 0x0F, 0, false);
    display.set_number((bcd_val >> 8)  & 0x0F, 1, false);
    display.set_number((bcd_val >> 4)  & 0x0F, 2, false);
    display.set_number(bcd_val         & 0x0F, 3, false);
    display.draw();
}


/**
 * @brief Display a three-digit temperature on the 4-digit display.
 *
 * @param value: The value to show.
 */
void display_tmp(double value) {

    // Convert the temperature value (a float) to a string value
    // fixed to two decimal places
    stringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    const string temp = stream.str();

    // Display the temperature on the LED
    uint8_t digit = 0;
    char previous_char = 0;
    char current_char = 0;
    for (uint32_t i = 0 ; (i < temp.length() || digit == 3) ; ++i) {
        current_char = temp[i];
        if (current_char == '.' && digit > 0) {
            display.set_alpha(previous_char, digit - 1, true);
        } else {
            display.set_alpha(current_char, digit);
            previous_char = current_char;
            ++digit;
        }
    }

    // Add a final 'c' and update the display
    display.set_alpha('c', 3).draw();
}


/*
 * RUNTIME START
 */

int main() {

    // DEBUG
#ifdef DEBUG
    stdio_init_all();
    // Pause to allow the USB path to initialize
    sleep_ms(2000);

    // Log app info
    Utils::log_device_info();
#endif

    // Set up the hardware
    setup();
    display.set_brightness(1);

    // Set up three tasks
    BaseType_t pico_task_status = xTaskCreate(led_task_pico, "PICO_LED_TASK",  128, nullptr, 1, &pico_task_handle);
    BaseType_t gpio_task_status = xTaskCreate(led_task_gpio, "GPIO_LED_TASK",  128, nullptr, 1, &gpio_task_handle);
    BaseType_t sens_task_status = xTaskCreate(sensor_read_task, "SENSOR_TASK", 128, nullptr, 1, &sens_task_handle);

    // Proceed if any of the tasks are good
    if (pico_task_status == pdPASS || gpio_task_status == pdPASS || sens_task_status == pdPASS) {
        // Set up the event queue
        queue = xQueueCreate(4, sizeof(uint8_t));

        // Start the sceduler
        vTaskStartScheduler();
    } else {
        // Flash board LED 5 times
        uint8_t count = 5;
        while (count > 0) {
            led_on();
            vTaskDelay(100);
            led_off();
            vTaskDelay(100);
            count--;
        }
    }

    // We should never get here, but just in case...
    while(true) {
        // NOP
    }
}
