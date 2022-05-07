/* Synchronous mode SCSI implementation.
 *
 * In synchronous mode, the handshake mechanism is not used. Instead
 * either end of the communication will just send a bunch of bytes
 * and only afterwards checks that the number of acknowledgement
 * pulses matches.
 * 
 * The receiving end should latch in the data at the falling edge of
 * the request pulse (on either REQ or ACK pin). We use the GD32 EXMC
 * peripheral to implement this latching with the NWAIT pin when
 * reading data from the host. NOE is used to generate the REQ pulses.
 * 
 * Writing data to the host is simpler, as we can just write it out
 * from the GPIO port at our own pace. A timer is used for generating
 * the output pulses on REQ pin.
 */

#include "scsi_accel_sync.h"
#include <ZuluSCSI_log.h>
#include <gd32f20x_exmc.h>
#include <scsi.h>

#ifndef SCSI_SYNC_MODE_AVAILABLE

void scsi_accel_sync_init() {}

void scsi_accel_sync_recv(uint8_t *data, uint32_t count, int* parityError, volatile int *resetFlag) {}
void scsi_accel_sync_send(const uint8_t* data, uint32_t count, volatile int *resetFlag) {}

#else

/********************************/
/* Transfer from host to device */
/********************************/

#define SYNC_DMA_BUFSIZE 512
static uint32_t g_sync_dma_buf[SYNC_DMA_BUFSIZE];

void scsi_accel_sync_init()
{
    rcu_periph_clock_enable(RCU_EXMC);
    rcu_periph_clock_enable(SCSI_EXMC_DMA_RCU);
    rcu_periph_clock_enable(SCSI_SYNC_TIMER_RCU);

    exmc_norsram_timing_parameter_struct timing_param = {
        .asyn_access_mode = EXMC_ACCESS_MODE_A,
        .syn_data_latency = EXMC_DATALAT_2_CLK,
        .syn_clk_division = EXMC_SYN_CLOCK_RATIO_2_CLK,
        .bus_latency = 1,
        .asyn_data_setuptime = 2,
        .asyn_address_holdtime = 2,
        .asyn_address_setuptime = 16
    };

    exmc_norsram_parameter_struct sram_param = {
        .norsram_region = EXMC_BANK0_NORSRAM_REGION0,
        .write_mode = EXMC_ASYN_WRITE,
        .extended_mode = DISABLE,
        .asyn_wait = ENABLE,
        .nwait_signal = ENABLE,
        .memory_write = DISABLE,
        .nwait_config = EXMC_NWAIT_CONFIG_DURING,
        .wrap_burst_mode = DISABLE,
        .nwait_polarity = EXMC_NWAIT_POLARITY_HIGH,
        .burst_mode = DISABLE,
        .databus_width = EXMC_NOR_DATABUS_WIDTH_16B,
        .memory_type = EXMC_MEMORY_TYPE_SRAM,
        .address_data_mux = DISABLE,
        .read_write_timing = &timing_param
    };

    EXMC_SNCTL(EXMC_BANK0_NORSRAM_REGION0) &= ~EXMC_SNCTL_NRBKEN;
    exmc_norsram_init(&sram_param);

    // DMA used to transfer data from EXMC to RAM
    // DMA is used so that if data transfer fails, we can at least abort by resetting CPU.
    // Accessing EXMC from the CPU directly hangs it totally if ACK pulses are not received.
    dma_parameter_struct exmc_dma_config =
    {
        .periph_addr = EXMC_NOR_PSRAM,
        .periph_width = DMA_PERIPHERAL_WIDTH_16BIT,
        .memory_addr = (uint32_t)g_sync_dma_buf,
        .memory_width = DMA_MEMORY_WIDTH_16BIT,
        .number = 0, // Filled before transfer
        .priority = DMA_PRIORITY_MEDIUM,
        .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
        .memory_inc = DMA_MEMORY_INCREASE_ENABLE,
        .direction = DMA_PERIPHERAL_TO_MEMORY
    };
    dma_init(SCSI_EXMC_DMA, SCSI_EXMC_DMACH, &exmc_dma_config);
    dma_memory_to_memory_enable(SCSI_EXMC_DMA, SCSI_EXMC_DMACH);

    gpio_init(SCSI_IN_ACK_EXMC_NWAIT_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_IN_ACK_EXMC_NWAIT_PIN);
    gpio_init(SCSI_TIMER_IN_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_TIMER_IN_PIN);

    // TIMER1 is used to count ACK pulses
    TIMER_CTL0(SCSI_SYNC_TIMER) = 0;
    TIMER_SMCFG(SCSI_SYNC_TIMER) = TIMER_SLAVE_MODE_EXTERNAL0 | TIMER_SMCFG_TRGSEL_CI0FE0;
    TIMER_CAR(SCSI_SYNC_TIMER) = 65535;
    TIMER_PSC(SCSI_SYNC_TIMER) = 0;
    TIMER_CHCTL0(SCSI_SYNC_TIMER) = 0x0001; // CH0 as input
}

