/**
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 *
 * This work incorporates work from the following
 *  Copyright (c) 2023 joshua stein <jcs@jcs.org>
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

/* Data flow in SCSI acceleration:
*
* 1. Application provides a buffer of bytes to send.
* 2. Code in this module adds parity bit to the bytes and packs two bytes into 32 bit words.
* 3. DMA controller copies the words to PIO peripheral FIFO.
* 4. PIO peripheral handles low-level SCSI handshake and writes bytes and parity to GPIO.
*/

#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "scsi_accel_target.h"
#include "timings_RP2MCU.h"
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/structs/iobank0.h>
#include <hardware/sync.h>
#include <pico/multicore.h>

#ifdef ENABLE_AUDIO_OUTPUT_SPDIF
#include "audio_spdif.h"
#endif // ENABLE_AUDIO_OUTPUT_SPDIF

#include "scsi_accel_target_RP2MCU.pio.h"

// SCSI bus write acceleration uses up to 3 PIO state machines:
// SM0: Convert data bytes to lookup addresses to add parity
// SM1: Write data to SCSI bus
// SM2: For synchronous mode only, count ACK pulses
#ifdef BLUESCSI_NETWORK
#  define SCSI_DMA_PIO pio0
#  define SCSI_PARITY_SM 1
#  define SCSI_DATA_SM 2
#  define SCSI_SYNC_SM 3
#else
#  define SCSI_DMA_PIO pio0
#  define SCSI_PARITY_SM 0
#  define SCSI_DATA_SM 1
#  define SCSI_SYNC_SM 2
#endif // BLUESCSI_NETWORK


// SCSI bus write acceleration uses 3 or 4 DMA channels (data flow A->B->C->D):
// A: Bytes from RAM to scsi_parity PIO
// B: Addresses from scsi_parity PIO to lookup DMA READ_ADDR register
// C: Lookup from g_scsi_parity_lookup and copy to scsi_accel_async_write or scsi_sync_write PIO
// D: For sync transfers, scsi_sync_write to scsi_sync_write_pacer PIO
//
// SCSI bus read acceleration uses 4 DMA channels (data flow D->C->B->A):
// A: Bytes from scsi_read_parity PIO to memory buffer
// B: Lookup from g_scsi_parity_check_lookup and copy to scsi_read_parity PIO
// C: Addresses from scsi_accel_read PIO to lookup DMA READ_ADDR register
// D: From pacer to data state machine to trigger transfers
#ifdef BLUESCSI_NETWORK
#  define SCSI_DMA_CH_A 6
#  define SCSI_DMA_CH_B 7
#  define SCSI_DMA_CH_C 8
#  define SCSI_DMA_CH_D 9
#else
#  define SCSI_DMA_CH_A 0
#  define SCSI_DMA_CH_B 1
#  define SCSI_DMA_CH_C 2
#  define SCSI_DMA_CH_D 3
#endif

static struct {
    uint8_t *app_buf; // Buffer provided by application
    uint32_t app_bytes; // Bytes available in application buffer
    uint32_t dma_bytes; // Bytes that have been scheduled for DMA so far

    uint8_t *next_app_buf; // Next buffer from application after current one finishes
    uint32_t next_app_bytes; // Bytes in next buffer

    // Synchronous mode?
    int syncOffset;
    int syncPeriod;
    int syncOffsetDivider; // Autopush/autopull threshold for the write pacer state machine
    int syncOffsetPreload; // Number of items to preload in the RX fifo of scsi_sync_write

    // PIO configurations
    uint32_t pio_offset_parity;
    uint32_t pio_offset_async_write;
    uint32_t pio_offset_sync_write_pacer;
    uint32_t pio_offset_sync_write;
    uint32_t pio_offset_read;
    uint32_t pio_offset_read_parity;
    uint32_t pio_offset_sync_read_pacer;
    pio_sm_config pio_cfg_parity;
    pio_sm_config pio_cfg_async_write;
    pio_sm_config pio_cfg_sync_write_pacer;
    pio_sm_config pio_cfg_sync_write;
    pio_sm_config pio_cfg_read;
    pio_sm_config pio_cfg_read_parity;
    pio_sm_config pio_cfg_sync_read_pacer;
    bool pio_removed_parity;
    bool pio_removed_async_write;
    bool pio_removed_sync_write_pacer;
    bool pio_removed_sync_write;
    bool pio_removed_read;
    bool pio_removed_read_parity;
    bool pio_removed_sync_read_pacer;

    // DMA configurations for write
    dma_channel_config dmacfg_write_chA; // Data from RAM to scsi_parity PIO
    dma_channel_config dmacfg_write_chB; // Addresses from scsi_parity PIO to lookup DMA
    dma_channel_config dmacfg_write_chC; // Data from g_scsi_parity_lookup to scsi write PIO
    dma_channel_config dmacfg_write_chD; // In synchronous mode only, transfer between state machines

    // DMA configurations for read
    dma_channel_config dmacfg_read_chA; // Data to destination memory buffer
    dma_channel_config dmacfg_read_chB; // From lookup table to scsi_read_parity PIO
    dma_channel_config dmacfg_read_chC; // From scsi_accel_read to channel B READ_ADDR
    dma_channel_config dmacfg_read_chD; // From pacer to data state machine
} g_scsi_dma;

enum scsidma_state_t { SCSIDMA_IDLE = 0,
                    SCSIDMA_WRITE, SCSIDMA_WRITE_DONE,
                    SCSIDMA_READ, SCSIDMA_READ_DONE };
static const char* scsidma_states[5] = {"IDLE", "WRITE", "WRITE_DONE", "READ", "READ_DONE"};
static volatile scsidma_state_t g_scsi_dma_state;
static bool g_channels_claimed = false;
static void scsidma_config_gpio();

void scsi_accel_log_state()
{
    logmsg("SCSI DMA state: ", scsidma_states[g_scsi_dma_state]);
    logmsg("Current buffer: ", g_scsi_dma.dma_bytes, "/", g_scsi_dma.app_bytes, ", next ", g_scsi_dma.next_app_bytes, " bytes");
    logmsg("SyncOffset: ", g_scsi_dma.syncOffset, " SyncPeriod ", g_scsi_dma.syncPeriod);
    logmsg("PIO Parity SM:",
        " tx_fifo ", (int)pio_sm_get_tx_fifo_level(SCSI_DMA_PIO, SCSI_PARITY_SM),
        ", rx_fifo ", (int)pio_sm_get_rx_fifo_level(SCSI_DMA_PIO, SCSI_PARITY_SM),
        ", pc ", (int)pio_sm_get_pc(SCSI_DMA_PIO, SCSI_PARITY_SM),
        ", instr ", SCSI_DMA_PIO->sm[SCSI_PARITY_SM].instr);
    logmsg("PIO Data SM:",
        " tx_fifo ", (int)pio_sm_get_tx_fifo_level(SCSI_DMA_PIO, SCSI_DATA_SM),
        ", rx_fifo ", (int)pio_sm_get_rx_fifo_level(SCSI_DMA_PIO, SCSI_DATA_SM),
        ", pc ", (int)pio_sm_get_pc(SCSI_DMA_PIO, SCSI_DATA_SM),
        ", instr ", SCSI_DMA_PIO->sm[SCSI_DATA_SM].instr);
    logmsg("PIO Sync SM:",
        " tx_fifo ", (int)pio_sm_get_tx_fifo_level(SCSI_DMA_PIO, SCSI_SYNC_SM),
        ", rx_fifo ", (int)pio_sm_get_rx_fifo_level(SCSI_DMA_PIO, SCSI_SYNC_SM),
        ", pc ", (int)pio_sm_get_pc(SCSI_DMA_PIO, SCSI_SYNC_SM),
        ", instr ", SCSI_DMA_PIO->sm[SCSI_SYNC_SM].instr);
    logmsg("DMA CH A:",
        " ctrl: ", dma_hw->ch[SCSI_DMA_CH_A].ctrl_trig,
        " count: ", dma_hw->ch[SCSI_DMA_CH_A].transfer_count);
    logmsg("DMA CH B:",
        " ctrl: ", dma_hw->ch[SCSI_DMA_CH_B].ctrl_trig,
        " count: ", dma_hw->ch[SCSI_DMA_CH_B].transfer_count);
    logmsg("DMA CH C:",
        " ctrl: ", dma_hw->ch[SCSI_DMA_CH_C].ctrl_trig,
        " count: ", dma_hw->ch[SCSI_DMA_CH_C].transfer_count);
    logmsg("DMA CH D:",
        " ctrl: ", dma_hw->ch[SCSI_DMA_CH_D].ctrl_trig,
        " count: ", dma_hw->ch[SCSI_DMA_CH_D].transfer_count);
    logmsg("GPIO states: ", sio_hw->gpio_in);
}

