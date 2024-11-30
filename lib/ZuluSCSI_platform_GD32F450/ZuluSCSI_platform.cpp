/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
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
#include "gd32f4xx_sdio.h"
#include "gd32f4xx_fmc.h"
#include "gd32f4xx_fwdgt.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
#include "usb_hs.h"
#include "usbd_conf.h"
#include "greenpak.h"
#include <SdFat.h>
#include <scsi.h>
#include <assert.h>
#include "usb_serial.h"

extern bool g_rawdrive_active;

extern "C" {

const char *g_platform_name = PLATFORM_NAME;
static bool g_enable_apple_quirks = false;

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
    if (ns <= 50) return; // Approximate call overhead
    ns -= 50;

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

// Initialize SPI and GPIO configuration
// Clock has already been initialized by system_gd32f20x.c
void platform_init()
{
    SystemCoreClockUpdate();

    // Enable SysTick to drive millis()
    // \todo not sure if this is needed
    // nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
    g_millisecond_counter = 0;
    SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 0x00U);

    // Enable DWT counter to drive delay_ns()
    g_ns_to_cycles = ((uint64_t)SystemCoreClock << 32) / 1000000000;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Enable debug output on SWO pin
    DBG_CTL0 |= DBG_CTL0_TRACE_IOEN;
    // if (TPI->ACPR == 0)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        TPI->ACPR = SystemCoreClock / 115200 - 1; // Serial speed baudrate for SWO
        // TPI->ACPR = SystemCoreClock / 2000000 - 1; // 2 Mbps baudrate for SWO
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
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOF);
    rcu_periph_clock_enable(RCU_GPIOG);

    // Switch to SWD debug port (disable JTAG) to release PB4 as GPIO
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4);

    // SCSI pins.
    // Initialize open drain outputs to high.
    SCSI_RELEASE_OUTPUTS();
    gpio_mode_set(SCSI_OUT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SCSI_OUT_DATA_MASK | SCSI_OUT_REQ);
    gpio_mode_set(SCSI_OUT_IO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,   SCSI_OUT_IO_PIN);
    gpio_mode_set(SCSI_OUT_CD_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,   SCSI_OUT_CD_PIN);
    gpio_mode_set(SCSI_OUT_SEL_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,  SCSI_OUT_SEL_PIN);
    gpio_mode_set(SCSI_OUT_MSG_PORT,  GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SCSI_OUT_MSG_PIN);
    gpio_mode_set(SCSI_OUT_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,  SCSI_OUT_RST_PIN);
    gpio_mode_set(SCSI_OUT_BSY_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,  SCSI_OUT_BSY_PIN);
    
    gpio_output_options_set(SCSI_OUT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ,     SCSI_OUT_DATA_MASK | SCSI_OUT_REQ);
    gpio_output_options_set(SCSI_OUT_IO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ,  SCSI_OUT_IO_PIN);
    gpio_output_options_set(SCSI_OUT_CD_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ,  SCSI_OUT_CD_PIN);
    gpio_output_options_set(SCSI_OUT_SEL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SCSI_OUT_SEL_PIN);
    gpio_output_options_set(SCSI_OUT_MSG_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SCSI_OUT_MSG_PIN);
    gpio_output_options_set(SCSI_OUT_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SCSI_OUT_RST_PIN);
    gpio_output_options_set(SCSI_OUT_BSY_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SCSI_OUT_BSY_PIN);


    gpio_mode_set(SCSI_IN_PORT,  GPIO_MODE_INPUT, GPIO_PUPD_NONE,   SCSI_IN_MASK);
    gpio_mode_set(SCSI_ATN_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,   SCSI_ATN_PIN);
    gpio_mode_set(SCSI_BSY_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,   SCSI_BSY_PIN);
    gpio_mode_set(SCSI_SEL_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,   SCSI_SEL_PIN);
    gpio_mode_set(SCSI_ACK_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,   SCSI_ACK_PIN);
    gpio_mode_set(SCSI_RST_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,   SCSI_RST_PIN);


    // Terminator enable
    gpio_bit_set(SCSI_TERM_EN_PORT, SCSI_TERM_EN_PIN);
    gpio_mode_set(SCSI_TERM_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SCSI_TERM_EN_PIN);
    gpio_output_options_set(SCSI_TERM_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, SCSI_TERM_EN_PIN);

#ifndef SD_USE_SDIO
    // SD card pins using SPI
    gpio_init(SD_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SD_CS_PIN);
    gpio_init(SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_CLK_PIN);
    gpio_init(SD_PORT, GPIO_MODE_IPU, 0, SD_MISO_PIN);
    gpio_init(SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_MOSI_PIN);
