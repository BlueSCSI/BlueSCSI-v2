/**
 * RP2040 FreeRTOS Template - App #3
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
 * STATIC FUNCTIONS
 */
[[noreturn]] static void task_led_pico(void* unused_arg);
[[noreturn]] static void task_led_gpio(void* unused_arg);
[[noreturn]] static void task_sensor_read(void* unused_arg);
[[noreturn]] static void task_sensor_alrt(void* unused_arg);


/*
 * GLOBALS
 */
// Inter-task queues
QueueHandle_t flip_queue = nullptr;
QueueHandle_t irq_queue = nullptr;

// Task handles
TaskHandle_t handle_task_pico = nullptr;
TaskHandle_t handle_task_gpio = nullptr;
TaskHandle_t handle_task_read = nullptr;
TaskHandle_t handle_task_alrt = nullptr;

// Timers
TimerHandle_t alert_timer = nullptr;

// Semaphores
SemaphoreHandle_t semaphore_irq = nullptr;

// The 4-digit display
HT16K33_Segment display;

// The sensor
MCP9808 sensor;
volatile double read_temp = 0.0;
volatile bool sensor_good = false;
volatile bool do_clear = false;


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
 * SETUP FUNCTIONS
 */

/**
 * @brief Umbrella hardware setup routine.
 */
void setup(void) {

    setup_i2c();
    setup_led();
    setup_gpio();
}


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
    sensor_good = sensor.begin();
    if (!sensor_good) printf("[ERROR] MCP9808 not present\n");
}


void setup_gpio(void) {

    // Configure the MCP9808 alert reader
    gpio_init(ALERT_SENSE_PIN);
    gpio_set_dir(ALERT_SENSE_PIN, GPIO_IN);

    // Configure the GPIO LED
    gpio_init(RED_LED_PIN);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);
    gpio_put(ALERT_LED_PIN, false);

    // Configure the GREEN LED
    gpio_init(ALERT_LED_PIN);
    gpio_set_dir(ALERT_LED_PIN, GPIO_OUT);
    gpio_put(ALERT_LED_PIN, false);
}


/*
 * IRQ
 */

/**
 * @brief ISR for GPIO.
 *
 * @param gpio:   The pin that generates the event.
 * @param events: Which event(s) triggered the IRQ.
 */
void gpio_isr(uint gpio, uint32_t events) {

    // See BLOG POST https://blog.smittytone.net/2022/03/20/fun-with-freertos-and-pi-pico-interrupts-semaphores-notifications/

    // Clear the IRQ source
    enable_irq(false);

    /* ISR FUNCTION BODY USING DIRECT TASK NOTIFICATIONS */
    // Signal the alert clearance task
    static auto higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(handle_task_alrt, &higher_priority_task_woken);

    // Exit to context switch if necessary
    portYIELD_FROM_ISR(higher_priority_task_woken);


    /* ISR FUNCTION BODY USING A SEMAPHORE */
    /*
    // Signal the alert clearance task
    static BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(semaphore_irq, &higher_priority_task_woken);

    // Exit to context switch if necessary
    portYIELD_FROM_ISR(higher_priority_task_woken);
     */


    /*  ISR FUNCTION BODY USING A QUEUE */
    /*
    // Signal the alert clearance task
    static bool state = 1;
    xQueueSendToBackFromISR(irq_queue, &state, 0);
     */
}


/**
 * @brief Enable or disable the IRQ,
 *
 * @param state: The enablement state. Default: `true`.
 */
void enable_irq(bool state) {

    gpio_set_irq_enabled_with_callback(ALERT_SENSE_PIN,
                                       GPIO_IRQ_LEVEL_LOW,
                                       state,
                                       &gpio_isr);
}


/*
 * TASKS
 */

/**
 * @brief Repeatedly flash the Pico's built-in LED.
 */
