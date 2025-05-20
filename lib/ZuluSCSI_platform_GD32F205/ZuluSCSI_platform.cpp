/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 * 
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#include "ZuluSCSI_platform.h"
#include "gd32f20x_sdio.h"
#include "gd32f20x_fmc.h"
#include "gd32f20x_fwdgt.h"
#include "gd32_sdio_sdcard.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
#include "usbd_conf.h"
#include "usb_serial.h"
#include "greenpak.h"
#include <SdFat.h>
#include <scsi.h>
#include <assert.h>
#include <audio_i2s.h>
#include <ZuluSCSI_audio.h>
#include <ZuluSCSI_settings.h>

extern SdFs SD;
extern bool g_rawdrive_active;

extern "C" {

const char *g_platform_name = PLATFORM_NAME;
static bool g_enable_apple_quirks = false;
bool g_direct_mode = false;
ZuluSCSIVersion_t g_zuluscsi_version = ZSVersion_unknown;
bool g_moved_select_in = false;
static bool g_led_blinking = false;
// hw_config.cpp c functions
#include "platform_hw_config.h"

// usb_log_poll() is called through function pointer to
// avoid including USB in SD card bootloader.
static void (*g_usb_log_poll_func)(void);
static void usb_log_poll();


/*************************/
/* Timing functions      */
/*************************/

static volatile uint32_t g_millisecond_counter;
static volatile uint32_t g_watchdog_timeout;
static uint32_t g_ns_to_cycles; // Q0.32 fixed point format

static void watchdog_handler(uint32_t *sp);

unsigned long millis()
{
    return g_millisecond_counter;
}

void delay(unsigned long ms)
{
    uint32_t start = g_millisecond_counter;
    while ((uint32_t)(g_millisecond_counter - start) < ms);
}

void delay_ns(unsigned long ns)
{
    uint32_t CNT_start = DWT->CYCCNT;
    if (ns <= 100) return; // Approximate call overhead
    ns -= 100;

    uint32_t cycles = ((uint64_t)ns * g_ns_to_cycles) >> 32;
    while ((uint32_t)(DWT->CYCCNT - CNT_start) < cycles);
}

void SysTick_Handler_inner(uint32_t *sp)
{
    g_millisecond_counter++;

    if (g_watchdog_timeout > 0)
    {
        g_watchdog_timeout--;

        const uint32_t busreset_time = WATCHDOG_CRASH_TIMEOUT - WATCHDOG_BUS_RESET_TIMEOUT;
        if (g_watchdog_timeout <= busreset_time)
        {
            if (!scsiDev.resetFlag)
            {
                logmsg("WATCHDOG TIMEOUT at PC ", sp[6], " LR ", sp[5], " attempting bus reset");
                scsiDev.resetFlag = 1;
            }

            if (g_watchdog_timeout == 0)
            {
                watchdog_handler(sp);
            }
        }
    }
}

__attribute__((interrupt, naked))
void SysTick_Handler(void)
{
    // Take note of stack pointer so that we can print debug
    // info in watchdog handler.
    asm("mrs r0, msp\n"
        "b SysTick_Handler_inner": : : "r0");
}

// This function is called by scsiPhy.cpp.
// It resets the systick counter to give 1 millisecond of uninterrupted transfer time.
// The total number of skips is kept track of to keep the correct time on average.
void SysTick_Handle_PreEmptively()
{
    static int skipped_clocks = 0;

    __disable_irq();
    uint32_t loadval = SysTick->LOAD;
    skipped_clocks += loadval - SysTick->VAL;
    SysTick->VAL = 0;

    if (skipped_clocks > loadval)
    {
        // We have skipped enough ticks that it is time to fake a call
        // to SysTick interrupt handler.
        skipped_clocks -= loadval;
        uint32_t stack_frame[8] = {0};
        stack_frame[6] = (uint32_t)__builtin_return_address(0);
        SysTick_Handler_inner(stack_frame);
    }
    __enable_irq();
}

uint32_t platform_sys_clock_in_hz()
{
    return rcu_clock_freq_get(CK_SYS);
}


/***************/
/* GPIO init   */
/***************/

#ifdef PLATFORM_VERSION_1_1_PLUS
static void init_audio_gpio()
{
        gpio_pin_remap1_config(GPIO_PCF5, GPIO_PCF5_SPI1_IO_REMAP1, ENABLE);
        gpio_pin_remap1_config(GPIO_PCF5, GPIO_PCF5_SPI1_NSCK_REMAP1, ENABLE);
        gpio_pin_remap1_config(GPIO_PCF4, GPIO_PCF4_SPI1_SCK_PD3_REMAP, ENABLE);
        gpio_init(I2S_CK_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, I2S_CK_PIN);
        gpio_init(I2S_SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, I2S_SD_PIN);
        gpio_init(I2S_WS_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, I2S_WS_PIN);
}
#endif

// Method of determining whichi scsi board is being used
static ZuluSCSIVersion_t get_zuluscsi_version()
{
#ifdef DIGITAL_VERSION_DETECT_PORT
    bool pull_down;
    bool pull_up;

    gpio_init(DIGITAL_VERSION_DETECT_PORT, GPIO_MODE_IPU, 0, DIGITAL_VERSION_DETECT_PIN);
    delay_us(10);
    pull_up  = SET == gpio_input_bit_get(DIGITAL_VERSION_DETECT_PORT, DIGITAL_VERSION_DETECT_PIN);

    gpio_init(DIGITAL_VERSION_DETECT_PORT, GPIO_MODE_IPD, 0, DIGITAL_VERSION_DETECT_PIN);
    delay_us(10);
    pull_down = RESET ==  gpio_input_bit_get(DIGITAL_VERSION_DETECT_PORT, DIGITAL_VERSION_DETECT_PIN);

    if (pull_up && pull_down)
        return ZSVersion_v1_1;
    if (pull_down && !pull_up)
        return ZSVersion_v1_1_ODE;
    if (pull_up && !pull_down)
    {
        return ZSVersion_v1_2;
    }
#endif // DIGITAL_DETECT_VERSION

    return ZSVersion_unknown;
}


// Initialize SPI and GPIO configuration
// Clock has already been initialized by system_gd32f20x.c
void platform_init()
{
    SystemCoreClockUpdate();

    // Enable SysTick to drive millis()
    g_millisecond_counter = 0;
    SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 0x00U);

    // Enable DWT counter to drive delay_ns()
    g_ns_to_cycles = ((uint64_t)SystemCoreClock << 32) / 1000000000;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Enable debug output on SWO pin
    DBG_CTL |= DBG_CTL_TRACE_IOEN;
    if (TPI->ACPR == 0)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        TPI->ACPR = SystemCoreClock / 2000000 - 1; // 2 Mbps baudrate for SWO
        // TPI->ACPR = SystemCoreClock / 30000000 - 1; // 30 Mbps baudrate for SWO
        TPI->SPPR = 2;
        TPI->FFCR = 0x100; // TPIU packet framing disabled
        // DWT->CTRL |= (1 << DWT_CTRL_EXCTRCENA_Pos);
        // DWT->CTRL |= (1 << DWT_CTRL_CYCTAP_Pos)
        //             | (15 << DWT_CTRL_POSTPRESET_Pos)
        //             | (1 << DWT_CTRL_PCSAMPLENA_Pos)
        //             | (3 << DWT_CTRL_SYNCTAP_Pos)
        //             | (1 << DWT_CTRL_CYCCNTENA_Pos);
        ITM->LAR = 0xC5ACCE55;
        ITM->TCR = (1 << ITM_TCR_DWTENA_Pos)
                    | (1 << ITM_TCR_SYNCENA_Pos)
                    | (1 << ITM_TCR_ITMENA_Pos);
        ITM->TER = 0xFFFFFFFF; // Enable all stimulus ports
    }

    // Enable needed clocks for GPIO
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    
    // Switch to SWD debug port (disable JTAG) to release PB4 as GPIO
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);    

    // SCSI pins.
    // Initialize open drain outputs to high.
    SCSI_RELEASE_OUTPUTS();

    // determine the ZulusSCSI board version
    g_zuluscsi_version = get_zuluscsi_version();
    g_moved_select_in = g_zuluscsi_version == ZSVersion_v1_1_ODE || g_zuluscsi_version == ZSVersion_v1_2;

    // Init SCSI pins GPIOs
    gpio_init(SCSI_OUT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_DATA_MASK | SCSI_OUT_REQ);
    gpio_init(SCSI_OUT_IO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_IO_PIN);
    gpio_init(SCSI_OUT_CD_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_CD_PIN);
    gpio_init(SCSI_OUT_SEL_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_SEL_PIN);
    gpio_init(SCSI_OUT_MSG_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_MSG_PIN);
    gpio_init(SCSI_OUT_RST_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_RST_PIN);
    gpio_init(SCSI_OUT_BSY_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_BSY_PIN);

    gpio_init(SCSI_IN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_IN_MASK);
    gpio_init(SCSI_ATN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_ATN_PIN);
    gpio_init(SCSI_BSY_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_BSY_PIN);
    gpio_init(SCSI_ACK_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_ACK_PIN);
    gpio_init(SCSI_RST_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_RST_PIN);

    // Terminator enable
    gpio_bit_set(SCSI_TERM_EN_PORT, SCSI_TERM_EN_PIN);
    gpio_init(SCSI_TERM_EN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, SCSI_TERM_EN_PIN);

