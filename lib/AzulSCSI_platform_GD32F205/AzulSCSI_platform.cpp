#include "AzulSCSI_platform.h"
#include "gd32f20x_spi.h"
#include "gd32f20x_dma.h"
#include "AzulSCSI_log.h"
#include "AzulSCSI_config.h"
#include <SdFat.h>

extern "C" {

const char *g_azplatform_name = "GD32F205 AzulSCSI v1.x";

static volatile uint32_t g_millisecond_counter;
static uint32_t g_ns_to_cycles; // Q0.32 fixed point format

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
    g_millisecond_counter = 0;
    SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 0x00U);

    // Enable DWT counter to drive delay_ns()
    g_ns_to_cycles = ((uint64_t)SystemCoreClock << 32) / 1000000000;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

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

    if (gpio_input_bit_get(DIP_PORT, DIPSW3_PIN))
    {
        azlog("DIPSW3 is ON: Enabling SCSI termination");
        gpio_bit_reset(SCSI_TERM_EN_PORT, SCSI_TERM_EN_PIN);
    }
    else
    {
        azlog("DIPSW3 is OFF: SCSI termination disabled");
    }

    if (gpio_input_bit_get(DIP_PORT, DIPSW2_PIN))
    {
        azlog("DIPSW2 is ON: enabling debug messages");
        g_azlog_debug = true;
    }
    else
    {
        g_azlog_debug = false;
    }
}

static void (*g_rst_callback)();

void azplatform_set_rst_callback(void (*callback)())
{
    g_rst_callback = callback;
    gpio_exti_source_select(SCSI_RST_EXTI_SOURCE_PORT, SCSI_RST_EXTI_SOURCE_PIN);
    exti_init(SCSI_RST_EXTI, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    NVIC_SetPriority(SCSI_RST_IRQn, 0x00U);
    NVIC_EnableIRQ(SCSI_RST_IRQn);
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

extern SdFs SD;

void azplatform_emergency_log_save()
{
    FsFile crashfile = SD.open(CRASHFILE, O_WRONLY | O_CREAT | O_TRUNC);

    if (!crashfile.isOpen())
    {
        // Try to reinitialize
        int max_retry = 10;
        while (max_retry-- > 0 && !SD.begin(SD_CONFIG));

        crashfile = SD.open(CRASHFILE, O_WRONLY | O_CREAT | O_TRUNC);
    }

    uint32_t startpos = 0;
    crashfile.write(azlog_get_buffer(&startpos));
    crashfile.write(azlog_get_buffer(&startpos));
    crashfile.flush();
    crashfile.close();
}

__attribute__((noinline))
void show_hardfault(uint32_t *sp)
{
    uint32_t pc = sp[6];
    uint32_t lr = sp[5];
    uint32_t cfsr = SCB->CFSR;
    
    azlog("--------------");
    azlog("CRASH!");
    azlog("Platform: ", g_azplatform_name);
    azlog("FW Version: ", g_azlog_firmwareversion);
    azlog("CFSR: ", cfsr);
    azlog("PC: ", pc);
    azlog("LR: ", lr);
    azlog("R0: ", sp[0]);
    azlog("R1: ", sp[1]);
    azlog("R2: ", sp[2]);
    azlog("R3: ", sp[3]);

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
            
            int delay = (pc & (1 << i)) ? (3 * base_delay) : base_delay;
            LED_ON();
            for (int j = 0; j < delay; j++) delay_ns(100000);
            LED_OFF();
        }

        for (int j = 0; j < base_delay * 10; j++) delay_ns(100000);
    }
}

__attribute__((naked))
void HardFault_Handler(void)
{
    // Copies stack pointer into first argument
    asm("mrs r0, msp\n"
        "b show_hardfault": : : "r0");
}

__attribute__((naked))
void MemManage_Handler(void)
{
    asm("mrs r0, msp\n"
        "b show_hardfault": : : "r0");
}

__attribute__((naked))
void BusFault_Handler(void)
{
    asm("mrs r0, msp\n"
        "b show_hardfault": : : "r0");
}

__attribute__((naked))
void UsageFault_Handler(void)
{
    asm("mrs r0, msp\n"
        "b show_hardfault": : : "r0");
}

} /* extern "C" */

/*****************************************/
/* Driver for GD32 SPI for SdFat library */
/*****************************************/