/****************************************/
/* Accelerated writes to SCSI bus       */
/****************************************/

// Load the SCSI parity state machine with the address of the parity lookup table.
// Also sets up DMA channels B and C
static void config_parity_sm_for_write()
{
    // Load base address to state machine register X
    uint32_t addrbase = (uint32_t)&g_scsi_parity_lookup[0];
    assert((addrbase & 0x1FF) == 0);
    pio_sm_init(SCSI_DMA_PIO, SCSI_PARITY_SM, g_scsi_dma.pio_offset_parity, &g_scsi_dma.pio_cfg_parity);
    pio_sm_put(SCSI_DMA_PIO, SCSI_PARITY_SM, addrbase >> 9);
    pio_sm_exec(SCSI_DMA_PIO, SCSI_PARITY_SM, pio_encode_pull(false, false));
    pio_sm_exec(SCSI_DMA_PIO, SCSI_PARITY_SM, pio_encode_mov(pio_x, pio_osr));

    // DMA channel B will copy addresses from parity PIO to DMA channel C read address register.
    // It is triggered by the parity SM RX FIFO request
    dma_channel_configure(SCSI_DMA_CH_B,
        &g_scsi_dma.dmacfg_write_chB,
        &dma_hw->ch[SCSI_DMA_CH_C].al3_read_addr_trig,
        &SCSI_DMA_PIO->rxf[SCSI_PARITY_SM],
        1, true);

    // DMA channel C will read g_scsi_parity_lookup to copy data + parity to SCSI write state machine.
    // It is triggered by SCSI write machine TX FIFO request and chains to re-enable channel B.
    dma_channel_configure(SCSI_DMA_CH_C,
        &g_scsi_dma.dmacfg_write_chC,
        &SCSI_DMA_PIO->txf[SCSI_DATA_SM],
        NULL,
        1, false);
}

static void start_dma_write()
{
    if (g_scsi_dma.app_bytes <= g_scsi_dma.dma_bytes)
    {
        // Buffer has been fully processed, swap it
        g_scsi_dma.dma_bytes = 0;
        g_scsi_dma.app_buf = g_scsi_dma.next_app_buf;
        g_scsi_dma.app_bytes = g_scsi_dma.next_app_bytes;
        g_scsi_dma.next_app_buf = 0;
        g_scsi_dma.next_app_bytes = 0;
    }

    // Check if we are all done.
    // From SCSIDMA_WRITE_DONE state we can either go to IDLE in stopWrite()
    // or back to WRITE in startWrite().
    uint32_t bytes_to_send = g_scsi_dma.app_bytes - g_scsi_dma.dma_bytes;
    if (bytes_to_send == 0)
    {
        g_scsi_dma_state = SCSIDMA_WRITE_DONE;
        return;
    }

    uint8_t *src_buf = &g_scsi_dma.app_buf[g_scsi_dma.dma_bytes];
    g_scsi_dma.dma_bytes += bytes_to_send;

    // Start DMA from current buffer to parity generator
    dma_channel_configure(SCSI_DMA_CH_A,
        &g_scsi_dma.dmacfg_write_chA,
        &SCSI_DMA_PIO->txf[SCSI_PARITY_SM],
        src_buf,
        bytes_to_send,
        true
    );
}

void scsi_accel_rp2040_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag)
{
    // Any read requests should be matched with a stopRead()
    assert(g_scsi_dma_state != SCSIDMA_READ && g_scsi_dma_state != SCSIDMA_READ_DONE);

    uint32_t saved_irq = save_and_disable_interrupts();
    if (g_scsi_dma_state == SCSIDMA_WRITE)
    {
        if (!g_scsi_dma.next_app_buf && data == g_scsi_dma.app_buf + g_scsi_dma.app_bytes)
        {
            // Combine with currently running request
            g_scsi_dma.app_bytes += count;
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
    restore_interrupts(saved_irq);

    // Check if the request was combined
    if (count == 0) return;

    if (g_scsi_dma_state != SCSIDMA_IDLE && g_scsi_dma_state != SCSIDMA_WRITE_DONE)
    {
        // Wait for previous request to finish
        scsi_accel_rp2040_finishWrite(resetFlag);
        if (*resetFlag)
        {
            return;
        }
    }

    bool must_reconfig_gpio = (g_scsi_dma_state == SCSIDMA_IDLE);
    g_scsi_dma_state = SCSIDMA_WRITE;
    g_scsi_dma.app_buf = (uint8_t*)data;
    g_scsi_dma.app_bytes = count;
    g_scsi_dma.dma_bytes = 0;
    g_scsi_dma.next_app_buf = 0;
    g_scsi_dma.next_app_bytes = 0;

    if (must_reconfig_gpio)
    {
        SCSI_ENABLE_DATA_OUT();

        if (g_scsi_dma.syncOffset == 0)
        {
            // Asynchronous write
            config_parity_sm_for_write();
            pio_sm_init(SCSI_DMA_PIO, SCSI_DATA_SM, g_scsi_dma.pio_offset_async_write, &g_scsi_dma.pio_cfg_async_write);
            scsidma_config_gpio();

            pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, true);
            pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_PARITY_SM, true);
        }
        else
        {
            // Synchronous write
            // Data state machine writes data to SCSI bus and dummy bits to its RX fifo.
            // Sync state machine empties the dummy bits every time ACK is received, to control the transmit pace.
            config_parity_sm_for_write();
            pio_sm_init(SCSI_DMA_PIO, SCSI_DATA_SM, g_scsi_dma.pio_offset_sync_write, &g_scsi_dma.pio_cfg_sync_write);
            pio_sm_init(SCSI_DMA_PIO, SCSI_SYNC_SM, g_scsi_dma.pio_offset_sync_write_pacer, &g_scsi_dma.pio_cfg_sync_write_pacer);
            scsidma_config_gpio();

            // Prefill RX fifo to set the syncOffset
            for (int i = 0; i < g_scsi_dma.syncOffsetPreload; i++)
            {
                pio_sm_exec(SCSI_DMA_PIO, SCSI_DATA_SM,
                    pio_encode_push(false, false) | pio_encode_sideset(1, 1));
            }

            // Fill the pacer TX fifo
            // DMA should start transferring only after ACK pulses are received
            for (int i = 0; i < 4; i++)
            {
                pio_sm_put(SCSI_DMA_PIO, SCSI_SYNC_SM, 0);
            }

            // Fill the pacer OSR
            pio_sm_exec(SCSI_DMA_PIO, SCSI_SYNC_SM,
                pio_encode_mov(pio_osr, pio_null));

            // Start DMA transfer to move dummy bits to write pacer
            dma_channel_configure(SCSI_DMA_CH_D,
                &g_scsi_dma.dmacfg_write_chD,
                &SCSI_DMA_PIO->txf[SCSI_SYNC_SM],
                &SCSI_DMA_PIO->rxf[SCSI_DATA_SM],
                0xFFFFFFFF,
                true
            );

            // Enable state machines
            pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, true);
            pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, true);
            pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_PARITY_SM, true);
        }

        dma_channel_set_irq0_enabled(SCSI_DMA_CH_A, true);
    }

    start_dma_write();
}