#ifndef SD_USE_SDIO
    // SD card pins using SPI
    gpio_init(SD_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SD_CS_PIN);
    gpio_init(SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_CLK_PIN);
    gpio_init(SD_PORT, GPIO_MODE_IPU, 0, SD_MISO_PIN);
    gpio_init(SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_MOSI_PIN);
#else
    // SD card pins using SDIO
    gpio_init(SD_SDIO_DATA_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_SDIO_D0 | SD_SDIO_D1 | SD_SDIO_D2 | SD_SDIO_D3);
    gpio_init(SD_SDIO_CLK_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_SDIO_CLK);
    gpio_init(SD_SDIO_CMD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_SDIO_CMD);
#endif

#ifdef PLATFORM_VERSION_1_1_PLUS
    if (g_zuluscsi_version == ZSVersion_v1_1)
    {
        // SCSI Select
        gpio_init(SCSI_SEL_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_SEL_PIN);

        // DIP switches
        gpio_init(DIP_PORT, GPIO_MODE_IPD, 0, DIPSW1_PIN | DIPSW2_PIN | DIPSW3_PIN);
        gpio_init(EJECT_1_PORT, GPIO_MODE_IPU, 0, EJECT_1_PIN);
        gpio_init(EJECT_2_PORT, GPIO_MODE_IPU, 0, EJECT_2_PIN);

    }
    else if (g_zuluscsi_version == ZSVersion_v1_1_ODE)
    {
        // SCSI Select
        gpio_init(SCSI_ODE_SEL_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_ODE_SEL_PIN);
        // DIP switches
        gpio_init(ODE_DIP_PORT, GPIO_MODE_IPD, 0, ODE_DIPSW1_PIN | ODE_DIPSW2_PIN | ODE_DIPSW3_PIN);
        // Buttons
        gpio_init(EJECT_BTN_PORT, GPIO_MODE_IPU, 0, EJECT_BTN_PIN);
        gpio_init(USER_BTN_PORT, GPIO_MODE_IPU, 0, USER_BTN_PIN);
        init_audio_gpio();
        g_audio_enabled = true;
    }
    else if (g_zuluscsi_version == ZSVersion_v1_2)
    {
        // SCSI Select
        gpio_init(SCSI_ODE_SEL_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_ODE_SEL_PIN);
        // General settings DIP switch
        gpio_init(V1_2_DIPSW_TERM_PORT, GPIO_MODE_IPD, 0, V1_2_DIPSW_TERM_PIN);
        gpio_init(V1_2_DIPSW_DBG_PORT, GPIO_MODE_IPD, 0, V1_2_DIPSW_DBG_PIN);
        gpio_init(V1_2_DIPSW_QUIRKS_PORT, GPIO_MODE_IPD, 0, V1_2_DIPSW_QUIRKS_PIN);
        // Direct/Raw Mode Select
        gpio_init(V1_2_DIPSW_DIRECT_MODE_PORT, GPIO_MODE_IPD, 0, V1_2_DIPSW_DIRECT_MODE_PIN);
        // SCSI ID dip switch
        gpio_init(DIPSW_SCSI_ID_BIT_PORT, GPIO_MODE_IPD, 0, DIPSW_SCSI_ID_BIT_PINS);
        // Device select BCD rotary DIP switch
        gpio_init(DIPROT_DEVICE_SEL_BIT_PORT, GPIO_MODE_IPD, 0, DIPROT_DEVICE_SEL_BIT_PINS);

        // Buttons
        gpio_init(EJECT_BTN_PORT, GPIO_MODE_IPU, 0, EJECT_BTN_PIN);
        gpio_init(USER_BTN_PORT,  GPIO_MODE_IPU, 0, USER_BTN_PIN);

        LED_EJECT_OFF();
        gpio_init(LED_EJECT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, LED_EJECT_PIN);
    }
