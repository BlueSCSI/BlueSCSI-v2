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

void scsi_accel_sync_read(uint8_t *data, uint32_t count, int* parityError, volatile int *resetFlag) {}
void scsi_accel_sync_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag) {}
void scsi_accel_sync_stopWrite() {}
void scsi_accel_sync_finishWrite(volatile int *resetFlag) {}
bool scsi_accel_sync_isWriteFinished(const uint8_t* data) { return true; }

#else

void scsi_accel_sync_init()
{
    rcu_periph_clock_enable(RCU_EXMC);
    rcu_periph_clock_enable(SCSI_TIMER_RCU);

    exmc_norsram_timing_parameter_struct timing_param = {
        .asyn_access_mode = EXMC_ACCESS_MODE_A,
        .syn_data_latency = EXMC_DATALAT_2_CLK,
        .syn_clk_division = EXMC_SYN_CLOCK_RATIO_2_CLK,
        .bus_latency = 1,
        .asyn_data_setuptime = 2,
        .asyn_address_holdtime = 2,
        .asyn_address_setuptime = 1
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

    exmc_norsram_init(&sram_param);

    gpio_init(SCSI_IN_ACK_EXMC_NWAIT_PORT, GPIO_MODE_IN_FLOATING, 0, SCSI_IN_ACK_EXMC_NWAIT_PIN);
}

void scsi_accel_sync_read(uint8_t *data, uint32_t count, int* parityError, volatile int *resetFlag)
{
    exmc_norsram_enable(EXMC_BANK0_NORSRAM_REGION0);
    gpio_init(SCSI_OUT_REQ_EXMC_NOE_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_REQ_EXMC_NOE_PIN);
    
    for (int i = 0; i < count; i++)
    {
        uint32_t value = *(volatile uint32_t*)EXMC_NOR_PSRAM;
        data[i] = ~(value >> SCSI_EXMC_DATA_SHIFT) & 0xFF;
    }

    gpio_init(SCSI_OUT_REQ_EXMC_NOE_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SCSI_OUT_REQ_EXMC_NOE_PIN);
    exmc_norsram_disable(EXMC_BANK0_NORSRAM_REGION0);
}

void scsi_accel_sync_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag)
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

void scsi_accel_sync_stopWrite()
{

}

void scsi_accel_sync_finishWrite(volatile int *resetFlag)
{

}

bool scsi_accel_sync_isWriteFinished(const uint8_t* data)
{
    return true;
}

#endif