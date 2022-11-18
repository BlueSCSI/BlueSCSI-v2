#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
#include <SdFat.h>
#include <scsi.h>
#include <assert.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/spi.h>
#include <hardware/structs/xip_ctrl.h>
#include <platform/mbed_error.h>

extern "C" {

// As of 2022-09-13, the platformio RP2040 core is missing cplusplus guard on flash.h
// For that reason this has to be inside the extern "C" here.
#include <hardware/flash.h>
#include "rp2040_flash_do_cmd.h"

const char *g_azplatform_name = PLATFORM_NAME;
static bool g_scsi_initiator = false;
static uint32_t g_flash_chip_size = 0;

void mbed_error_hook(const mbed_error_ctx * error_context);

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

    bool dbglog = !gpio_get(DIP_DBGLOG);
    bool termination = !gpio_get(DIP_TERM);

    /* Initialize logging to SWO pin (UART0) */
    gpio_conf(SWO_PIN,        GPIO_FUNC_UART,false,false, true,  false, true);
    uart_init(uart0, 1000000);
    mbed_set_error_hook(mbed_error_hook);

    azlog("Platform: ", g_azplatform_name);
    azlog("FW Version: ", g_azlog_firmwareversion);

    azlog("DIP switch settings: debug log ", (int)dbglog, ", termination ", (int)termination);

    g_azlog_debug = dbglog;
    
    if (termination)
    {
        azlog("SCSI termination is enabled");
    }
    else
    {
        azlog("NOTE: SCSI termination is disabled");
    }

    // Get flash chip size
    uint8_t cmd_read_jedec_id[4] = {0x9f, 0, 0, 0};
    uint8_t response_jedec[4] = {0};
    flash_do_cmd(cmd_read_jedec_id, response_jedec, 4);
    g_flash_chip_size = (1 << response_jedec[3]);
    azlog("Flash chip size: ", (int)(g_flash_chip_size / 1024), " kB");

    // SD card pins
    // Card is used in SDIO mode for main program, and in SPI mode for crash handler & bootloader.
    //        pin             function       pup   pdown  out    state fast
    gpio_conf(SD_SPI_SCK,     GPIO_FUNC_SPI, true, false, true,  true, true);
    gpio_conf(SD_SPI_MOSI,    GPIO_FUNC_SPI, true, false, true,  true, true);
    gpio_conf(SD_SPI_MISO,    GPIO_FUNC_SPI, true, false, false, true, true);
    gpio_conf(SD_SPI_CS,      GPIO_FUNC_SIO, true, false, true,  true, true);
    gpio_conf(SDIO_D1,        GPIO_FUNC_SIO, true, false, false, true, true);
    gpio_conf(SDIO_D2,        GPIO_FUNC_SIO, true, false, false, true, true);

    // LED pin
    gpio_conf(LED_PIN,        GPIO_FUNC_SIO, false,false, true,  false, false);

    // I2C pins
    //        pin             function       pup   pdown  out    state fast
    gpio_conf(GPIO_I2C_SCL,   GPIO_FUNC_I2C, true,false, false,  true, true);
    gpio_conf(GPIO_I2C_SDA,   GPIO_FUNC_I2C, true,false, false,  true, true);
}

static bool read_initiator_dip_switch()
{
    /* Revision 2022d hardware has problems reading initiator DIP switch setting.
     * The 74LVT245 hold current is keeping the GPIO_ACK state too strongly.
     * Detect this condition by toggling the pin up and down and seeing if it sticks.
     */

    // Strong output high, then pulldown
    //        pin             function       pup   pdown   out    state  fast
    gpio_conf(DIP_INITIATOR,  GPIO_FUNC_SIO, false, false, true,  true,  false);
    gpio_conf(DIP_INITIATOR,  GPIO_FUNC_SIO, false, true,  false, true,  false);
    delay(1);
    bool initiator_state1 = gpio_get(DIP_INITIATOR);
    
    // Strong output low, then pullup
    //        pin             function       pup   pdown   out    state  fast
    gpio_conf(DIP_INITIATOR,  GPIO_FUNC_SIO, false, false, true,  false, false);
    gpio_conf(DIP_INITIATOR,  GPIO_FUNC_SIO, true,  false, false, false, false);
    delay(1);
    bool initiator_state2 = gpio_get(DIP_INITIATOR);

    if (initiator_state1 == initiator_state2)
    {
        // Ok, was able to read the state directly
        return !initiator_state1;
    }

    // Enable OUT_BSY for a short time.
    // If in target mode, this will force GPIO_ACK high.
    gpio_put(SCSI_OUT_BSY, 0);
    delay_100ns();
    gpio_put(SCSI_OUT_BSY, 1);

    return !gpio_get(DIP_INITIATOR);
}