#else
    // SCSI Select
    gpio_init(SCSI_SEL_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_SEL_PIN);
    // DIP switches
    gpio_init(DIP_PORT, GPIO_MODE_IPD, 0, DIPSW1_PIN | DIPSW2_PIN | DIPSW3_PIN);
    // Ejection buttons
    gpio_init(EJECT_1_PORT, GPIO_MODE_IPU, 0, EJECT_1_PIN);
    gpio_init(EJECT_2_PORT, GPIO_MODE_IPU, 0, EJECT_2_PIN);

#endif // PLATFORM_VERSION_1_1_PLUS
    // LED pins
    gpio_bit_set(LED_PORT, LED_PINS);
    gpio_init(LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, LED_PINS);


    // SWO trace pin on PB3
    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);
}

static void set_termination(uint32_t port, uint32_t pin, const char *switch_name)
{
    if (gpio_input_bit_get(port, pin))
    {
        logmsg(switch_name, " is ON: Enabling SCSI termination");
        gpio_bit_reset(SCSI_TERM_EN_PORT, SCSI_TERM_EN_PIN);
    }
    else
    {
        logmsg(switch_name, " is OFF: Disabling SCSI termination");
    }
}

static bool get_debug(uint32_t port, uint32_t pin, const char *switch_name)
{
    if (gpio_input_bit_get(port, pin))
    {
        logmsg(switch_name, " is ON: Enabling debug messages");
        return true;
    }
    logmsg(switch_name, " is OFF: Disabling debug messages");
    return false;
}

