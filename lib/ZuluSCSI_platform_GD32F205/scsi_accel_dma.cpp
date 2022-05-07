#include "scsi_accel_dma.h"
#include <AzulSCSI_log.h>
#include <gd32f20x_timer.h>
#include <gd32f20x_rcu.h>
#include <assert.h>
#include <string.h>

#ifndef SCSI_ACCEL_DMA_AVAILABLE

void scsi_accel_timer_dma_init() {}
void scsi_accel_greenpak_dma_init() {}
void scsi_accel_dma_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag) {}
void scsi_accel_dma_stopWrite() {}
void scsi_accel_dma_finishWrite(volatile int *resetFlag) {}
bool scsi_accel_dma_isWriteFinished(const uint8_t* data) { return true; }


#else

static void greenpak_refill_dmabuf();
static void greenpak_start_dma();
static void greenpak_stop_dma();

enum greenpak_state_t { GREENPAK_IO1_LOW = 0, GREENPAK_IO1_HIGH, GREENPAK_STOP};

#define DMA_BUF_SIZE 256
#define DMA_BUF_MASK (DMA_BUF_SIZE - 1)
static struct {
    uint8_t *app_buf; // Buffer provided by application
    uint32_t dma_buf[DMA_BUF_SIZE]; // Buffer of data formatted for GPIO BOP register
    uint32_t dma_idx; // Write index to DMA buffer
    uint32_t dma_fillto; // Point up to which DMA buffer is available for refilling
    uint32_t timer_buf; // Control value for timer SWEVG register
    uint32_t bytes_app; // Bytes available in application buffer
    uint32_t bytes_dma; // Bytes (words) written so far to DMA buffer
    uint32_t scheduled_dma; // Bytes (words) that DMA data count was last set to
    greenpak_state_t greenpak_state; // Toggle signal state for greenpak

    uint8_t *next_app_buf; // Next buffer from application after current one finishes
    uint32_t next_app_bytes; // Bytes in next buffer
} g_scsi_dma;

enum scsidma_state_t { SCSIDMA_IDLE = 0, SCSIDMA_WRITE };
static volatile scsidma_state_t g_scsi_dma_state;
static bool g_scsi_dma_use_greenpak;

