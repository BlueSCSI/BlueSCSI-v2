/**
 * BlueSCSI - Copyright (c) 2026 Eric Helgeson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * ----
 *
 * Panel I2C Slave Driver Implementation
 *
 * Interrupt-driven I2C slave (pico_i2c_slave) for communication with the
 * ESP32-C3 front panel on BlueSCSI v2. Drives the same transport-agnostic
 * panel_protocol handlers as the SPI slave (panel_spi.cpp).
 *
 * Wire model (see panel_protocol_defs.h):
 *   - The master writes a 5-byte header (+ payload for write commands) in one
 *     I2C write transaction, then reads the response in a separate read
 *     transaction (the ESP32 inserts a short inter-phase delay between them).
 *   - Read responses are prepared in the FINISH ISR of the header-write
 *     transaction (the SPI slave likewise prepares reads in IRQ context), so
 *     the data is ready when the master's read transaction arrives.
 *   - Write commands run their (possibly multi-ms) SD I/O in the main loop and
 *     are deferred while the SCSI bus is active, mirroring the SPI slave.
 */

#include "panel_i2c.h"
#include "panel_protocol_defs.h"
#include "panel_protocol.h"
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_initiator.h"

#ifdef ENABLE_PANEL_I2C

extern "C" {
#include <scsi.h>
}

#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/i2c_slave.h>
#include <string.h>

// I2C wiring on BlueSCSI v2: GPIO16/17 are the i2c0 SDA/SCL pins. When the
// front panel is enabled these pins are the panel link exclusively (no IO
// expander / buttons / SPDIF), so claiming them here is safe.
#define PANEL_I2C_INST   i2c0
#define PANEL_I2C_IRQ    I2C0_IRQ
#define PANEL_I2C_SDA    GPIO_I2C_SDA   // GPIO16 on v2
#define PANEL_I2C_SCL    GPIO_I2C_SCL   // GPIO17 on v2
#define PANEL_I2C_ADDR   0x50           // matches the ESP32 master (HOST_DEVICE_ADDR)
#define PANEL_I2C_BAUD   1000000        // 1 MHz (fast-mode-plus); master drives the clock

// Size of the dedicated synchronous-read response buffer. Must hold the largest
// IRQ-context read response (panel_playback_status_t = 76 bytes); 128 leaves
// margin. Kept separate from tx_payload so a small sync read can never clobber
// an async result staged in tx_payload still awaiting its POLL_OP_READY drain.
#define PANEL_SYNC_RESPONSE_SIZE 128

// What the TX buffer currently staged for a read transaction represents, so the
// FINISH after a read can sequence the POLL_OP_READY -> result follow-up read.
typedef enum {
    TX_NONE,
    TX_SYNC,    // a normal synchronous read response
    TX_STATUS,  // a POLL_OP_READY 3-byte status response
    TX_RESULT,  // an async result payload (the read following a READY status)
} panel_tx_purpose_t;

static struct {
    bool initialized;
    i2c_inst_t* i2c;

    // --- RX assembly (ISR context) ---
    // Header bytes of the in-progress write transaction.
    uint8_t rx_header_bytes[PANEL_PROTOCOL_HEADER_SIZE];
    volatile uint16_t rx_count;     // bytes seen in the current write transaction
    panel_protocol_header_t cur_header;  // parsed once the 5th header byte lands
    uint8_t* payload_dest;          // where post-header bytes land this transaction
    volatile uint16_t payload_idx;  // payload bytes received this transaction

    // --- TX serving (ISR context) ---
    const uint8_t* tx_src;
    volatile uint16_t tx_len;
    volatile uint16_t tx_idx;
    volatile panel_tx_purpose_t tx_purpose;
    volatile bool serve_result_next;   // a READY status will be followed by a result read

    // Buffers
    uint8_t tx_payload[PANEL_PROTOCOL_MAX_PAYLOAD] __attribute__((aligned(4)));
    uint8_t sync_response[PANEL_SYNC_RESPONSE_SIZE] __attribute__((aligned(4)));
    panel_status_response_t status_response;