static bool get_quirks(uint32_t port, uint32_t pin, const char *switch_name)
{
    if (gpio_input_bit_get(port, pin))
    {
        logmsg(switch_name, " is ON: Enabling Apple quirks by default");
        return true;
    }
    logmsg(switch_name, " is OFF: Disabling Apple quirks mode by default");
    return false;
}

#ifdef PLATFORM_VERSION_1_1_PLUS
static bool get_direct_mode(uint32_t port, uint32_t pin, const char *switch_name)
{
    if (!gpio_input_bit_get(port, pin))
    {
        logmsg(switch_name, " is OFF: Enabling direct/raw mode");
        return true;
    }
    logmsg(switch_name, " is ON: Disabling direct/raw mode");
    return false;
}
#endif

void platform_late_init()
{

    // Initialize usb for CDC serial output
    usb_serial_init();
    g_usb_log_poll_func = &usb_log_poll;

    logmsg("Platform: ", g_platform_name);
    logmsg("FW Version: ", g_log_firmwareversion);

#ifdef PLATFORM_VERSION_1_1_PLUS
    if (ZSVersion_v1_1 == g_zuluscsi_version)
    {
        logmsg("Board Version: ZuluSCSI v1.1 Standard Edition");
        set_termination(DIP_PORT, DIPSW3_PIN, "DIPSW3");
        g_log_debug = get_debug(DIP_PORT, DIPSW2_PIN, "DIPSW2");
        g_enable_apple_quirks = get_quirks(DIP_PORT, DIPSW1_PIN, "DIPSW1");
        greenpak_load_firmware();
    }
    else if (ZSVersion_v1_1_ODE == g_zuluscsi_version)
    {
        logmsg("Board Version: ZuluSCSI v1.1 ODE");
        logmsg("ODE - Optical Drive Emulator");
        set_termination(ODE_DIP_PORT, ODE_DIPSW3_PIN, "DIPSW3");
        g_log_debug = get_debug(ODE_DIP_PORT, ODE_DIPSW2_PIN, "DIPSW2");
        g_enable_apple_quirks = get_quirks(ODE_DIP_PORT, ODE_DIPSW1_PIN, "DIPSW1");
        audio_setup();
    }
    else if (ZSVersion_v1_2 == g_zuluscsi_version)
    {
        logmsg("Board Version: ZuluSCSI v1.2");
        hw_config_init_gpios();
        set_termination(V1_2_DIPSW_TERM_PORT, V1_2_DIPSW_TERM_PIN, "DIPSW4");
        g_log_debug = get_debug(V1_2_DIPSW_DBG_PORT, V1_2_DIPSW_DBG_PIN, "DIPSW3");
        g_direct_mode = get_direct_mode(V1_2_DIPSW_DIRECT_MODE_PORT, V1_2_DIPSW_DIRECT_MODE_PIN, "DIPSW2");
        g_enable_apple_quirks = get_quirks(V1_2_DIPSW_QUIRKS_PORT, V1_2_DIPSW_QUIRKS_PIN, "DIPSW1");
        hw_config_init_state(g_direct_mode);

    }
#else // PLATFORM_VERSION_1_1_PLUS - ZuluSCSI v1.0 and v1.0 minis gpio config
    #ifdef ZULUSCSI_V1_0_mini
        logmsg("SCSI termination is always on");
    #elif defined(ZULUSCSI_V1_0)
        set_termination(DIP_PORT, DIPSW3_PIN, "DIPSW3");
        g_log_debug = get_debug(DIP_PORT, DIPSW2_PIN, "DIPSW2");
        g_enable_apple_quirks = get_quirks(DIP_PORT, DIPSW1_PIN, "DIPSW1");
    #endif // ZULUSCSI_V1_0_mini
#endif // PLATFORM_VERSION_1_1_PLUS
    
}

