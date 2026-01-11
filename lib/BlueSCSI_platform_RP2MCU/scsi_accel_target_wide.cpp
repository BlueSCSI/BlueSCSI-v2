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

#ifdef RP2MCU_SCSI_ACCEL_WIDE

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

/* Data flow in 16-bit SCSI acceleration:
*
* 1. Application provides a buffer of bytes to send.
* 2. CPU expands the data to 32 bit GPIO words, which contain parity and 8 or 16 data bits.
* 3. DMA controller copies the bytes to PIO peripheral FIFO.
* 4. PIO peripheral handles low-level SCSI handshake and writes bytes and parity to GPIO.
*/

#include "scsi_accel_target_RP2350_wide.pio.h"

// SCSI bus write acceleration uses 2 PIO state machines:
// SM1: Write data to SCSI bus
// SM2: For synchronous mode only, count ACK pulses
#define SCSI_DMA_PIO pio0
#define SCSI_DATA_SM 1
#define SCSI_SYNC_SM 2

// SCSI bus write acceleration uses 1 or 2 DMA channels:
// A: GPIO words from RAM to scsi_accel_async_write or scsi_sync_write PIO
// B: For sync transfers, scsi_sync_write to scsi_sync_write_pacer PIO
//
// SCSI bus read acceleration uses 2 DMA channels:
// A: GPIO words from scsi_accel_read to RAM
// B: From scsi_sync_read_pacer to data state machine to trigger transfers
#ifdef BLUESCSI_NETWORK
#  define SCSI_DMA_CH_A 6
#  define SCSI_DMA_CH_B 7
#else
#  define SCSI_DMA_CH_A 10
#  define SCSI_DMA_CH_B 11
#endif

// This buffer stores GPIO words ready for transmission, or that have been received.
#define SCSI_DMA_IOBUF_SIZE 512
#define SCSI_DMA_IOBUF_COUNT 2
static uint32_t g_scsi_dma_iobuf[SCSI_DMA_IOBUF_COUNT][SCSI_DMA_IOBUF_SIZE];

static struct {
    // Spin lock for locking access to the app_buf .. next_app_bytes
    // variables that are accessed by IRQ (which may be running on core1)
    spin_lock_t *spin_lock;

    uint8_t *app_buf; // Buffer provided by application
    uint32_t app_bytes; // Bytes available in application buffer
    uint32_t dma_bytes; // Bytes that have been copied to/from DMA io buffer so far
    uint8_t *next_app_buf; // Next buffer from application after current one finishes
    uint32_t next_app_bytes; // Bytes in next buffer

    bool irq_setup_done;

    // DMA IO buffer management
    // One buffer is active in DMA, another one is processed by CPU.
    uint32_t dma_iobuf_words[SCSI_DMA_IOBUF_COUNT];
    int dma_active_iobuf;

    // Wide bus mode?
    bool wide;

    // Detected parity error on read?
    bool parityerror;

    // Synchronous mode?
    int syncOffset;
    int syncPeriod;
    int syncOffsetDivider; // Autopush/autopull threshold for the write pacer state machine
    int syncOffsetPreload; // Number of items to preload in the RX fifo of scsi_sync_write

    // PIO configurations
    uint32_t pio_offset_async_write;
    uint32_t pio_offset_sync_write_pacer;
    uint32_t pio_offset_sync_write;
    uint32_t pio_offset_read;
    uint32_t pio_offset_sync_read_pacer;
    pio_sm_config pio_cfg_async_write;
    pio_sm_config pio_cfg_sync_write_pacer;
    pio_sm_config pio_cfg_sync_write;
    pio_sm_config pio_cfg_read;
    pio_sm_config pio_cfg_sync_read_pacer;
    bool pio_removed_async_write;
    bool pio_removed_sync_write_pacer;
    bool pio_removed_sync_write;
    bool pio_removed_read;
    bool pio_removed_sync_read_pacer;

    // DMA configurations for write
    dma_channel_config dmacfg_write_chA; // Data from RAM to scsi_parity PIO
    dma_channel_config dmacfg_write_chB; // In synchronous mode only, transfer between state machines

    // DMA configurations for read
    dma_channel_config dmacfg_read_chA; // Data to destination memory buffer
    dma_channel_config dmacfg_read_chB; // From pacer to data state machine
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
    logmsg("GPIO states: ", sio_hw->gpio_in);
}

/****************************************/
/* Accelerated writes to SCSI bus       */
/****************************************/

// Check if data buffer from application has been fully processed.
// If there is a next one, swap it.
// If all is done, return true.
// Assumption: g_scsi_dma.spin_lock is held
static bool check_write_app_buffer_done()
{
    if (g_scsi_dma.app_bytes <= g_scsi_dma.dma_bytes)
    {
        // Input buffer has been fully processed, swap it
        g_scsi_dma.dma_bytes = 0;
        g_scsi_dma.app_buf = g_scsi_dma.next_app_buf;
        g_scsi_dma.app_bytes = g_scsi_dma.next_app_bytes;
        g_scsi_dma.next_app_buf = 0;
        g_scsi_dma.next_app_bytes = 0;
    }
    bool result = (g_scsi_dma.dma_bytes >= g_scsi_dma.app_bytes);

    return result;
}