bool scsi_accel_rp2040_isWriteFinished(const uint8_t* data)
{
    // Check if everything has completed
    if (g_scsi_dma_state == SCSIDMA_IDLE || g_scsi_dma_state == SCSIDMA_WRITE_DONE)
    {
        return true;
    }

    if (!data)
        return false;

    // Check if this data item is still in queue.
    bool finished = true;
    uint32_t saved_irq = save_and_disable_interrupts();
    if (data >= g_scsi_dma.app_buf &&
        data < g_scsi_dma.app_buf + g_scsi_dma.app_bytes &&
        (uint32_t)data >= dma_hw->ch[SCSI_DMA_CH_A].al1_read_addr)
    {
        finished = false; // In current transfer
    }
    else if (data >= g_scsi_dma.next_app_buf &&
            data < g_scsi_dma.next_app_buf + g_scsi_dma.next_app_bytes)
    {
        finished = false; // In queued transfer
    }
    restore_interrupts(saved_irq);

    return finished;
}

// Once DMA has finished, check if all PIO queues have been drained
static bool scsi_accel_rp2040_isWriteDone()
{
    // Check if data is still waiting in PIO FIFO
    if (!pio_sm_is_tx_fifo_empty(SCSI_DMA_PIO, SCSI_PARITY_SM) ||
        !pio_sm_is_rx_fifo_empty(SCSI_DMA_PIO, SCSI_PARITY_SM) ||
        !pio_sm_is_tx_fifo_empty(SCSI_DMA_PIO, SCSI_DATA_SM))
    {
        return false;
    }

    if (g_scsi_dma.syncOffset > 0)
    {
        // Check if all bytes of synchronous write have been acknowledged
        if (pio_sm_get_rx_fifo_level(SCSI_DMA_PIO, SCSI_DATA_SM) > g_scsi_dma.syncOffsetPreload)
            return false;
    }
    else
    {
        // Check if state machine has written out its OSR
        if (pio_sm_get_pc(SCSI_DMA_PIO, SCSI_DATA_SM) != g_scsi_dma.pio_offset_async_write)
            return false;
    }

    // Check if ACK of the final byte has finished
    if (SCSI_IN(ACK))
        return false;

    return true;
}

static void scsi_accel_rp2040_stopWrite(volatile int *resetFlag)
{
    // Wait for TX fifo to be empty and ACK to go high
    // For synchronous writes wait for all ACKs to be received also
    uint32_t start = millis();
    while (!scsi_accel_rp2040_isWriteDone() && !*resetFlag)
    {
        if ((uint32_t)(millis() - start) > 5000)
        {
            logmsg("scsi_accel_rp2040_stopWrite() timeout");
            scsi_accel_log_state();
            *resetFlag = 1;
            break;
        }
    }

    dma_channel_abort(SCSI_DMA_CH_A);
    dma_channel_abort(SCSI_DMA_CH_B);
    dma_channel_abort(SCSI_DMA_CH_C);
    dma_channel_abort(SCSI_DMA_CH_D);
    dma_channel_set_irq0_enabled(SCSI_DMA_CH_A, false);
    g_scsi_dma_state = SCSIDMA_IDLE;
    SCSI_RELEASE_DATA_REQ();
    scsidma_config_gpio();
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_PARITY_SM, false);
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, false);
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, false);
}

void scsi_accel_rp2040_finishWrite(volatile int *resetFlag)
{
    uint32_t start = millis();
    while (g_scsi_dma_state != SCSIDMA_IDLE && !*resetFlag)
    {
        if ((uint32_t)(millis() - start) > 5000)
        {
            logmsg("scsi_accel_rp2040_finishWrite() timeout");
            scsi_accel_log_state();
            *resetFlag = 1;
            break;
        }

        if (g_scsi_dma_state == SCSIDMA_WRITE_DONE || *resetFlag)
        {
            // DMA done, wait for PIO to finish also and reconfig GPIO.
            scsi_accel_rp2040_stopWrite(resetFlag);
        }
    }
}

/****************************************/
/* Accelerated reads from SCSI bus      */
/****************************************/

// Load the SCSI read state machine with the address of the parity lookup table.
// Also sets up DMA channels B, C and D
static void config_parity_sm_for_read()
{
    // Configure parity check state machine
    pio_sm_init(SCSI_DMA_PIO, SCSI_PARITY_SM, g_scsi_dma.pio_offset_read_parity, &g_scsi_dma.pio_cfg_read_parity);

    // Load base address to state machine register X
    uint32_t addrbase = (uint32_t)&g_scsi_parity_check_lookup[0];
    assert((addrbase & 0x3FF) == 0);
    pio_sm_init(SCSI_DMA_PIO, SCSI_DATA_SM, g_scsi_dma.pio_offset_read, &g_scsi_dma.pio_cfg_read);
    pio_sm_put(SCSI_DMA_PIO, SCSI_DATA_SM, addrbase >> 10);
    pio_sm_exec(SCSI_DMA_PIO, SCSI_DATA_SM, pio_encode_pull(false, false) | pio_encode_sideset(1, 1));
    pio_sm_exec(SCSI_DMA_PIO, SCSI_DATA_SM, pio_encode_mov(pio_y, pio_osr) | pio_encode_sideset(1, 1));

    // For synchronous mode, the REQ pin is driven by SCSI_SYNC_SM, so disable it in SCSI_DATA_SM
    if (g_scsi_dma.syncOffset > 0)
    {
        pio_sm_set_sideset_pins(SCSI_DMA_PIO, SCSI_DATA_SM, 0);
    }

    // DMA channel B will read g_scsi_parity_check_lookup and write to scsi_read_parity PIO.
    dma_channel_configure(SCSI_DMA_CH_B,
        &g_scsi_dma.dmacfg_read_chB,
        &SCSI_DMA_PIO->txf[SCSI_PARITY_SM],
        NULL,
        1, false);

    // DMA channel C will copy addresses from data PIO to DMA channel B read address register.
    // It is triggered by the data SM RX FIFO request.
    // This triggers channel B by writing to READ_ADDR_TRIG
    // Channel B chaining re-enables this channel.
    dma_channel_configure(SCSI_DMA_CH_C,
        &g_scsi_dma.dmacfg_read_chC,
        &dma_hw->ch[SCSI_DMA_CH_B].al3_read_addr_trig,
        &SCSI_DMA_PIO->rxf[SCSI_DATA_SM],
        1, true);

    if (g_scsi_dma.syncOffset == 0)
    {
        // DMA channel D will copy dummy words to scsi_accel_read PIO to set the number
        // of bytes to transfer.
        static const uint32_t dummy = 0;
        dma_channel_configure(SCSI_DMA_CH_D,
            &g_scsi_dma.dmacfg_read_chD,
            &SCSI_DMA_PIO->txf[SCSI_DATA_SM],
            &dummy,
            0, false);
    }
    else
    {
        pio_sm_init(SCSI_DMA_PIO, SCSI_SYNC_SM, g_scsi_dma.pio_offset_sync_read_pacer, &g_scsi_dma.pio_cfg_sync_read_pacer);

        // DMA channel D will copy words from scsi_sync_read_pacer to scsi_accel_read PIO
        // to control the offset between REQ pulses sent and ACK pulses received.
        dma_channel_configure(SCSI_DMA_CH_D,
            &g_scsi_dma.dmacfg_read_chD,
            &SCSI_DMA_PIO->txf[SCSI_DATA_SM],
            &SCSI_DMA_PIO->rxf[SCSI_SYNC_SM],
            0, false);
    }

    // Clear PIO IRQ flag that is used to detect parity error
    SCSI_DMA_PIO->irq = 1;
}