void platform_post_sd_card_init()
{
    #ifdef PLATFORM_VERSION_1_1_PLUS
    if (ZSVersion_v1_2 == g_zuluscsi_version && g_scsi_settings.getSystem()->enableCDAudio)
    {
        logmsg("Audio enabled - an external audio DAC is required on the I2S expansion header");
        init_audio_gpio();
        g_audio_enabled = true;
        audio_setup();
    }
    #endif
}

void platform_write_led(bool state)
{
    if (g_led_blinking) return;

    if (g_scsi_settings.getSystem()->invertStatusLed)
        state = !state;

    if (state)
        gpio_bit_reset(LED_PORT, LED_PINS);
    else
        gpio_bit_set(LED_PORT, LED_PINS);
}

void platform_set_blink_status(bool status)
{
    g_led_blinking = status;
}

void platform_write_led_override(bool state)
{
    if (g_scsi_settings.getSystem()->invertStatusLed)
        state = !state;

    if (state)
        gpio_bit_reset(LED_PORT, LED_PINS);
    else
        gpio_bit_set(LED_PORT, LED_PINS);
}

void platform_disable_led(void)
{   
    gpio_init(LED_PORT, GPIO_MODE_IPU, 0, LED_PINS);
    logmsg("Disabling status LED");
}

uint8_t platform_no_sd_card_on_init_error_code()
{
    return 0x80 | SD_CMD_RESP_TIMEOUT;
}
/*****************************************/
/* Supply voltage monitor                */
/*****************************************/

// Use ADC to implement supply voltage monitoring for the +3.0V rail.
// This works by sampling the Vrefint, which has
// a voltage of 1.2 V, allowing to calculate the VDD voltage.
static void adc_poll()
{
#if PLATFORM_VDD_WARNING_LIMIT_mV > 0
    static bool initialized = false;
    static int lowest_vdd_seen = PLATFORM_VDD_WARNING_LIMIT_mV;

    if (!initialized)
    {
        rcu_periph_clock_enable(RCU_ADC0);
        adc_enable(ADC0);
        adc_calibration_enable(ADC0);
        adc_tempsensor_vrefint_enable();
        adc_inserted_channel_config(ADC0, 0, ADC_CHANNEL_17, ADC_SAMPLETIME_239POINT5);
        adc_external_trigger_source_config(ADC0, ADC_INSERTED_CHANNEL, ADC0_1_2_EXTTRIG_INSERTED_NONE);
        adc_external_trigger_config(ADC0, ADC_INSERTED_CHANNEL, ENABLE);
        adc_software_trigger_enable(ADC0, ADC_INSERTED_CHANNEL);
        initialized = true;
    }

    // Read previous result and start new one
    int adc_value = ADC_IDATA0(ADC0);
    adc_software_trigger_enable(ADC0, ADC_INSERTED_CHANNEL);

    // adc_value = 1200mV * 4096 / Vdd
    // => Vdd = 1200mV * 4096 / adc_value
    // To avoid wasting time on division, compare against
    // limit directly.
    const int limit = (1200 * 4096) / PLATFORM_VDD_WARNING_LIMIT_mV;
    if (adc_value > limit)
    {
        // Warn once, and then again if we detect even a lower drop.
        int vdd_mV = (1200 * 4096) / adc_value;
        if (vdd_mV < lowest_vdd_seen)
        {
            logmsg("WARNING: Detected supply voltage drop to ", vdd_mV, "mV. Verify power supply is adequate.");
            lowest_vdd_seen = vdd_mV - 50; // Small hysteresis to avoid excessive warnings
        }
    }
#endif
}