    // Async operation state (same model as the SPI slave)
    volatile panel_async_state_t async_state;
    volatile uint16_t async_response_size;
    uint8_t pending_async_command;

    // IRQ suspend during initiator SCSI bus operations
    bool irq_suspended;

    // Logging
    bool first_transaction_logged;
} g_panel;

// Stable shadow for a received write command, handed from the ISR to the main
// loop. The ISR receives the payload directly into .payload (no IRQ memcpy) and
// flips .ready; the main loop dispatches it once the SCSI bus is idle. Protocol
// guarantees no back-to-back writes without an intervening POLL_OP_READY, and
// POLL_OP_READY/read transactions never touch this buffer, so it stays intact
// across the polling window.
static struct {
    volatile bool ready;
    uint8_t command;
    uint16_t argument;
    uint16_t payload_size;
    uint8_t payload[PANEL_PROTOCOL_MAX_PAYLOAD] __attribute__((aligned(4)));
} g_deferred_write;

// CRC-16-CCITT (poly 0x1021, init 0xFFFF, no reflection) — matches the RP2040
// DMA sniffer config the SPI slave uses, so chunk CRCs validate identically.
static uint16_t panel_i2c_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

// ============================================================================
// I2C slave event handler (runs in the I2C ISR)
// ============================================================================

// Prepare the response for the read transaction that follows a header write.
// Called from the FINISH ISR; reads are serviced here (not the main loop) so
// the data is ready when the master's separate read transaction arrives.
static void panel_i2c_prepare_read(void) {
    uint8_t cmd = g_panel.cur_header.command;

    if (cmd == PANEL_CMD_POLL_OP_READY) {
        g_panel.status_response.ready_flag = g_panel.async_state;
        g_panel.status_response.response_size =
            (g_panel.async_state == PANEL_ASYNC_READY) ? g_panel.async_response_size : 0;
        g_panel.tx_src = (const uint8_t*)&g_panel.status_response;
        g_panel.tx_len = sizeof(g_panel.status_response);
        g_panel.tx_idx = 0;
        g_panel.tx_purpose = TX_STATUS;
        g_panel.serve_result_next =
            (g_panel.async_state == PANEL_ASYNC_READY && g_panel.async_response_size > 0);
        return;
    }

    // Normal synchronous read. Write the response into the dedicated
    // sync_response buffer so it cannot clobber an async result staged in
    // tx_payload that is still awaiting POLL_OP_READY. Oversized requests (none
    // are legitimate) fall back to tx_payload so the source is always large
    // enough for payload_size bytes.
    uint16_t want = g_panel.cur_header.payload_size;
    uint8_t* buf = (want <= PANEL_SYNC_RESPONSE_SIZE) ? g_panel.sync_response : g_panel.tx_payload;
    size_t cap = (want <= PANEL_SYNC_RESPONSE_SIZE) ? PANEL_SYNC_RESPONSE_SIZE : PANEL_PROTOCOL_MAX_PAYLOAD;
    panel_protocol_handle_read(cmd, g_panel.cur_header.argument, buf, cap);
    g_panel.tx_src = buf;
    g_panel.tx_len = want;
    g_panel.tx_idx = 0;
    g_panel.tx_purpose = TX_SYNC;
}

// Process a completed header-write transaction (header, plus payload for write
// commands). Reads are staged for the follow-up read; writes are shadowed for
// the main loop.
static void panel_i2c_process_write_txn(void) {
    if (g_panel.rx_count < PANEL_PROTOCOL_HEADER_SIZE) {
        return;  // short/malformed header — ignore
    }

    uint8_t cmd = g_panel.cur_header.command;

    if (PANEL_CMD_IS_READ(cmd)) {
        panel_i2c_prepare_read();
        return;
    }

    // Write command: shadow it for the main loop. Async state goes PROCESSING
    // now so the ESP32's POLL_OP_READY sees the command was received even while
    // dispatch is deferred.
    if (PANEL_CMD_IS_ASYNC(cmd)) {
        g_panel.pending_async_command = cmd;
        g_panel.async_state = PANEL_ASYNC_PROCESSING;
    }
    g_deferred_write.command = cmd;
    g_deferred_write.argument = g_panel.cur_header.argument;
    // Payload was received straight into g_deferred_write.payload; trust the
    // bytes actually received over a header that claims more than arrived.
    // Copy out of the packed header / volatile field before comparing.
    uint16_t hdr_size = g_panel.cur_header.payload_size;
    uint16_t got = g_panel.payload_idx;
    g_deferred_write.payload_size = (got < hdr_size) ? got : hdr_size;
    g_deferred_write.ready = true;
}

