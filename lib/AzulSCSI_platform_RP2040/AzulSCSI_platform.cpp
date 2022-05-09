#include "AzulSCSI_platform.h"
#include "AzulSCSI_log.h"
#include "AzulSCSI_config.h"
#include <SdFat.h>
#include <scsi.h>
#include <assert.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/spi.h>

extern "C" {

const char *g_azplatform_name = PLATFORM_NAME;

/***************/
/* GPIO init   */
/***************/

// Helper function to configure whole GPIO in one line
static void gpio_conf(uint gpio, enum gpio_function fn, bool pullup, bool pulldown, bool output, bool initial_state, bool fast_slew)
{
    gpio_put(gpio, initial_state);
    gpio_set_dir(gpio, output);
    gpio_set_pulls(gpio, pullup, pulldown);
    gpio_set_function(gpio, fn);

    if (fast_slew)
    {
        padsbank0_hw->io[gpio] |= PADS_BANK0_GPIO0_SLEWFAST_BITS;
    }
}

void azplatform_init()
{
    /* First configure the pins that affect external buffer directions.
     * RP2040 defaults to pulldowns, while these pins have external pull-ups.
     */
    //        pin             function       pup   pdown  out    state fast
    gpio_conf(SCSI_DATA_DIR,  GPIO_FUNC_SIO, false,false, true,  true, true);
    gpio_conf(SCSI_OUT_RST,   GPIO_FUNC_SIO, false,false, true,  true, true);
    gpio_conf(SCSI_OUT_BSY,   GPIO_FUNC_SIO, false,false, true,  true, true);
    gpio_conf(SCSI_OUT_SEL,   GPIO_FUNC_SIO, false,false, true,  true, true);

    /* Check dip switch settings */
    gpio_conf(DIP_INITIATOR,  GPIO_FUNC_SIO, false, false, false, false, false);
    gpio_conf(DIP_DBGLOG,     GPIO_FUNC_SIO, false, false, false, false, false);
    gpio_conf(DIP_TERM,       GPIO_FUNC_SIO, false, false, false, false, false);

    delay(10); // 10 ms delay to let pull-ups do their work

    bool initiator = !gpio_get(DIP_INITIATOR);
    bool dbglog = !gpio_get(DIP_DBGLOG);
    bool termination = !gpio_get(DIP_TERM);

    /* Initialize logging to SWO pin (UART0) */
    gpio_conf(SWO_PIN,        GPIO_FUNC_UART,false,false, true,  false, true);
    uart_init(uart0, 2000000);

    azlog("DIP switch settings: initiator ", (int)initiator, ", debug log ", (int)dbglog, ", termination ", (int)termination);

    if (initiator)
    {
        azlog("ERROR: SCSI initiator mode is not implemented yet, turn DIP switch off for proper operation!");
    }

    if (dbglog)
    {
        g_azlog_debug = true;
    }

    if (termination)
    {
        azlog("SCSI termination is enabled");
    }
    else
    {
        azlog("NOTE: SCSI termination is disabled");
    }

    /* Initialize SCSI and SD card pins to required modes.
     * SCSI pins should be inactive / input at this point.
     */

    // SCSI data bus
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

    // SCSI control outputs
    //        pin             function       pup   pdown  out    state fast
    gpio_conf(SCSI_OUT_IO,    GPIO_FUNC_SIO, false,false, true,  true, true);
    gpio_conf(SCSI_OUT_MSG,   GPIO_FUNC_SIO, false,false, true,  true, true);
    gpio_conf(SCSI_OUT_REQ,   GPIO_FUNC_SIO, false,false, true,  true, true);

    // Shared pins are changed to input / output depending on communication phase
    gpio_conf(SCSI_IN_SEL,    GPIO_FUNC_SIO, true, false, false, true, true);
    if (SCSI_OUT_CD != SCSI_IN_SEL)
    {
        gpio_conf(SCSI_OUT_CD,    GPIO_FUNC_SIO, false,false, true,  true, true);
    }

    gpio_conf(SCSI_IN_BSY,    GPIO_FUNC_SIO, true, false, false, true, true);
    if (SCSI_OUT_MSG != SCSI_IN_BSY)
    {
        gpio_conf(SCSI_OUT_MSG,    GPIO_FUNC_SIO, false,false, true,  true, true);
    }

    // SCSI control inputs
    //        pin             function       pup   pdown  out    state fast
    gpio_conf(SCSI_IN_ACK,    GPIO_FUNC_SIO, true, false, false, true, false);
    gpio_conf(SCSI_IN_ATN,    GPIO_FUNC_SIO, true, false, false, true, false);
    gpio_conf(SCSI_IN_RST,    GPIO_FUNC_SIO, true, false, false, true, false);

    // SD card pins
    gpio_conf(SD_SPI_SCK,     GPIO_FUNC_SPI, false,false, true,  true, true);
    gpio_conf(SD_SPI_MOSI,    GPIO_FUNC_SPI, false,false, true,  true, true);
    gpio_conf(SD_SPI_MISO,    GPIO_FUNC_SPI, false,false, false, true, true);
    gpio_conf(SD_SPI_CS,      GPIO_FUNC_SIO, false,false, true,  true, true);

    // LED pin
    gpio_conf(LED_PIN,        GPIO_FUNC_SIO, false,false, true,  false, false);
}

void azplatform_late_init()
{
    /* This function can usually be left empty.
     * It can be used for initialization code that should not run in bootloader.
     */
}

/*****************************************/
/* Crash handlers                        */
/*****************************************/

extern SdFs SD;
extern uint32_t __StackTop;

// void azplatform_emergency_log_save()
// {
//     azplatform_set_sd_callback(NULL, NULL);

//     SD.begin(SD_CONFIG_CRASH);
//     FsFile crashfile = SD.open(CRASHFILE, O_WRONLY | O_CREAT | O_TRUNC);

//     if (!crashfile.isOpen())
//     {
//         // Try to reinitialize
//         int max_retry = 10;
//         while (max_retry-- > 0 && !SD.begin(SD_CONFIG_CRASH));

//         crashfile = SD.open(CRASHFILE, O_WRONLY | O_CREAT | O_TRUNC);
//     }

//     uint32_t startpos = 0;
//     crashfile.write(azlog_get_buffer(&startpos));
//     crashfile.write(azlog_get_buffer(&startpos));
//     crashfile.flush();
//     crashfile.close();
// }

// void mbed_error_hook(const mbed_error_ctx * error_context)
// {
//     azlog("--------------");
//     azlog("CRASH!");
//     azlog("Platform: ", g_azplatform_name);
//     azlog("FW Version: ", g_azlog_firmwareversion);
//     azlog("error_status: ", (int)error_context->error_status);
//     azlog("error_address: ", error_context->error_address);
//     azlog("error_valu: ", error_context->error_value);

//     uint32_t *p = (uint32_t*)((uint32_t)error_context->thread_current_sp & ~3);
//     for (int i = 0; i < 8; i++)
//     {
//         if (p == &__StackTop) break; // End of stack

//         azlog("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
//         p += 4;
//     }

//     azplatform_emergency_log_save();

//     while (1)
//     {
//         // Flash the crash address on the LED
//         // Short pulse means 0, long pulse means 1
//         int base_delay = 1000;
//         for (int i = 31; i >= 0; i--)
//         {
//             LED_OFF();
//             for (int j = 0; j < base_delay; j++) delay_ns(100000);

//             int delay = (error_context->error_address & (1 << i)) ? (3 * base_delay) : base_delay;
//             LED_ON();
//             for (int j = 0; j < delay; j++) delay_ns(100000);
//             LED_OFF();
//         }

//         for (int j = 0; j < base_delay * 10; j++) delay_ns(100000);
//     }
// }

// void __assert_func(const char *file, int line, const char *func, const char *expr)
// {
//     uint32_t dummy = 0;

//     azlog("--------------");
//     azlog("ASSERT FAILED!");
//     azlog("Platform: ", g_azplatform_name);
//     azlog("FW Version: ", g_azlog_firmwareversion);
//     azlog("Assert failed: ", file , ":", line, " in ", func, ":", expr);

//     uint32_t *p = (uint32_t*)((uint32_t)&dummy & ~3);
//     for (int i = 0; i < 8; i++)
//     {
//         if (p == &__StackTop) break; // End of stack

//         azlog("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
//         p += 4;
//     }

//     azplatform_emergency_log_save();

//     while(1)
//     {
//         LED_OFF();
//         for (int j = 0; j < 1000; j++) delay_ns(100000);
//         LED_ON();
//         for (int j = 0; j < 1000; j++) delay_ns(100000);
//     }
// }

/*****************************************/
/* Debug logging and watchdor            */
/*****************************************/

// This function is called for every log message.
void azplatform_log(const char *s)
{
    uart_puts(uart0, s);
}

// This function can be used to periodically reset watchdog timer for crash handling.
// It can also be left empty if the platform does not use a watchdog timer.
void azplatform_reset_watchdog()
{
}

/**********************************************/
/* Mapping from data bytes to GPIO BOP values */
/**********************************************/

/* A lookup table is the fastest way to calculate parity and convert the IO pin mapping for
 * data bus. The method below uses the BOP register of GD32, this is called BSRR on STM32.
 * If there are no other pins on the same port, you can also use direct writes to the GPIO.
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
    (PARITY(n)  ? 0 : (1 << SCSI_IO_DBP)) | \
    (1 << SCSI_OUT_REQ) \
)

const uint32_t g_scsi_out_byte_lookup[256] =
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

} /* extern "C" */
