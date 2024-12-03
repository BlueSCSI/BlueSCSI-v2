// Copyright (c) 2022 Rabbit Hole Computing™
// Copyright (c) 2023 Eric Helgeson
// Copyright (c) 2023 Tech by Androda, LLC

#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#ifndef LIB_FREERTOS_KERNEL
#include <SdFat.h>
#else
#include <stdio.h>
#endif
#include <scsi.h>
#include <assert.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/pll.h>
#include <hardware/clocks.h>
#include <hardware/spi.h>
#include <hardware/adc.h>
#include <hardware/flash.h>
#include <hardware/structs/xip_ctrl.h>
#include <hardware/structs/usb.h>
#ifdef ENABLE_AUDIO_OUTPUT
#include "audio.h"
#endif

#ifndef LIB_FREERTOS_KERNEL
#ifndef __MBED__
#include <Adafruit_TinyUSB.h>
# include <class/cdc/cdc_device.h>
#else
# include <platform/mbed_error.h>
# include <USB/PluggableUSBSerial.h>
#endif // __MBED__
#endif // LIB_FREERTOS_KERNEL

#include <pico/multicore.h>
#include "scsi_accel_rp2040.h"
#include "hardware/i2c.h"

extern "C" {

const char *g_platform_name = PLATFORM_NAME;
static bool g_scsi_initiator = false;
static bool g_supports_initiator = false;
static uint32_t g_flash_chip_size = 0;
static bool g_uart_initialized = false;
SCSI_PINS scsi_pins = {  // Default values, to be tweaked later as needed
    .OUT_IO = SCSI_OUT_IO,
    .OUT_CD = SCSI_OUT_CD,
    .OUT_REQ = SCSI_OUT_REQ,
    .OUT_SEL = SCSI_OUT_SEL,
    .OUT_MSG = SCSI_OUT_MSG,
    .OUT_RST = SCSI_OUT_RST,
    .OUT_BSY = SCSI_OUT_BSY,
    .OUT_ACK = SCSI_OUT_ACK,

    .IN_IO = SCSI_IN_IO,
    .IN_CD = SCSI_IN_CD,
    .IN_MSG = SCSI_IN_MSG,
    .IN_REQ = SCSI_IN_REQ,
    .IN_SEL = SCSI_IN_SEL,
    .IN_BSY = SCSI_IN_BSY,
    .IN_RST = SCSI_IN_RST,
    .IN_ACK = SCSI_IN_ACK,
    .IN_ATN = SCSI_IN_ATN,

    .SCSI_ACCEL_PINMASK = SCSI_ACCEL_SETPINS
};

#ifdef MBED
void mbed_error_hook(const mbed_error_ctx * error_context);
#endif

/***************/
/* GPIO init   */
/***************/

// Helper function to configure whole GPIO in one line
static void gpio_conf(uint gpio, gpio_function_t fn, bool pullup, bool pulldown, bool output, bool initial_state, bool fast_slew)
{
    gpio_put(gpio, initial_state);
    gpio_set_dir(gpio, output);
    gpio_set_pulls(gpio, pullup, pulldown);
    gpio_set_function(gpio, fn);

    if (fast_slew)
    {
        pads_bank0_hw->io[gpio] |= PADS_BANK0_GPIO0_SLEWFAST_BITS;
    }
}

#ifdef ENABLE_AUDIO_OUTPUT
// Increases clk_sys and clk_peri to 135.428571MHz at runtime to support
// division to audio output rates. Invoke before anything is using clk_peri
// except for the logging UART, which is handled below.
static void reclock_for_audio() {
    // ensure UART is fully drained before we mess up its clock
    uart_tx_wait_blocking(uart0);
    // switch clk_sys and clk_peri to pll_usb
    // see code in 2.15.6.1 of the datasheet for useful comments
    clock_configure(clk_sys,
            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
            48 * MHZ,
            48 * MHZ);
    clock_configure(clk_peri,
            0,
            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
            48 * MHZ,
            48 * MHZ);
    // reset PLL for 135.428571MHz
    pll_init(pll_sys, 1, 948000000, 7, 1);
    // switch clocks back to pll_sys
    clock_configure(clk_sys,
            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
            CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
            135428571,
            135428571);
    clock_configure(clk_peri,
            0,
            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
            135428571,
            135428571);
    // reset UART for the new clock speed
    uart_init(uart0, 1000000);
}
#endif

void platform_init()
{
    // Make sure second core is stopped
    multicore_reset_core1();
#ifndef LIB_FREERTOS_KERNEL
#ifndef __MBED__
    Serial.begin(115200);
#endif // __MBED__
#endif // LIB_FREERTOS_KERNEL

    // Default debug logging to disabled
    g_log_debug = false;

    // Report platform and firmware version
    log("Platform: ", g_platform_name);
    log("FW Version: ", g_log_firmwareversion);
    debuglog("PicoSDK: ", PICO_SDK_VERSION_STRING);

    /* First configure the pins that affect external buffer directions.
     * RP2040 defaults to pulldowns, while these pins have external pull-ups.
     */
    //        pin             function       pup   pdown  out    state fast
    gpio_conf(SCSI_DATA_DIR,  GPIO_FUNC_SIO, false,false, true,  false, true);
    
    gpio_conf(scsi_pins.OUT_BSY,   GPIO_FUNC_SIO, false,false, true,  true, false);   
    //gpio_set_drive_strength(SCSI_OUT_BSY, GPIO_DRIVE_STRENGTH_8MA);
    gpio_conf(scsi_pins.OUT_SEL,   GPIO_FUNC_SIO, true, false, true,  true, false);
    gpio_conf(scsi_pins.OUT_ACK,   GPIO_FUNC_SIO, true, false, true,  true, false);
    gpio_conf(scsi_pins.OUT_IO,    GPIO_FUNC_SIO, true, false, true,  true, false);
    gpio_conf(scsi_pins.OUT_REQ,   GPIO_FUNC_SIO, true, false, true,  true, false);

    // Determine whether I2C is supported
    // If G16 and G17 are high, this is the 2023_09a revision or later desktop board
    gpio_conf(GPIO_I2C_SCL,   GPIO_FUNC_I2C, false, false, false,  false, true);
    gpio_conf(GPIO_I2C_SDA,   GPIO_FUNC_I2C, false, false, false,  false, true);
    delay(10);
    bool d50_2023_09a = gpio_get(GPIO_I2C_SCL) && gpio_get(GPIO_I2C_SDA);

    if (d50_2023_09a) {
        log("I2C Supported");
        g_supports_initiator = true;
        gpio_conf(GPIO_I2C_SCL,   GPIO_FUNC_I2C, true, false, false,  true, true);
        gpio_conf(GPIO_I2C_SDA,   GPIO_FUNC_I2C, true, false, false,  true, true);

        // Use Pico SDK methods
        gpio_set_function(GPIO_I2C_SCL, GPIO_FUNC_I2C);
        gpio_set_function(GPIO_I2C_SDA, GPIO_FUNC_I2C);
        // gpio_pull_up(GPIO_I2C_SCL);  // TODO necessary?
        // gpio_pull_up(GPIO_I2C_SDA);
    } else {
        /* Check option switch settings */
        // Option switches: S1 is iATN, S2 is iACK
        gpio_conf(scsi_pins.IN_ACK,    GPIO_FUNC_SIO, true, false, false, false, false);
        gpio_conf(scsi_pins.IN_ATN,    GPIO_FUNC_SIO, false, false, false, false, false);
        delay(10); /// Settle time
        // Check option switches
        [[maybe_unused]] bool optionS1 = !gpio_get(scsi_pins.IN_ATN);
        [[maybe_unused]] bool optionS2 = !gpio_get(scsi_pins.IN_ACK);

        // Reset REQ to appropriate pin for older hardware
        scsi_pins.OUT_REQ = SCSI_OUT_REQ_BEFORE_2023_09a;
        scsi_pins.SCSI_ACCEL_PINMASK = SCSI_ACCEL_SETPINS_PRE09A;

        // Initialize logging to SWO pin (UART0) 
        gpio_conf(SWO_PIN,        GPIO_FUNC_UART,false,false, true,  false, true);
        uart_init(uart0, 115200);
        g_uart_initialized = true;
    #ifdef MBED
        mbed_set_error_hook(mbed_error_hook);
    #endif
    }
    // TODO Disable I2C if debug logging is enabled later?  Switch to Serial output mode?

    //log("DIP switch settings: debug log ", (int)dbglog, ", termination ", (int)termination);

#ifdef ENABLE_AUDIO_OUTPUT
    log("SP/DIF audio to expansion header enabled");
    log("-- Overclocking to 135.428571MHz");
    reclock_for_audio();
#endif

    // Get flash chip size
    uint8_t cmd_read_jedec_id[4] = {0x9f, 0, 0, 0};
    uint8_t response_jedec[4] = {0};
    uint32_t status = save_and_disable_interrupts();
    flash_do_cmd(cmd_read_jedec_id, response_jedec, 4);
    restore_interrupts_from_disabled(status);
    g_flash_chip_size = (1 << response_jedec[3]);
    log("Flash chip size: ", (int)(g_flash_chip_size / 1024), " kB");

    // SD card pins
    // Card is used in SDIO mode for main program, and in SPI mode for crash handler & bootloader.
    //        pin             function       pup   pdown  out    state fast
    gpio_conf(SD_SPI_SCK,     GPIO_FUNC_SPI, true, false, true,  true, true);
    gpio_conf(SD_SPI_MOSI,    GPIO_FUNC_SPI, true, false, true,  true, true);
    gpio_conf(SD_SPI_MISO,    GPIO_FUNC_SPI, true, false, false, true, true);
    gpio_conf(SD_SPI_CS,      GPIO_FUNC_SIO, true, false, true,  true, true);
    gpio_conf(SDIO_D1,        GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SDIO_D2,        GPIO_FUNC_SIO, true, false, false, true, true);

    if (!platform_network_supported()) {
        // LED pin
        gpio_conf(LED_PIN,        GPIO_FUNC_SIO, false,false, true,  false, false);
    }

#ifndef ENABLE_AUDIO_OUTPUT
#ifdef GPIO_I2C_SDA
    // I2C pins
    //        pin             function       pup   pdown  out    state fast
    //gpio_conf(GPIO_I2C_SCL,   GPIO_FUNC_I2C, true,false, false,  true, true);
    //gpio_conf(GPIO_I2C_SDA,   GPIO_FUNC_I2C, true,false, false,  true, true);
#endif
#else
    //        pin             function       pup   pdown  out    state fast
    //gpio_conf(GPIO_EXP_AUDIO, GPIO_FUNC_SPI, true,false, false,  true, true);
    //gpio_conf(GPIO_EXP_SPARE, GPIO_FUNC_SIO, true,false, false,  true, false);
    // configuration of corresponding SPI unit occurs in audio_setup()
#endif
}


// late_init() only runs in main application, SCSI not needed in bootloader
void platform_late_init()
{
    /* Initialize SCSI pins to required modes.
     * SCSI pins should be inactive / input at this point.
     */

    // SCSI data bus direction is switched by DATA_DIR signal.
    // Pullups make sure that no glitches occur when switching direction.
    //        pin             function       pup   pdown  out    state fast
    gpio_conf(SCSI_IO_DB0,    GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SCSI_IO_DB1,    GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SCSI_IO_DB2,    GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SCSI_IO_DB3,    GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SCSI_IO_DB4,    GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SCSI_IO_DB5,    GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SCSI_IO_DB6,    GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SCSI_IO_DB7,    GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SCSI_IO_DBP,    GPIO_FUNC_SIO, true, false, false, true, true);

    if (!g_scsi_initiator)
    {
        // Act as SCSI device / target

        // SCSI control outputs
        //        pin                   function       pup   pdown  out    state fast
        gpio_conf(scsi_pins.OUT_IO,    GPIO_FUNC_SIO, false,false, true,  true, true);
        gpio_conf(scsi_pins.OUT_MSG,   GPIO_FUNC_SIO, false,false, true,  true, true);

        // REQ pin is switched between PIO and SIO, pull-up makes sure no glitches
        gpio_conf(scsi_pins.OUT_REQ,   GPIO_FUNC_SIO, true ,false, true,  true, true);

        // Shared pins are changed to input / output depending on communication phase
        gpio_conf(scsi_pins.IN_SEL,    GPIO_FUNC_SIO, true, false, false, true, true);
        if (scsi_pins.OUT_CD != scsi_pins.IN_SEL)
        {
            gpio_conf(scsi_pins.OUT_CD,    GPIO_FUNC_SIO, false,false, true,  true, true);
        }

        gpio_conf(scsi_pins.IN_BSY,    GPIO_FUNC_SIO, true, false, false, true, true);
        if (scsi_pins.OUT_MSG != scsi_pins.IN_BSY)
        {
            gpio_conf(scsi_pins.OUT_MSG,    GPIO_FUNC_SIO, false,false, true,  true, true);
        }

        // SCSI control inputs
        //        pin                   function       pup   pdown  out    state fast
        gpio_conf(scsi_pins.IN_ACK,    GPIO_FUNC_SIO, true, false, false, true, false);
        gpio_conf(scsi_pins.IN_ATN,    GPIO_FUNC_SIO, false, false, false, true, false);
        gpio_conf(scsi_pins.IN_RST,    GPIO_FUNC_SIO, true, false, false, true, false);



#ifdef ENABLE_AUDIO_OUTPUT
        // one-time control setup for DMA channels and second core
        audio_setup();
#endif
    }
    else
    {
        // Act as SCSI Initiator

        //        pin                   function       pup   pdown  out    state fast
        gpio_conf(scsi_pins.IN_IO,     GPIO_FUNC_SIO, true ,false, false, true, false);
        gpio_conf(scsi_pins.IN_MSG,    GPIO_FUNC_SIO, true ,false, false, true, false);
        gpio_conf(scsi_pins.IN_CD,     GPIO_FUNC_SIO, true ,false, false, true, false);
        gpio_conf(scsi_pins.IN_REQ,    GPIO_FUNC_SIO, true ,false, false, true, false);
        gpio_conf(scsi_pins.IN_BSY,    GPIO_FUNC_SIO, true, false, false, true, false);
        gpio_conf(scsi_pins.IN_RST,    GPIO_FUNC_SIO, true, false, false, true, false);
        gpio_conf(scsi_pins.OUT_SEL,   GPIO_FUNC_SIO, false,false, true,  true, true);
        gpio_conf(scsi_pins.OUT_ACK,   GPIO_FUNC_SIO, true,false, true,  true, true);
        //gpio_conf(SCSI_OUT_ATN,   GPIO_FUNC_SIO, false,false, true,  true, true);  // ATN output is unused
    }
}

void platform_enable_initiator_mode() {
    if (g_supports_initiator) {
        g_scsi_initiator = true;
        log("SCSI Initiator Mode.  Will scan the bus for drives to image.");
    } else {
        log("SCSI Initiator Mode requested, but not supported.");
    }
}

bool platform_is_initiator_mode_enabled()
{
    return g_scsi_initiator;
}

void platform_disable_led(void)
{
    if (!platform_network_supported()) {
        //        pin      function       pup   pdown  out    state fast
        gpio_conf(LED_PIN, GPIO_FUNC_SIO, false,false, false, false, false);
    }
    log("Disabling status LED");
}

/*****************************************/
/* Crash handlers                        */
/*****************************************/

#ifndef LIB_FREERTOS_KERNEL
extern SdFs SD;
#endif
extern uint32_t __StackTop;

void platform_emergency_log_save()
{
    platform_set_sd_callback(NULL, NULL);

#ifndef LIB_FREERTOS_KERNEL
    SD.begin(SD_CONFIG_CRASH);
    FsFile crashfile = SD.open(CRASHFILE, O_WRONLY | O_CREAT | O_TRUNC);

    if (!crashfile.isOpen())
    {
        // Try to reinitialize
        int max_retry = 10;
        while (max_retry-- > 0 && !SD.begin(SD_CONFIG_CRASH));

        crashfile = SD.open(CRASHFILE, O_WRONLY | O_CREAT | O_TRUNC);
    }

    uint32_t startpos = 0;
    crashfile.write(log_get_buffer(&startpos));
    crashfile.write(log_get_buffer(&startpos));
    crashfile.flush();
    crashfile.close();
#endif
}

#ifdef MBED
void mbed_error_hook(const mbed_error_ctx * error_context)
{
    log("--------------");
    log("CRASH!");
    log("Platform: ", g_platform_name);
    log("FW Version: ", g_log_firmwareversion);
    log("error_status: ", (uint32_t)error_context->error_status);
    log("error_address: ", error_context->error_address);
    log("error_value: ", error_context->error_value);
    log("scsiDev.cdb: ", bytearray(scsiDev.cdb, 12));
    log("scsiDev.phase: ", (int)scsiDev.phase);
    scsi_accel_log_state();

    uint32_t *p = (uint32_t*)((uint32_t)error_context->thread_current_sp & ~3);
    for (int i = 0; i < 8; i++)
    {
        if (p == &__StackTop) break; // End of stack

        log("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
        p += 4;
    }

    platform_emergency_log_save();

    while (1)
    {
        // Flash the crash address on the LED
        // Short pulse means 0, long pulse means 1
        int base_delay = 1000;
        for (int i = 31; i >= 0; i--)
        {
            LED_OFF();
            for (int j = 0; j < base_delay; j++) delay_ns(100000);

            int delay = (error_context->error_address & (1 << i)) ? (3 * base_delay) : base_delay;
            LED_ON();
            for (int j = 0; j < delay; j++) delay_ns(100000);
            LED_OFF();
        }

        for (int j = 0; j < base_delay * 10; j++) delay_ns(100000);
    }
}
#endif

/*****************************************/
/* Debug logging and watchdog            */
/*****************************************/
#ifndef LIB_FREERTOS_KERNEL
// Send log data to USB UART if USB is connected.
// Data is retrieved from the shared log ring buffer and
// this function sends as much as fits in USB CDC buffer.
//
// This is normally called by platform_reset_watchdog() in
// the normal polling loop. If code hangs, the watchdog_callback()
// also starts calling this after 2 seconds.
// This ensures that log messages get passed even if code hangs,
// but does not unnecessarily delay normal execution.
static void usb_log_poll()
{
    static uint32_t logpos = 0;
#ifndef __MBED__
    if (Serial.availableForWrite())
    {
        // Retrieve pointer to log start and determine number of bytes available.
        uint32_t available = 0;
        const char *data = log_get_buffer(&logpos, &available);
                // Limit to CDC packet size
        uint32_t len = available;
        if (len == 0) return;
        if (len > CFG_TUD_CDC_EP_BUFSIZE) len = CFG_TUD_CDC_EP_BUFSIZE;

        // Update log position by the actual number of bytes sent
        // If USB CDC buffer is full, this may be 0
        uint32_t actual = 0;
        actual = Serial.write(data, len);
        logpos -= available - actual;
    }
#else
    if (_SerialUSB.ready())
    {
        // Retrieve pointer to log start and determine number of bytes available.
        uint32_t available = 0;
        const char *data = log_get_buffer(&logpos, &available);

        // Limit to CDC packet size
        uint32_t len = available;
        if (len == 0) return;
        if (len > CDC_MAX_PACKET_SIZE) len = CDC_MAX_PACKET_SIZE;

        // Update log position by the actual number of bytes sent
        // If USB CDC buffer is full, this may be 0
        uint32_t actual = 0;
        _SerialUSB.send_nb((uint8_t*)data, len, &actual);
        logpos -= available - actual;
    }
#endif // __MBED__
}
#endif //LIB_FREERTOS_KERNEL

// Use ADC to implement supply voltage monitoring for the +3.0V rail.
// This works by sampling the temperature sensor channel, which has
// a voltage of 0.7 V, allowing to calculate the VDD voltage.
static bool adc_initial_log = true;
static void adc_poll()
{
#if PLATFORM_VDD_WARNING_LIMIT_mV > 0
    static bool initialized = false;
    static int lowest_vdd_seen = PLATFORM_VDD_WARNING_LIMIT_mV;

    if (!initialized)
    {
        adc_init();
        adc_set_temp_sensor_enabled(true);
        adc_set_clkdiv(65535); // Lowest samplerate, about 2 kHz
        adc_select_input(4);
        adc_fifo_setup(true, false, 0, false, false);
        adc_run(true);
        initialized = true;
    }

#ifdef ENABLE_AUDIO_OUTPUT
    /*
    * If ADC sample reads are done, either via direct reading, FIFO, or DMA,
    * at the same time a SPI DMA write begins, it appears that the first
    * 16-bit word of the DMA data is lost. This causes the bitstream to glitch
    * and audio to 'pop' noticeably. For now, just disable ADC reads when audio
    * is playing.
    */
   if (audio_is_active()) return;
#endif

    int adc_value_max = 0;
    while (!adc_fifo_is_empty())
    {
        int adc_value = adc_fifo_get();
        if (adc_value > adc_value_max) adc_value_max = adc_value;
    }

    // adc_value = 700mV * 4096 / Vdd
    // => Vdd = 700mV * 4096 / adc_value
    // To avoid wasting time on division, compare against
    // limit directly.
    const int limit = (700 * 4096) / PLATFORM_VDD_WARNING_LIMIT_mV;
    if (adc_value_max > limit)
    {
        // Warn once, and then again if we detect even a lower drop.
        int vdd_mV = (700 * 4096) / adc_value_max;
        if (vdd_mV < lowest_vdd_seen)
        {
            log("WARNING: Detected voltage drop to ", (vdd_mV / 1000.0), "V - See: https://www.github.com/BlueSCSI/BlueSCSI-v2/wiki/Low-Voltage");
            lowest_vdd_seen = vdd_mV - 50; // Small hysteresis to avoid excessive warnings
        }
    }
    else if (adc_initial_log && adc_value_max != 0)
    {
        adc_initial_log = false;
        int vdd_mV = (700 * 4096) / adc_value_max;
        log("INFO: Pico Voltage: ", (vdd_mV / 1000.0), "V.");
    }
#endif
}

// This function is called for every log message.
void platform_log(const char *s)
{
#ifdef LIB_FREERTOS_KERNEL
    printf(s);
#else
    if (g_uart_initialized)
    {
        uart_puts(uart0, s);
    }
#endif
}

static int g_watchdog_timeout;
static bool g_watchdog_initialized;

static void watchdog_callback(unsigned alarm_num)
{
    g_watchdog_timeout -= 1000;

#ifndef LIB_FREERTOS_KERNEL
    if (g_watchdog_timeout < WATCHDOG_CRASH_TIMEOUT - 1000)
    {
        // Been stuck for at least a second, start dumping USB log
        usb_log_poll();
    }
#endif

    if (g_watchdog_timeout <= WATCHDOG_CRASH_TIMEOUT - WATCHDOG_BUS_RESET_TIMEOUT)
    {
        if (!scsiDev.resetFlag || !g_scsiHostPhyReset)
        {
            log("--------------");
            log("WATCHDOG TIMEOUT, attempting bus reset");
            log("Platform: ", g_platform_name);
            log("FW Version: ", g_log_firmwareversion);
            log("GPIO states: out ", sio_hw->gpio_out, " oe ", sio_hw->gpio_oe, " in ", sio_hw->gpio_in);
            log("scsiDev.cdb: ", bytearray(scsiDev.cdb, 12));
            log("scsiDev.phase: ", (int)scsiDev.phase);
            scsi_accel_log_state();

#ifdef __MBED__
            uint32_t *p =  (uint32_t*)__get_PSP();
#else
            uint32_t msp;
            asm volatile ("MRS %0, msp" : "=r" (msp) );
            uint32_t *p = (uint32_t*)msp;
#endif
            for (int i = 0; i < 8; i++)
            {
            if (p == &__StackTop) break; // End of stack

                log("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
                p += 4;
            }

            scsiDev.resetFlag = 1;
            g_scsiHostPhyReset = true;
        }

        if (g_watchdog_timeout <= 0)
        {
            log("--------------");
            log("WATCHDOG TIMEOUT, already attempted bus reset, rebooting");
            log("Platform: ", g_platform_name);
            log("FW Version: ", g_log_firmwareversion);
            log("GPIO states: out ", sio_hw->gpio_out, " oe ", sio_hw->gpio_oe, " in ", sio_hw->gpio_in);
            log("scsiDev.cdb: ", bytearray(scsiDev.cdb, 12));
            log("scsiDev.phase: ", (int)scsiDev.phase);

#ifdef __MBED__
            uint32_t *p =  (uint32_t*)__get_PSP();
#else
            uint32_t msp;
            asm volatile ("MRS %0, msp" : "=r" (msp) );
            uint32_t *p = (uint32_t*)msp;
#endif
            for (int i = 0; i < 8; i++)
            {
                if (p == &__StackTop) break; // End of stack

                log("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
                p += 4;
            }

#ifndef LIB_FREERTOS_KERNEL
            usb_log_poll();
#endif

            platform_emergency_log_save();

            platform_boot_to_main_firmware();
        }
    }

    hardware_alarm_set_target(alarm_num, delayed_by_ms(get_absolute_time(), 1000));
}

// This function can be used to periodically reset watchdog timer for crash handling.
// It can also be left empty if the platform does not use a watchdog timer.
void platform_reset_watchdog()
{
    g_watchdog_timeout = WATCHDOG_CRASH_TIMEOUT;

    if (!g_watchdog_initialized)
    {
        int alarm_num = -1;
        for (unsigned int i = 0; i < NUM_GENERIC_TIMERS; i++)
        {
            if (!hardware_alarm_is_claimed(i))
            {
                alarm_num = i;
                break;
            }
        }
        if (alarm_num == -1)
        {
            log("No free watchdog hardware alarms to claim");
            return;
        }
        hardware_alarm_claim(alarm_num);
        hardware_alarm_set_callback(alarm_num, &watchdog_callback);
        hardware_alarm_set_target(alarm_num, delayed_by_ms(get_absolute_time(), 1000));
        g_watchdog_initialized = true;
    }

#ifndef LIB_FREERTOS_KERNEL
    // USB log is polled here also to make sure any log messages in fault states
    // get passed to USB.
    usb_log_poll();
#endif
}

// Poll function that is called every few milliseconds.
// Can be left empty or used for platform-specific processing.
void platform_poll()
{
#ifndef LIB_FREERTOS_KERNEL
    usb_log_poll();
#endif
    adc_poll();
    
#ifdef ENABLE_AUDIO_OUTPUT
    audio_poll();
#endif
}

uint8_t platform_get_buttons() {return 0;}
// TODO figure this out
/*uint8_t platform_get_buttons()
{
    uint8_t buttons = 0;

#if defined(ENABLE_AUDIO_OUTPUT)
    // pulled to VCC via resistor, sinking when pressed
    if (!gpio_get(GPIO_EXP_SPARE)) buttons |= 1;
#elif defined(GPIO_I2C_SDA)
    // SDA = button 1, SCL = button 2
    if (!gpio_get(GPIO_I2C_SDA)) buttons |= 1;
    if (!gpio_get(GPIO_I2C_SCL)) buttons |= 2;
#endif

    // Simple debouncing logic: handle button releases after 100 ms delay.
    static uint32_t debounce;
    static uint8_t buttons_debounced = 0;

    if (buttons != 0)
    {
        buttons_debounced = buttons;
        debounce = millis();
    }
    else if ((uint32_t)(millis() - debounce) > 100)
    {
        buttons_debounced = 0;
    }

    return buttons_debounced;
}*/

// Used by setup methods to determine which hardware version is in use
bool is202309a() {
    return scsi_pins.OUT_REQ == SCSI_OUT_REQ;
}

/*****************************************/
/* Flash reprogramming from bootloader   */
/*****************************************/

#ifdef PLATFORM_BOOTLOADER_SIZE

extern uint32_t __real_vectors_start;
extern uint32_t __StackTop;
static volatile void *g_bootloader_exit_req;

__attribute__((section(".time_critical.platform_rewrite_flash_page")))
bool platform_rewrite_flash_page(uint32_t offset, uint8_t buffer[PLATFORM_FLASH_PAGE_SIZE])
{
    if (offset == PLATFORM_BOOTLOADER_SIZE)
    {
        if (buffer[3] != 0x20 || buffer[7] != 0x10)
        {
            log("Invalid firmware file, starts with: ", bytearray(buffer, 16));
            return false;
        }
    }
#ifdef __MBED__
    if (NVIC_GetEnableIRQ(USBCTRL_IRQn))
    {
        log("Disabling USB during firmware flashing");
        NVIC_DisableIRQ(USBCTRL_IRQn);
        usb_hw->main_ctrl = 0;
    }
#endif // __MBED__

    debuglog("Writing flash at offset ", offset, " data ", bytearray(buffer, 4));
    assert(offset % PLATFORM_FLASH_PAGE_SIZE == 0);
    assert(offset >= PLATFORM_BOOTLOADER_SIZE);

    // Avoid any mbed timer interrupts triggering during the flashing.
    uint32_t status = save_and_disable_interrupts();

    // For some reason any code executed after flashing crashes
    // unless we disable the XIP cache.
    // Not sure why this happens, as flash_range_program() is flushing
    // the cache correctly.
    // The cache is now enabled from bootloader start until it starts
    // flashing, and again after reset to main firmware.
    xip_ctrl_hw->ctrl = 0;

    flash_range_erase(offset, PLATFORM_FLASH_PAGE_SIZE);
    flash_range_program(offset, buffer, PLATFORM_FLASH_PAGE_SIZE);

    uint32_t *buf32 = (uint32_t*)buffer;
    uint32_t num_words = PLATFORM_FLASH_PAGE_SIZE / 4;
    for (uint32_t i = 0; i < num_words; i++)
    {
        uint32_t expected = buf32[i];
        uint32_t actual = *(volatile uint32_t*)(XIP_SRAM_BASE + offset + i * 4);

        if (actual != expected)
        {
            log("Flash verify failed at offset ", offset + i * 4, " got ", actual, " expected ", expected);
            restore_interrupts_from_disabled(status);
            return false;
        }
    }

    restore_interrupts_from_disabled(status);

    return true;
}

void platform_boot_to_main_firmware()
{
    // To ensure that the system state is reset properly, we perform
    // a SYSRESETREQ and jump straight from the reset vector to main application.
    g_bootloader_exit_req = &g_bootloader_exit_req;
    scb_hw->aircr = 0x05FA0004;
    while(1);
}

void btldr_reset_handler()
{
    uint32_t* application_base = &__real_vectors_start;
    if (g_bootloader_exit_req == &g_bootloader_exit_req)
    {
        // Boot to main application
        application_base = (uint32_t*)(XIP_BASE + PLATFORM_BOOTLOADER_SIZE);
    }

    scb_hw->aircr = (uint32_t)application_base;
    __asm__(
        "msr msp, %0\n\t"
        "bx %1" : : "r" (application_base[0]),
                    "r" (application_base[1]) : "memory");
}

// Replace the reset handler when building the bootloader
// The rp2040_btldr.ld places real vector table at an offset.
__attribute__((section(".btldr_vectors")))
const void * btldr_vectors[2] = {&__StackTop, (void*)&btldr_reset_handler};

#endif

/************************************/
/* ROM drive in extra flash space   */
/************************************/

#ifdef PLATFORM_HAS_ROM_DRIVE

// Reserve up to 352 kB for firmware.
#define ROMDRIVE_OFFSET (352 * 1024)

uint32_t platform_get_romdrive_maxsize()
{
    if (g_flash_chip_size >= ROMDRIVE_OFFSET)
    {
        return g_flash_chip_size - ROMDRIVE_OFFSET;
    }
    else
    {
        // Failed to read flash chip size, default to 2 MB
        return 2048 * 1024 - ROMDRIVE_OFFSET;
    }
}

bool platform_read_romdrive(uint8_t *dest, uint32_t start, uint32_t count)
{
    xip_ctrl_hw->stream_ctr = 0;

    while (!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY))
    {
        (void) xip_ctrl_hw->stream_fifo;
    }

    xip_ctrl_hw->stream_addr = start + ROMDRIVE_OFFSET;
    xip_ctrl_hw->stream_ctr = count / 4;

    // Transfer happens in multiples of 4 bytes
    assert(start < platform_get_romdrive_maxsize());
    assert((count & 3) == 0);
    assert((((uint32_t)dest) & 3) == 0);

    uint32_t *dest32 = (uint32_t*)dest;
    uint32_t words_remain = count / 4;
    while (words_remain > 0)
    {
        if (!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY))
        {
            *dest32++ = xip_ctrl_hw->stream_fifo;
            words_remain--;
        }
    }

    return true;
}

bool platform_write_romdrive(const uint8_t *data, uint32_t start, uint32_t count)
{
    assert(start < platform_get_romdrive_maxsize());
    assert((count % PLATFORM_ROMDRIVE_PAGE_SIZE) == 0);

    uint32_t status = save_and_disable_interrupts();
    flash_range_erase(start + ROMDRIVE_OFFSET, count);
    flash_range_program(start + ROMDRIVE_OFFSET, data, count);
    restore_interrupts_from_disabled(status);
    return true;
}

#endif

/**********************************************/
/* Mapping from data bytes to GPIO BOP values */
/**********************************************/

/* A lookup table is the fastest way to calculate parity and convert the IO pin mapping for data bus.
 * For RP2040 we expect that the bits are consecutive and in order.
 * The PIO-based parity scheme also requires that the lookup table is aligned to 512-byte increment.
 * The parity table is placed into SRAM4 area to reduce bus contention.
 */

#define PARITY(n) ((1 ^ (n) ^ ((n)>>1) ^ ((n)>>2) ^ ((n)>>3) ^ ((n)>>4) ^ ((n)>>5) ^ ((n)>>6) ^ ((n)>>7)) & 1)
#define X(n) (\
    ((n & 0x01) ? 0 : (1 << SCSI_IO_DB0)) | \
    ((n & 0x02) ? 0 : (1 << SCSI_IO_DB1)) | \
    ((n & 0x04) ? 0 : (1 << SCSI_IO_DB2)) | \
    ((n & 0x08) ? 0 : (1 << SCSI_IO_DB3)) | \
    ((n & 0x10) ? 0 : (1 << SCSI_IO_DB4)) | \
    ((n & 0x20) ? 0 : (1 << SCSI_IO_DB5)) | \
    ((n & 0x40) ? 0 : (1 << SCSI_IO_DB6)) | \
    ((n & 0x80) ? 0 : (1 << SCSI_IO_DB7)) | \
    (PARITY(n)  ? 0 : (1 << SCSI_IO_DBP)) \
)

const uint16_t g_scsi_parity_lookup[256] __attribute__((aligned(512), section(".scratch_x.parity"))) =
{
    X(0x00), X(0x01), X(0x02), X(0x03), X(0x04), X(0x05), X(0x06), X(0x07), X(0x08), X(0x09), X(0x0a), X(0x0b), X(0x0c), X(0x0d), X(0x0e), X(0x0f),
    X(0x10), X(0x11), X(0x12), X(0x13), X(0x14), X(0x15), X(0x16), X(0x17), X(0x18), X(0x19), X(0x1a), X(0x1b), X(0x1c), X(0x1d), X(0x1e), X(0x1f),
    X(0x20), X(0x21), X(0x22), X(0x23), X(0x24), X(0x25), X(0x26), X(0x27), X(0x28), X(0x29), X(0x2a), X(0x2b), X(0x2c), X(0x2d), X(0x2e), X(0x2f),
    X(0x30), X(0x31), X(0x32), X(0x33), X(0x34), X(0x35), X(0x36), X(0x37), X(0x38), X(0x39), X(0x3a), X(0x3b), X(0x3c), X(0x3d), X(0x3e), X(0x3f),
    X(0x40), X(0x41), X(0x42), X(0x43), X(0x44), X(0x45), X(0x46), X(0x47), X(0x48), X(0x49), X(0x4a), X(0x4b), X(0x4c), X(0x4d), X(0x4e), X(0x4f),
    X(0x50), X(0x51), X(0x52), X(0x53), X(0x54), X(0x55), X(0x56), X(0x57), X(0x58), X(0x59), X(0x5a), X(0x5b), X(0x5c), X(0x5d), X(0x5e), X(0x5f),
    X(0x60), X(0x61), X(0x62), X(0x63), X(0x64), X(0x65), X(0x66), X(0x67), X(0x68), X(0x69), X(0x6a), X(0x6b), X(0x6c), X(0x6d), X(0x6e), X(0x6f),
    X(0x70), X(0x71), X(0x72), X(0x73), X(0x74), X(0x75), X(0x76), X(0x77), X(0x78), X(0x79), X(0x7a), X(0x7b), X(0x7c), X(0x7d), X(0x7e), X(0x7f),
    X(0x80), X(0x81), X(0x82), X(0x83), X(0x84), X(0x85), X(0x86), X(0x87), X(0x88), X(0x89), X(0x8a), X(0x8b), X(0x8c), X(0x8d), X(0x8e), X(0x8f),
    X(0x90), X(0x91), X(0x92), X(0x93), X(0x94), X(0x95), X(0x96), X(0x97), X(0x98), X(0x99), X(0x9a), X(0x9b), X(0x9c), X(0x9d), X(0x9e), X(0x9f),
    X(0xa0), X(0xa1), X(0xa2), X(0xa3), X(0xa4), X(0xa5), X(0xa6), X(0xa7), X(0xa8), X(0xa9), X(0xaa), X(0xab), X(0xac), X(0xad), X(0xae), X(0xaf),
    X(0xb0), X(0xb1), X(0xb2), X(0xb3), X(0xb4), X(0xb5), X(0xb6), X(0xb7), X(0xb8), X(0xb9), X(0xba), X(0xbb), X(0xbc), X(0xbd), X(0xbe), X(0xbf),
    X(0xc0), X(0xc1), X(0xc2), X(0xc3), X(0xc4), X(0xc5), X(0xc6), X(0xc7), X(0xc8), X(0xc9), X(0xca), X(0xcb), X(0xcc), X(0xcd), X(0xce), X(0xcf),
    X(0xd0), X(0xd1), X(0xd2), X(0xd3), X(0xd4), X(0xd5), X(0xd6), X(0xd7), X(0xd8), X(0xd9), X(0xda), X(0xdb), X(0xdc), X(0xdd), X(0xde), X(0xdf),
    X(0xe0), X(0xe1), X(0xe2), X(0xe3), X(0xe4), X(0xe5), X(0xe6), X(0xe7), X(0xe8), X(0xe9), X(0xea), X(0xeb), X(0xec), X(0xed), X(0xee), X(0xef),
    X(0xf0), X(0xf1), X(0xf2), X(0xf3), X(0xf4), X(0xf5), X(0xf6), X(0xf7), X(0xf8), X(0xf9), X(0xfa), X(0xfb), X(0xfc), X(0xfd), X(0xfe), X(0xff)
};

#undef X

/* Similarly, another lookup table is used to verify parity of received data.
 * This table is indexed by the 8 data bits + 1 parity bit from SCSI bus (active low)
 * Each word contains the data byte (inverted to active-high) and a bit indicating whether parity is valid.
 */
#define X(n) (\
    ((n & 0xFF) ^ 0xFF) | \
    (((PARITY(n & 0xFF) ^ (n >> 8)) & 1) << 8) \
)

