#include "AzulSCSI_platform.h"
#include "gd32f20x_spi.h"
#include <SdFat.h>

extern "C" {

static volatile uint32_t g_millisecond_counter;

unsigned long millis()
{
    return g_millisecond_counter;
}

void delay(unsigned long ms)
{
    uint32_t start = g_millisecond_counter;
    while ((uint32_t)(g_millisecond_counter - start) < ms);
}

void SysTick_Handler(void)
{
    g_millisecond_counter++;
}

// Writes log data to the PB3 SWO pin
void azplatform_log(const char *s)
{
    while (*s)
    {
        // Write to SWO pin
        while (ITM->PORT[0].u32 == 0);
        ITM->PORT[0].u8 = *s++;
    }
}

// Initialize SPI and GPIO configuration
// Clock has already been initialized by system_gd32f20x.c
void azplatform_init()
{
    SystemCoreClockUpdate();

    // Enable SysTick to drive millis()
    SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 0x00U);

    // Enable debug output on SWO pin
    DBG_CTL |= DBG_CTL_TRACE_IOEN;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    TPI->ACPR = SystemCoreClock / 2000000 - 1; // 2 Mbps baudrate for SWO
    TPI->SPPR = 2;
    TPI->FFCR = 0x100; // TPIU packet framing disabled
    // DWT->CTRL = (1 << DWT_CTRL_CYCTAP_Pos)
    //             | (15 << DWT_CTRL_POSTPRESET_Pos)
    //             | (1 << DWT_CTRL_PCSAMPLENA_Pos)
    //             | (3 << DWT_CTRL_SYNCTAP_Pos)
    //             | (1 << DWT_CTRL_CYCCNTENA_Pos);
    ITM->LAR = 0xC5ACCE55;
    ITM->TCR = (1 << ITM_TCR_DWTENA_Pos)
                | (1 << ITM_TCR_SYNCENA_Pos)
                | (1 << ITM_TCR_ITMENA_Pos);
    ITM->TER = 0xFFFFFFFF; // Enable all stimulus ports

    // Enable needed clocks for GPIO
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    
    // Switch to SWD debug port (disable JTAG) to release PB4 as GPIO
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);    

    // SCSI pins.
    // Initialize open drain outputs to high.
    gpio_bit_set(SCSI_OUT_PORT, SCSI_OUT_MASK);
    gpio_init(SCSI_OUT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_MASK);
    gpio_init(SCSI_IN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_IN_MASK);
    gpio_init(SCSI_ATN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_ATN_PIN);
    gpio_init(SCSI_BSY_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_BSY_PIN);
    gpio_init(SCSI_SEL_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_SEL_PIN);
    gpio_init(SCSI_ACK_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_ACK_PIN);
    gpio_init(SCSI_RST_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_RST_PIN);

    // Terminator enable
    gpio_bit_set(SCSI_TERM_EN_PORT, SCSI_TERM_EN_PIN);
    gpio_init(SCSI_TERM_EN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, SCSI_TERM_EN_PIN);

    // SD card pins
    gpio_init(SD_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SD_CS_PIN);
    gpio_init(SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_CLK_PIN);
    gpio_init(SD_PORT, GPIO_MODE_IPU, 0, SD_MISO_PIN);
    gpio_init(SD_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SD_MOSI_PIN);

    // DIP switches
    gpio_init(DIP_PORT, GPIO_MODE_IPD, 0, DIPSW1_PIN | DIPSW2_PIN | DIPSW3_PIN);

    // LED pins
    gpio_bit_set(LED_PORT, LED_PINS);
    gpio_init(LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, LED_PINS);

    // SWO trace pin on PB3
    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);

    azplatform_log("GPIO init complete\n");    
}

static void (*g_rst_callback)();

void azplatform_set_rst_callback(void (*callback)())
{
    g_rst_callback = callback;
    gpio_exti_source_select(SCSI_RST_EXTI_SOURCE_PORT, SCSI_RST_EXTI_SOURCE_PIN);
    exti_init(SCSI_RST_EXTI, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    NVIC_SetPriority(SCSI_RST_IRQn, 0x00U);
}

void SCSI_RST_IRQ (void)
{
    if (exti_interrupt_flag_get(SCSI_RST_EXTI))
    {
        exti_interrupt_flag_clear(SCSI_RST_EXTI);
        if (g_rst_callback)
        {
            g_rst_callback();
        }
    }
}

/*****************************************/
/* Crash handlers                        */
/*****************************************/

void HardFault_Handler(void)
{
    while (1);
}

void MemManage_Handler(void)
{
    while (1);
}

void BusFault_Handler(void)
{
    while (1);
}

void UsageFault_Handler(void)
{
    while (1);
}

} /* extern "C" */

/*****************************************/
/* Driver for GD32 SPI for SdFat library */
/*****************************************/

#define SD_SPI SPI0
class GD32SPIDriver : public SdSpiBaseClass
{
public:
    void begin(SdSpiConfig config) {
        rcu_periph_clock_enable(RCU_SPI0);
    }
        