void scsi_accel_sync_recv(uint8_t *data, uint32_t count, int* parityError, volatile int *resetFlag)
{
    // Enable EXMC to drive REQ from EXMC_NOE pin
    EXMC_SNCTL(EXMC_BANK0_NORSRAM_REGION0) |= EXMC_SNCTL_NRBKEN;
    uint32_t oldmode = GPIO_CTL0(SCSI_OUT_REQ_EXMC_NOE_PORT);
    uint32_t newmode = oldmode & ~(0xF << (SCSI_OUT_REQ_EXMC_NOE_IDX * 4));
    newmode |= 0xB << (SCSI_OUT_REQ_EXMC_NOE_IDX * 4);
    GPIO_CTL0(SCSI_OUT_REQ_EXMC_NOE_PORT) = newmode;
    
    while (count > 0)
    {
        uint32_t blocksize = (count > SYNC_DMA_BUFSIZE * 2) ? (SYNC_DMA_BUFSIZE * 2) : count;
        count -= blocksize;

        DMA_CHCNT(SCSI_EXMC_DMA, SCSI_EXMC_DMACH) = blocksize;
        DMA_CHCTL(SCSI_EXMC_DMA, SCSI_EXMC_DMACH) |= DMA_CHXCTL_CHEN;

        uint16_t *src = (uint16_t*)g_sync_dma_buf;
        uint8_t *dst = data;
        uint8_t *end = data + blocksize;
        uint32_t start = millis();
        while (dst < end)
        {
            uint32_t remain = DMA_CHCNT(SCSI_EXMC_DMA, SCSI_EXMC_DMACH);

            while (dst < end - remain)
            {
                *dst++ = ~(*src++) >> SCSI_EXMC_DATA_SHIFT;
            }

            if ((uint32_t)(millis() - start) > 500 || *resetFlag)
            {
                // We are in a pinch here: without ACK pulses coming, the EXMC and DMA peripherals
                // are locked up. The only way out is a whole system reset.
                azlog("SCSI Synchronous read timeout: resetting system");
                NVIC_SystemReset();
            }
        }

        DMA_CHCTL(SCSI_EXMC_DMA, SCSI_EXMC_DMACH) &= ~DMA_CHXCTL_CHEN;
        data = end;
    }

    GPIO_CTL0(SCSI_OUT_REQ_EXMC_NOE_PORT) = oldmode;
    EXMC_SNCTL(EXMC_BANK0_NORSRAM_REGION0) &= ~EXMC_SNCTL_NRBKEN;
}

/********************************/
/* Transfer from device to host */
/********************************/

// Simple delay, about 10 ns.
// This is less likely to get optimized away by CPU pipeline than nop
#define ASM_DELAY()  \
"   ldr     %[tmp2], [%[reset_flag]] \n"

// Take 8 bits from d and format them for writing
// d is name of data operand, b is bit offset
#define ASM_LOAD_DATA(b) \
"        ubfx    %[tmp1], %[data], #" b ", #8 \n" \
"        ldr     %[tmp1], [%[byte_lookup], %[tmp1], lsl #2] \n"

