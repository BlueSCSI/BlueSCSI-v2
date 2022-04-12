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
#include <AzulSCSI_log.h>
#include <gd32f20x_exmc.h>

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
    rcu_periph_clock_enable(SCSI_TIMER_RCU);
    rcu_periph_clock_enable(SCSI_EXMC_DMA_RCU);

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
    }

    GPIO_CTL0(SCSI_OUT_REQ_EXMC_NOE_PORT) = oldmode;
    EXMC_SNCTL(EXMC_BANK0_NORSRAM_REGION0) &= ~EXMC_SNCTL_NRBKEN;
}

/********************************/
/* Transfer from device to host */
/********************************/

void scsi_accel_sync_send(const uint8_t* data, uint32_t count, volatile int *resetFlag)
{
    for (int i = 0; i < count; i++)
    {
        SCSI_OUT_DATA(data[i]);
        delay_100ns();
        SCSI_OUT(REQ, 1);
        delay_ns(200);
        SCSI_OUT(REQ, 0);
        delay_ns(500);
    }
    SCSI_RELEASE_DATA_REQ();
}


#endif