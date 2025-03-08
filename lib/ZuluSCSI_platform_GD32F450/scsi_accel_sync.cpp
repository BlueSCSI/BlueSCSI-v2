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
#include <gd32f4xx_exmc.h>
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
    // TODO: Figure out why DMA does not work correctly with EXMC on GD32F450
    // dma_single_data_parameter_struct exmc_dma_config =
    // {
    //     .periph_addr = EXMC_NOR_PSRAM,
    //     .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
    //     .memory0_addr = (uint32_t)g_sync_dma_buf, 
    //     .memory_inc = DMA_MEMORY_INCREASE_ENABLE,
    //     .periph_memory_width = DMA_PERIPH_WIDTH_16BIT,
    //     .circular_mode = DMA_CIRCULAR_MODE_DISABLE,
    //     .direction = DMA_MEMORY_TO_MEMORY,
    //     .number = 0, // Filled before transfer
    //     .priority = DMA_PRIORITY_MEDIUM
    // };
    // dma_single_data_mode_init(SCSI_EXMC_DMA, SCSI_EXMC_DMACH, &exmc_dma_config);
    gpio_mode_set(SCSI_IN_ACK_EXMC_NWAIT_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SCSI_IN_ACK_EXMC_NWAIT_PIN);
    gpio_output_options_set(SCSI_IN_ACK_EXMC_NWAIT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SCSI_IN_ACK_EXMC_NWAIT_PIN);
    gpio_af_set(SCSI_IN_ACK_EXMC_NWAIT_PORT, GPIO_AF_12, SCSI_IN_ACK_EXMC_NWAIT_PIN);

    // TIMER1 CH0 port and pin enable
    gpio_mode_set(SCSI_ACK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SCSI_ACK_PIN);
    gpio_af_set(SCSI_ACK_PORT, GPIO_AF_1, SCSI_ACK_PIN);
    
    // TIMER1 is used to count ACK pulses
    TIMER_CTL0(SCSI_SYNC_TIMER) = 0;
    TIMER_SMCFG(SCSI_SYNC_TIMER) = TIMER_SLAVE_MODE_EXTERNAL0 | TIMER_SMCFG_TRGSEL_CI0FE0;
    TIMER_CAR(SCSI_SYNC_TIMER) = 65535;
    TIMER_PSC(SCSI_SYNC_TIMER) = 0;
    TIMER_CHCTL0(SCSI_SYNC_TIMER) = 0x0001; // CH0 as input
}

void scsi_accel_sync_recv(uint8_t *data, uint32_t count, int* parityError, volatile int *resetFlag)
{
    // Set SCSI data IN pins to external memory mode
    gpio_mode_set(SCSI_IN_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SCSI_IN_MASK);
    gpio_output_options_set(SCSI_IN_PORT, GPIO_PUPD_NONE, GPIO_OSPEED_200MHZ, SCSI_IN_MASK);
    gpio_af_set(SCSI_IN_PORT, GPIO_AF_12, SCSI_IN_MASK);

    
    // Enable EXMC to drive REQ from EXMC_NOE pin
    EXMC_SNCTL(EXMC_BANK0_NORSRAM_REGION0) |= EXMC_SNCTL_NRBKEN;

    // save GPIO registers to restore after method is done
    uint32_t oldmode_gpio_ctl = GPIO_CTL(SCSI_OUT_REQ_EXMC_NOE_PORT);
    uint32_t oldmode_gpio_pud = GPIO_PUD(SCSI_OUT_REQ_EXMC_NOE_PORT);
    uint32_t oldmode_gpio_ospd = GPIO_OSPD(SCSI_OUT_REQ_EXMC_NOE_PORT);
    uint32_t oldmode_gpio_omode = GPIO_OMODE(SCSI_OUT_REQ_EXMC_NOE_PORT);
    uint32_t oldmode_gpio_af = GPIO_AFSEL0(SCSI_OUT_REQ_EXMC_NOE_PORT);

    gpio_af_set(SCSI_OUT_REQ_EXMC_NOE_PORT, GPIO_AF_12, SCSI_OUT_REQ_EXMC_NOE_PIN);
    gpio_output_options_set(SCSI_OUT_REQ_EXMC_NOE_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, SCSI_OUT_REQ_EXMC_NOE_PIN);
    gpio_mode_set(SCSI_OUT_REQ_EXMC_NOE_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SCSI_OUT_REQ_EXMC_NOE_PIN);
    
    while (count > 0)
    {
        uint32_t blocksize = (count > SYNC_DMA_BUFSIZE * 2) ? (SYNC_DMA_BUFSIZE * 2) : count;
        count -= blocksize;

        // dma_memory_address_config(SCSI_EXMC_DMA, SCSI_EXMC_DMACH, 0, (uint32_t)g_sync_dma_buf);
        // dma_transfer_number_config(SCSI_EXMC_DMA, SCSI_EXMC_DMACH, blocksize);
        // dma_channel_enable(SCSI_EXMC_DMA, SCSI_EXMC_DMACH);
        uint16_t *src = (uint16_t*)g_sync_dma_buf;
        uint8_t *dst = data;
        uint8_t *end = data + blocksize;
        uint32_t start = millis();

        while (dst < end)
        {
            // Read from EXMC and write to internal RAM
            // Note that this will hang the CPU if host does not send ACK pulses.
            uint16_t word = *(uint16_t*)EXMC_NOR_PSRAM;
            *dst++ = (~word) >> SCSI_EXMC_DATA_SHIFT;
            
            // TODO: Figure out why DMA does not work correctly with EXMC on GD32F450
            // uint32_t remain = DMA_CHCNT(SCSI_EXMC_DMA, SCSI_EXMC_DMACH);
            // while (dst < end - remain)
            // {
            //     *dst++ = ~(*src++) >> SCSI_EXMC_DATA_SHIFT;
            // }
            // if ((uint32_t)(millis() - start) > 500 || *resetFlag)
            // {
            //     // We are in a pinch here: without ACK pulses coming, the EXMC and DMA peripherals
            //     // are locked up. The only way out is a whole system reset.
            //     logmsg("SCSI Synchronous read timeout: resetting system");
            //     NVIC_SystemReset();
            // }
        }
        // dma_channel_disable(SCSI_EXMC_DMA, SCSI_EXMC_DMACH);
        data = end;
    }

    // Set SCSI data IN pins back to input mode
    gpio_mode_set(SCSI_IN_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, SCSI_IN_MASK);
    EXMC_SNCTL(EXMC_BANK0_NORSRAM_REGION0) &= ~EXMC_SNCTL_NRBKEN;
    GPIO_CTL(SCSI_OUT_REQ_EXMC_NOE_PORT) = oldmode_gpio_ctl;
    GPIO_OSPD(SCSI_OUT_REQ_EXMC_NOE_PORT) = oldmode_gpio_ospd;
    GPIO_OMODE(SCSI_OUT_REQ_EXMC_NOE_PORT) = oldmode_gpio_omode;
    GPIO_PUD(SCSI_OUT_REQ_EXMC_NOE_PORT) = oldmode_gpio_pud;
    GPIO_AFSEL0(SCSI_OUT_REQ_EXMC_NOE_PORT) = oldmode_gpio_af;


}