extern volatile bool g_busreset;

#define SCSI_WAIT_ACTIVE(pin) \
  if (!SCSI_IN(pin)) { \
    if (!SCSI_IN(pin)) { \
      while(!SCSI_IN(pin) && !g_busreset); \
    } \
  }

#define SCSI_WAIT_INACTIVE(pin) \
  if (SCSI_IN(pin)) { \
    if (SCSI_IN(pin)) { \
      while(SCSI_IN(pin) && !g_busreset); \
    } \
  }

// Optimized ASM blocks for the SCSI communication subroutine

// Take 8 bits from d and format them for writing
// d is name of data operand, b is bit offset, x is unique label
#define ASM_LOAD_DATA(d, b, x) \
"    load_data1_" x "_%=: \n" \
"        ubfx    %[tmp1], %[" d "], #" b ", #8 \n" \
"        ldr     %[tmp1], [%[byte_lookup], %[tmp1], lsl #2] \n"

// Write data to SCSI port and set REQ high
#define ASM_SEND_DATA(x) \
"    send_data" x "_%=: \n" \
"        str     %[tmp1], [%[out_port_bop]] \n"

// Wait for ACK to be high, set REQ low, wait ACK low
#define ASM_HANDSHAKE(x) \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        str     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        str     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        str     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"    wait_ack_inactive" x "_%=: \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        str     %[tmp2], [%[req_pin_bb]] \n" \
"        cbnz    %[tmp2], req_is_low_now" x "_%= \n" \
"        b.n     wait_ack_inactive" x "_%= \n" \
"    req_is_low_now" x "_%=: \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"    wait_ack_active" x "_%=: \n" \
"        ldr     %[tmp2], [%[ack_pin_bb]] \n" \
"        cbz     %[tmp2], over_ack_active" x "_%= \n" \
"        b.n     wait_ack_active" x "_%= \n" \
"    over_ack_active" x "_%=: \n" \

// Send bytes to SCSI bus using the asynchronous handshake mechanism
// Takes 4 bytes at a time for sending from buf.
// Returns the next buffer pointer.
static inline uint32_t *scsi_send_words_async(uint32_t *buf, uint32_t num_words)
{
    volatile uint32_t *out_port_bop = (volatile uint32_t*)&GPIO_BOP(SCSI_OUT_PORT);
    const uint32_t *byte_lookup = g_scsi_out_byte_to_bop;
    uint32_t ack_pin_bb = PERIPH_BB_BASE + (((uint32_t)&GPIO_ISTAT(SCSI_ACK_PORT)) - APB1_BUS_BASE) * 32 + 12 * 4;
    uint32_t req_pin_bb = PERIPH_BB_BASE + (((uint32_t)out_port_bop) - APB1_BUS_BASE) * 32 + (9 + 16) * 4;
    register uint32_t tmp1 = 0;
    register uint32_t tmp2 = 0;
    register uint32_t data = 0;

    asm volatile (
    "   ldr      %[data], [%[buf]], #4 \n" \
        ASM_LOAD_DATA("data", "0", "first")

    "inner_loop_%=: \n" \
        ASM_SEND_DATA("0")
        ASM_LOAD_DATA("data", "8", "8")
        ASM_HANDSHAKE("0")
        
        ASM_SEND_DATA("8")
        ASM_LOAD_DATA("data", "16", "16")
        ASM_HANDSHAKE("8")

        ASM_SEND_DATA("16")
        ASM_LOAD_DATA("data", "24", "24")
        ASM_HANDSHAKE("16")

        ASM_SEND_DATA("24")
    "   ldr      %[data], [%[buf]], #4 \n" \
        ASM_LOAD_DATA("data", "0", "0")
        ASM_HANDSHAKE("24")

    "   subs     %[num_words], %[num_words], #1 \n" \
    "   bne     inner_loop_%= \n"
    : /* Output */ [tmp1] "+l" (tmp1), [tmp2] "+l" (tmp2), [data] "+r" (data),
                   [buf] "+r" (buf), [num_words] "+r" (num_words)
    : /* Input */ [ack_pin_bb] "r" (ack_pin_bb),
                  [req_pin_bb] "r" (req_pin_bb),
                  [out_port_bop] "r"(out_port_bop),
                  [byte_lookup] "r" (byte_lookup)
    : /* Clobber */ );

    return buf - 1;
}