// late_init() only runs in main application, SCSI not needed in bootloader
void azplatform_late_init()
{
    if (read_initiator_dip_switch())
    {
        g_scsi_initiator = true;
        azlog("SCSI initiator mode selected by DIP switch, expecting SCSI disks on the bus");
    }
    else
    {
        g_scsi_initiator = false;
        azlog("SCSI target mode selected by DIP switch, acting as an SCSI disk");
    }

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
        //        pin             function       pup   pdown  out    state fast
        gpio_conf(SCSI_OUT_IO,    GPIO_FUNC_SIO, false,false, true,  true, true);
        gpio_conf(SCSI_OUT_MSG,   GPIO_FUNC_SIO, false,false, true,  true, true);

        // REQ pin is switched between PIO and SIO, pull-up makes sure no glitches
        gpio_conf(SCSI_OUT_REQ,   GPIO_FUNC_SIO, true ,false, true,  true, true);

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
    }
    else
    {
        // Act as SCSI initiator

        //        pin             function       pup   pdown  out    state fast
        gpio_conf(SCSI_IN_IO,     GPIO_FUNC_SIO, true ,false, false, true, false);
        gpio_conf(SCSI_IN_MSG,    GPIO_FUNC_SIO, true ,false, false, true, false);
        gpio_conf(SCSI_IN_CD,     GPIO_FUNC_SIO, true ,false, false, true, false);
        gpio_conf(SCSI_IN_REQ,    GPIO_FUNC_SIO, true ,false, false, true, false);
        gpio_conf(SCSI_IN_BSY,    GPIO_FUNC_SIO, true, false, false, true, false);
        gpio_conf(SCSI_IN_RST,    GPIO_FUNC_SIO, true, false, false, true, false);
        gpio_conf(SCSI_OUT_SEL,   GPIO_FUNC_SIO, false,false, true,  true, true);
        gpio_conf(SCSI_OUT_ACK,   GPIO_FUNC_SIO, false,false, true,  true, true);
        gpio_conf(SCSI_OUT_ATN,   GPIO_FUNC_SIO, false,false, true,  true, true);
    }
}

bool azplatform_is_initiator_mode_enabled()
{
    return g_scsi_initiator;
}

/*****************************************/
/* Crash handlers                        */
/*****************************************/

extern SdFs SD;
extern uint32_t __StackTop;

void azplatform_emergency_log_save()
{
    azplatform_set_sd_callback(NULL, NULL);

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
    crashfile.write(azlog_get_buffer(&startpos));
    crashfile.write(azlog_get_buffer(&startpos));
    crashfile.flush();
    crashfile.close();
}