static void start_dma_read()
{
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_PARITY_SM, false);
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, false);
    pio_sm_clear_fifos(SCSI_DMA_PIO, SCSI_PARITY_SM);
    pio_sm_clear_fifos(SCSI_DMA_PIO, SCSI_DATA_SM);

    if (g_scsi_dma.app_bytes <= g_scsi_dma.dma_bytes)
    {
        // Buffer has been fully processed, swap it
        g_scsi_dma.dma_bytes = 0;
        g_scsi_dma.app_buf = g_scsi_dma.next_app_buf;
        g_scsi_dma.app_bytes = g_scsi_dma.next_app_bytes;
        g_scsi_dma.next_app_buf = 0;
        g_scsi_dma.next_app_bytes = 0;
    }

    // Check if we are all done.
    // From SCSIDMA_READ_DONE state we can either go to IDLE in stopRead()
    // or back to READ in startWrite().
    uint32_t bytes_to_read = g_scsi_dma.app_bytes - g_scsi_dma.dma_bytes;
    if (bytes_to_read == 0)
    {
        g_scsi_dma_state = SCSIDMA_READ_DONE;
        return;
    }

    if (g_scsi_dma.syncOffset == 0)
    {
        // Start sending dummy words to scsi_accel_read state machine
        dma_channel_set_trans_count(SCSI_DMA_CH_D, bytes_to_read, true);
    }
    else
    {
        // Set number of bytes to receive to the scsi_sync_read_pacer state machine register X
        pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, false);
        hw_clear_bits(&SCSI_DMA_PIO->sm[SCSI_SYNC_SM].shiftctrl, PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS);
        pio_sm_put(SCSI_DMA_PIO, SCSI_SYNC_SM, bytes_to_read - 1);
        pio_sm_exec(SCSI_DMA_PIO, SCSI_SYNC_SM, pio_encode_pull(false, false) | pio_encode_sideset(1, 1));
        pio_sm_exec(SCSI_DMA_PIO, SCSI_SYNC_SM, pio_encode_mov(pio_x, pio_osr) | pio_encode_sideset(1, 1));
        hw_set_bits(&SCSI_DMA_PIO->sm[SCSI_SYNC_SM].shiftctrl, PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS);

        // Prefill FIFOs to get correct syncOffset
        int prefill = 12 - g_scsi_dma.syncOffset;

        // Always at least 1 word to avoid race condition between REQ and ACK pulses
        if (prefill < 1) prefill = 1;

        // Up to 4 words in SCSI_DATA_SM TX fifo
        for (int i = 0; i < 4 && prefill > 0; i++)
        {
            pio_sm_put(SCSI_DMA_PIO, SCSI_DATA_SM, 0);
            prefill--;
        }

        // Up to 8 words in SCSI_SYNC_SM RX fifo
        for (int i = 0; i < 8 && prefill > 0; i++)
        {
            pio_sm_exec(SCSI_DMA_PIO, SCSI_SYNC_SM, pio_encode_push(false, false) | pio_encode_sideset(1, 1));
            prefill--;
        }

        pio_sm_exec(SCSI_DMA_PIO, SCSI_SYNC_SM, pio_encode_jmp(g_scsi_dma.pio_offset_sync_read_pacer) | pio_encode_sideset(1, 1));

        // Start transfers
        dma_channel_set_trans_count(SCSI_DMA_CH_D, bytes_to_read, true);
    }

    // Start DMA to fill the destination buffer
    uint8_t *dest_buf = &g_scsi_dma.app_buf[g_scsi_dma.dma_bytes];
    g_scsi_dma.dma_bytes += bytes_to_read;
    dma_channel_configure(SCSI_DMA_CH_A,
        &g_scsi_dma.dmacfg_read_chA,
        dest_buf,
        &SCSI_DMA_PIO->rxf[SCSI_PARITY_SM],
        bytes_to_read,
        true
    );

    // Ready to start the data and parity check state machines
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_PARITY_SM, true);
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, true);

    if (g_scsi_dma.syncOffset > 0)
    {
        // Start sending REQ pulses
        pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, true);
    }
}

void scsi_accel_rp2040_startRead(uint8_t *data, uint32_t count, int *parityError, volatile int *resetFlag)
{
    // Any write requests should be matched with a stopWrite()
    assert(g_scsi_dma_state != SCSIDMA_WRITE && g_scsi_dma_state != SCSIDMA_WRITE_DONE);

    uint32_t saved_irq = save_and_disable_interrupts();
    if (g_scsi_dma_state == SCSIDMA_READ)
    {
        if (!g_scsi_dma.next_app_buf && data == g_scsi_dma.app_buf + g_scsi_dma.app_bytes)
        {
            // Combine with currently running request
            g_scsi_dma.app_bytes += count;
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
    restore_interrupts(saved_irq);

    // Check if the request was combined
    if (count == 0) return;

    if (g_scsi_dma_state != SCSIDMA_IDLE && g_scsi_dma_state != SCSIDMA_READ_DONE)
    {
        // Wait for previous request to finish
        scsi_accel_rp2040_finishRead(NULL, 0, parityError, resetFlag);
        if (*resetFlag)
        {
            return;
        }
    }

    bool must_reconfig_gpio = (g_scsi_dma_state == SCSIDMA_IDLE);
    g_scsi_dma_state = SCSIDMA_READ;
    g_scsi_dma.app_buf = (uint8_t*)data;
    g_scsi_dma.app_bytes = count;
    g_scsi_dma.dma_bytes = 0;
    g_scsi_dma.next_app_buf = 0;
    g_scsi_dma.next_app_bytes = 0;

    if (must_reconfig_gpio)
    {
        config_parity_sm_for_read();
        scsidma_config_gpio();
        dma_channel_set_irq0_enabled(SCSI_DMA_CH_A, true);
    }

    start_dma_read();
}

bool scsi_accel_rp2040_isReadFinished(const uint8_t* data)
{
    // Check if everything has completed
    if (g_scsi_dma_state == SCSIDMA_IDLE || g_scsi_dma_state == SCSIDMA_READ_DONE)
    {
        return true;
    }

    if (!data)
        return false;

    // Check if this data item is still in queue.
    bool finished = true;
    uint32_t saved_irq = save_and_disable_interrupts();
    if (data >= g_scsi_dma.app_buf &&
        data < g_scsi_dma.app_buf + g_scsi_dma.app_bytes &&
        (uint32_t)data >= dma_hw->ch[SCSI_DMA_CH_A].write_addr)
    {
        finished = false; // In current transfer
    }
    else if (data >= g_scsi_dma.next_app_buf &&
            data < g_scsi_dma.next_app_buf + g_scsi_dma.next_app_bytes)
    {
        finished = false; // In queued transfer
    }
    restore_interrupts(saved_irq);

    return finished;
}

static void scsi_accel_rp2040_stopRead()
{
    dma_channel_abort(SCSI_DMA_CH_A);
    dma_channel_abort(SCSI_DMA_CH_B);
    dma_channel_abort(SCSI_DMA_CH_C);
    dma_channel_abort(SCSI_DMA_CH_D);
    dma_channel_set_irq0_enabled(SCSI_DMA_CH_A, false);
    g_scsi_dma_state = SCSIDMA_IDLE;
    SCSI_RELEASE_DATA_REQ();
    scsidma_config_gpio();
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_PARITY_SM, false);
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, false);
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, false);
}