/********************************/
/* Transfer from device to host */
/********************************/

// Simple delay, about 20 ns.
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

// Delay 1 is typically longest and delay 2 shortest.
// Tuning these is just trial and error.
#define ASM_DELAY1() ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY()
#define ASM_DELAY2() ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY()

    asm volatile (
    "main_loop_%=: \n"
        "   subs  %[num_bytes], %[num_bytes], #16 \n"
        "   bmi     last_bytes_%= \n"

        /* At each point make sure there is at most 15 bytes in flight */
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("26")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("22")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("18")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("14")
        ASM_DELAY2()

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

        "   subs %[num_bytes], %[num_bytes], #1 \n"
        "   bne  last_bytes_loop_%= \n"
    "all_done_%=: \n"
        ASM_DELAY()

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

#define ASM_DELAY1() ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY()
#define ASM_DELAY2() ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY()

    asm volatile (
    "main_loop_%=: \n"
        "   subs  %[num_bytes], %[num_bytes], #16 \n"
        "   bmi     last_bytes_%= \n"

        /* At each point make sure there is at most 15 bytes in flight */
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("26")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("22")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("18")
        ASM_DELAY2()
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("14")

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
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY()
#define ASM_DELAY2() ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY() \
                     ASM_DELAY() ASM_DELAY() ASM_DELAY() ASM_DELAY()

    asm volatile (
    "main_loop_%=: \n"
        "   subs  %[num_bytes], %[num_bytes], #4 \n"
        "   bmi     last_bytes_%= \n"

        /* At each point make sure there is at most 3 bytes in flight */
        "   ldr   %[data], [%[buf]], #4 \n"
        ASM_SEND_4BYTES_WAIT("7")
        ASM_DELAY2()
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
        dbgmsg("No optimized routine for syncOffset=", syncOffset, " syndPeriod=", syncPeriod, ", using fallback");
        while (count-- > 0)
        {
            while (TIMER_CNT(SCSI_SYNC_TIMER) > count + syncOffset && !*resetFlag);

            SCSI_OUT_DATA(*data++);
            delay_ns(syncPeriod * 2);
            SCSI_OUT(REQ, 1);
            delay_ns(syncPeriod * 2);
        }
        delay_ns(syncPeriod * 2);
        SCSI_RELEASE_DATA_REQ();
    }

    while (TIMER_CNT(SCSI_SYNC_TIMER) > 0 && !*resetFlag);

    if (*resetFlag)
    {
        dbgmsg("Bus reset during sync transfer, total ", (int)count,
              " bytes, remaining ACK count ", (int)TIMER_CNT(SCSI_SYNC_TIMER));
    }

    TIMER_CTL0(SCSI_SYNC_TIMER) = 0;
}


#endif