/*****************************************/
/* Debug logging and watchdog            */
/*****************************************/

// Send log data to USB UART if USB is connected.
// Data is retrieved from the shared log ring buffer and
// this function sends as much as fits in USB CDC buffer.

static void usb_log_poll()
{
    static uint32_t logpos = 0;

    if (usb_serial_ready())
    {
        // Retrieve pointer to log start and determine number of bytes available.
        uint32_t available = 0;
        const char *data = log_get_buffer(&logpos, &available);
        // Limit to CDC packet size
        uint32_t len = available;
        if (len == 0) return;
        if (len > USB_CDC_DATA_PACKET_SIZE) len = USB_CDC_DATA_PACKET_SIZE;

        // Update log position by the actual number of bytes sent
        // If USB CDC buffer is full, this may be 0
        usb_serial_send((uint8_t*)data, len);
        logpos -= available - len;
    }
}

/*****************************************/
/* Crash handlers                        */
/*****************************************/
// Writes log data to the PB3 SWO pin
void platform_log(const char *s)
{
    while (*s)
    {
        // Write to SWO pin
        while (ITM->PORT[0].u32 == 0);
        ITM->PORT[0].u8 = *s++;
    }
}

void platform_emergency_log_save()
{
    if (g_rawdrive_active)
        return;
#ifdef ZULUSCSI_HARDWARE_CONFIG
    if (g_hw_config.is_active())
        return;
#endif
    platform_set_sd_callback(NULL, NULL);

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
}

extern uint32_t _estack;

__attribute__((noinline))
void show_hardfault(uint32_t *sp)
{
    uint32_t pc = sp[6];
    uint32_t lr = sp[5];
    uint32_t cfsr = SCB->CFSR;
    
    logmsg("--------------");
    logmsg("CRASH!");
    logmsg("Platform: ", g_platform_name);
    logmsg("FW Version: ", g_log_firmwareversion);
    logmsg("scsiDev.cdb: ", bytearray(scsiDev.cdb, 12));
    logmsg("scsiDev.phase: ", (int)scsiDev.phase);
    logmsg("CFSR: ", cfsr);
    logmsg("SP: ", (uint32_t)sp);
    logmsg("PC: ", pc);
    logmsg("LR: ", lr);
    logmsg("R0: ", sp[0]);
    logmsg("R1: ", sp[1]);
    logmsg("R2: ", sp[2]);
    logmsg("R3: ", sp[3]);

    uint32_t *p = (uint32_t*)((uint32_t)sp & ~3);
    for (int i = 0; i < 8; i++)
    {
        if (p == &_estack) break; // End of stack
        
        logmsg("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
        p += 4;
    }
 
    platform_emergency_log_save();

    while (1)
    {
        if (g_usb_log_poll_func) g_usb_log_poll_func();
        // Flash the crash address on the LED
        // Short pulse means 0, long pulse means 1
        int base_delay = 1000;
        for (int i = 31; i >= 0; i--)
        {
            LED_OFF();
            for (int j = 0; j < base_delay; j++) delay_ns(100000);
            
            int delay = (pc & (1 << i)) ? (3 * base_delay) : base_delay;
            LED_ON();
            for (int j = 0; j < delay; j++) delay_ns(100000);
            LED_OFF();
        }

        for (int j = 0; j < base_delay * 10; j++) delay_ns(100000);
    }
}

__attribute__((naked, interrupt))
void HardFault_Handler(void)
{
    // Copies stack pointer into first argument
    asm("mrs r0, msp\n"
        "b show_hardfault": : : "r0");
}

__attribute__((naked, interrupt))
void MemManage_Handler(void)
{
    asm("mrs r0, msp\n"
        "b show_hardfault": : : "r0");
}

__attribute__((naked, interrupt))
void BusFault_Handler(void)
{
    asm("mrs r0, msp\n"
        "b show_hardfault": : : "r0");
}

__attribute__((naked, interrupt))
void UsageFault_Handler(void)
{
    asm("mrs r0, msp\n"
        "b show_hardfault": : : "r0");
}

void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    uint32_t dummy = 0;

    logmsg("--------------");
    logmsg("ASSERT FAILED!");
    logmsg("Platform: ", g_platform_name);
    logmsg("FW Version: ", g_log_firmwareversion);
    logmsg("scsiDev.cdb: ", bytearray(scsiDev.cdb, 12));
    logmsg("scsiDev.phase: ", (int)scsiDev.phase);
    logmsg("Assert failed: ", file , ":", line, " in ", func, ":", expr);

    uint32_t *p = (uint32_t*)((uint32_t)&dummy & ~3);
    for (int i = 0; i < 8; i++)
    {
        if (p == &_estack) break; // End of stack

        logmsg("STACK ", (uint32_t)p, ":    ", p[0], " ", p[1], " ", p[2], " ", p[3]);
        p += 4;
    }

    platform_emergency_log_save();

    while(1)
    {
        if (g_usb_log_poll_func) g_usb_log_poll_func();
        LED_OFF();
        for (int j = 0; j < 1000; j++) delay_ns(100000);
        LED_ON();
        for (int j = 0; j < 1000; j++) delay_ns(100000);
    }
}

} /* extern "C" */