// Generate parity and GPIO words in 8-bit bus mode.
// Reads count bytes from src and writes count words to dest.
// Only DBP and bits 0-7 of the output word are significant.
// Returns pointer to one past the last word written to dest.
static uint32_t *scsi_generate_parity_8bit(uint8_t *src, uint32_t count, uint32_t *dest)
{
    // This loop executes while count >= 8.
    // It handles parity for 8 bytes at a time, producing 8 output words.
    // It is hand-optimized equivalent of the "while (count >= 4)" loop below.
    // Interleaving the two words eliminates result use latency.
    // The routine has 26 instructions per 8 input bytes
    // From measurements this takes approx 14 µs per 512 bytes at 150 MHz
    // This gives 1.2 clock cycles per instruction, and throughput of 34 MB/s.
    asm(R"(
        1:
            ldr r0, [%0], #4        // Load first source word (4 bytes)
            subs %1, %1, #8         // count -= 8
            eor r1, r0, r0, lsr #4  //      Start calculating parity for first source word
            bmi 2f                  // End of data (count < 0)
            eor r1, r1, r1, lsr #2  //      First word parity
            ldr r2, [%0], #4        // Load second source word
            eor r1, r1, r1, lsr #1  //      First word parity
            eor r3, r2, r2, lsr #4  //          Second word parity
            pkhbt r5, r0, r1, lsl #16 //    Data from bottom of r0, parity from bottom of r1, dest0 => r5
            eor r3, r3, r3, lsr #2  //          Second word parity
            str r5, [%2], #4        //      Store dest[0]
            lsr r5, r5, #8          //      Second byte and its parity to r5
            eor r3, r3, r3, lsr #1  //          Second word parity
            str r5, [%2], #4        //      Store dest[1]
            pkhtb r5, r1, r0, asr #16 //    Data from top of r0, parity from top of r1, dest2 => r5
            str r5, [%2], #4        //      Store dest[2]
            lsr r5, r5, #8          //      Fourth byte and its parity to r5
            pkhbt r4, r2, r3, lsl #16 //        Data from bottom of r2, parity from bottom of r3, dest4 => r4
            str r5, [%2], #4        //      Store dest[3]
            lsr r5, r4, #8          //          Second byte and its parity to r5
            str r4, [%2], #4        //      Store dest[4]
            pkhtb r4, r3, r2, asr #16 //    Data from top of r2, parity from top of r3, dest6 => r4
            str r5, [%2], #4        //      Store dest[5]
            lsr r5, r4, #8          //          Fourth byte and its parity to r5
            str r4, [%2], #4        //      Store dest[6]
            str r5, [%2], #4        //      Store dest[7]
            b 1b                    // Continue loop

        2:
            sub %0, %0, #4      // Fix up src pointer at end of loop
            add %1, %1, #8      // Fix up count back to >= 0
    )" : "+r"(src), "+r"(count), "+r"(dest)
       : : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "memory");


    // Usually we have a multiple of 8 bytes
    if (__builtin_expect(count > 0, 0))
    {
        while (count >= 4)
        {
            // Calculate parity bit in parallel for the 4 bytes in the word
            uint32_t w = *(uint32_t*)src;
            uint32_t w2 = (w >> 4) ^ w;
            uint32_t w3 = (w2 >> 2) ^ w2;
            uint32_t w4 = (w3 >> 1) ^ w3;
            uint32_t w5 = w4 & 0xFFFF0000;

            dest[0] = ((w >>  0) & 0xFF) | (w4 << 16);
            dest[1] = ((w >>  8) & 0xFF) | (w4 <<  8);
            dest[2] = ((w >> 16) & 0xFF) | (w5 <<  0);
            dest[3] = ((w >> 24) & 0xFF) | (w5 >>  8);

            src += 4;
            count -= 4;
            dest += 4;
        }

        while (count > 0)
        {
            // We invert the bits in hardware but normal SCSI_OUT does not.
            *dest++ = scsi_generate_parity(*src++) ^ 0xFFFF;
            count -= 1;
        }
    }

    return dest;
}

// Generate parity and GPIO words in 16-bit bus mode.
// Reads count bytes from src and writes count/2 words to dest.
// Only 18 bottom bits of the output word are significant.
// Returns pointer to one past the last word written to dest.
static uint32_t *scsi_generate_parity_16bit(uint8_t *src, uint32_t count, uint32_t *dest)
{
    // This loop executes while count >= 8.
    // It handles parity for 8 bytes at a time, producing 4 output words.
    // It is hand-optimized equivalent of the "while (count >= 4)" loop below.
    // Interleaving the two words eliminates result use latency.
    // The routine has 20 instructions per 8 input bytes
    // From measurements this takes approx 23 µs per 1024 bytes at 150 MHz
    // This gives 1.2 clock cycles per instruction, and throughput of 44 MB/s.
    asm(R"(
            ldm %[src]!, {r0, r2}    // Load source words
            subs %[cnt], %[cnt], #7  // Make sure count goes <= 0 when there is less than 8 left
            mov r4, #0x02040000      // Load multiplication constant for bit shuffling
            ble 2f                   // Skip loop if count is too low

        1:
            eor r1, r0, r0, lsl #4  //      Start calculating parity for first source word
            eor r3, r2, r2, lsl #4  //          Second word parity
            eor r1, r1, r1, lsl #2  //      First word parity
            eor r3, r3, r3, lsl #2  //          Second word parity
            eor r1, r1, r1, lsl #1  //      First word parity
            and r1, r1, #0x80808080 //      First word parity bit masking (w5 => r1)
            umull r5, r1, r4, r1    //      Bit shuffle (w6 => r1), r5 dummy
            eor r3, r3, r3, lsl #1  //          Second word parity
            pkhbt r5, r0, r1, lsl #16 //    Data from bottom of r0, parity from bottom of r1, dest0 => r5
            pkhtb r6, r1, r0, asr #16 //    Data from top of r0, parity from top of r1, dest1 => r6
            and r3, r3, #0x80808080 //          Second word parity bit masking (w5 => r3)
            stm %[dst]!, {r5, r6}   //      Store 1st, 2nd output word
            umull r6, r3, r4, r3    //          Bit shuffle (w6 => r3), r6 dummy
            ldr r0, [%[src]], #4      // Load next source word 1
            pkhbt r5, r2, r3, lsl #16 //        Data from bottom of r2, parity from bottom of r3, dest2 => r5
            pkhtb r6, r3, r2, asr #16 //        Data from top of r2, parity from top of r3, dest3 => r6
            ldr r2, [%[src]], #4      // Load next source word 2
            subs %[cnt], %[cnt], #8   // count -= 8
            stm %[dst]!, {r5, r6}     //        Store 3rd, 4th output word
            bgt 1b                    // Continue loop while count > 0

        2:
            add %[cnt], %[cnt], #7      // Fix up count at end of loop
            sub %[src], %[src], #8      // Fix up src (we already loaded 2 words)
    )" : [src] "+r"(src), [cnt] "+r"(count), [dst] "+r"(dest)
       : : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "memory");

    // Usually we have a multiple of 8 bytes
    if (__builtin_expect(count > 0, 0))
    {
        while (count >= 4)
        {
            // Calculate parity bit in parallel for the 4 bytes in the word
            uint32_t w = *(uint32_t*)src;
            uint32_t w2 = (w << 4) ^ w;
            uint32_t w3 = (w2 << 2) ^ w2;
            uint32_t w4 = (w3 << 1) ^ w3;
            uint32_t w5 = w4 & 0x80808080;

            // Collect the DBP (low byte) and DBP1 (high byte) parity bits next to each other
            uint32_t w6 = ((uint64_t)w5 * 0x02040000) >> 32;

            dest[0] = (w & 0xFFFF) | ((w6 & 0x0003) << 16);
            dest[1] = (w >> 16)    | (w6 & 0x00030000);

            src += 4;
            dest += 2;
            count -= 4;
        }

        while (count >= 2)
        {
            // We invert the bits in hardware but normal SCSI_OUT does not.
            *dest++ = scsi_generate_parity(*(uint16_t*)src) ^ 0xFFFF;
            src += 2;
            count -= 2;
        }

        if (count > 0)
        {
            *dest++ = scsi_generate_parity(*src++) ^ 0xFFFF;
            count -= 1;
        }
    }

    return dest;
}