#else
    // SD card pins using SDIO
    gpio_mode_set(SD_SDIO_DATA_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SD_SDIO_D0 | SD_SDIO_D1 | SD_SDIO_D2 | SD_SDIO_D3);
    gpio_output_options_set(SD_SDIO_DATA_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SD_SDIO_D0 | SD_SDIO_D1 | SD_SDIO_D2 | SD_SDIO_D3);
    gpio_af_set(SD_SDIO_DATA_PORT, GPIO_AF_12, SD_SDIO_D0 | SD_SDIO_D1 | SD_SDIO_D2 | SD_SDIO_D3);

    gpio_mode_set(SD_SDIO_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SD_SDIO_CLK);
    gpio_output_options_set(SD_SDIO_CLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SD_SDIO_CLK);
    gpio_af_set(SD_SDIO_CLK_PORT, GPIO_AF_12, SD_SDIO_CLK);

    gpio_mode_set(SD_SDIO_CMD_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SD_SDIO_CMD);
    gpio_output_options_set(SD_SDIO_CMD_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SD_SDIO_CMD);
    gpio_af_set(SD_SDIO_CMD_PORT, GPIO_AF_12, SD_SDIO_CMD);

#endif

    // @TODO confirm dip switch 1 is not longer JTAG NJTRST
    // Switch to SWD debug port (disable JTAG) to release PB4 as GPIO
    //gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);   

    // DIP switches
    gpio_mode_set(DIP_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, DIPSW1_PIN | DIPSW2_PIN | DIPSW3_PIN);


    // LED pins
    gpio_bit_set(LED_PORT, LED_PINS);
    gpio_mode_set(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PINS);
    gpio_output_options_set(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, LED_PINS);
 
    // SWO trace pin on PB3
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_3);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, GPIO_PIN_3);
    gpio_af_set(GPIOB, GPIO_AF_0, GPIO_PIN_3);   
}

void platform_late_init()
{
    logmsg("Platform: ", g_platform_name);
    logmsg("FW Version: ", g_log_firmwareversion);
    
    if (gpio_input_bit_get(DIP_PORT, DIPSW3_PIN))
    {
        logmsg("DIPSW3 is ON: Enabling SCSI termination");
        gpio_bit_reset(SCSI_TERM_EN_PORT, SCSI_TERM_EN_PIN);
    }
    else
    {
        logmsg("DIPSW3 is OFF: SCSI termination disabled");
    }

    if (gpio_input_bit_get(DIP_PORT, DIPSW2_PIN))
    {
        logmsg("DIPSW2 is ON: enabling debug messages");
        g_log_debug = true;
    }
    else
    {
        g_log_debug = false;
    }

    if (gpio_input_bit_get(DIP_PORT, DIPSW1_PIN))
    {
        logmsg("DIPSW1 is ON: enabling Apple quirks by default");
        g_enable_apple_quirks = true;
    }
    usb_hs_init();
    greenpak_load_firmware();
}

void platform_post_sd_card_init() {}

void platform_disable_led(void)
{   
    gpio_mode_set(LED_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, LED_PINS);
    logmsg("Disabling status LED");
}

/*****************************************/
/* Debug logging and watchdog            */
/*****************************************/

// Send log data to USB UART if USB is connected.
// Data is retrieved from the shared log ring buffer and
// this function sends as much as fits in USB CDC buffer.

// \todo add serial logging for the F4
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

extern SdFs SD;

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
}

void platform_reset_mcu()
{
    // reset in 2 sec ( 1 / (32KHz / 32) * 2000 == 2sec)
    fwdgt_config(2000, FWDGT_PSC_DIV32);
    fwdgt_enable();

}

// Poll function that is called every few milliseconds.
// Can be left empty or used for platform-specific processing.
void platform_poll()
{
    // adc_poll();
    usb_log_poll();
}

uint8_t platform_get_buttons()
{
    return 0;
}


/***********************/
/* Flash reprogramming */
/***********************/
#define SECTOR_NUMBER_TO_ID_ERROR 0xFFFFFFFF

static uint32_t sector_number_to_id(uint32_t sector_number)
{
    if(11 >= sector_number){
        return CTL_SN(sector_number);
    }else if(23 >= sector_number){
        return CTL_SN(sector_number + 4);
    }else if(27 >= sector_number){
        return CTL_SN(sector_number - 12);
    }
    return SECTOR_NUMBER_TO_ID_ERROR;
}

static bool erase_flash_sector(uint32_t sector)
{
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
    uint32_t sector_id = sector_number_to_id(sector);
    if (sector_id == SECTOR_NUMBER_TO_ID_ERROR)
    {
        logmsg("Sector ", (int) sector, " does not exist");
        return false;
    }

    if (FMC_READY != fmc_sector_erase(sector_id))
    {
        logmsg("Failed flash failed to erase sector, ", (int) sector);
        LED_OFF();
        return false;
    }
    fmc_lock();
    return true;
}