static void watchdog_handler(uint32_t *sp)
{
    logmsg("-------------- WATCHDOG TIMEOUT");
    show_hardfault(sp);
}

void platform_reset_watchdog()
{
    // This uses a software watchdog based on systick timer interrupt.
    // It gives us opportunity to collect better debug info than the
    // full hardware reset that would be caused by hardware watchdog.
    g_watchdog_timeout = WATCHDOG_CRASH_TIMEOUT;

    // USB log is polled here also to make sure any log messages in fault states
    // get passed to USB.
    usb_log_poll();
}

void platform_reset_mcu()
{
    // reset in 2 sec ( 1 / (40KHz / 32) * 2500 == 2sec)
    fwdgt_config(2500, FWDGT_PSC_DIV32);
    fwdgt_enable();

}

// Poll function that is called every few milliseconds.
// Can be left empty or used for platform-specific processing.
void platform_poll()
{
#ifdef ENABLE_AUDIO_OUTPUT
    audio_poll();
#endif
    adc_poll();
    usb_log_poll();
}

uint8_t platform_get_buttons()
{
    // Buttons are active low: internal pull-up is enabled,
    // and when button is pressed the pin goes low.
    uint8_t buttons = 0;
#ifdef PLATFORM_VERSION_1_1_PLUS
    if (g_zuluscsi_version == ZSVersion_v1_1_ODE || g_zuluscsi_version == ZSVersion_v1_2)
    {
        if (!gpio_input_bit_get(EJECT_BTN_PORT, EJECT_BTN_PIN))   buttons |= 1;
        if (!gpio_input_bit_get(USER_BTN_PORT, USER_BTN_PIN))   buttons |= 4;
    }
    else
    {
        if (!gpio_input_bit_get(EJECT_1_PORT, EJECT_1_PIN))   buttons |= 1;
        if (!gpio_input_bit_get(EJECT_2_PORT, EJECT_2_PIN))   buttons |= 2;
    }
#else
    if (!gpio_input_bit_get(EJECT_1_PORT, EJECT_1_PIN))   buttons |= 1;
    if (!gpio_input_bit_get(EJECT_2_PORT, EJECT_2_PIN))   buttons |= 2;
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

#ifdef PLATFORM_VERSION_1_1_PLUS
    if(g_zuluscsi_version == ZSVersion_v1_1_ODE || g_zuluscsi_version == ZSVersion_v1_2)
    {
        static uint8_t previous = 0x00;
        uint8_t bitmask = buttons_debounced & USER_BTN_MASK;
        uint8_t ejectors = (previous ^ bitmask) & previous;
        previous = bitmask;
        if (ejectors & USER_BTN_MASK)
        {
            logmsg("User button pressed - feature not yet implemented");
        }
    }
#endif

    return buttons_debounced;
}

