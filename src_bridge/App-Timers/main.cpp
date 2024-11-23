/**
 * RP2040 FreeRTOS Template - App #4
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#include "main.h"

using std::string;
using std::stringstream;


/*
 * GLOBALS
 */

// Task handles
TaskHandle_t handle_task_pico = nullptr;

// Misc
volatile TimerHandle_t led_on_timer;


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
 * TASKS
 */

/**
 * @brief Repeatedly flash the Pico's built-in LED.
 */
[[noreturn]] void task_led_pico(void* unused_arg) {

    // Create a repeating (parameter 3) timer
    led_on_timer = xTimerCreate("LED_ON_TIMER",
                                pdMS_TO_TICKS(LED_FLASH_PERIOD_MS),
                                pdTRUE,
                                (void*)TIMER_ID_LED_ON,
                                timer_fired_callback);

    // Start the repeating timer
    if (led_on_timer != nullptr) xTimerStart(led_on_timer, 0);

    // Start the task loop
    while (true) {
        // NOP
    }
}


/**
 * @brief Callback actioned when the post IRQ timer fires.
 *
 * @param timer: The triggering timer.
 */
void timer_fired_callback(TimerHandle_t timer) {

#ifdef DEBUG
    // Report the timer that fired
    auto timer_id = (uint32_t)pvTimerGetTimerID(timer);
    stringstream log_stream;
    log_stream << "Timer fired. ID: " << timer_id << ", LED: " << (timer_id == TIMER_ID_LED_ON ? "on" : "off");
    Utils::log_debug(log_stream.str());
#endif

    if (timer == led_on_timer) {
        // The LED ON timer fired so turn the LED on briefly
        led_on();

        // Create and start a one-shot timer to turn the LED off
        TimerHandle_t led_off_timer = xTimerCreate("LED_OFF_TIMER",
                                                   pdMS_TO_TICKS(LED_OFF_PERIOD_MS),
                                                   pdFALSE,
                                                   (void*)TIMER_ID_LED_OFF,
                                                   timer_fired_callback);

        // Start the one-shot timer
        if (led_off_timer != nullptr) xTimerStart(led_off_timer, 0);
    } else {
        // The LED OFF timer fired so turn the LED off
        led_off();

        // FROM 1.4.1 -- Kill the timer to prevent OOM panics
        //               Thanks, @hepoun!
        xTimerDelete(timer, 0);
    }
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

    // Set up LED
    setup_led();

    // Set up one task
    BaseType_t status_task_pico = xTaskCreate(task_led_pico, "PICO_LED_TASK",  128, nullptr, 1, &handle_task_pico);

    // Start the FreeRTOS scheduler if any of the tasks are good
    if (status_task_pico == pdPASS) {
        // Start the scheduler
        vTaskStartScheduler();
    } else {
        // Flash board LED 5 times
        uint8_t count = LED_ERROR_FLASHES;
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