static bool write_flash(uint32_t offset, uint32_t length, uint8_t buffer[PLATFORM_FLASH_WRITE_BUFFER_SIZE])
{
        
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
    fmc_state_enum status;
    uint32_t *buf32 = (uint32_t*)buffer;
    uint32_t memory_address = FLASH_BASE + offset;
    uint32_t num_words = length / 4;
    if (length % 4 == 0)
    {
        for (int i = 0; i < num_words; i++)
        {
            status = fmc_word_program(memory_address, buf32[i]);
            if (status != FMC_READY)
            {
                logmsg("Flash write failed at address: ", memory_address, " with code ", (int)status);
                return false;
            }
            memory_address += 4;   
        }
    }
    else
    {
       logmsg("Firmware size expected to be word (4byte) aligned");
    }

    fmc_lock();
    memory_address = FLASH_BASE + offset;
    for (int i = 0; i < num_words; i++)
    {
        uint32_t expected = buf32[i];
        uint32_t actual = *(volatile uint32_t*)(memory_address);
        if (actual != expected)
        {
            logmsg("Flash word verify failed memory address ", memory_address, " got ", actual, " expected ", expected);
            return false;
        }
        memory_address += 4;
    }

    return true;
}

// the size of the main code without the bootloader
static uint32_t firmware_size(FsFile &file)
{
    uint32_t fwsize = file.size();
    if (fwsize <=  PLATFORM_BOOTLOADER_SIZE )
    {
        logmsg("Firmware file size too small: ", fwsize, " bootloader fits in the first : ", PLATFORM_BOOTLOADER_SIZE, " bytes");
        return false;
    }
    return fwsize - PLATFORM_BOOTLOADER_SIZE;
}

bool platform_firmware_erase(FsFile &file)
{
    uint32_t bootloader_sector_index = 0;
    uint32_t bootloader_sector_byte_count = 0;
    const uint32_t map_length = sizeof(platform_flash_sector_map)/sizeof(platform_flash_sector_map[0]);
    // Find at which sector the bootloader ends so it isn't overwritten
    for(;;)
    {
        if (bootloader_sector_index < map_length)
        {
            bootloader_sector_byte_count += platform_flash_sector_map[bootloader_sector_index];
            if (bootloader_sector_byte_count < PLATFORM_BOOTLOADER_SIZE)
            {
                bootloader_sector_index++;
            }
            else
            {
                break;
            }    
        }
        else
        {
            logmsg("Bootloader does not fit in flash");
            return false;
        }
                
    }
    
    // find the last sector the mainline firmware ends
    uint32_t fwsize = firmware_size(file);
    uint32_t firmware_sector_start = bootloader_sector_index + 1;
    uint32_t last_sector_index = firmware_sector_start;
    uint32_t last_sector_byte_count = 0;
    for(;;)
    {
        if (last_sector_index < map_length)
        {
            last_sector_byte_count += platform_flash_sector_map[last_sector_index];
            if (fwsize > last_sector_byte_count)
            {
                last_sector_index++;
            }
            else
            {
                break;
            }
        }
        else
        {
            logmsg("Firmware too large: ", (int) fwsize, 
                    " space left after the bootloader ",  last_sector_byte_count,
                    " total flash size ", (int)PLATFORM_FLASH_TOTAL_SIZE);
            return false;
        }
    }

    // Erase the sectors the mainline firmware will be written to
    for (int i = firmware_sector_start; i <= last_sector_index; i++)
    {
        if (i % 2 == 0)
        {
            LED_ON();
        }
        else
        {
            LED_OFF();
        }

        if (!erase_flash_sector(i))
        {
            logmsg("Flash failed to erase sector ", i);
            return false;
        }
        
    }
    LED_OFF();
    return true;
}

bool platform_firmware_program(FsFile &file)
{
    // write the mainline firmware to flash
    int32_t bytes_read = 0;
    uint32_t address_offset = PLATFORM_BOOTLOADER_SIZE;
    
    // Make sure the buffer is aligned to word boundary
    static uint32_t buffer32[PLATFORM_FLASH_WRITE_BUFFER_SIZE / 4];
    uint8_t *buffer = (uint8_t*)buffer32;

    if (!file.seek(PLATFORM_BOOTLOADER_SIZE))
    {
        logmsg("Seek failed");
        return false;
    }

    dbgmsg("Writing flash at firmware offset ", address_offset, " data ", bytearray(buffer, 4));

    for(;;)
    {
        if ((address_offset - PLATFORM_BOOTLOADER_SIZE) / PLATFORM_FLASH_WRITE_BUFFER_SIZE % 2)
        {
            LED_ON();
        }
        else
        {
            LED_OFF();
        }
        
        bytes_read = file.read(buffer, PLATFORM_FLASH_WRITE_BUFFER_SIZE);
        if ( bytes_read < 0)
        {
            logmsg("Firmware file read failed, error code ", (int) bytes_read);
            return false;
        }
        if (!write_flash(address_offset, bytes_read, buffer))
        {
            logmsg("Failed to write flash at offset: ", address_offset, " bytes read: ",(int) bytes_read);
            return false;
        }
        
        // check the mainline firmware is valid
        if (address_offset == PLATFORM_BOOTLOADER_SIZE)
        {
            if (buffer[3] != 0x20 || buffer[7] != 0x08)
            {
                logmsg("Invalid firmware file, starts with: ", bytearray(buffer, 16));
                return false;
            }

        }

        if (bytes_read < PLATFORM_FLASH_WRITE_BUFFER_SIZE)
        {
            break;
        }
        address_offset += bytes_read;
        
    }
    LED_OFF();
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