// Convert data bytes to GPIO words ready for DMA
static void fill_dma_writebuf()
{
    uint32_t saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);

    // Supports only IOBUF_COUNT = 2 for now
    int idx = (g_scsi_dma.dma_active_iobuf + 1) % SCSI_DMA_IOBUF_COUNT;
    uint32_t words_in_buf = g_scsi_dma.dma_iobuf_words[idx];
    uint32_t space_available = SCSI_DMA_IOBUF_SIZE - words_in_buf;
    if (g_scsi_dma.wide) space_available *= 2;
    uint32_t bytes_to_send = g_scsi_dma.app_bytes - g_scsi_dma.dma_bytes;

    if (space_available > 0 && bytes_to_send > 0)
    {
        if (bytes_to_send > space_available)
        {
            bytes_to_send = space_available;
        }

        uint32_t *dest = &g_scsi_dma_iobuf[idx][words_in_buf];
        uint8_t *src = &g_scsi_dma.app_buf[g_scsi_dma.dma_bytes];

        spin_unlock(g_scsi_dma.spin_lock, saved_irq); // Unlock during long operation

        if (g_scsi_dma.wide)
        {
            dest = scsi_generate_parity_16bit(src, bytes_to_send, dest);
        }
        else
        {
            dest = scsi_generate_parity_8bit(src, bytes_to_send, dest);
        }

        saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);

        g_scsi_dma.dma_bytes += bytes_to_send; // Application can now reuse the buffer
        g_scsi_dma.dma_iobuf_words[idx] = dest - &g_scsi_dma_iobuf[idx][0];
    }

    spin_unlock(g_scsi_dma.spin_lock, saved_irq);
}

static void scsi_accel_rp2040_stopWrite(volatile int *resetFlag);

static bool start_dma_write()
{
    // Get the buffer that should be ready for transmission
    int idx = (g_scsi_dma.dma_active_iobuf + 1) % SCSI_DMA_IOBUF_COUNT;
    uint32_t words = g_scsi_dma.dma_iobuf_words[idx];

    if (words < SCSI_DMA_IOBUF_SIZE)
    {
        uint32_t saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);

        // Process new data from application buffer, if space and data is available.
        // This must be inside a lock to prevent race condition with startWrite().
        bool buffer_done = check_write_app_buffer_done();

        // Check if we are all done.
        // From SCSIDMA_WRITE_DONE state we can either go to IDLE in stopWrite()
        // or back to WRITE in startWrite().
        if (buffer_done && words == 0)
        {
            g_scsi_dma_state = SCSIDMA_WRITE_DONE;

        }
        spin_unlock(g_scsi_dma.spin_lock, saved_irq);

        if (!buffer_done)
        {
            fill_dma_writebuf();
        }

        words = g_scsi_dma.dma_iobuf_words[idx];
    }

    // Swap DMA transmit buffers and mark previous one empty
    g_scsi_dma.dma_iobuf_words[g_scsi_dma.dma_active_iobuf] = 0;
    g_scsi_dma.dma_active_iobuf = idx;

    if (words == 0)
    {
        return false;
    }

    // Start DMA from current buffer to PIO
    dma_channel_set_read_addr(SCSI_DMA_CH_A, g_scsi_dma_iobuf[idx], false);
    dma_channel_set_trans_count(SCSI_DMA_CH_A, words, true);

    return true;
}