// Write data to SCSI port and set REQ high
#define ASM_SEND_DATA() \
"        str     %[tmp1], [%[out_port_bop]] \n"

// Set REQ low
#define ASM_SET_REQ_LOW() \
"        mov     %[tmp2], %[bop_req_low] \n" \
"        str     %[tmp2], [%[out_port_bop]] \n"

// Wait for ACK_TIMER - n to be less than num_bytes
#define ASM_WAIT_ACK_TIMER(n) \
    "wait_acks_" n "_%=: \n" \
        "   ldr     %[tmp2], [%[ack_timer]] \n" \
        "   sub     %[tmp2], # " n " \n" \
        "   cmp     %[tmp2], %[num_bytes] \n" \
        "   ble     got_acks_" n "_%= \n" \
        "   ldr     %[tmp2], [%[reset_flag]] \n" \
        "   cmp     %[tmp2], #0 \n" \
        "   bne     all_done_%= \n" \
        "   b       wait_acks_" n "_%= \n" \
    "got_acks_" n "_%=: \n"

// Send 4 bytes
#define ASM_SEND_4BYTES() \
ASM_LOAD_DATA("0") \
ASM_SEND_DATA() \
ASM_DELAY1() \
ASM_SET_REQ_LOW() \
ASM_DELAY2() \
ASM_LOAD_DATA("8") \
ASM_SEND_DATA() \
ASM_DELAY1() \
ASM_SET_REQ_LOW() \
ASM_DELAY2() \
ASM_LOAD_DATA("16") \
ASM_SEND_DATA() \
ASM_DELAY1() \
ASM_SET_REQ_LOW() \
ASM_DELAY2() \
ASM_LOAD_DATA("24") \
ASM_SEND_DATA() \
ASM_DELAY1() \
ASM_SET_REQ_LOW()

// Send 1 byte, wait for ACK_TIMER to be less than num_bytes + n and send 3 bytes more
// This interleaving minimizes the delay caused by WAIT_ACK_TIMER.
#define ASM_SEND_4BYTES_WAIT(n) \
ASM_LOAD_DATA("0") \
ASM_SEND_DATA() \
ASM_DELAY2() \
ASM_LOAD_DATA("8") \
ASM_SET_REQ_LOW() \
ASM_DELAY2() \
"   ldr     %[tmp2], [%[ack_timer]] \n" \
"   sub     %[tmp2], # " n " \n" \
ASM_SEND_DATA() \
"   cmp     %[tmp2], %[num_bytes] \n" \
"   ble     got_acks_" n "_%= \n" \
ASM_WAIT_ACK_TIMER(n) \
ASM_DELAY2() \
ASM_SET_REQ_LOW() \
ASM_DELAY2() \
ASM_LOAD_DATA("16") \
ASM_SEND_DATA() \
ASM_DELAY1() \
ASM_SET_REQ_LOW() \
ASM_DELAY2() \
ASM_LOAD_DATA("24") \
ASM_SEND_DATA() \
ASM_DELAY1() \
ASM_SET_REQ_LOW() \