class GD32SPIDriver : public SdSpiBaseClass
{
public:
    void begin(SdSpiConfig config) {
        rcu_periph_clock_enable(RCU_SPI0);
        rcu_periph_clock_enable(RCU_DMA0);

        dma_parameter_struct rx_dma_config =
        {
            .periph_addr = (uint32_t)&SPI_DATA(SD_SPI),
            .periph_width = DMA_PERIPHERAL_WIDTH_8BIT,
            .memory_addr = 0, // Set before transfer
            .memory_width = DMA_MEMORY_WIDTH_8BIT,
            .number = 0, // Set before transfer
            .priority = DMA_PRIORITY_ULTRA_HIGH,
            .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
            .memory_inc = DMA_MEMORY_INCREASE_ENABLE,
            .direction = DMA_PERIPHERAL_TO_MEMORY
        };
        dma_init(DMA0, SD_SPI_RX_DMA_CHANNEL, &rx_dma_config);

        dma_parameter_struct tx_dma_config =
        {
            .periph_addr = (uint32_t)&SPI_DATA(SD_SPI),
            .periph_width = DMA_PERIPHERAL_WIDTH_8BIT,
            .memory_addr = 0, // Set before transfer
            .memory_width = DMA_MEMORY_WIDTH_8BIT,
            .number = 0, // Set before transfer
            .priority = DMA_PRIORITY_HIGH,
            .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
            .memory_inc = DMA_MEMORY_INCREASE_ENABLE,
            .direction = DMA_MEMORY_TO_PERIPHERAL
        };
        dma_init(DMA0, SD_SPI_TX_DMA_CHANNEL, &tx_dma_config);
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
        int divider = (SystemCoreClock + m_sckfreq / 2) / m_sckfreq;
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

        if (buf == m_stream_buffer + m_stream_status)
        {
            // Stream data directly to SCSI bus
            return stream_receive(buf, count);
        }
        
        // Stream to memory
        
        // Use DMA to stream dummy TX data and store RX data
        uint8_t tx_data = 0xFF;
        DMA_INTC(DMA0) = DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_RX_DMA_CHANNEL);
        DMA_INTC(DMA0) = DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_TX_DMA_CHANNEL);
        DMA_CHMADDR(DMA0, SD_SPI_RX_DMA_CHANNEL) = (uint32_t)buf;
        DMA_CHMADDR(DMA0, SD_SPI_TX_DMA_CHANNEL) = (uint32_t)&tx_data;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) &= ~DMA_CHXCTL_MNAGA; // No memory increment for TX
        DMA_CHCNT(DMA0, SD_SPI_RX_DMA_CHANNEL) = count;
        DMA_CHCNT(DMA0, SD_SPI_TX_DMA_CHANNEL) = count;
        DMA_CHCTL(DMA0, SD_SPI_RX_DMA_CHANNEL) |= DMA_CHXCTL_CHEN;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) |= DMA_CHXCTL_CHEN;

        SPI_CTL1(SD_SPI) |= SPI_CTL1_DMAREN | SPI_CTL1_DMATEN;
        
        uint32_t start = millis();
        while (!(DMA_INTF(DMA0) & DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_RX_DMA_CHANNEL)))
        {
            if (millis() - start > 500)
            {
                azlog("ERROR: SPI DMA receive of ", (int)count, " bytes timeouted");
                return 1;
            }
        }

        if (DMA_INTF(DMA0) & DMA_FLAG_ADD(DMA_FLAG_ERR, SD_SPI_RX_DMA_CHANNEL))
        {
            azlog("ERROR: SPI DMA receive set DMA_FLAG_ERR");
        }

        SPI_CTL1(SD_SPI) &= ~(SPI_CTL1_DMAREN | SPI_CTL1_DMATEN);
        DMA_CHCTL(DMA0, SD_SPI_RX_DMA_CHANNEL) &= ~DMA_CHXCTL_CHEN;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) &= ~DMA_CHXCTL_CHEN;

        return 0;
    }

    // Stream data directly to SCSI bus
    uint8_t stream_receive(uint8_t *buf, size_t count)
    {
        uint8_t tx_data = 0xFF;
        DMA_INTC(DMA0) = DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_RX_DMA_CHANNEL);
        DMA_INTC(DMA0) = DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_TX_DMA_CHANNEL);
        DMA_CHMADDR(DMA0, SD_SPI_RX_DMA_CHANNEL) = (uint32_t)buf;
        DMA_CHMADDR(DMA0, SD_SPI_TX_DMA_CHANNEL) = (uint32_t)&tx_data;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) &= ~DMA_CHXCTL_MNAGA; // No memory increment for TX
        DMA_CHCNT(DMA0, SD_SPI_RX_DMA_CHANNEL) = count;
        DMA_CHCNT(DMA0, SD_SPI_TX_DMA_CHANNEL) = count;
        DMA_CHCTL(DMA0, SD_SPI_RX_DMA_CHANNEL) |= DMA_CHXCTL_CHEN;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) |= DMA_CHXCTL_CHEN;

        SPI_CTL1(SD_SPI) |= SPI_CTL1_DMAREN | SPI_CTL1_DMATEN;
        
        // DMA transfer is now running, we can start sending received bytes to SCSI
        uint32_t *word_ptr = (uint32_t*)buf;
        uint32_t *end_ptr = word_ptr + (count / 4);
        while (word_ptr < end_ptr)
        {
            uint32_t words_available = (count - DMA_CHCNT(DMA0, SD_SPI_RX_DMA_CHANNEL)) / 4;
            if (words_available > 0)
            {
                if (word_ptr + words_available > end_ptr)
                {
                    words_available = end_ptr - word_ptr;
                }

                word_ptr = scsi_send_words_async(word_ptr, words_available);
            }
        }
        
        SCSI_RELEASE_DATA_REQ();

        if (DMA_INTF(DMA0) & DMA_FLAG_ADD(DMA_FLAG_ERR, SD_SPI_RX_DMA_CHANNEL))
        {
            azlog("ERROR: SPI DMA receive set DMA_FLAG_ERR");
        }

        SPI_CTL1(SD_SPI) &= ~(SPI_CTL1_DMAREN | SPI_CTL1_DMATEN);
        DMA_CHCTL(DMA0, SD_SPI_RX_DMA_CHANNEL) &= ~DMA_CHXCTL_CHEN;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) &= ~DMA_CHXCTL_CHEN;

        m_stream_status += count;
        return 0;
    }

    void send(uint8_t data) {
        SPI_DATA(SD_SPI) = data;
        wait_idle();
    }

    void send(const uint8_t* buf, size_t count) {
        if (buf == m_stream_buffer + m_stream_status)
        {
            stream_send(count);
            return;
        }

        for (size_t i = 0; i < count; i++) {
            while (!(SPI_STAT(SD_SPI) & SPI_STAT_TBE));
            SPI_DATA(SD_SPI) = buf[i];
        }
        wait_idle();
    }

    // Stream data directly from SCSI bus
    void stream_send(size_t count)
    {
        for (size_t i = 0; i < count; i++) {
            SCSI_OUT(REQ, 1);
            SCSI_WAIT_ACTIVE(ACK);
            delay_100ns(); // ACK.Fall to DB output delay 100ns(MAX)  (DTC-510B)
            uint8_t data = SCSI_IN_DATA();
            SCSI_OUT(REQ, 0);

            while (!(SPI_STAT(SD_SPI) & SPI_STAT_TBE));
            SPI_DATA(SD_SPI) = data;

            SCSI_WAIT_INACTIVE(ACK);
        }
        wait_idle();

        m_stream_status += count;
    }

    void setSckSpeed(uint32_t maxSck) {
        m_sckfreq = maxSck;
    }

    void prepare_stream(uint8_t *buffer)
    {
        m_stream_buffer = buffer;
        m_stream_status = 0;
    }

    size_t finish_stream()
    {
        size_t result = m_stream_status;
        m_stream_status = 0;
        m_stream_buffer = NULL;
        return result;
    }

private:
    uint32_t m_sckfreq;
    uint8_t *m_stream_buffer;
    size_t m_stream_status; // Number of bytes transferred so far
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
SdSpiConfig g_sd_spi_config(0, DEDICATED_SPI, SD_SCK_MHZ(30), &g_sd_spi_port);

void azplatform_prepare_stream(uint8_t *buffer)
{
    g_sd_spi_port.prepare_stream(buffer);
}

size_t azplatform_finish_stream()
{
    return g_sd_spi_port.finish_stream();
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