void scsi_accel_timer_dma_init()
{
    g_scsi_dma_state = SCSIDMA_IDLE;
    g_scsi_dma_use_greenpak = false;
    rcu_periph_clock_enable(SCSI_TIMER_RCU);
    rcu_periph_clock_enable(SCSI_TIMER_DMA_RCU);

    // DMA Channel A: data copy
    // GPIO DMA copies data from memory buffer to GPIO BOP register.
    // The memory buffer is filled by interrupt routine.
    dma_parameter_struct gpio_dma_config =
    {
        .periph_addr = (uint32_t)&GPIO_BOP(SCSI_OUT_PORT),
        .periph_width = DMA_PERIPHERAL_WIDTH_32BIT,
        .memory_addr = 0, // Filled before transfer
        .memory_width = DMA_MEMORY_WIDTH_32BIT,
        .number = DMA_BUF_SIZE,
        .priority = DMA_PRIORITY_ULTRA_HIGH,
        .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
        .memory_inc = DMA_MEMORY_INCREASE_ENABLE,
        .direction = DMA_MEMORY_TO_PERIPHERAL
    };
    dma_init(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA, &gpio_dma_config);
    dma_circulation_enable(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA);
    NVIC_SetPriority(SCSI_TIMER_DMACHA_IRQn, 1);
    NVIC_EnableIRQ(SCSI_TIMER_DMACHA_IRQn);

    // DMA Channel B: timer update
    // Timer DMA causes update event to restart timer after
    // GPIO DMA operation is done.
    dma_parameter_struct timer_dma_config =
    {
        .periph_addr = (uint32_t)&TIMER_SWEVG(SCSI_TIMER),
        .periph_width = DMA_PERIPHERAL_WIDTH_32BIT,
        .memory_addr = (uint32_t)&g_scsi_dma.timer_buf,
        .memory_width = DMA_MEMORY_WIDTH_32BIT,
        .number = DMA_BUF_SIZE,
        .priority = DMA_PRIORITY_HIGH,
        .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
        .memory_inc = DMA_PERIPH_INCREASE_DISABLE,
        .direction = DMA_MEMORY_TO_PERIPHERAL
    };
    dma_init(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB, &timer_dma_config);
    NVIC_SetPriority(SCSI_TIMER_DMACHB_IRQn, 2);
    NVIC_EnableIRQ(SCSI_TIMER_DMACHB_IRQn);

    g_scsi_dma.timer_buf = TIMER_SWEVG_UPG;

    // Timer is used to toggle the request signal based on external trigger input.
    // OUT_REQ is driven by timer output.
    // 1. On timer update event, REQ is set low.
    // 2. When ACK goes low, timer counts and OUT_REQ is set high.
    //    Simultaneously a DMA request is triggered to write next data to GPIO.
    // 3. When ACK goes high, a DMA request is triggered to cause timer update event.
    //    The DMA request priority is set so that 2. always completes before it.
    TIMER_CTL0(SCSI_TIMER) = 0;
    TIMER_SMCFG(SCSI_TIMER) = TIMER_SLAVE_MODE_EXTERNAL0 | TIMER_SMCFG_TRGSEL_CI0F_ED;
    TIMER_CAR(SCSI_TIMER) = 65535;
    TIMER_PSC(SCSI_TIMER) = 0;
    TIMER_DMAINTEN(SCSI_TIMER) = 0;
    TIMER_CHCTL0(SCSI_TIMER) = 0x6001; // CH0 as input, CH1 as DMA trigger
    TIMER_CHCTL1(SCSI_TIMER) = 0x6074; // CH2 as fast PWM output, CH3 as DMA trigger
    TIMER_CHCTL2(SCSI_TIMER) = TIMER_CHCTL2_CH2NEN;
    TIMER_CCHP(SCSI_TIMER) = TIMER_CCHP_POEN;
    TIMER_CH1CV(SCSI_TIMER) = 1; // Copy data when ACK goes low
    TIMER_CH2CV(SCSI_TIMER) = 1; // REQ is low until ACK goes low
    TIMER_CH3CV(SCSI_TIMER) = 2; // Reset timer after ACK goes high & previous DMA is complete
    gpio_init(SCSI_TIMER_IN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_TIMER_IN_PIN);

    scsi_accel_dma_stopWrite();
}

// Select whether OUT_REQ is connected to timer or GPIO port
static void scsi_dma_gpio_config(bool enable)
{
    if (enable)
    {
        gpio_init(SCSI_OUT_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, SCSI_OUT_REQ);

        if (g_scsi_dma_use_greenpak)
        {
            GPIO_BC(SCSI_OUT_PORT) = GREENPAK_PLD_IO1;
            GPIO_BOP(SCSI_OUT_PORT) = GREENPAK_PLD_IO2;
        }
        else
        {
            gpio_init(SCSI_TIMER_OUT_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SCSI_TIMER_OUT_PIN);
        }
    }
    else
    {
        GPIO_BC(SCSI_OUT_PORT) = GREENPAK_PLD_IO2;
        gpio_init(SCSI_TIMER_OUT_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, SCSI_TIMER_OUT_PIN);
        gpio_init(SCSI_OUT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_REQ);
    }
}

// Convert input bytes into BOP values in the DMA buffer
static void refill_dmabuf()
{
    if (g_scsi_dma_use_greenpak)
    {
        greenpak_refill_dmabuf();
        return;
    }

    // Check how many bytes we have available from the application
    uint32_t count = g_scsi_dma.bytes_app - g_scsi_dma.bytes_dma;
    
    // Check amount of free space in DMA buffer
    uint32_t max = g_scsi_dma.dma_fillto - g_scsi_dma.dma_idx;
    if (count > max) count = max;
    if (count == 0) return;

    uint8_t *src = g_scsi_dma.app_buf + g_scsi_dma.bytes_dma;
    uint32_t *dst = g_scsi_dma.dma_buf;
    uint32_t pos = g_scsi_dma.dma_idx;
    uint32_t end = pos + count;
    g_scsi_dma.dma_idx = end;
    g_scsi_dma.bytes_dma += count;

    while (pos + 4 <= end)
    {
        uint32_t input = *(uint32_t*)src;
        src += 4;

        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop[(input >> 0) & 0xFF];
        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop[(input >> 8) & 0xFF];
        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop[(input >> 16) & 0xFF];
        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop[(input >> 24) & 0xFF];
    }

    while (pos < end)
    {
        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop[*src++];
    }

    if (end < g_scsi_dma.dma_fillto)
    {
        // Partial buffer fill, this will get refilled from interrupt if we
        // get more data. Set next byte to an invalid parity value so that
        // any race conditions will get caught as parity error.
        dst[pos & DMA_BUF_MASK] = g_scsi_out_byte_to_bop[0] ^ SCSI_OUT_DBP;
    }
}