void scsi_accel_rp2040_finishRead(const uint8_t *data, uint32_t count, int *parityError, volatile int *resetFlag)
{
    uint32_t start = millis();
    const uint8_t *query_addr = (data ? (data + count - 1) : NULL);
    while (!scsi_accel_rp2040_isReadFinished(query_addr) && !*resetFlag)
    {
        if ((uint32_t)(millis() - start) > 5000)
        {
            logmsg("scsi_accel_rp2040_finishRead timeout");
            scsi_accel_log_state();
            *resetFlag = 1;
            break;
        }
    }

    if (g_scsi_dma_state == SCSIDMA_READ_DONE || *resetFlag)
    {
        // This was last buffer, release bus
        scsi_accel_rp2040_stopRead();
    }

    // Check if any parity errors have been detected during the transfer so far
    if (parityError != NULL && (SCSI_DMA_PIO->irq & 1))
    {
        dbgmsg("scsi_accel_rp2040_finishRead(", bytearray(data, count), ") detected parity error");
        *parityError = true;
    }
}

/*******************************************************/
/* Write SCSI PIO program timings and ACK pin          */
/*******************************************************/
static void blue_pio_remove_program(PIO pio, const pio_program_t *program, uint loaded_offset, bool &removed)
{
    if (!removed)
    {
        pio_remove_program(pio, program, loaded_offset);
        removed = true;
    }
}

static int pio_add_scsi_accel_async_write_program()
{
    blue_pio_remove_program(SCSI_DMA_PIO,
        &scsi_accel_async_write_program,
        g_scsi_dma.pio_offset_async_write,
        g_scsi_dma.pio_removed_async_write);

    uint16_t rewrote_instructions[sizeof(scsi_accel_async_write_program_instructions)/sizeof(scsi_accel_async_write_program_instructions[0])];
    pio_program rewrote_program = {rewrote_instructions,
        scsi_accel_async_write_program.length,
        scsi_accel_async_write_program.origin,
        scsi_accel_async_write_program.pio_version};

    memcpy(rewrote_instructions,
        scsi_accel_async_write_program_instructions,
        sizeof(scsi_accel_async_write_program_instructions));

    // out null, 23         side 1  [0] ;[REQ_DLY-2]      ; Discard unused bits, wait for data preset time
    uint8_t delay = g_bluescsi_timings->scsi.req_delay - 2;
    assert( delay <= 0xF);
    rewrote_instructions[2] |= pio_encode_delay(delay);
    // wait 1 gpio ACK      side 1      ; Wait for ACK to be inactive
    rewrote_instructions[3] = pio_encode_wait_gpio(true, SCSI_IN_ACK) | pio_encode_sideset(1, 1);
    // wait 0 gpio ACK      side 0      ; Assert REQ, wait for ACK low
    rewrote_instructions[4] = pio_encode_wait_gpio(false, SCSI_IN_ACK) | pio_encode_sideset(1, 0);

    g_scsi_dma.pio_removed_async_write = false;
    return pio_add_program(SCSI_DMA_PIO, &rewrote_program);
}

static int pio_add_scsi_accel_read_program()
{
    blue_pio_remove_program(SCSI_DMA_PIO,
        &scsi_accel_read_program,
        g_scsi_dma.pio_offset_read,
        g_scsi_dma.pio_removed_read);

    uint16_t rewrote_instructions[sizeof(scsi_accel_read_program_instructions)/sizeof(scsi_accel_read_program_instructions[0])];

    pio_program rewrote_program = {
        rewrote_instructions,
        scsi_accel_read_program.length,
        scsi_accel_read_program.origin,
        scsi_accel_read_program.pio_version};

    memcpy(rewrote_instructions,
        scsi_accel_read_program_instructions,
        sizeof(scsi_accel_read_program_instructions));

    // wait 1 gpio ACK             side 1  ; Wait for ACK high
    rewrote_instructions[1] = pio_encode_wait_gpio(true, SCSI_IN_ACK) | pio_encode_sideset(1, 1);
    // wait 0 gpio ACK             side 0  ; Assert REQ, wait for ACK low
    rewrote_instructions[3] = pio_encode_wait_gpio(false, SCSI_IN_ACK) | pio_encode_sideset(1, 0);
    g_scsi_dma.pio_removed_read = false;
    return pio_add_program(SCSI_DMA_PIO, &rewrote_program);
}

static int pio_add_scsi_sync_write_pacer_program()
{
    blue_pio_remove_program(SCSI_DMA_PIO,
        &scsi_sync_write_pacer_program,
        g_scsi_dma.pio_offset_sync_write_pacer,
        g_scsi_dma.pio_removed_sync_write_pacer);

    uint16_t rewrote_instructions[sizeof(scsi_sync_write_pacer_program_instructions)/sizeof(scsi_sync_write_pacer_program_instructions[0])];

    pio_program rewrote_program = {
        rewrote_instructions,
        scsi_sync_write_pacer_program.length,
        scsi_sync_write_pacer_program.origin,
        scsi_sync_write_pacer_program.pio_version};

    memcpy(rewrote_instructions,
        scsi_sync_write_pacer_program_instructions,
        sizeof(scsi_sync_write_pacer_program_instructions));

    // wait 1 gpio ACK
    rewrote_instructions[0] = pio_encode_wait_gpio(true, SCSI_IN_ACK);
    // wait 0 gpio ACK   ; Wait for falling edge on ACK
    rewrote_instructions[1] = pio_encode_wait_gpio(false, SCSI_IN_ACK);
    g_scsi_dma.pio_removed_sync_write_pacer = false;
    return pio_add_program(SCSI_DMA_PIO, &rewrote_program);
}

static int pio_add_scsi_parity_program()
{
    g_scsi_dma.pio_removed_parity = false;
    return pio_add_program(SCSI_DMA_PIO, &scsi_parity_program);
}

static int pio_add_scsi_sync_read_pacer_program()
{
    g_scsi_dma.pio_removed_sync_read_pacer = false;
    return pio_add_program(SCSI_DMA_PIO, &scsi_sync_read_pacer_program);
}

static int pio_add_scsi_read_parity_program()
{
    g_scsi_dma.pio_removed_read_parity = false;
    return pio_add_program(SCSI_DMA_PIO, &scsi_read_parity_program);
}