    void activate() {
        spi_parameter_struct config = {
            SPI_MASTER,
            SPI_TRANSMODE_FULLDUPLEX,
            SPI_FRAMESIZE_8BIT,
            SPI_NSS_SOFT,
            SPI_ENDIAN_MSB,
            SPI_CK_PL_LOW_PH_1EDGE,
            SPI_PSC_256
        };

        // Select closest available divider based on system frequency
        int divider = SystemCoreClock / m_sckfreq;
        if (divider <= 2)
            config.prescale = SPI_PSC_2;
        else if (divider <= 4)
            config.prescale = SPI_PSC_4;
        else if (divider <= 8)
            config.prescale = SPI_PSC_8;
        else if (divider <= 16)
            config.prescale = SPI_PSC_16;
        else if (divider <= 32)
            config.prescale = SPI_PSC_32;
        else if (divider <= 64)
            config.prescale = SPI_PSC_64;
        else if (divider <= 128)
            config.prescale = SPI_PSC_128;
        else
            config.prescale = SPI_PSC_256;

        spi_init(SD_SPI, &config);
        spi_enable(SD_SPI);
    }
    
    void deactivate() {
        spi_disable(SD_SPI);
    }

    void wait_idle() {
        while (!(SPI_STAT(SD_SPI) & SPI_STAT_TBE));
        while (SPI_STAT(SD_SPI) & SPI_STAT_TRANS);
    }

    uint8_t receive() {
        // Wait for idle and clear RX buffer
        wait_idle();
        (void)SPI_DATA(SD_SPI);

        // Send dummy byte and wait for receive
        SPI_DATA(SD_SPI) = 0xFF;
        while (!(SPI_STAT(SD_SPI) & SPI_STAT_RBNE));
        return SPI_DATA(SD_SPI);
    }

    uint8_t receive(uint8_t* buf, size_t count) {
        // Wait for idle and clear RX buffer
        wait_idle();
        (void)SPI_DATA(SD_SPI);

        for (size_t i = 0; i < count; i++)
        {
            while (!(SPI_STAT(SD_SPI) & SPI_STAT_TBE));
            SPI_DATA(SD_SPI) = 0xFF;
            while (!(SPI_STAT(SD_SPI) & SPI_STAT_RBNE));
            buf[i] = SPI_DATA(SD_SPI);
        }

        return 0;
    }

    void send(uint8_t data) {
        SPI_DATA(SD_SPI) = data;
        wait_idle();
    }

    void send(const uint8_t* buf, size_t count) {
        for (size_t i = 0; i < count; i++) {
            while (!(SPI_STAT(SD_SPI) & SPI_STAT_TBE));
            SPI_DATA(SD_SPI) = buf[i];
        }
        wait_idle();
    }

    void setSckSpeed(uint32_t maxSck) {
        m_sckfreq = maxSck;
    }

private:
    uint32_t m_sckfreq;
};

void sdCsInit(SdCsPin_t pin)
{
}

void sdCsWrite(SdCsPin_t pin, bool level)
{
    if (level)
        GPIO_BOP(SD_PORT) = SD_CS_PIN;
    else
        GPIO_BC(SD_PORT) = SD_CS_PIN;
}

GD32SPIDriver g_sd_spi_port;
SdSpiConfig g_sd_spi_config(0, DEDICATED_SPI, SD_SCK_MHZ(25), &g_sd_spi_port);

/**********************************************/
/* Mapping from data bytes to GPIO BOP values */
/**********************************************/

#define PARITY(n) ((1 ^ (n) ^ ((n)>>1) ^ ((n)>>2) ^ ((n)>>3) ^ ((n)>>4) ^ ((n)>>5) ^ ((n)>>6) ^ ((n)>>7)) & 1)
#define X(n) (\
    ((n & 0x01) ? SCSI_OUT_DB0 : (SCSI_OUT_DB0 << 16)) | \
    ((n & 0x02) ? SCSI_OUT_DB1 : (SCSI_OUT_DB1 << 16)) | \
    ((n & 0x04) ? SCSI_OUT_DB2 : (SCSI_OUT_DB2 << 16)) | \
    ((n & 0x08) ? SCSI_OUT_DB3 : (SCSI_OUT_DB3 << 16)) | \
    ((n & 0x10) ? SCSI_OUT_DB4 : (SCSI_OUT_DB4 << 16)) | \
    ((n & 0x20) ? SCSI_OUT_DB5 : (SCSI_OUT_DB5 << 16)) | \
    ((n & 0x40) ? SCSI_OUT_DB6 : (SCSI_OUT_DB6 << 16)) | \
    ((n & 0x80) ? SCSI_OUT_DB7 : (SCSI_OUT_DB7 << 16)) | \
    (PARITY(n)  ? SCSI_OUT_DBP : (SCSI_OUT_DBP << 16)) \
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