const uint16_t g_scsi_parity_check_lookup[512] __attribute__((aligned(1024), section(".scratch_x.parity"))) =
{
    X(0x000), X(0x001), X(0x002), X(0x003), X(0x004), X(0x005), X(0x006), X(0x007), X(0x008), X(0x009), X(0x00a), X(0x00b), X(0x00c), X(0x00d), X(0x00e), X(0x00f),
    X(0x010), X(0x011), X(0x012), X(0x013), X(0x014), X(0x015), X(0x016), X(0x017), X(0x018), X(0x019), X(0x01a), X(0x01b), X(0x01c), X(0x01d), X(0x01e), X(0x01f),
    X(0x020), X(0x021), X(0x022), X(0x023), X(0x024), X(0x025), X(0x026), X(0x027), X(0x028), X(0x029), X(0x02a), X(0x02b), X(0x02c), X(0x02d), X(0x02e), X(0x02f),
    X(0x030), X(0x031), X(0x032), X(0x033), X(0x034), X(0x035), X(0x036), X(0x037), X(0x038), X(0x039), X(0x03a), X(0x03b), X(0x03c), X(0x03d), X(0x03e), X(0x03f),
    X(0x040), X(0x041), X(0x042), X(0x043), X(0x044), X(0x045), X(0x046), X(0x047), X(0x048), X(0x049), X(0x04a), X(0x04b), X(0x04c), X(0x04d), X(0x04e), X(0x04f),
    X(0x050), X(0x051), X(0x052), X(0x053), X(0x054), X(0x055), X(0x056), X(0x057), X(0x058), X(0x059), X(0x05a), X(0x05b), X(0x05c), X(0x05d), X(0x05e), X(0x05f),
    X(0x060), X(0x061), X(0x062), X(0x063), X(0x064), X(0x065), X(0x066), X(0x067), X(0x068), X(0x069), X(0x06a), X(0x06b), X(0x06c), X(0x06d), X(0x06e), X(0x06f),
    X(0x070), X(0x071), X(0x072), X(0x073), X(0x074), X(0x075), X(0x076), X(0x077), X(0x078), X(0x079), X(0x07a), X(0x07b), X(0x07c), X(0x07d), X(0x07e), X(0x07f),
    X(0x080), X(0x081), X(0x082), X(0x083), X(0x084), X(0x085), X(0x086), X(0x087), X(0x088), X(0x089), X(0x08a), X(0x08b), X(0x08c), X(0x08d), X(0x08e), X(0x08f),
    X(0x090), X(0x091), X(0x092), X(0x093), X(0x094), X(0x095), X(0x096), X(0x097), X(0x098), X(0x099), X(0x09a), X(0x09b), X(0x09c), X(0x09d), X(0x09e), X(0x09f),
    X(0x0a0), X(0x0a1), X(0x0a2), X(0x0a3), X(0x0a4), X(0x0a5), X(0x0a6), X(0x0a7), X(0x0a8), X(0x0a9), X(0x0aa), X(0x0ab), X(0x0ac), X(0x0ad), X(0x0ae), X(0x0af),
    X(0x0b0), X(0x0b1), X(0x0b2), X(0x0b3), X(0x0b4), X(0x0b5), X(0x0b6), X(0x0b7), X(0x0b8), X(0x0b9), X(0x0ba), X(0x0bb), X(0x0bc), X(0x0bd), X(0x0be), X(0x0bf),
    X(0x0c0), X(0x0c1), X(0x0c2), X(0x0c3), X(0x0c4), X(0x0c5), X(0x0c6), X(0x0c7), X(0x0c8), X(0x0c9), X(0x0ca), X(0x0cb), X(0x0cc), X(0x0cd), X(0x0ce), X(0x0cf),
    X(0x0d0), X(0x0d1), X(0x0d2), X(0x0d3), X(0x0d4), X(0x0d5), X(0x0d6), X(0x0d7), X(0x0d8), X(0x0d9), X(0x0da), X(0x0db), X(0x0dc), X(0x0dd), X(0x0de), X(0x0df),
    X(0x0e0), X(0x0e1), X(0x0e2), X(0x0e3), X(0x0e4), X(0x0e5), X(0x0e6), X(0x0e7), X(0x0e8), X(0x0e9), X(0x0ea), X(0x0eb), X(0x0ec), X(0x0ed), X(0x0ee), X(0x0ef),
    X(0x0f0), X(0x0f1), X(0x0f2), X(0x0f3), X(0x0f4), X(0x0f5), X(0x0f6), X(0x0f7), X(0x0f8), X(0x0f9), X(0x0fa), X(0x0fb), X(0x0fc), X(0x0fd), X(0x0fe), X(0x0ff),
    X(0x100), X(0x101), X(0x102), X(0x103), X(0x104), X(0x105), X(0x106), X(0x107), X(0x108), X(0x109), X(0x10a), X(0x10b), X(0x10c), X(0x10d), X(0x10e), X(0x10f),
    X(0x110), X(0x111), X(0x112), X(0x113), X(0x114), X(0x115), X(0x116), X(0x117), X(0x118), X(0x119), X(0x11a), X(0x11b), X(0x11c), X(0x11d), X(0x11e), X(0x11f),
    X(0x120), X(0x121), X(0x122), X(0x123), X(0x124), X(0x125), X(0x126), X(0x127), X(0x128), X(0x129), X(0x12a), X(0x12b), X(0x12c), X(0x12d), X(0x12e), X(0x12f),
    X(0x130), X(0x131), X(0x132), X(0x133), X(0x134), X(0x135), X(0x136), X(0x137), X(0x138), X(0x139), X(0x13a), X(0x13b), X(0x13c), X(0x13d), X(0x13e), X(0x13f),
    X(0x140), X(0x141), X(0x142), X(0x143), X(0x144), X(0x145), X(0x146), X(0x147), X(0x148), X(0x149), X(0x14a), X(0x14b), X(0x14c), X(0x14d), X(0x14e), X(0x14f),
    X(0x150), X(0x151), X(0x152), X(0x153), X(0x154), X(0x155), X(0x156), X(0x157), X(0x158), X(0x159), X(0x15a), X(0x15b), X(0x15c), X(0x15d), X(0x15e), X(0x15f),
    X(0x160), X(0x161), X(0x162), X(0x163), X(0x164), X(0x165), X(0x166), X(0x167), X(0x168), X(0x169), X(0x16a), X(0x16b), X(0x16c), X(0x16d), X(0x16e), X(0x16f),
    X(0x170), X(0x171), X(0x172), X(0x173), X(0x174), X(0x175), X(0x176), X(0x177), X(0x178), X(0x179), X(0x17a), X(0x17b), X(0x17c), X(0x17d), X(0x17e), X(0x17f),
    X(0x180), X(0x181), X(0x182), X(0x183), X(0x184), X(0x185), X(0x186), X(0x187), X(0x188), X(0x189), X(0x18a), X(0x18b), X(0x18c), X(0x18d), X(0x18e), X(0x18f),
    X(0x190), X(0x191), X(0x192), X(0x193), X(0x194), X(0x195), X(0x196), X(0x197), X(0x198), X(0x199), X(0x19a), X(0x19b), X(0x19c), X(0x19d), X(0x19e), X(0x19f),
    X(0x1a0), X(0x1a1), X(0x1a2), X(0x1a3), X(0x1a4), X(0x1a5), X(0x1a6), X(0x1a7), X(0x1a8), X(0x1a9), X(0x1aa), X(0x1ab), X(0x1ac), X(0x1ad), X(0x1ae), X(0x1af),
    X(0x1b0), X(0x1b1), X(0x1b2), X(0x1b3), X(0x1b4), X(0x1b5), X(0x1b6), X(0x1b7), X(0x1b8), X(0x1b9), X(0x1ba), X(0x1bb), X(0x1bc), X(0x1bd), X(0x1be), X(0x1bf),
    X(0x1c0), X(0x1c1), X(0x1c2), X(0x1c3), X(0x1c4), X(0x1c5), X(0x1c6), X(0x1c7), X(0x1c8), X(0x1c9), X(0x1ca), X(0x1cb), X(0x1cc), X(0x1cd), X(0x1ce), X(0x1cf),
    X(0x1d0), X(0x1d1), X(0x1d2), X(0x1d3), X(0x1d4), X(0x1d5), X(0x1d6), X(0x1d7), X(0x1d8), X(0x1d9), X(0x1da), X(0x1db), X(0x1dc), X(0x1dd), X(0x1de), X(0x1df),
    X(0x1e0), X(0x1e1), X(0x1e2), X(0x1e3), X(0x1e4), X(0x1e5), X(0x1e6), X(0x1e7), X(0x1e8), X(0x1e9), X(0x1ea), X(0x1eb), X(0x1ec), X(0x1ed), X(0x1ee), X(0x1ef),
    X(0x1f0), X(0x1f1), X(0x1f2), X(0x1f3), X(0x1f4), X(0x1f5), X(0x1f6), X(0x1f7), X(0x1f8), X(0x1f9), X(0x1fa), X(0x1fb), X(0x1fc), X(0x1fd), X(0x1fe), X(0x1ff),
};

#undef X

} /* extern "C" */

#ifdef MBED
/* Logging from mbed */

static class LogTarget: public mbed::FileHandle {
public:
    virtual ssize_t read(void *buffer, size_t size) { return 0; }
    virtual ssize_t write(const void *buffer, size_t size)
    {
        // A bit inefficient but mbed seems to write() one character
        // at a time anyways.
        for (int i = 0; i < size; i++)
        {
            char buf[2] = {((const char*)buffer)[i], 0};
            log_raw(buf);
        }
        return size;
    }

    virtual off_t seek(off_t offset, int whence = SEEK_SET) { return offset; }
    virtual int close() { return 0; }
    virtual off_t size() { return 0; }
} g_LogTarget;

mbed::FileHandle *mbed::mbed_override_console(int fd)
{
    return &g_LogTarget;
}
#endif