void scsi_accel_rp2040_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag)
{
    // Any read requests should be matched with a stopRead()
    assert(g_scsi_dma_state != SCSIDMA_READ && g_scsi_dma_state != SCSIDMA_READ_DONE);

    uint32_t saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);
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
    spin_unlock(g_scsi_dma.spin_lock, saved_irq);

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
    g_scsi_dma.dma_active_iobuf = 1; // Buffer 0 will be filled and started first
    g_scsi_dma.dma_iobuf_words[0] = 0;
    g_scsi_dma.dma_iobuf_words[1] = 0;

    if (must_reconfig_gpio)
    {
        SCSI_ENABLE_DATA_OUT();

        if (g_scsi_dma.syncOffset == 0)
        {
            // Asynchronous write
            pio_sm_init(SCSI_DMA_PIO, SCSI_DATA_SM, g_scsi_dma.pio_offset_async_write, &g_scsi_dma.pio_cfg_async_write);
            scsidma_config_gpio();

            pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, true);
        }
        else
        {
            // Synchronous write
            // Data state machine writes data to SCSI bus and dummy bits to its RX fifo.
            // Sync state machine empties the dummy bits every time ACK is received, to control the transmit pace.
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
            dma_channel_configure(SCSI_DMA_CH_B,
                &g_scsi_dma.dmacfg_write_chB,
                &SCSI_DMA_PIO->txf[SCSI_SYNC_SM],
                &SCSI_DMA_PIO->rxf[SCSI_DATA_SM],
                0xFFFFFFFF,
                true
            );

            // Enable state machines
            pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, true);
            pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, true);
        }

        dma_channel_configure(SCSI_DMA_CH_A,
            &g_scsi_dma.dmacfg_write_chA,
            &SCSI_DMA_PIO->txf[SCSI_DATA_SM],
            NULL, // Configured later
            0,
            false
        );

        dma_irqn_set_channel_enabled(SCSI_DMA_IRQ_IDX, SCSI_DMA_CH_A, true);
    }

    // Trigger IRQ so that it will start the write.
    // This way the parity processing can occur on second core
    dma_hw->irq_ctrl[SCSI_DMA_IRQ_IDX].intf = (1 << SCSI_DMA_CH_A);
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
    uint32_t saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);
    if (data >= g_scsi_dma.app_buf + g_scsi_dma.dma_bytes &&
        data < g_scsi_dma.app_buf + g_scsi_dma.app_bytes)
    {
        finished = false; // In current transfer
    }
    else if (data >= g_scsi_dma.next_app_buf &&
            data < g_scsi_dma.next_app_buf + g_scsi_dma.next_app_bytes)
    {
        finished = false; // In queued transfer
    }
    spin_unlock(g_scsi_dma.spin_lock, saved_irq);

    return finished;
}

// Once DMA has finished, check if all PIO queues have been drained
static bool scsi_accel_rp2040_isWriteDone()
{
    // Check if data is still waiting in PIO FIFO
    if (!pio_sm_is_tx_fifo_empty(SCSI_DMA_PIO, SCSI_DATA_SM))
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
    uint32_t start = platform_millis();
    while (!scsi_accel_rp2040_isWriteDone() && !*resetFlag)
    {
        if ((uint32_t)(platform_millis() - start) > 5000)
        {
            logmsg("scsi_accel_rp2040_stopWrite() timeout");
            scsi_accel_log_state();
            *resetFlag = 1;
            break;
        }
    }

    dma_channel_abort(SCSI_DMA_CH_A);
    dma_channel_abort(SCSI_DMA_CH_B);
    dma_irqn_set_channel_enabled(SCSI_DMA_IRQ_IDX, SCSI_DMA_CH_A, false);
    g_scsi_dma_state = SCSIDMA_IDLE;
    SCSI_RELEASE_DATA_REQ();
    scsidma_config_gpio();
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, false);
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, false);
}