// Start DMA transfer
static void start_dma()
{
    if (g_scsi_dma_use_greenpak)
    {
        greenpak_start_dma();
        return;
    }

    // Disable channels while configuring
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) &= ~DMA_CHXCTL_CHEN;
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) &= ~DMA_CHXCTL_CHEN;
    TIMER_CTL0(SCSI_TIMER) = 0;

    // Set new buffer address and size
    // CHA / Data channel is in circular mode and always has DMA_BUF_SIZE buffer size.
    // CHB / Update channel limits the number of data.
    DMA_CHMADDR(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) = (uint32_t)g_scsi_dma.dma_buf;
    DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) = DMA_BUF_SIZE;
    uint32_t dma_to_schedule = g_scsi_dma.bytes_app - g_scsi_dma.scheduled_dma;
    DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) = dma_to_schedule;
    g_scsi_dma.scheduled_dma += dma_to_schedule;
    
    // Clear pending DMA events
    TIMER_DMAINTEN(SCSI_TIMER) = 0;
    TIMER_DMAINTEN(SCSI_TIMER) = TIMER_DMAINTEN_CH1DEN | TIMER_DMAINTEN_CH3DEN;

    // Clear and enable interrupt
    DMA_INTC(SCSI_TIMER_DMA) = DMA_FLAG_ADD(DMA_FLAG_HTF | DMA_FLAG_FTF | DMA_FLAG_ERR, SCSI_TIMER_DMACHA);
    DMA_INTC(SCSI_TIMER_DMA) = DMA_FLAG_ADD(DMA_FLAG_HTF | DMA_FLAG_FTF | DMA_FLAG_ERR, SCSI_TIMER_DMACHB);
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) |= DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE;
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) |= DMA_CHXCTL_FTFIE;

    // Enable channels
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) |= DMA_CHXCTL_CHEN;
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) |= DMA_CHXCTL_CHEN;

    // Make sure REQ is initially high
    TIMER_CNT(SCSI_TIMER) = 16;
    TIMER_CHCTL1(SCSI_TIMER) = 0x6050;
    TIMER_CHCTL1(SCSI_TIMER) = 0x6074;

    // Enable timer
    TIMER_CTL0(SCSI_TIMER) |= TIMER_CTL0_CEN;

    // Generate first events
    TIMER_SWEVG(SCSI_TIMER) = TIMER_SWEVG_CH1G;
    TIMER_SWEVG(SCSI_TIMER) = TIMER_SWEVG_CH3G;
}

// Stop DMA transfer
static void stop_dma()
{
    greenpak_stop_dma();
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) &= ~DMA_CHXCTL_CHEN;
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) &= ~DMA_CHXCTL_CHEN;
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) &= ~(DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE);
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) &= ~DMA_CHXCTL_FTFIE;
    TIMER_CTL0(SCSI_TIMER) &= ~TIMER_CTL0_CEN;
    g_scsi_dma_state = SCSIDMA_IDLE;
    SCSI_RELEASE_DATA_REQ();
}

static void check_dma_next_buffer()
{
    // Check if we are at the end of the application buffer
    if (g_scsi_dma.next_app_buf && g_scsi_dma.bytes_dma == g_scsi_dma.bytes_app)
    {
        // Switch to next buffer
        assert(g_scsi_dma.scheduled_dma == g_scsi_dma.bytes_app);
        g_scsi_dma.app_buf = g_scsi_dma.next_app_buf;
        g_scsi_dma.bytes_app = g_scsi_dma.next_app_bytes;
        g_scsi_dma.bytes_dma = 0;
        g_scsi_dma.scheduled_dma = 0;
        g_scsi_dma.next_app_buf = 0;
        g_scsi_dma.next_app_bytes = 0;
        refill_dmabuf();
    }
}