void mbed_error_hook(const mbed_error_ctx * error_context)
{
    azlog("--------------");
    azlog("CRASH!");
    azlog("Platform: ", g_azplatform_name);
    azlog("FW Version: ", g_azlog_firmwareversion);
    azlog("error_status: ", (uint32_t)error_context->error_status);
    azlog("error_address: ", error_context->error_address);
    azlog("error_value: ", error_context->error_value);

    uint32_t *p = (uint32_t*)((uint32_t)error_context->thread_current_sp & ~3);
    for (int i = 0; i < 8; i++)
    {
        if (p == &__StackTop) break; // End of stack

        azlog("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
        p += 4;
    }

    azplatform_emergency_log_save();

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

/*****************************************/
/* Debug logging and watchdog            */
/*****************************************/

// This function is called for every log message.
void azplatform_log(const char *s)
{
    uart_puts(uart0, s);
}

static int g_watchdog_timeout;
static bool g_watchdog_initialized;

static void watchdog_callback(unsigned alarm_num)
{
    g_watchdog_timeout -= 1000;

    if (g_watchdog_timeout <= WATCHDOG_CRASH_TIMEOUT - WATCHDOG_BUS_RESET_TIMEOUT)
    {
        if (!scsiDev.resetFlag || !g_scsiHostPhyReset)
        {
            azlog("--------------");
            azlog("WATCHDOG TIMEOUT, attempting bus reset");
            azlog("GPIO states: out ", sio_hw->gpio_out, " oe ", sio_hw->gpio_oe, " in ", sio_hw->gpio_in);

            uint32_t *p = (uint32_t*)__get_PSP();
            for (int i = 0; i < 8; i++)
            {
                if (p == &__StackTop) break; // End of stack

                azlog("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
                p += 4;
            }

            scsiDev.resetFlag = 1;
            g_scsiHostPhyReset = true;
        }

        if (g_watchdog_timeout <= 0)
        {
            azlog("--------------");
            azlog("WATCHDOG TIMEOUT!");
            azlog("Platform: ", g_azplatform_name);
            azlog("FW Version: ", g_azlog_firmwareversion);
            azlog("GPIO states: out ", sio_hw->gpio_out, " oe ", sio_hw->gpio_oe, " in ", sio_hw->gpio_in);

            uint32_t *p = (uint32_t*)__get_PSP();
            for (int i = 0; i < 8; i++)
            {
                if (p == &__StackTop) break; // End of stack

                azlog("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
                p += 4;
            }

            azplatform_emergency_log_save();

            azplatform_boot_to_main_firmware();
        }
    }

    hardware_alarm_set_target(3, delayed_by_ms(get_absolute_time(), 1000));
}

// This function can be used to periodically reset watchdog timer for crash handling.
// It can also be left empty if the platform does not use a watchdog timer.
void azplatform_reset_watchdog()
{
    g_watchdog_timeout = WATCHDOG_CRASH_TIMEOUT;

    if (!g_watchdog_initialized)
    {
        hardware_alarm_claim(3);
        hardware_alarm_set_callback(3, &watchdog_callback);
        hardware_alarm_set_target(3, delayed_by_ms(get_absolute_time(), 1000));
        g_watchdog_initialized = true;
    }
}

/*****************************************/
/* Flash reprogramming from bootloader   */
/*****************************************/

#ifdef AZPLATFORM_BOOTLOADER_SIZE

extern uint32_t __real_vectors_start;
extern uint32_t __StackTop;
static volatile void *g_bootloader_exit_req;

bool azplatform_rewrite_flash_page(uint32_t offset, uint8_t buffer[AZPLATFORM_FLASH_PAGE_SIZE])
{
    if (offset == AZPLATFORM_BOOTLOADER_SIZE)
    {
        if (buffer[3] != 0x20 || buffer[7] != 0x10)
        {
            azlog("Invalid firmware file, starts with: ", bytearray(buffer, 16));
            return false;
        }
    }

    azdbg("Writing flash at offset ", offset, " data ", bytearray(buffer, 4));
    assert(offset % AZPLATFORM_FLASH_PAGE_SIZE == 0);
    assert(offset >= AZPLATFORM_BOOTLOADER_SIZE);

    // Avoid any mbed timer interrupts triggering during the flashing.
    __disable_irq();

    // For some reason any code executed after flashing crashes
    // unless we disable the XIP cache.
    // Not sure why this happens, as flash_range_program() is flushing
    // the cache correctly.
    // The cache is now enabled from bootloader start until it starts
    // flashing, and again after reset to main firmware.
    xip_ctrl_hw->ctrl = 0;

    flash_range_erase(offset, AZPLATFORM_FLASH_PAGE_SIZE);
    flash_range_program(offset, buffer, AZPLATFORM_FLASH_PAGE_SIZE);

    uint32_t *buf32 = (uint32_t*)buffer;
    uint32_t num_words = AZPLATFORM_FLASH_PAGE_SIZE / 4;
    for (int i = 0; i < num_words; i++)
    {
        uint32_t expected = buf32[i];
        uint32_t actual = *(volatile uint32_t*)(XIP_NOCACHE_BASE + offset + i * 4);

        if (actual != expected)
        {
            azlog("Flash verify failed at offset ", offset + i * 4, " got ", actual, " expected ", expected);
            return false;
        }
    }

    __enable_irq();

    return true;
}

void azplatform_boot_to_main_firmware()
{
    // To ensure that the system state is reset properly, we perform
    // a SYSRESETREQ and jump straight from the reset vector to main application.
    g_bootloader_exit_req = &g_bootloader_exit_req;
    SCB->AIRCR = 0x05FA0004;
    while(1);
}

void btldr_reset_handler()
{
    uint32_t* application_base = &__real_vectors_start;
    if (g_bootloader_exit_req == &g_bootloader_exit_req)
    {
        // Boot to main application
        application_base = (uint32_t*)(XIP_BASE + AZPLATFORM_BOOTLOADER_SIZE);
    }

    SCB->VTOR = (uint32_t)application_base;
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

uint32_t azplatform_get_romdrive_maxsize()
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

bool azplatform_read_romdrive(uint8_t *dest, uint32_t start, uint32_t count)
{
    xip_ctrl_hw->stream_ctr = 0;

    while (!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY))
    {
        (void) xip_ctrl_hw->stream_fifo;
    }

    xip_ctrl_hw->stream_addr = start + ROMDRIVE_OFFSET;
    xip_ctrl_hw->stream_ctr = count / 4;

    // Transfer happens in multiples of 4 bytes
    assert(start < azplatform_get_romdrive_maxsize());
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

bool azplatform_write_romdrive(const uint8_t *data, uint32_t start, uint32_t count)
{
    assert(start < azplatform_get_romdrive_maxsize());
    assert((count % AZPLATFORM_ROMDRIVE_PAGE_SIZE) == 0);

    __disable_irq();
    flash_range_erase(start + ROMDRIVE_OFFSET, count);
    flash_range_program(start + ROMDRIVE_OFFSET, data, count);
    __enable_irq();
    return true;
}

#endif

/**********************************************/
/* Mapping from data bytes to GPIO BOP values */
/**********************************************/

/* A lookup table is the fastest way to calculate parity and convert the IO pin mapping for data bus.
 * For RP2040 we expect that the bits are consecutive and in order.
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

const uint32_t g_scsi_parity_lookup[256] =
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
            azlog_raw(buf);
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