void scsi_accel_rp2040_finishWrite(volatile int *resetFlag)
{
    uint32_t start = platform_millis();
    while (g_scsi_dma_state != SCSIDMA_IDLE && !*resetFlag)
    {
        if ((uint32_t)(platform_millis() - start) > 5000)
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

// Swap filled read buffer to be processed, and other one for DMA usage.
// Sets dma_iobuf_words according to remaining space in application buffer.
// Returns false if there is no more data to receive.
static void swap_dma_readbuf()
{
    uint32_t bytes_awaiting_processing = g_scsi_dma.dma_iobuf_words[g_scsi_dma.dma_active_iobuf];
    if (g_scsi_dma.wide) bytes_awaiting_processing *= 2;

    int idx = (g_scsi_dma.dma_active_iobuf + 1) % SCSI_DMA_IOBUF_COUNT;
    assert(g_scsi_dma.dma_iobuf_words[idx] == 0); // New io buffer should be empty before usage
    g_scsi_dma.dma_active_iobuf = idx;

    uint32_t bytes_to_rx = g_scsi_dma.app_bytes - g_scsi_dma.dma_bytes - bytes_awaiting_processing;

    if (bytes_to_rx == 0 && g_scsi_dma.next_app_bytes > 0)
    {
        // We cannot swap application buffer yet because data has not yet been written to it,
        // but we can start next DMA request to iobuf.
        bytes_to_rx = g_scsi_dma.next_app_bytes;
    }

    uint32_t words_to_rx = (g_scsi_dma.wide ? (bytes_to_rx / 2) : bytes_to_rx);
    if (words_to_rx > SCSI_DMA_IOBUF_SIZE) words_to_rx = SCSI_DMA_IOBUF_SIZE;
    g_scsi_dma.dma_iobuf_words[idx] = words_to_rx;
}

// Process previously filled io buffer and copy data to application buffer.
__attribute__((optimize("O3")))
static void process_dma_readbuf()
{
    // Supports only IOBUF_COUNT = 2 for now
    int idx = (g_scsi_dma.dma_active_iobuf + 1) % SCSI_DMA_IOBUF_COUNT;

    uint32_t words = g_scsi_dma.dma_iobuf_words[idx];
    uint32_t *src = g_scsi_dma_iobuf[idx];
    uint32_t *end = src + words;
    uint8_t *dst = &g_scsi_dma.app_buf[g_scsi_dma.dma_bytes];

    if (g_scsi_dma.wide)
    {
        // Read 16 bits per IO word
        uint32_t parity = 0xFFFFFFFF;
        while (src + 4 < end)
        {
            for (int unroll = 0; unroll < 4; unroll++)
            {
                uint32_t word = *src++;
                *(uint16_t*)dst = (uint16_t)word;
                dst += 2;
                parity ^= word;
            }
        }

        while (src < end)
        {
            uint32_t word = *src++;
            *(uint16_t*)dst = (uint16_t)word;
            dst += 2;
            parity ^= word;
        }

        if (!scsi_check_parity_16bit(parity))
        {
            dbgmsg("Parity error at ", (int)(dst - g_scsi_dma.app_buf), "/", (int)g_scsi_dma.app_bytes, " xor ", parity);
            g_scsi_dma.parityerror = true;
        }
    }
    else
    {
        // Read 8 bits per IO word
        uint32_t parity = 0xFFFFFFFF;
        while (src + 4 < end)
        {
            for (int unroll = 0; unroll < 4; unroll++)
            {
                uint32_t word = *src++;
                *dst++ = (uint8_t)word;
                parity ^= word;
            }
        }

        while (src < end)
        {
            uint32_t word = *src++;
            *dst++ = (uint8_t)word;
            parity ^= word;
        }

        if (!scsi_check_parity(parity))
        {
            dbgmsg("Parity error at ", (int)(dst - g_scsi_dma.app_buf), "/", (int)g_scsi_dma.app_bytes, " xor ", parity);
            g_scsi_dma.parityerror = true;
        }
    }

    g_scsi_dma.dma_iobuf_words[idx] = 0;
    g_scsi_dma.dma_bytes = dst - g_scsi_dma.app_buf;
}

static bool start_dma_read()
{
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, false);
    pio_sm_clear_fifos(SCSI_DMA_PIO, SCSI_DATA_SM);

    swap_dma_readbuf();

    // Check if we are all done.
    // From SCSIDMA_READ_DONE state we can either go to IDLE in stopRead()
    // or back to READ in startWrite().
    uint32_t words_to_rx = g_scsi_dma.dma_iobuf_words[g_scsi_dma.dma_active_iobuf];
    if (words_to_rx == 0)
    {
        return false;
    }

    if (g_scsi_dma.syncOffset == 0)
    {
        // Start sending dummy words to scsi_accel_read state machine
        dma_channel_set_trans_count(SCSI_DMA_CH_B, words_to_rx, true);
    }
    else
    {
        // Set number of bytes to receive to the scsi_sync_read_pacer state machine register X
        pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, false);
        hw_clear_bits(&SCSI_DMA_PIO->sm[SCSI_SYNC_SM].shiftctrl, PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS);
        pio_sm_put(SCSI_DMA_PIO, SCSI_SYNC_SM, words_to_rx - 1);
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
        dma_channel_set_trans_count(SCSI_DMA_CH_B, words_to_rx, true);
    }

    // Start DMA to fill the destination buffer
    dma_channel_configure(SCSI_DMA_CH_A,
        &g_scsi_dma.dmacfg_read_chA,
        g_scsi_dma_iobuf[g_scsi_dma.dma_active_iobuf],
        &SCSI_DMA_PIO->rxf[SCSI_DATA_SM],
        words_to_rx,
        true
    );

    // Ready to start the data and parity check state machines
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, true);

    if (g_scsi_dma.syncOffset > 0)
    {
        // Start sending REQ pulses
        pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, true);
    }

    return true;
}

void scsi_accel_rp2040_startRead(uint8_t *data, uint32_t count, int *parityError, volatile int *resetFlag)
{
    // Any write requests should be matched with a stopWrite()
    assert(g_scsi_dma_state != SCSIDMA_WRITE && g_scsi_dma_state != SCSIDMA_WRITE_DONE);

    uint32_t saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);
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
    spin_unlock(g_scsi_dma.spin_lock, saved_irq);

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
    g_scsi_dma.dma_active_iobuf = 1; // Buffer 0 will be selected and started first
    g_scsi_dma.dma_iobuf_words[0] = 0;
    g_scsi_dma.dma_iobuf_words[1] = 0;
    g_scsi_dma.parityerror = false;

    if (must_reconfig_gpio)
    {
        pio_sm_init(SCSI_DMA_PIO, SCSI_DATA_SM, g_scsi_dma.pio_offset_read, &g_scsi_dma.pio_cfg_read);

        // For synchronous mode, the REQ pin is driven by SCSI_SYNC_SM, so disable it in SCSI_DATA_SM
        if (g_scsi_dma.syncOffset > 0)
        {
            pio_sm_set_sideset_pins(SCSI_DMA_PIO, SCSI_DATA_SM, 0);
        }

        if (g_scsi_dma.syncOffset == 0)
        {
            // DMA channel B will copy dummy words to scsi_accel_read PIO to set the number
            // of bytes to transfer.
            static const uint32_t dummy = 0;
            dma_channel_configure(SCSI_DMA_CH_B,
                &g_scsi_dma.dmacfg_read_chB,
                &SCSI_DMA_PIO->txf[SCSI_DATA_SM],
                &dummy,
                0, false);
        }
        else
        {
            pio_sm_init(SCSI_DMA_PIO, SCSI_SYNC_SM, g_scsi_dma.pio_offset_sync_read_pacer, &g_scsi_dma.pio_cfg_sync_read_pacer);

            // DMA channel B will copy words from scsi_sync_read_pacer to scsi_accel_read PIO
            // to control the offset between REQ pulses sent and ACK pulses received.
            dma_channel_configure(SCSI_DMA_CH_B,
                &g_scsi_dma.dmacfg_read_chB,
                &SCSI_DMA_PIO->txf[SCSI_DATA_SM],
                &SCSI_DMA_PIO->rxf[SCSI_SYNC_SM],
                0, false);
        }

        scsidma_config_gpio();
        dma_irqn_set_channel_enabled(SCSI_DMA_IRQ_IDX, SCSI_DMA_CH_A, true);
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
    uint32_t saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);
    if (data >= g_scsi_dma.app_buf + g_scsi_dma.dma_bytes &&
        data < g_scsi_dma.app_buf + g_scsi_dma.app_bytes)
    {
        finished = false; // In current transfer
    }
    else if (data >= g_scsi_dma.next_app_buf &&
            data < g_scsi_dma.next_app_buf + g_scsi_dma.next_app_bytes)
    {
        finished = false; // In queued transfer
    }
    spin_unlock(g_scsi_dma.spin_lock, saved_irq);

    return finished;
}