// Convert new data from application buffer to DMA buffer
extern "C" void SCSI_TIMER_DMACHA_IRQ()
{
    // azdbg("DMA irq A, counts: ", DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA), " ",
    //             DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB), " ",
    //             TIMER_CNT(SCSI_TIMER));

    uint32_t intf = DMA_INTF(SCSI_TIMER_DMA);
    const uint32_t half_flag = DMA_FLAG_ADD(DMA_FLAG_HTF, SCSI_TIMER_DMACHA);
    const uint32_t full_flag = DMA_FLAG_ADD(DMA_FLAG_FTF, SCSI_TIMER_DMACHA);
    if (intf & half_flag)
    {
        if (intf & full_flag)
        {
            azlog("ERROR: SCSI DMA overrun: ", intf,
               " bytes_app: ", g_scsi_dma.bytes_app,
               " bytes_dma: ", g_scsi_dma.bytes_dma,
               " dma_idx: ", g_scsi_dma.dma_idx,
               " sched_dma: ", g_scsi_dma.scheduled_dma);
            stop_dma();
            return;
        }

        DMA_INTC(SCSI_TIMER_DMA) = DMA_FLAG_ADD(DMA_FLAG_HTF, SCSI_TIMER_DMACHA);
        g_scsi_dma.dma_fillto += DMA_BUF_SIZE / 2;
    }
    else if (intf & full_flag)
    {
        DMA_INTC(SCSI_TIMER_DMA) = DMA_FLAG_ADD(DMA_FLAG_FTF, SCSI_TIMER_DMACHA);
        g_scsi_dma.dma_fillto += DMA_BUF_SIZE / 2;
    }

    // Fill DMA buffer with data from current application buffer
    refill_dmabuf();

    check_dma_next_buffer();
}

// Check if enough data is available to continue DMA transfer
extern "C" void SCSI_TIMER_DMACHB_IRQ()
{
    // azdbg("DMA irq B, counts: ", DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA), " ",
    //             DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB), " ",
    //             TIMER_CNT(SCSI_TIMER));
    uint32_t intf = DMA_INTF(SCSI_TIMER_DMA);
    if (intf & DMA_FLAG_ADD(DMA_FLAG_FTF, SCSI_TIMER_DMACHB))
    {
        DMA_INTC(SCSI_TIMER_DMA) = DMA_FLAG_ADD(DMA_FLAG_FTF, SCSI_TIMER_DMACHB);

        if (g_scsi_dma.bytes_app > g_scsi_dma.scheduled_dma)
        {
            if (g_scsi_dma.dma_idx < g_scsi_dma.dma_fillto)
            {
                // Previous request didn't have a complete buffer worth of data.
                // Refill the buffer and ensure that the first byte of the new data gets
                // written to outputs.
                __disable_irq();
                refill_dmabuf();
                __enable_irq();
            }

            // Verify the first byte of the new data has been written to outputs
            // It may have been updated after the DMA write occurred.
            __disable_irq();
            uint32_t first_data_idx = g_scsi_dma.dma_idx - (g_scsi_dma.bytes_dma - g_scsi_dma.scheduled_dma);
            uint32_t first_data = g_scsi_dma.dma_buf[first_data_idx & DMA_BUF_MASK];
            GPIO_BOP(SCSI_OUT_PORT) = first_data;
            __enable_irq();

            // Update the total number of bytes available for DMA
            uint32_t dma_to_schedule = g_scsi_dma.bytes_app - g_scsi_dma.scheduled_dma;
            DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) &= ~DMA_CHXCTL_CHEN;
            DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) = dma_to_schedule;
            DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) |= DMA_CHXCTL_CHEN;
            g_scsi_dma.scheduled_dma += dma_to_schedule;
        }
        else
        {
            // No more data available
            stop_dma();
        }
    }
}