// Specialized routine for settings:
// <=100 ns period, >=15 outstanding REQs
static void sync_send_100ns_15off(const uint8_t *buf, uint32_t num_bytes, volatile int *resetFlag)
{
    volatile uint32_t *out_port_bop = (volatile uint32_t*)&GPIO_BOP(SCSI_OUT_PORT);
    volatile uint32_t *ack_timer = &TIMER_CNT(SCSI_SYNC_TIMER);
    const uint32_t *byte_lookup = g_scsi_out_byte_to_bop;
    register uint32_t tmp1 = 0;
    register uint32_t tmp2 = 0;
    register uint32_t data = 0;

#define ASM_DELAY1()
#define ASM_DELAY2() ASM_DELAY()

    asm volatile (
    "main_loop_%=: \n"
        "   subs  %[num_bytes], %[num_bytes], #16 \n"
        "   bmi     last_bytes_%= \n"

        /* At each point make sure there is at most 15 bytes in flight */
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("22")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES()
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("14")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES()

        "   cbz   %[num_bytes], all_done_%= \n"
        "   b     main_loop_%= \n"

    "last_bytes_%=: \n"
        "   add  %[num_bytes], %[num_bytes], #16 \n"
    "last_bytes_loop_%=: \n"
        "   ldrb    %[data], [%[buf]], #1 \n"
        ASM_LOAD_DATA("0")

        ASM_WAIT_ACK_TIMER("15")
        ASM_SEND_DATA()
        ASM_DELAY1()
        ASM_SET_REQ_LOW()
        ASM_DELAY2()

        "   subs %[num_bytes], %[num_bytes], #1 \n"
        "   bne  last_bytes_loop_%= \n"
    "all_done_%=: \n"
        ASM_DELAY1()

    : /* Output */ [tmp1] "+l" (tmp1), [tmp2] "+l" (tmp2), [data] "+r" (data),
                   [buf] "+r" (buf), [num_bytes] "+r" (num_bytes)
    : /* Input */ [ack_timer] "r" (ack_timer),
                  [bop_req_low] "I" (SCSI_OUT_REQ << 16),
                  [out_port_bop] "r"(out_port_bop),
                  [byte_lookup] "r" (byte_lookup),
                  [reset_flag] "r" (resetFlag)
    : /* Clobber */);

#undef ASM_DELAY1
#undef ASM_DELAY2

    SCSI_RELEASE_DATA_REQ();
}

// Specialized routine for settings:
// <=200 ns period, >=15 outstanding REQs
static void sync_send_200ns_15off(const uint8_t *buf, uint32_t num_bytes, volatile int *resetFlag)
{
    volatile uint32_t *out_port_bop = (volatile uint32_t*)&GPIO_BOP(SCSI_OUT_PORT);
    volatile uint32_t *ack_timer = &TIMER_CNT(SCSI_SYNC_TIMER);
    const uint32_t *byte_lookup = g_scsi_out_byte_to_bop;
    register uint32_t tmp1 = 0;
    register uint32_t tmp2 = 0;
    register uint32_t data = 0;

#define ASM_DELAY1() ASM_DELAY() ASM_DELAY() ASM_DELAY()
#define ASM_DELAY2() ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY()

    asm volatile (
    "main_loop_%=: \n"
        "   subs  %[num_bytes], %[num_bytes], #16 \n"
        "   bmi     last_bytes_%= \n"

        /* At each point make sure there is at most 15 bytes in flight */
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("22")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES()
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("14")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES()

        "   cbz   %[num_bytes], all_done_%= \n"
        "   b     main_loop_%= \n"

    "last_bytes_%=: \n"
        "   add  %[num_bytes], %[num_bytes], #16 \n"
    "last_bytes_loop_%=: \n"
        "   ldrb    %[data], [%[buf]], #1 \n"
        ASM_LOAD_DATA("0")

        ASM_WAIT_ACK_TIMER("15")
        ASM_SEND_DATA()
        ASM_DELAY1()
        ASM_SET_REQ_LOW()
        ASM_DELAY2()

        "   subs %[num_bytes], %[num_bytes], #1 \n"
        "   bne  last_bytes_loop_%= \n"
    "all_done_%=: \n"
        ASM_DELAY1()

    : /* Output */ [tmp1] "+l" (tmp1), [tmp2] "+l" (tmp2), [data] "+r" (data),
                   [buf] "+r" (buf), [num_bytes] "+r" (num_bytes)
    : /* Input */ [ack_timer] "r" (ack_timer),
                  [bop_req_low] "I" (SCSI_OUT_REQ << 16),
                  [out_port_bop] "r"(out_port_bop),
                  [byte_lookup] "r" (byte_lookup),
                  [reset_flag] "r" (resetFlag)
    : /* Clobber */);

#undef ASM_DELAY1
#undef ASM_DELAY2

    SCSI_RELEASE_DATA_REQ();
}