static int pio_add_scsi_sync_write_program()
{
    g_scsi_dma.pio_removed_sync_write = false;
    return pio_add_program(SCSI_DMA_PIO, &scsi_sync_write_program);
}
/*******************************************************/
/* Initialization functions common to read/write       */
/*******************************************************/

static void scsi_dma_irq()
{
#ifndef ENABLE_AUDIO_OUTPUT_SPDIF
    dma_hw->ints0 = (1 << SCSI_DMA_CH_A);
#else
    // see audio_spdif.h for whats going on here
    if (dma_hw->intr & (1 << SCSI_DMA_CH_A)) {
        dma_hw->ints0 = (1 << SCSI_DMA_CH_A);
    } else {
        audio_dma_irq();
        return;
    }
#endif

    scsidma_state_t state = g_scsi_dma_state;
    if (state == SCSIDMA_WRITE)
    {
        // Start writing from next buffer, if any, or set state to SCSIDMA_WRITE_DONE
        start_dma_write();
    }
    else if (state == SCSIDMA_READ)
    {
        // Start reading into next buffer, if any, or set state to SCSIDMA_READ_DONE
        start_dma_read();
    }
}

// Select GPIO from PIO peripheral or from software controlled SIO
static void scsidma_config_gpio()
{
    if (g_scsi_dma_state == SCSIDMA_IDLE)
    {
        iobank0_hw->io[SCSI_IO_DB0].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_IO_DB1].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_IO_DB2].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_IO_DB3].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_IO_DB4].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_IO_DB5].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_IO_DB6].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_IO_DB7].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_IO_DBP].ctrl  = GPIO_FUNC_SIO;
        iobank0_hw->io[SCSI_OUT_REQ].ctrl = GPIO_FUNC_SIO;
    }
    else if (g_scsi_dma_state == SCSIDMA_WRITE)
    {
        // Make sure the initial state of all pins is high and output
        pio_sm_set_pins(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ));
        pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DB0, 9, true);
        pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_OUT_REQ, 1, true);

        io_bank0_hw->io[SCSI_IO_DB0].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_IO_DB1].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_IO_DB2].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_IO_DB3].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_IO_DB4].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_IO_DB5].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_IO_DB6].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_IO_DB7].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_IO_DBP].ctrl  = GPIO_FUNC_PIO0;
        io_bank0_hw->io[SCSI_OUT_REQ].ctrl = GPIO_FUNC_PIO0;
    }
    else if (g_scsi_dma_state == SCSIDMA_READ)
    {
        if (g_scsi_dma.syncOffset == 0)
        {
            // Asynchronous read
            // Data bus as input, REQ pin as output
            pio_sm_set_pins(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ));
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DB0, 9, false);
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_OUT_REQ, 1, true);
        }
        else
        {
            // Synchronous read, REQ pin is written by SYNC_SM
            pio_sm_set_pins(SCSI_DMA_PIO, SCSI_SYNC_SM, SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ));
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DB0, 9, false);
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_SYNC_SM, SCSI_OUT_REQ, 1, true);
        }

        io_bank0_hw->io[SCSI_IO_DB0].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_IO_DB1].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_IO_DB2].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_IO_DB3].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_IO_DB4].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_IO_DB5].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_IO_DB6].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_IO_DB7].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_IO_DBP].ctrl  = GPIO_FUNC_SIO;
        io_bank0_hw->io[SCSI_OUT_REQ].ctrl = GPIO_FUNC_PIO0;
    }
}