[[noreturn]] static void task_led_pico(void* unused_arg) {

    // Store the Pico LED state
    uint8_t pico_led_state = LED_OFF;

    int count = -1;
    bool state = true;
    TickType_t then = 0;

    // Enable IRQ on the sensor pin
    if (sensor_good) enable_irq(true);

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
                pico_led_state = LED_OFF;
                xQueueSendToBack(flip_queue, &pico_led_state, 0);
                count++;
                display_int(count);
            } else {
                led_off();
                pico_led_state = LED_ON;
                xQueueSendToBack(flip_queue, &pico_led_state, 0);
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
[[noreturn]] static void task_led_gpio(void* unused_arg) {
    // This variable will take a copy of the value
    // added to the FreeRTOS xQueue
    uint8_t passed_value_buffer = LED_OFF;

    while (true) {
        // Wait for an event: check for an item in the FreeRTOS xQueue
        if (xQueueReceive(flip_queue, &passed_value_buffer, portMAX_DELAY) == pdPASS) {
            // Received a value so flash the GPIO LED accordingly
            // (NOT the sent value)
#ifdef DEBUG
            if (passed_value_buffer == LED_ON) Utils::log_debug("GPIO LED FLASH");
#endif

            gpio_put(RED_LED_PIN, passed_value_buffer);
        }

        // Yield -- uncomment the next line to enable,
        // See BLOG POST https://blog.smittytone.net/2022/03/04/further-fun-with-freertos-scheduling/
        //vTaskDelay(0);
    }
}


/**
 * @brief Repeatedly read the sensor and store the current
 *        temperature.
 */
[[noreturn]] static void task_sensor_read(void* unused_arg) {
    while (true) {
        // Just read the sensor and yield
        read_temp = sensor.read_temp();
        vTaskDelay(SENSOR_TASK_DELAY_TICKS);
    }
}


/**
 * @brief Repeatedly check for an ISR-issued notification that
 *        the sensor alert was triggered.
 */
[[noreturn]] static void task_sensor_alrt(void* unused_arg) {
    // See BLOG POST https://blog.smittytone.net/2022/03/20/fun-with-freertos-and-pi-pico-interrupts-semaphores-notifications/

    /*  ALERT HANDLER TASK FUNCTION BODY USING DIRECT TASK NOTIFICATIONS */
    while (true) {
        // Block until a notification arrives
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

#ifdef DEBUG
        Utils::log_debug("IRQ detected");
#endif

        // Show the IRQ was hit
        gpio_put(ALERT_LED_PIN, true);

        // Set and start a timer to clear the alert
        set_alert_timer();
    }

    /*  ALERT HANDLER TASK FUNCTION BODY USING A SEMAPHORE */
    /*
    while (true) {
        // Wait for event: is there a semaphore?
        if (xSemaphoreTake(semaphore_irq, portMAX_DELAY) == pdPASS) {
             #ifdef DEBUG
             Utils::log_debug("IRQ detected");
             #endif

             // Show the IRQ was hit
             gpio_put(ALERT_LED_PIN, true);

             // Set and start a timer to clear the alert
             set_alert_timer();
         }
     }
     */


    /*  ALERT HANDLER TASK FUNCTION BODY USING A QUEUE */
    /*
    uint8_t passed_value_buffer = 0;

    while (true) {
        // Wait for event: is a message pending in the IRQ queue?
        if (xQueueReceive(irq_queue, &passed_value_buffer, portMAX_DELAY) == pdPASS) {
            if (passed_value_buffer == 1) {
                #ifdef DEBUG
                Utils::log_debug("IRQ detected");
                #endif

                // Show the IRQ was hit
                gpio_put(ALERT_LED_PIN, true);

                // Set and start a timer to clear the alert
                set_alert_timer();
            }
        }
    }
     */
}


/**
 * @brief Callback actioned when the post IRQ timer fires.
 *
 * @param timer: The triggering timer.
 */
void timer_fired_callback(TimerHandle_t timer) {

#ifdef DEBUG
    Utils::log_debug("Timer fired");
#endif

    if (read_temp < (double)TEMP_UPPER_LIMIT_C) {
        gpio_put(ALERT_LED_PIN, false);
        alert_timer = nullptr;

        // Reset the sensor alert
        sensor.clear_alert(true);

        // IRQ disabled at this point, so reenable it
        // NOTE This has to come after the previous line, or it
        //      will trip immediately!
        enable_irq(true);
    } else {
        // Start the timer again
        set_alert_timer();
    }
}


/**
 * @brief Set and start a timer to clear the alert.
 */
void set_alert_timer(void) {
    alert_timer = xTimerCreate("ALERT_TIMER",
                               pdMS_TO_TICKS(ALERT_DISPLAY_PERIOD_MS),
                               pdFALSE,
                               nullptr,
                               timer_fired_callback);
    if (alert_timer != nullptr) xTimerStart(alert_timer, SENSOR_TASK_DELAY_TICKS);
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
    #endif

    // Set up the hardware
    setup();
    display.set_brightness(1);

    // Log app info
    #ifdef DEBUG
    Utils::log_device_info();
    #endif

    // Set up four tasks
    BaseType_t status_task_pico = xTaskCreate(task_led_pico, "PICO_LED_TASK",  128, nullptr, 1, &handle_task_pico);
    BaseType_t status_task_gpio = xTaskCreate(task_led_gpio, "GPIO_LED_TASK",  128, nullptr, 1, &handle_task_gpio);
    BaseType_t status_task_read = xTaskCreate(task_sensor_read, "SENSOR_TASK", 128, nullptr, 1, &handle_task_read);
    BaseType_t status_task_alrt = xTaskCreate(task_sensor_alrt, "ALERT_TASK",  128, nullptr, 1, &handle_task_alrt);

    // Start the FreeRTOS scheduler if any of the tasks are good
    if (status_task_pico == pdPASS || status_task_gpio == pdPASS || (status_task_read == pdPASS && status_task_alrt == pdPASS)) {
        // Set up the event queues: one for flips, one for IRQs
        flip_queue = xQueueCreate(4, sizeof(uint8_t));
        irq_queue = xQueueCreate(1, sizeof(uint8_t));

        // Create a binary semaphore to signal IRQs
        semaphore_irq = xSemaphoreCreateBinary();
        assert(semaphore_irq != nullptr);

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