void scsi_accel_dma_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag)
{
    __disable_irq();
    if (g_scsi_dma_state == SCSIDMA_WRITE)
    {
        if (!g_scsi_dma.next_app_buf && data == g_scsi_dma.app_buf + g_scsi_dma.bytes_app)
        {
            // Combine with currently running request
            g_scsi_dma.bytes_app += count;
            count = 0;
        }
        else if (data == g_scsi_dma.next_app_buf + g_scsi_dma.next_app_bytes)
        {
            // Combine with queued request
            g_scsi_dma.next_app_bytes += count;
            count = 0;
        }
        else if (!g_scsi_dma.next_app_buf)
        {
            // Add as queued request
            g_scsi_dma.next_app_buf = (uint8_t*)data;
            g_scsi_dma.next_app_bytes = count;
            count = 0;
        }
    }
    __enable_irq();

    // Check if the request was combined
    if (count == 0) return;

    if (g_scsi_dma_state != SCSIDMA_IDLE)
    {
        // Wait for previous request to finish
        scsi_accel_dma_finishWrite(resetFlag);
        if (*resetFlag)
        {
            return;
        }
    }

    // azdbg("Starting DMA write of ", (int)count, " bytes");
    scsi_dma_gpio_config(true);
    g_scsi_dma_state = SCSIDMA_WRITE;
    g_scsi_dma.app_buf = (uint8_t*)data;
    g_scsi_dma.dma_idx = 0;
    g_scsi_dma.dma_fillto = DMA_BUF_SIZE;
    g_scsi_dma.bytes_app = count;
    g_scsi_dma.bytes_dma = 0;
    g_scsi_dma.scheduled_dma = 0;
    g_scsi_dma.next_app_buf = NULL;
    g_scsi_dma.next_app_bytes = 0;
    g_scsi_dma.greenpak_state = GREENPAK_IO1_LOW;
    refill_dmabuf();
    start_dma();
}

bool scsi_accel_dma_isWriteFinished(const uint8_t* data)
{
    // Check if everything has completed
    if (g_scsi_dma_state == SCSIDMA_IDLE)
    {
        return true;
    }

    if (!data)
        return false;
    
    // Check if this data item is still in queue.
    __disable_irq();
    bool finished = true;
    if (data >= g_scsi_dma.app_buf + g_scsi_dma.bytes_dma &&
        data < g_scsi_dma.app_buf + g_scsi_dma.bytes_app)
    {
        finished = false; // In current transfer
    }
    else if (data >= g_scsi_dma.next_app_buf &&
             data < g_scsi_dma.next_app_buf + g_scsi_dma.next_app_bytes)
    {
        finished = false; // In queued transfer
    }
    __enable_irq();

    return finished;
}

void scsi_accel_dma_stopWrite()
{
    stop_dma();
    scsi_dma_gpio_config(false);
}

void scsi_accel_dma_finishWrite(volatile int *resetFlag)
{
    uint32_t start = millis();
    while (g_scsi_dma_state != SCSIDMA_IDLE && !*resetFlag)
    {
        if ((uint32_t)(millis() - start) > 5000)
        {
            azlog("scsi_accel_dma_finishWrite() timeout, DMA counts ",
                DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA), " ",
                DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB), " ",
                TIMER_CNT(SCSI_TIMER));
            *resetFlag = 1;
            break;
        }
    }

    scsi_accel_dma_stopWrite();
}

/************************************************/
/* Functions using external GreenPAK logic chip */
/************************************************/