// Specialized routine for settings:
// <=260 ns period, >=7 outstanding REQs
static void sync_send_260ns_7off(const uint8_t *buf, uint32_t num_bytes, volatile int *resetFlag)
{
    volatile uint32_t *out_port_bop = (volatile uint32_t*)&GPIO_BOP(SCSI_OUT_PORT);
    volatile uint32_t *ack_timer = &TIMER_CNT(SCSI_SYNC_TIMER);
    const uint32_t *byte_lookup = g_scsi_out_byte_to_bop;
    register uint32_t tmp1 = 0;
    register uint32_t tmp2 = 0;
    register uint32_t data = 0;

#define ASM_DELAY1() ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY()
#define ASM_DELAY2() ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY()

    asm volatile (
    "main_loop_%=: \n"
        "   subs  %[num_bytes], %[num_bytes], #4 \n"
        "   bmi     last_bytes_%= \n"

        /* At each point make sure there is at most 3 bytes in flight */
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("7")

        "   cbz   %[num_bytes], all_done_%= \n"
        "   b     main_loop_%= \n"

    "last_bytes_%=: \n"
        "   add  %[num_bytes], %[num_bytes], #4 \n"
    "last_bytes_loop_%=: \n"
        "   ldrb    %[data], [%[buf]], #1 \n"
        ASM_LOAD_DATA("0")

        ASM_WAIT_ACK_TIMER("5")
        ASM_SEND_DATA()
        ASM_DELAY1()
        ASM_SET_REQ_LOW()
        ASM_DELAY2()

        "   subs %[num_bytes], %[num_bytes], #1 \n"
        "   bne  last_bytes_loop_%= \n"
    "all_done_%=: \n"
        ASM_DELAY1()

    : /* Output */ [tmp1] "+l" (tmp1), [tmp2] "+l" (tmp2), [data] "+r" (data),
                   [buf] "+r" (buf), [num_bytes] "+r" (num_bytes)
    : /* Input */ [ack_timer] "r" (ack_timer),
                  [bop_req_low] "I" (SCSI_OUT_REQ << 16),
                  [out_port_bop] "r"(out_port_bop),
                  [byte_lookup] "r" (byte_lookup),
                  [reset_flag] "r" (resetFlag)
    : /* Clobber */);

#undef ASM_DELAY1
#undef ASM_DELAY2

    SCSI_RELEASE_DATA_REQ();
}

void scsi_accel_sync_send(const uint8_t* data, uint32_t count, volatile int *resetFlag)
{
    // Timer counts down from the initial number of bytes.
    TIMER_CNT(SCSI_SYNC_TIMER) = count;
    TIMER_CTL0(SCSI_SYNC_TIMER) = TIMER_CTL0_CEN | TIMER_CTL0_DIR;

    int syncOffset = scsiDev.target->syncOffset;
    int syncPeriod = scsiDev.target->syncPeriod;

    if (syncOffset >= 15 && syncPeriod <= 25)
    {
        sync_send_100ns_15off(data, count, resetFlag);
    }
    else if (syncOffset >= 15 && syncPeriod <= 50)
    {
        sync_send_200ns_15off(data, count, resetFlag);
    }
    else if (syncOffset >= 7 && syncPeriod <= 65)
    {
        sync_send_260ns_7off(data, count, resetFlag);
    }
    else
    {
        azdbg("No optimized routine for syncOffset=", syncOffset, " syndPeriod=", syncPeriod, ", using fallback");
        while (count-- > 0)
        {
            while (TIMER_CNT(SCSI_SYNC_TIMER) > count + syncOffset && !*resetFlag);

            SCSI_OUT_DATA(*data++);
            delay_ns(syncPeriod * 2);
            SCSI_OUT(REQ, 0);
            delay_ns(syncPeriod * 2);
        }
        delay_ns(syncPeriod * 2);
        SCSI_RELEASE_DATA_REQ();
    }

    while (TIMER_CNT(SCSI_SYNC_TIMER) > 0 && !*resetFlag);

    TIMER_CTL0(SCSI_SYNC_TIMER) = 0;
}


#endif