void scsi_accel_rp2040_init()
{
    g_scsi_dma_state = SCSIDMA_IDLE;
    scsidma_config_gpio();

    static bool first_init = true;
    if (first_init)
    {
        g_scsi_dma.pio_removed_parity = true;
        g_scsi_dma.pio_removed_async_write = true;
        g_scsi_dma.pio_removed_sync_write_pacer = true;
        g_scsi_dma.pio_removed_sync_write = true;
        g_scsi_dma.pio_removed_read = true;
        g_scsi_dma.pio_removed_read_parity = true;
        g_scsi_dma.pio_removed_sync_read_pacer = true;
        first_init = false;
    }

    if (g_channels_claimed) {
        // Un-claim all SCSI state machines
        pio_sm_unclaim(SCSI_DMA_PIO, SCSI_PARITY_SM);
        pio_sm_unclaim(SCSI_DMA_PIO, SCSI_DATA_SM);
        pio_sm_unclaim(SCSI_DMA_PIO, SCSI_SYNC_SM);

        // Remove all SCSI programs
        blue_pio_remove_program(SCSI_DMA_PIO, &scsi_parity_program, g_scsi_dma.pio_offset_parity, g_scsi_dma.pio_removed_parity);
        blue_pio_remove_program(SCSI_DMA_PIO, &scsi_accel_async_write_program, g_scsi_dma.pio_offset_async_write, g_scsi_dma.pio_removed_async_write);
        blue_pio_remove_program(SCSI_DMA_PIO, &scsi_sync_write_pacer_program, g_scsi_dma.pio_offset_sync_write_pacer, g_scsi_dma.pio_removed_sync_write_pacer);
        blue_pio_remove_program(SCSI_DMA_PIO, &scsi_accel_read_program, g_scsi_dma.pio_offset_read, g_scsi_dma.pio_removed_read);
        blue_pio_remove_program(SCSI_DMA_PIO, &scsi_sync_read_pacer_program, g_scsi_dma.pio_offset_sync_read_pacer, g_scsi_dma.pio_removed_sync_read_pacer);
        blue_pio_remove_program(SCSI_DMA_PIO, &scsi_read_parity_program, g_scsi_dma.pio_offset_read_parity, g_scsi_dma.pio_removed_read_parity);
        blue_pio_remove_program(SCSI_DMA_PIO, &scsi_sync_write_program, g_scsi_dma.pio_offset_sync_write, g_scsi_dma.pio_removed_sync_write);

        // Un-claim all SCSI DMA channels
        dma_channel_unclaim(SCSI_DMA_CH_A);
        dma_channel_unclaim(SCSI_DMA_CH_B);
        dma_channel_unclaim(SCSI_DMA_CH_C);
        dma_channel_unclaim(SCSI_DMA_CH_D);

        // Set flag to re-initialize SCSI PIO system
        g_channels_claimed = false;
    }

    if (!g_channels_claimed)
    {
        // Mark channels as being in use, unless it has been done already
        pio_sm_claim(SCSI_DMA_PIO, SCSI_PARITY_SM);
        pio_sm_claim(SCSI_DMA_PIO, SCSI_DATA_SM);
        pio_sm_claim(SCSI_DMA_PIO, SCSI_SYNC_SM);
        dma_channel_claim(SCSI_DMA_CH_A);
        dma_channel_claim(SCSI_DMA_CH_B);
        dma_channel_claim(SCSI_DMA_CH_C);
        dma_channel_claim(SCSI_DMA_CH_D);
        g_channels_claimed = true;
    }

    // Parity lookup generator
    g_scsi_dma.pio_offset_parity = pio_add_scsi_parity_program();
    g_scsi_dma.pio_cfg_parity = scsi_parity_program_get_default_config(g_scsi_dma.pio_offset_parity);
    sm_config_set_out_shift(&g_scsi_dma.pio_cfg_parity, true, false, 32);
    sm_config_set_in_shift(&g_scsi_dma.pio_cfg_parity, true, true, 32);

    // Asynchronous SCSI write
    g_scsi_dma.pio_offset_async_write = pio_add_scsi_accel_async_write_program();
    g_scsi_dma.pio_cfg_async_write = scsi_accel_async_write_program_get_default_config(g_scsi_dma.pio_offset_async_write);
    sm_config_set_out_pins(&g_scsi_dma.pio_cfg_async_write, SCSI_IO_DB0, 9);
    sm_config_set_sideset_pins(&g_scsi_dma.pio_cfg_async_write, SCSI_OUT_REQ);
    sm_config_set_fifo_join(&g_scsi_dma.pio_cfg_async_write, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&g_scsi_dma.pio_cfg_async_write, true, false, 32);

    // Synchronous SCSI write pacer / ACK handler
    g_scsi_dma.pio_offset_sync_write_pacer = pio_add_scsi_sync_write_pacer_program();
    g_scsi_dma.pio_cfg_sync_write_pacer = scsi_sync_write_pacer_program_get_default_config(g_scsi_dma.pio_offset_sync_write_pacer);
    sm_config_set_out_shift(&g_scsi_dma.pio_cfg_sync_write_pacer, true, true, 1);

    // Asynchronous / synchronous SCSI read
    g_scsi_dma.pio_offset_read = pio_add_scsi_accel_read_program();
    g_scsi_dma.pio_cfg_read = scsi_accel_read_program_get_default_config(g_scsi_dma.pio_offset_read);
    sm_config_set_in_pins(&g_scsi_dma.pio_cfg_read, SCSI_IO_DB0);
    sm_config_set_sideset_pins(&g_scsi_dma.pio_cfg_read, SCSI_OUT_REQ);
    sm_config_set_out_shift(&g_scsi_dma.pio_cfg_read, true, false, 32);
    sm_config_set_in_shift(&g_scsi_dma.pio_cfg_read, true, true, 32);

    // Synchronous SCSI read pacer
    g_scsi_dma.pio_offset_sync_read_pacer = pio_add_scsi_sync_read_pacer_program();
    g_scsi_dma.pio_cfg_sync_read_pacer = scsi_sync_read_pacer_program_get_default_config(g_scsi_dma.pio_offset_sync_read_pacer);
    sm_config_set_sideset_pins(&g_scsi_dma.pio_cfg_sync_read_pacer, SCSI_OUT_REQ);

    // Read parity check
    g_scsi_dma.pio_offset_read_parity = pio_add_scsi_read_parity_program();
    g_scsi_dma.pio_cfg_read_parity = scsi_read_parity_program_get_default_config(g_scsi_dma.pio_offset_read_parity);
    sm_config_set_out_shift(&g_scsi_dma.pio_cfg_read_parity, true, true, 32);
    sm_config_set_in_shift(&g_scsi_dma.pio_cfg_read_parity, true, false, 32);

    // Synchronous SCSI data writer
    g_scsi_dma.pio_offset_sync_write = pio_add_scsi_sync_write_program();
    g_scsi_dma.pio_cfg_sync_write = scsi_sync_write_program_get_default_config(g_scsi_dma.pio_offset_sync_write);
    sm_config_set_out_pins(&g_scsi_dma.pio_cfg_sync_write, SCSI_IO_DB0, 9);
    sm_config_set_sideset_pins(&g_scsi_dma.pio_cfg_sync_write, SCSI_OUT_REQ);
    sm_config_set_out_shift(&g_scsi_dma.pio_cfg_sync_write, true, true, 32);
    sm_config_set_in_shift(&g_scsi_dma.pio_cfg_sync_write, true, true, 1);


    // Create DMA channel configurations so they can be applied quickly later

    // For write to SCSI BUS:
    // Channel A: Bytes from RAM to scsi_parity PIO
    dma_channel_config cfg = dma_channel_get_default_config(SCSI_DMA_CH_A);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_PARITY_SM, true));
    g_scsi_dma.dmacfg_write_chA = cfg;

    // Channel B: Addresses from scsi_parity PIO to lookup DMA READ_ADDR register
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_B);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_PARITY_SM, false));
    g_scsi_dma.dmacfg_write_chB = cfg;

    // Channel C: Lookup from g_scsi_parity_lookup and copy to scsi_accel_async_write or scsi_sync_write PIO
    // When done, chain to channel B
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_C);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_DATA_SM, true));
    channel_config_set_chain_to(&cfg, SCSI_DMA_CH_B);
    g_scsi_dma.dmacfg_write_chC = cfg;

    // Channel D: In synchronous mode a second DMA channel is used to transfer dummy bits
    // from first state machine to second one.
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_D);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_SYNC_SM, true));
    g_scsi_dma.dmacfg_write_chD = cfg;

    // For read from SCSI BUS:
    // Channel A: Bytes from scsi_read_parity PIO to destination memory buffer
    // This takes the bottom 8 bits which is the data without parity bit.
    // Triggered by scsi_read_parity RX FIFO.
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_A);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_PARITY_SM, false));
    g_scsi_dma.dmacfg_read_chA = cfg;

    // Channel B: Lookup from g_scsi_parity_check_lookup and copy to scsi_read_parity PIO
    // Triggered by channel C writing to READ_ADDR_TRIG
    // Re-enables channel C by chaining after done.
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_B);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, DREQ_FORCE);
    channel_config_set_chain_to(&cfg, SCSI_DMA_CH_C);
    cfg.ctrl |= DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS;
    g_scsi_dma.dmacfg_read_chB = cfg;

    // Channel C: Addresses from scsi_read PIO to channel B READ_ADDR register
    // A single transfer starts when PIO RX FIFO has data.
    // The DMA channel is re-enabled by channel B chaining.
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_C);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_high_priority(&cfg, true);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_DATA_SM, false));
    g_scsi_dma.dmacfg_read_chC = cfg;

    // Channel D: In synchronous mode a second DMA channel is used to transfer dummy words
    // from first state machine to second one to control the pace of data transfer.
    // In asynchronous mode this just transfers words to control the number of bytes.
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_D);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_DATA_SM, true));
    g_scsi_dma.dmacfg_read_chD = cfg;

    // Interrupts are used for data buffer swapping
    irq_set_exclusive_handler(DMA_IRQ_0, scsi_dma_irq);
    irq_set_enabled(DMA_IRQ_0, true);
}