void scsi_accel_greenpak_dma_init()
{
    g_scsi_dma_state = SCSIDMA_IDLE;
    g_scsi_dma_use_greenpak = true;
    rcu_periph_clock_enable(SCSI_TIMER_RCU);
    rcu_periph_clock_enable(SCSI_TIMER_DMA_RCU);

    // DMA Channel A: data copy
    // GPIO DMA copies data from memory buffer to GPIO BOP register.
    // The memory buffer is filled by interrupt routine.
    dma_parameter_struct gpio_dma_config =
    {
        .periph_addr = (uint32_t)&GPIO_BOP(SCSI_OUT_PORT),
        .periph_width = DMA_PERIPHERAL_WIDTH_32BIT,
        .memory_addr = (uint32_t)g_scsi_dma.dma_buf,
        .memory_width = DMA_MEMORY_WIDTH_32BIT,
        .number = DMA_BUF_SIZE,
        .priority = DMA_PRIORITY_ULTRA_HIGH,
        .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
        .memory_inc = DMA_MEMORY_INCREASE_ENABLE,
        .direction = DMA_MEMORY_TO_PERIPHERAL
    };
    dma_init(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA, &gpio_dma_config);
    dma_circulation_enable(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA);
    NVIC_SetPriority(SCSI_TIMER_DMACHA_IRQn, 2);
    NVIC_EnableIRQ(SCSI_TIMER_DMACHA_IRQn);
    NVIC_DisableIRQ(SCSI_TIMER_DMACHB_IRQn);

    // EXTI channel is used to trigger when we reach end of the transfer.
    // Because the main DMA is circular and transfer size may not be even
    // multiple of it, we cannot trigger the end at the DMA interrupt.
    gpio_exti_source_select(GREENPAK_PLD_IO2_EXTI_SOURCE_PORT, GREENPAK_PLD_IO2_EXTI_SOURCE_PIN);
    exti_init(GREENPAK_PLD_IO2_EXTI, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(GREENPAK_PLD_IO2_EXTI);
    exti_interrupt_disable(GREENPAK_PLD_IO2_EXTI);
    NVIC_SetPriority(GREENPAK_IRQn, 1);
    NVIC_EnableIRQ(GREENPAK_IRQn);
    
    // Timer is used to trigger DMA requests
    // OUT_REQ is driven by timer output.
    // 1. On timer update event, REQ is set low.
    // 2. When ACK goes low, timer counts and OUT_REQ is set high.
    //    Simultaneously a DMA request is triggered to write next data to GPIO.
    // 3. When ACK goes high, a DMA request is triggered to cause timer update event.
    //    The DMA request priority is set so that 2. always completes before it.
    TIMER_CTL0(SCSI_TIMER) = 0;
    TIMER_SMCFG(SCSI_TIMER) = TIMER_SLAVE_MODE_EXTERNAL0 | TIMER_SMCFG_TRGSEL_CI0F_ED;
    TIMER_CAR(SCSI_TIMER) = 1;
    TIMER_PSC(SCSI_TIMER) = 0;
    TIMER_DMAINTEN(SCSI_TIMER) = 0;
    TIMER_CHCTL0(SCSI_TIMER) = 0x6001; // CH0 as input, CH1 as DMA trigger
    TIMER_CHCTL1(SCSI_TIMER) = 0;
    TIMER_CHCTL2(SCSI_TIMER) = 0;
    TIMER_CCHP(SCSI_TIMER) = 0;
    TIMER_CH1CV(SCSI_TIMER) = 1; // Copy data when ACK goes low
    gpio_init(SCSI_TIMER_IN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_TIMER_IN_PIN);
}

extern const uint32_t g_scsi_out_byte_to_bop_pld1hi[256];
extern const uint32_t g_scsi_out_byte_to_bop_pld1lo[256];

static void greenpak_refill_dmabuf()
{
    if (g_scsi_dma.greenpak_state == GREENPAK_STOP)
    {
        // Wait for previous DMA block to end first
        return;
    }

    // Check how many bytes we have available from the application
    uint32_t count = g_scsi_dma.bytes_app - g_scsi_dma.bytes_dma;
    
    // Check amount of free space in DMA buffer
    uint32_t max = g_scsi_dma.dma_fillto - g_scsi_dma.dma_idx;
    if (count > max) count = max;

    uint8_t *src = g_scsi_dma.app_buf + g_scsi_dma.bytes_dma;
    uint32_t *dst = g_scsi_dma.dma_buf;
    uint32_t pos = g_scsi_dma.dma_idx;
    uint32_t end = pos + count;
    g_scsi_dma.dma_idx = end;
    g_scsi_dma.bytes_dma += count;
    g_scsi_dma.scheduled_dma = g_scsi_dma.bytes_dma;

    if (pos < end && g_scsi_dma.greenpak_state == GREENPAK_IO1_HIGH)
    {
        // Fix alignment so that main loop begins with PLD1HI
        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop_pld1lo[*src++];
        g_scsi_dma.greenpak_state = GREENPAK_IO1_LOW;
    }

    while (pos + 4 <= end)
    {
        uint32_t input = *(uint32_t*)src;
        src += 4;

        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop_pld1hi[(input >> 0) & 0xFF];
        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop_pld1lo[(input >> 8) & 0xFF];
        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop_pld1hi[(input >> 16) & 0xFF];
        dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop_pld1lo[(input >> 24) & 0xFF];
    }

    while (pos < end)
    {
        if (g_scsi_dma.greenpak_state == GREENPAK_IO1_HIGH)
        {
            dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop_pld1lo[*src++];
            g_scsi_dma.greenpak_state = GREENPAK_IO1_LOW;
        }
        else
        {
            dst[(pos++) & DMA_BUF_MASK] = g_scsi_out_byte_to_bop_pld1hi[*src++];
            g_scsi_dma.greenpak_state = GREENPAK_IO1_HIGH;
        }
    }

    uint32_t remain = g_scsi_dma.dma_fillto - g_scsi_dma.dma_idx;
    if (!g_scsi_dma.next_app_buf && remain > 0)
    {
        // Mark the end of transfer by turning PD2 off
        dst[(pos++) & DMA_BUF_MASK] = (GREENPAK_PLD_IO2 << 16) | (GREENPAK_PLD_IO1 << 16);
        g_scsi_dma.dma_idx = pos;
        g_scsi_dma.greenpak_state = GREENPAK_STOP;
    }
}

extern "C" void GREENPAK_IRQ()
{
    if (EXTI_PD & GREENPAK_PLD_IO2_EXTI)
    {
        EXTI_PD = GREENPAK_PLD_IO2_EXTI;

        if (g_scsi_dma.bytes_app > g_scsi_dma.bytes_dma || g_scsi_dma.next_app_buf)
        {
            assert(g_scsi_dma.greenpak_state == GREENPAK_STOP);
            g_scsi_dma.greenpak_state = GREENPAK_IO1_LOW;
            // More data is available
            check_dma_next_buffer();
            refill_dmabuf();
            
            // Continue transferring
            GPIO_BOP(SCSI_OUT_PORT) = GREENPAK_PLD_IO2;
            TIMER_SWEVG(SCSI_TIMER) = TIMER_SWEVG_CH1G;
        }
        else
        {
            stop_dma();
        }
    }
}

static void greenpak_start_dma()
{
    // Disable channels while configuring
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) &= ~DMA_CHXCTL_CHEN;
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHB) &= ~DMA_CHXCTL_CHEN;
    TIMER_CTL0(SCSI_TIMER) = 0;

    // Set buffer address and size
    DMA_CHMADDR(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) = (uint32_t)g_scsi_dma.dma_buf;
    DMA_CHCNT(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) = DMA_BUF_SIZE;

    // Clear pending DMA events
    TIMER_DMAINTEN(SCSI_TIMER) = 0;
    TIMER_DMAINTEN(SCSI_TIMER) = TIMER_DMAINTEN_CH1DEN | TIMER_DMAINTEN_CH3DEN;

    // Clear and enable interrupt
    DMA_INTC(SCSI_TIMER_DMA) = DMA_FLAG_ADD(DMA_FLAG_HTF | DMA_FLAG_FTF | DMA_FLAG_ERR, SCSI_TIMER_DMACHA);
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) |= DMA_CHXCTL_FTFIE | DMA_CHXCTL_HTFIE;
    exti_interrupt_flag_clear(GREENPAK_PLD_IO2_EXTI);
    exti_interrupt_enable(GREENPAK_PLD_IO2_EXTI);

    // Enable channels
    DMA_CHCTL(SCSI_TIMER_DMA, SCSI_TIMER_DMACHA) |= DMA_CHXCTL_CHEN;
    
    // Enable timer
    TIMER_CNT(SCSI_TIMER) = 0;
    TIMER_CTL0(SCSI_TIMER) |= TIMER_CTL0_CEN;

    // Generate first event
    TIMER_SWEVG(SCSI_TIMER) = TIMER_SWEVG_CH1G;
}

static void greenpak_stop_dma()
{
    exti_interrupt_disable(GREENPAK_PLD_IO2_EXTI);
}

#endif