static void panel_i2c_handler(i2c_inst_t* i2c, i2c_slave_event_t event) {
    switch (event) {
        case I2C_SLAVE_RECEIVE: {
            uint8_t b = i2c_read_byte_raw(i2c);
            if (g_panel.rx_count < PANEL_PROTOCOL_HEADER_SIZE) {
                g_panel.rx_header_bytes[g_panel.rx_count] = b;
                g_panel.rx_count++;
                if (g_panel.rx_count == PANEL_PROTOCOL_HEADER_SIZE) {
                    // Header complete — parse and pick the payload destination.
                    memcpy(&g_panel.cur_header, g_panel.rx_header_bytes,
                           sizeof(g_panel.cur_header));
                    g_panel.payload_idx = 0;
                    // Only write commands carry a payload after the header; for
                    // read commands the master sends no further bytes, so drop
                    // anything unexpected (NULL dest) instead of buffering it.
                    g_panel.payload_dest = PANEL_CMD_IS_WRITE(g_panel.cur_header.command)
                                           ? g_deferred_write.payload : NULL;
                }
            } else {
                // Post-header payload byte (write commands only in practice).
                if (g_panel.payload_dest && g_panel.payload_idx < PANEL_PROTOCOL_MAX_PAYLOAD) {
                    g_panel.payload_dest[g_panel.payload_idx] = b;
                }
                g_panel.payload_idx++;
            }
            break;
        }

        case I2C_SLAVE_REQUEST: {
            // Master is reading. Serve staged bytes, pad with zero past the end.
            uint8_t b = (g_panel.tx_idx < g_panel.tx_len) ? g_panel.tx_src[g_panel.tx_idx] : 0x00;
            if (g_panel.tx_idx < g_panel.tx_len) {
                g_panel.tx_idx++;
            }
            i2c_write_byte_raw(i2c, b);
            break;
        }

        case I2C_SLAVE_FINISH: {
            if (g_panel.rx_count > 0) {
                // This was a write/header transaction.
                panel_i2c_process_write_txn();
                g_panel.rx_count = 0;
                g_panel.payload_idx = 0;
            } else {
                // This was a read transaction; the master consumed our TX.
                switch (g_panel.tx_purpose) {
                    case TX_STATUS:
                        if (g_panel.serve_result_next) {
                            // Stage the async result for the follow-up read.
                            g_panel.tx_src = g_panel.tx_payload;
                            g_panel.tx_len = g_panel.async_response_size;
                            g_panel.tx_idx = 0;
                            g_panel.tx_purpose = TX_RESULT;
                            g_panel.serve_result_next = false;
                        } else {
                            g_panel.tx_purpose = TX_NONE;
                        }
                        break;
                    case TX_RESULT:
                        // Async result delivered — clear state (mirrors the SPI
                        // slave's PHASE_PAYLOAD cleanup).
                        g_panel.async_state = PANEL_ASYNC_IDLE;
                        g_panel.async_response_size = 0;
                        g_panel.tx_purpose = TX_NONE;
                        break;
                    default:
                        g_panel.tx_purpose = TX_NONE;
                        break;
                }
            }
            break;
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

bool panel_i2c_init(void) {
    if (g_panel.initialized) {
        return true;
    }

    memset(&g_panel, 0, sizeof(g_panel));
    g_panel.i2c = PANEL_I2C_INST;
    g_panel.tx_purpose = TX_NONE;
    g_deferred_write.ready = false;

    logmsg("Panel I2C: Initializing slave (7-bit addr=", (int)PANEL_I2C_ADDR,
           " i2c0, SDA=", (int)PANEL_I2C_SDA, " SCL=", (int)PANEL_I2C_SCL, ")");

    gpio_set_function(PANEL_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PANEL_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PANEL_I2C_SDA);
    gpio_pull_up(PANEL_I2C_SCL);

    i2c_init(g_panel.i2c, PANEL_I2C_BAUD);
    i2c_slave_init(g_panel.i2c, PANEL_I2C_ADDR, &panel_i2c_handler);

    panel_protocol_init();

    g_panel.initialized = true;
    logmsg("Panel I2C: Initialized successfully");
    return true;
}

void panel_i2c_deinit(void) {
    if (!g_panel.initialized) {
        return;
    }
    i2c_slave_deinit(g_panel.i2c);
    i2c_deinit(g_panel.i2c);
    g_panel.initialized = false;
    logmsg("Panel I2C: Deinitialized");
}

static void panel_i2c_dispatch_write(void) {
    uint8_t cmd = g_deferred_write.command;
    uint16_t arg = g_deferred_write.argument;
    uint16_t size = g_deferred_write.payload_size;

    if (size > 0) {
        uint16_t crc = panel_i2c_crc16(g_deferred_write.payload, size);
        panel_protocol_handle_write(cmd, arg, g_deferred_write.payload, size, crc);
    } else {
        panel_protocol_handle_write(cmd, arg, NULL, 0, 0);
    }
}

void panel_i2c_poll(void) {
    if (!g_panel.initialized) {
        return;
    }

    // Refresh the device-status snapshot from the main loop so the IRQ-context
    // read handlers never touch img->file (which switchNextImage reassigns).
    panel_protocol_refresh_device_snapshot();

    // During initiator SCSI bus operations, suspend the I2C IRQ; resume cleanly
    // when the bus is free.
    if (scsiInitiatorBusBusy()) {
        if (!g_panel.irq_suspended) {
            irq_set_enabled(PANEL_I2C_IRQ, false);
            g_panel.irq_suspended = true;
        }
        return;
    }
    if (g_panel.irq_suspended) {
        // Drain any partial transaction state and resume.
        g_panel.rx_count = 0;
        g_panel.payload_idx = 0;
        g_panel.tx_purpose = TX_NONE;
        g_panel.serve_result_next = false;
        while (i2c_get_read_available(g_panel.i2c)) {
            (void)i2c_read_byte_raw(g_panel.i2c);
        }
        g_panel.irq_suspended = false;
        irq_set_enabled(PANEL_I2C_IRQ, true);
    }

    if (!g_deferred_write.ready) {
        return;
    }

    // Defer write dispatch (and its SD I/O) until the SCSI bus is idle.
    if (panel_scsi_bus_busy()) {
        return;
    }

    g_deferred_write.ready = false;

    if (!g_panel.first_transaction_logged) {
        g_panel.first_transaction_logged = true;
        logmsg("Panel I2C: first write command dispatched (cmd=0x", (int)g_deferred_write.command,
               ") - front panel connected");
    }
    panel_protocol_drain_irq_log();

    panel_i2c_dispatch_write();
}

bool panel_i2c_is_initialized(void) {
    return g_panel.initialized;
}

void panel_i2c_set_async_result(const uint8_t* data, size_t size) {
    if (size > PANEL_PROTOCOL_MAX_PAYLOAD) {
        size = PANEL_PROTOCOL_MAX_PAYLOAD;
    }
    if (size > 0 && data != NULL && data != g_panel.tx_payload) {
        memcpy(g_panel.tx_payload, data, size);
    }
    g_panel.async_response_size = size;
    g_panel.async_state = PANEL_ASYNC_READY;
}

void panel_i2c_set_async_error(void) {
    g_panel.async_response_size = 0;
    g_panel.async_state = PANEL_ASYNC_ERROR;
}

uint8_t* panel_i2c_get_tx_buffer(void) {
    return g_panel.tx_payload;
}

#endif // ENABLE_PANEL_I2C