static void scsi_accel_rp2040_stopRead()
{
    dma_channel_abort(SCSI_DMA_CH_A);
    dma_channel_abort(SCSI_DMA_CH_B);
    dma_irqn_set_channel_enabled(SCSI_DMA_IRQ_IDX, SCSI_DMA_CH_A, false);
    g_scsi_dma_state = SCSIDMA_IDLE;
    SCSI_RELEASE_DATA_REQ();
    scsidma_config_gpio();
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_DATA_SM, false);
    pio_sm_set_enabled(SCSI_DMA_PIO, SCSI_SYNC_SM, false);
}

void scsi_accel_rp2040_finishRead(const uint8_t *data, uint32_t count, int *parityError, volatile int *resetFlag)
{
    uint32_t start = platform_millis();
    const uint8_t *query_addr = (data ? (data + count - 1) : NULL);
    while (!scsi_accel_rp2040_isReadFinished(query_addr) && !*resetFlag)
    {
        if ((uint32_t)(platform_millis() - start) > 5000)
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
    if (parityError != NULL && g_scsi_dma.parityerror)
    {
        dbgmsg("scsi_accel_rp2040_finishRead(", bytearray(data, count), ") detected parity error");
        *parityError = true;
    }
}

/*******************************************************/
/* Write SCSI PIO program timings and ACK pin          */
/*******************************************************/
static void pio_remove_program(PIO pio, const pio_program_t *program, uint loaded_offset, bool &removed)
{
    if (!removed)
    {
        pio_remove_program(pio, program, loaded_offset);
        removed = true;
    }
}

static int pio_add_scsi_accel_async_write_program()
{
    pio_remove_program(SCSI_DMA_PIO,
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

    // out pins, 18         side 1  [0]   <-- Set data preset time
    uint8_t delay = g_bluescsi_timings->scsi.req_delay - 2;
    assert( delay <= 0xF);
    rewrote_instructions[0] |= pio_encode_delay(delay);
    // wait 1 gpio ACK      side 1      ; Wait for ACK to be inactive
    rewrote_instructions[1] |= pio_encode_wait_gpio(true, SCSI_IN_ACK) | pio_encode_sideset(1, 1);
    // wait 0 gpio ACK      side 0      ; Assert REQ, wait for ACK low
    rewrote_instructions[2] = pio_encode_wait_gpio(false, SCSI_IN_ACK) | pio_encode_sideset(1, 0);

    g_scsi_dma.pio_removed_async_write = false;
    return pio_add_program(SCSI_DMA_PIO, &rewrote_program);
}

static int pio_add_scsi_accel_read_program()
{
    pio_remove_program(SCSI_DMA_PIO,
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
    rewrote_instructions[2] = pio_encode_wait_gpio(false, SCSI_IN_ACK) | pio_encode_sideset(1, 0);
    g_scsi_dma.pio_removed_read = false;
    return pio_add_program(SCSI_DMA_PIO, &rewrote_program);
}

static int pio_add_scsi_sync_write_pacer_program()
{
    pio_remove_program(SCSI_DMA_PIO,
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

static int pio_add_scsi_sync_read_pacer_program()
{
    g_scsi_dma.pio_removed_sync_read_pacer = false;
    return pio_add_program(SCSI_DMA_PIO, &scsi_sync_read_pacer_program);
}

static int pio_add_scsi_sync_write_program()
{
    g_scsi_dma.pio_removed_sync_write = false;
    return pio_add_program(SCSI_DMA_PIO, &scsi_sync_write_program);
}

/*******************************************************/
/* Initialization functions common to read/write       */
/*******************************************************/

__attribute__((optimize("O3")))
static void scsi_dma_irq()
{
    dma_hw->irq_ctrl[SCSI_DMA_IRQ_IDX].intf = 0;
    dma_irqn_acknowledge_channel(SCSI_DMA_IRQ_IDX, SCSI_DMA_CH_A);

    scsidma_state_t state = g_scsi_dma_state;
    if (state == SCSIDMA_WRITE)
    {
        // Start writing from next buffer, if any, or set state to SCSIDMA_WRITE_DONE
        bool started = start_dma_write();

        if (started)
        {
            // Prefill next DMA buffer
            fill_dma_writebuf();
        }
    }
    else if (state == SCSIDMA_READ)
    {
        uint32_t saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);
        // Start reading into next buffer, if any
        bool started = start_dma_read();
        spin_unlock(g_scsi_dma.spin_lock, saved_irq);

        // Process filled DMA buffer
        // Lock is released during this because it takes some time
        process_dma_readbuf();

        saved_irq = spin_lock_blocking(g_scsi_dma.spin_lock);
        if (g_scsi_dma.app_bytes <= g_scsi_dma.dma_bytes)
        {
            // Application provided buffer is full, swap it
            g_scsi_dma.dma_bytes = 0;
            g_scsi_dma.app_buf = g_scsi_dma.next_app_buf;
            g_scsi_dma.app_bytes = g_scsi_dma.next_app_bytes;
            g_scsi_dma.next_app_buf = 0;
            g_scsi_dma.next_app_bytes = 0;

            if (g_scsi_dma.app_bytes == 0)
            {
                // Set state to DONE only after readbuf processing is done
                g_scsi_dma_state = SCSIDMA_READ_DONE;
            }
        }

        if (!started && g_scsi_dma_state != SCSIDMA_READ_DONE)
        {
            // We got new request in middle of readbuf processing
            start_dma_read();
        }
        spin_unlock(g_scsi_dma.spin_lock, saved_irq);
    }
}

static void scsidma_set_data_gpio_func(uint32_t func, bool invert_data, bool wide)
{
    uint32_t invert = 0;

    // Invert input and output data in hardware to speed things up.
    if (invert_data) invert |= (1 << 16) | (1 << 12);

    for (int i = SCSI_IO_DB0; i <= SCSI_IO_DB7; i++)
    {
        iobank0_hw->io[i].ctrl  = func | invert;
    }
    iobank0_hw->io[SCSI_IO_DBP].ctrl  = func;

    // In 8-bit mode we don't drive the upper bits
    if (wide)
    {
        for (int i = SCSI_IO_DB8; i <= SCSI_IO_DB15; i++)
        {
            iobank0_hw->io[i].ctrl  = func | invert;
        }
        iobank0_hw->io[SCSI_IO_DBP1].ctrl = func;
    }
    else
    {
        for (int i = SCSI_IO_DB8; i <= SCSI_IO_DB15; i++)
        {
            iobank0_hw->io[i].ctrl  = GPIO_FUNC_SIO | invert;
        }
        iobank0_hw->io[SCSI_IO_DBP1].ctrl = GPIO_FUNC_SIO;
    }
}

// Select GPIO from PIO peripheral or from software controlled SIO
static void scsidma_config_gpio()
{
    if (g_scsi_dma_state == SCSIDMA_IDLE)
    {
        scsidma_set_data_gpio_func(GPIO_FUNC_SIO, false, false);
        iobank0_hw->io[SCSI_OUT_REQ].ctrl = GPIO_FUNC_SIO;
    }
    else if (g_scsi_dma_state == SCSIDMA_WRITE)
    {
        // Make sure the initial state of all pins is high and output
        pio_sm_set_pins(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ));
        pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DB0, 16, true);
        pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DBP, 1, true);
        pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DBP1, 1, true);
        pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_OUT_REQ, 1, true);

        scsidma_set_data_gpio_func(GPIO_FUNC_PIO0, true, g_scsi_dma.wide);
        iobank0_hw->io[SCSI_OUT_REQ].ctrl = GPIO_FUNC_PIO0;
    }
    else if (g_scsi_dma_state == SCSIDMA_READ)
    {
        if (g_scsi_dma.syncOffset == 0)
        {
            // Asynchronous read
            // Data bus as input, REQ pin as output
            pio_sm_set_pins(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ));
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DB0, 16, false);
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DBP,  1, false);
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DBP1, 1, false);
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_OUT_REQ, 1, true);
        }
        else
        {
            // Synchronous read, REQ pin is written by SYNC_SM
            pio_sm_set_pins(SCSI_DMA_PIO, SCSI_SYNC_SM, SCSI_IO_DATA_MASK | (1 << SCSI_OUT_REQ));
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DB0, 16, false);
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DBP,  1, false);
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_DATA_SM, SCSI_IO_DBP1, 1, false);
            pio_sm_set_consecutive_pindirs(SCSI_DMA_PIO, SCSI_SYNC_SM, SCSI_OUT_REQ, 1, true);
        }

        scsidma_set_data_gpio_func(GPIO_FUNC_PIO0, true, g_scsi_dma.wide);
        iobank0_hw->io[SCSI_OUT_REQ].ctrl = GPIO_FUNC_PIO0;
    }
}