bool platform_has_phy_eject_button()
{
    return g_zuluscsi_version == ZSVersion_v1_1_ODE || g_zuluscsi_version == ZSVersion_v1_2;
}

/***********************/
/* Flash reprogramming */
/***********************/

bool platform_rewrite_flash_page(uint32_t offset, uint8_t buffer[PLATFORM_FLASH_PAGE_SIZE])
{
    if (offset == 0)
    {
        if (buffer[3] != 0x20 || buffer[7] != 0x08)
        {
            logmsg("Invalid firmware file, starts with: ", bytearray(buffer, 16));
            return false;
        }
    }

    dbgmsg("Writing flash at offset ", offset, " data ", bytearray(buffer, 4));
    assert(offset % PLATFORM_FLASH_PAGE_SIZE == 0);
    assert(offset >= PLATFORM_BOOTLOADER_SIZE);
    
    fmc_unlock();
    fmc_bank0_unlock();

    fmc_state_enum status;
    status = fmc_page_erase(FLASH_BASE + offset);
    if (status != FMC_READY)
    {
        logmsg("Erase failed: ", (int)status);
        return false;
    }

    uint32_t *buf32 = (uint32_t*)buffer;
    uint32_t num_words = PLATFORM_FLASH_PAGE_SIZE / 4;
    for (int i = 0; i < num_words; i++)
    {
        status = fmc_word_program(FLASH_BASE + offset + i * 4, buf32[i]);
        if (status != FMC_READY)
        {
            logmsg("Flash write failed: ", (int)status);
            return false;
        }   
    }

    fmc_lock();

    for (int i = 0; i < num_words; i++)
    {
        uint32_t expected = buf32[i];
        uint32_t actual = *(volatile uint32_t*)(FLASH_BASE + offset + i * 4);
        if (actual != expected)
        {
            logmsg("Flash verify failed at offset ", offset + i * 4, " got ", actual, " expected ", expected);
            return false;
        }
    }
    return true;
}

void platform_boot_to_main_firmware()
{
    uint32_t *mainprogram_start = (uint32_t*)(0x08000000 + PLATFORM_BOOTLOADER_SIZE);
    SCB->VTOR = (uint32_t)mainprogram_start;
  
    __asm__(
        "msr msp, %0\n\t"
        "bx %1" : : "r" (mainprogram_start[0]),
                    "r" (mainprogram_start[1]) : "memory");
}

/**************************************/
/* SCSI configuration based on DIPSW1 */
/**************************************/

void platform_config_hook(S2S_TargetCfg *config)
{
    // Enable Apple quirks by dip switch
    if (g_enable_apple_quirks)
    {
        if (config->quirks == S2S_CFG_QUIRKS_NONE)
        {
            config->quirks = S2S_CFG_QUIRKS_APPLE;
        }
    }
}

/**********************************************/
/* Mapping from data bytes to GPIO BOP values */
/**********************************************/

#define PARITY(n) ((1 ^ (n) ^ ((n)>>1) ^ ((n)>>2) ^ ((n)>>3) ^ ((n)>>4) ^ ((n)>>5) ^ ((n)>>6) ^ ((n)>>7)) & 1)
#define X(n) (\
    ((n & 0x01) ? (SCSI_OUT_DB0 << 16) : SCSI_OUT_DB0) | \
    ((n & 0x02) ? (SCSI_OUT_DB1 << 16) : SCSI_OUT_DB1) | \
    ((n & 0x04) ? (SCSI_OUT_DB2 << 16) : SCSI_OUT_DB2) | \
    ((n & 0x08) ? (SCSI_OUT_DB3 << 16) : SCSI_OUT_DB3) | \
    ((n & 0x10) ? (SCSI_OUT_DB4 << 16) : SCSI_OUT_DB4) | \
    ((n & 0x20) ? (SCSI_OUT_DB5 << 16) : SCSI_OUT_DB5) | \
    ((n & 0x40) ? (SCSI_OUT_DB6 << 16) : SCSI_OUT_DB6) | \
    ((n & 0x80) ? (SCSI_OUT_DB7 << 16) : SCSI_OUT_DB7) | \
    (PARITY(n)  ? (SCSI_OUT_DBP << 16) : SCSI_OUT_DBP) | \
    (SCSI_OUT_REQ) \
)
    
const uint32_t g_scsi_out_byte_to_bop[256] =
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