bool scsi_accel_rp2040_setSyncMode(int syncOffset, int syncPeriod)
{
    if (g_scsi_dma_state != SCSIDMA_IDLE)
    {
        logmsg("ERROR: SCSI DMA was in state ", (int)g_scsi_dma_state, " when changing sync mode, forcing bus reset");
        scsi_accel_log_state();
        return false;
    }

    if (syncOffset != g_scsi_dma.syncOffset || syncPeriod != g_scsi_dma.syncPeriod)
    {
        g_scsi_dma.syncOffset = syncOffset;
        g_scsi_dma.syncPeriod = syncPeriod;

        if (syncOffset > 0)
        {
            // Set up offset amount to PIO state machine configs.
            // The RX fifo of scsi_sync_write has 4 slots.
            // We can preload it with 0-3 items and set the autopush threshold 1, 2, 4 ... 32
            // to act as a divider. This allows offsets 1 to 128 bytes.
            // SCSI2SD code currently only uses offsets up to 15.
            if (syncOffset <= 4)
            {
                g_scsi_dma.syncOffsetDivider = 1;
                g_scsi_dma.syncOffsetPreload = 5 - syncOffset;
            }
            else if (syncOffset <= 8)
            {
                g_scsi_dma.syncOffsetDivider = 2;
                g_scsi_dma.syncOffsetPreload = 5 - syncOffset / 2;
            }
            else if (syncOffset <= 16)
            {
                g_scsi_dma.syncOffsetDivider = 4;
                g_scsi_dma.syncOffsetPreload = 5 - syncOffset / 4;
            }
            else
            {
                g_scsi_dma.syncOffsetDivider = 4;
                g_scsi_dma.syncOffsetPreload = 0;
            }

            // To properly detect when all bytes have been ACKed,
            // we need at least one vacant slot in the FIFO.
            if (g_scsi_dma.syncOffsetPreload > 3)
                g_scsi_dma.syncOffsetPreload = 3;

            sm_config_set_out_shift(&g_scsi_dma.pio_cfg_sync_write_pacer, true, true, g_scsi_dma.syncOffsetDivider);
            sm_config_set_in_shift(&g_scsi_dma.pio_cfg_sync_write, true, true, g_scsi_dma.syncOffsetDivider);

            // Set up the timing parameters to PIO program
            // The scsi_sync_write PIO program consists of three instructions.
            // The delays are in clock cycles, each taking 6.66 ns. (@150 MHz)
            // delay0: Delay from data write to REQ assertion (data setup)
            // delay1: Delay from REQ assert to REQ deassert (req pulse width)
            // delay2: Delay from REQ deassert to data write (negation period)
            // see timings.c for delay periods in clock cycles
            int delay0, delay1, delay2;

            // setup timings for sync_read_pacer
            // total period = rdelay0 + redelay1
            // rdelay0: wait for transfer period (total_period +  rtotal_period_adjust - rdelay1)
            // rdelay1: req assert period
            int rdelay1;

            uint32_t up_rounder = g_bluescsi_timings->scsi.clk_period_ps / 2 + 1;
            uint32_t delay_in_ps = (syncPeriod * 4) * 1000;
            // This is the period in clock cycles rounded up
            int totalPeriod = (delay_in_ps + up_rounder) / g_bluescsi_timings->scsi.clk_period_ps;
            int rtotalPeriod = totalPeriod;
            int clkdiv = 0;
            if (syncPeriod < 25)
            {
                // Fast-20 SCSI timing: 15 ns assertion period
                // The hardware rise and fall time require some extra delay,
                // These delays are in addition to the 1 cycle that the PIO takes to execute the instruction
                totalPeriod += g_bluescsi_timings->scsi_20.total_period_adjust;
                delay0 = g_bluescsi_timings->scsi_20.delay0; //Data setup time, should be min 11.5ns according to the spec for FAST-20
                delay1 = g_bluescsi_timings->scsi_20.delay1; //pulse width, should be min 15ns according to the spec for FAST-20
                delay2 = totalPeriod - delay0 - delay1 - 3;  //Data hold time, should be min 16.5ns from REQ falling edge according to the spec for FAST-20
                if (delay2 < 0) delay2 = 0;
                if (delay2 > 15) delay2 = 15;
                rdelay1 = g_bluescsi_timings->scsi_20.rdelay1;
                rtotalPeriod += g_bluescsi_timings->scsi_20.rtotal_period_adjust;
            }
            else if (syncPeriod < 50 )
            {
                // Fast-10 SCSI timing: 30 ns assertion period, 25 ns skew delay
                // The hardware rise and fall time require some extra delay,
                totalPeriod += g_bluescsi_timings->scsi_10.total_period_adjust;
                delay0 = g_bluescsi_timings->scsi_10.delay0; // 4;
                delay1 = g_bluescsi_timings->scsi_10.delay1; // 6;
                delay2 = totalPeriod - delay0 - delay1 - 3;
                if (delay2 < 0) delay2 = 0;
                if (delay2 > 15) delay2 = 15;
                rdelay1 = g_bluescsi_timings->scsi_10.rdelay1;
                rtotalPeriod += g_bluescsi_timings->scsi_10.rtotal_period_adjust;
            }
            else
            {
                // Slow SCSI timing: 90 ns assertion period, 55 ns skew delay
                // Delay2 must be at least 2 to keep negation period well above the 90 ns minimum
                totalPeriod += g_bluescsi_timings->scsi_5.total_period_adjust;
                delay0 = g_bluescsi_timings->scsi_5.delay0;
                delay1 = g_bluescsi_timings->scsi_5.delay1;
                delay2 = totalPeriod - delay0 - delay1 - 3;
                if (delay2 < 2) delay2 = 2;
                if (delay2 > 15) delay2 = 15;
                rdelay1 = g_bluescsi_timings->scsi_5.rdelay1;
                rtotalPeriod += g_bluescsi_timings->scsi_5.rtotal_period_adjust;
            }

            // Patch the delay values into the instructions in scsi_sync_write.
            // The code in scsi_accel.pio must have delay set to 0 for this to work correctly.
            uint16_t instr0 = scsi_sync_write_program_instructions[0] | pio_encode_delay(delay0);
            uint16_t instr1 = scsi_sync_write_program_instructions[1] | pio_encode_delay(delay1);
            uint16_t instr2 = scsi_sync_write_program_instructions[2] | pio_encode_delay(delay2);
            SCSI_DMA_PIO->instr_mem[g_scsi_dma.pio_offset_sync_write + 0] = instr0;
            SCSI_DMA_PIO->instr_mem[g_scsi_dma.pio_offset_sync_write + 1] = instr1;
            SCSI_DMA_PIO->instr_mem[g_scsi_dma.pio_offset_sync_write + 2] = instr2;

            // The DMA-based parity verification method will start dropping bytes
            // if total period for read from SCSI bus is less than 13 clock cycles.
            // Limit it to 14 to be safe.
#ifdef BLUESCSI_MCU_RP23XX
            if (rtotalPeriod < 14) rtotalPeriod = 14;
#else
            if (rtotalPeriod < 18) rtotalPeriod = 18; // RP2040 DMA is slightly slower
#endif

            // And similar patching for scsi_sync_read_pacer
            int rdelay0 = rtotalPeriod - rdelay1 - 2;
            if (rdelay0 > 15) rdelay0 = 15;
            if (rdelay0 < 0) rdelay0 = 0;
            uint16_t rinstr0 = scsi_sync_read_pacer_program_instructions[0] | pio_encode_delay(rdelay0);
            uint16_t rinstr1 = (scsi_sync_read_pacer_program_instructions[1] + g_scsi_dma.pio_offset_sync_read_pacer) | pio_encode_delay(rdelay1);
            SCSI_DMA_PIO->instr_mem[g_scsi_dma.pio_offset_sync_read_pacer + 0] = rinstr0;
            SCSI_DMA_PIO->instr_mem[g_scsi_dma.pio_offset_sync_read_pacer + 1] = rinstr1;

            if (clkdiv > 0)
            {
                // Add divider to REQ controlling programs in order to satisfy slowest
                // SCSI-5 timing requirements.
                sm_config_set_clkdiv_int_frac(&g_scsi_dma.pio_cfg_sync_read_pacer, clkdiv, 0);
                sm_config_set_clkdiv_int_frac(&g_scsi_dma.pio_cfg_sync_write, clkdiv, 0);
            }
            else
            {
                // No clock divider
                sm_config_set_clkdiv_int_frac(&g_scsi_dma.pio_cfg_sync_read_pacer, 1, 0);
                sm_config_set_clkdiv_int_frac(&g_scsi_dma.pio_cfg_sync_write, 1, 0);
            }
        }
    }

    return true;
}