static void setup_scsi_dma_irq()
{
    // Interrupts are used for data buffer swapping and parity generation.
    // It can run on core0 or core1.
    irq_set_exclusive_handler(SCSI_DMA_IRQ_NUM, scsi_dma_irq);
    irq_set_enabled(SCSI_DMA_IRQ_NUM, true);
    g_scsi_dma.irq_setup_done = true;
}

void scsi_accel_rp2040_init()
{
    g_scsi_dma_state = SCSIDMA_IDLE;
    scsidma_config_gpio();

    static bool first_init = true;
    if (first_init)
    {
        g_scsi_dma.spin_lock = spin_lock_init(spin_lock_claim_unused(true));
        g_scsi_dma.pio_removed_async_write = true;
        g_scsi_dma.pio_removed_sync_write_pacer = true;
        g_scsi_dma.pio_removed_sync_write = true;
        g_scsi_dma.pio_removed_read = true;
        g_scsi_dma.pio_removed_sync_read_pacer = true;
        first_init = false;
    }

    if (g_channels_claimed) {
        // Un-claim all SCSI state machines
        pio_sm_unclaim(SCSI_DMA_PIO, SCSI_DATA_SM);
        pio_sm_unclaim(SCSI_DMA_PIO, SCSI_SYNC_SM);

        // Remove all SCSI programs
        pio_remove_program(SCSI_DMA_PIO, &scsi_accel_async_write_program, g_scsi_dma.pio_offset_async_write, g_scsi_dma.pio_removed_async_write);
        pio_remove_program(SCSI_DMA_PIO, &scsi_sync_write_pacer_program, g_scsi_dma.pio_offset_sync_write_pacer, g_scsi_dma.pio_removed_sync_write_pacer);
        pio_remove_program(SCSI_DMA_PIO, &scsi_accel_read_program, g_scsi_dma.pio_offset_read, g_scsi_dma.pio_removed_read);
        pio_remove_program(SCSI_DMA_PIO, &scsi_sync_read_pacer_program, g_scsi_dma.pio_offset_sync_read_pacer, g_scsi_dma.pio_removed_sync_read_pacer);
        pio_remove_program(SCSI_DMA_PIO, &scsi_sync_write_program, g_scsi_dma.pio_offset_sync_write, g_scsi_dma.pio_removed_sync_write);

        // Un-claim all SCSI DMA channels
        dma_channel_unclaim(SCSI_DMA_CH_A);
        dma_channel_unclaim(SCSI_DMA_CH_B);

        // Set flag to re-initialize SCSI PIO system
        g_channels_claimed = false;
    }

    if (!g_channels_claimed)
    {
        // Mark channels as being in use, unless it has been done already
        pio_sm_claim(SCSI_DMA_PIO, SCSI_DATA_SM);
        pio_sm_claim(SCSI_DMA_PIO, SCSI_SYNC_SM);
        dma_channel_claim(SCSI_DMA_CH_A);
        dma_channel_claim(SCSI_DMA_CH_B);
        g_channels_claimed = true;
    }

    // Asynchronous SCSI write
    g_scsi_dma.pio_offset_async_write = pio_add_scsi_accel_async_write_program();
    g_scsi_dma.pio_cfg_async_write = scsi_accel_async_write_program_get_default_config(g_scsi_dma.pio_offset_async_write);
    sm_config_set_out_pins(&g_scsi_dma.pio_cfg_async_write, SCSI_IO_DB0, 18);
    sm_config_set_sideset_pins(&g_scsi_dma.pio_cfg_async_write, SCSI_OUT_REQ);

    // Synchronous SCSI write pacer / ACK handler
    g_scsi_dma.pio_offset_sync_write_pacer = pio_add_scsi_sync_write_pacer_program();
    g_scsi_dma.pio_cfg_sync_write_pacer = scsi_sync_write_pacer_program_get_default_config(g_scsi_dma.pio_offset_sync_write_pacer);
    sm_config_set_out_shift(&g_scsi_dma.pio_cfg_sync_write_pacer, true, true, 1);

    // Asynchronous / synchronous SCSI read
    g_scsi_dma.pio_offset_read = pio_add_scsi_accel_read_program();
    g_scsi_dma.pio_cfg_read = scsi_accel_read_program_get_default_config(g_scsi_dma.pio_offset_read);
    sm_config_set_in_pins(&g_scsi_dma.pio_cfg_read, SCSI_IO_DB0);
    sm_config_set_sideset_pins(&g_scsi_dma.pio_cfg_read, SCSI_OUT_REQ);

    // Synchronous SCSI read pacer
    g_scsi_dma.pio_offset_sync_read_pacer = pio_add_scsi_sync_read_pacer_program();
    g_scsi_dma.pio_cfg_sync_read_pacer = scsi_sync_read_pacer_program_get_default_config(g_scsi_dma.pio_offset_sync_read_pacer);
    sm_config_set_sideset_pins(&g_scsi_dma.pio_cfg_sync_read_pacer, SCSI_OUT_REQ);

    // Synchronous SCSI data writer
    g_scsi_dma.pio_offset_sync_write = pio_add_scsi_sync_write_program();
    g_scsi_dma.pio_cfg_sync_write = scsi_sync_write_program_get_default_config(g_scsi_dma.pio_offset_sync_write);
    sm_config_set_out_pins(&g_scsi_dma.pio_cfg_sync_write, SCSI_IO_DB0, 18);
    sm_config_set_sideset_pins(&g_scsi_dma.pio_cfg_sync_write, SCSI_OUT_REQ);

    // Create DMA channel configurations so they can be applied quickly later

    // For write to SCSI BUS:
    // Channel A: GPIO words from g_scsi_dma_iobuf to SCSI_DATA_SM PIO
    dma_channel_config cfg = dma_channel_get_default_config(SCSI_DMA_CH_A);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_DATA_SM, true));
    g_scsi_dma.dmacfg_write_chA = cfg;

    // Channel B: In synchronous mode a second DMA channel is used to transfer dummy bits
    // from first state machine to second one.
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_B);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_SYNC_SM, true));
    g_scsi_dma.dmacfg_write_chB = cfg;

    // For read from SCSI BUS:
    // Channel A: GPIO words from SCSI_DATA_SM PIO to g_scsi_dma_iobuf
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_A);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_DATA_SM, false));
    g_scsi_dma.dmacfg_read_chA = cfg;

    // Channel B: In synchronous mode a second DMA channel is used to transfer dummy words
    // from first state machine to second one to control the pace of data transfer.
    // In asynchronous mode this just transfers words to control the number of bytes.
    cfg = dma_channel_get_default_config(SCSI_DMA_CH_B);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(SCSI_DMA_PIO, SCSI_DATA_SM, true));
    g_scsi_dma.dmacfg_read_chB = cfg;

#ifdef RP2MCU_CPU_PARITY_CORE1
    multicore_fifo_push_blocking((uintptr_t) &setup_scsi_dma_irq);
    sleep_ms(1);
    if (!g_scsi_dma.irq_setup_done)
    {
        logmsg("ERROR: Failed to setup core1 SCSI DMA IRQ, falling back to core 0!");
        setup_scsi_dma_irq();
    }
#else
    setup_scsi_dma_irq();
#endif
}


bool scsi_accel_rp2040_setSyncMode(int syncOffset, int syncPeriod, bool wide)
{
    g_scsi_dma.wide = wide;

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
                delay2 = totalPeriod - delay0 - delay1 - 3;  //Data hold time, should be min 16.5ns according to the spec for FAST-20
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
                clkdiv = g_bluescsi_timings->scsi_5.clkdiv;
                if (clkdiv > 0) { totalPeriod /= clkdiv; rtotalPeriod /= clkdiv; }
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


#endif
