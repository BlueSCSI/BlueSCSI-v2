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
 * Panel SPI Slave Driver Implementation
 *
 * DMA-based SPI slave for communication with ESP32-C3 front panel.
 */

#include "panel_spi.h"
#include "panel_protocol_defs.h"
#include "panel_protocol_defs_initiator.h"
#include "panel_protocol.h"
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_initiator.h"

#ifdef ENABLE_PANEL_SPI

extern "C" {
#include <scsi.h>
}

#include <hardware/spi.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <string.h>

// Transaction phases
typedef enum {
    PHASE_IDLE,         // Waiting for transaction
    PHASE_HEADER,       // Receiving/sending header
    PHASE_PAYLOAD,      // Receiving/sending payload
    PHASE_PENDING       // Async operation in progress
} panel_phase_t;

// Size of the dedicated synchronous-read response buffer. Must hold the
// largest IRQ-context read response (panel_playback_status_t = 76 bytes);
// 128 leaves margin and keeps it cheap.
#define PANEL_SYNC_RESPONSE_SIZE 128

// Static state
static struct {
    bool initialized;
    spi_inst_t* spi;
    int dma_rx_channel;
    int dma_tx_channel;

    volatile panel_phase_t phase;
    volatile bool dma_complete;

    // Header buffers
    panel_protocol_header_t rx_header;
    panel_protocol_header_t tx_header;

    // Payload buffers (4KB each)
    uint8_t rx_payload[PANEL_PROTOCOL_MAX_PAYLOAD] __attribute__((aligned(4)));
    uint8_t tx_payload[PANEL_PROTOCOL_MAX_PAYLOAD] __attribute__((aligned(4)));

    // Dedicated buffer for synchronous (IRQ-context) read responses, kept
    // separate from tx_payload so a small sync read (e.g. GET_PLAYBACK_STATUS)
    // can never overwrite a larger async result staged in tx_payload that is
    // still awaiting its POLL_OP_READY drain.
    uint8_t sync_response[PANEL_SYNC_RESPONSE_SIZE] __attribute__((aligned(4)));

    // Status response for POLL_OP_READY
    panel_status_response_t status_response;

    // Async operation state
    volatile panel_async_state_t async_state;
    volatile uint16_t async_response_size;
    uint8_t pending_async_command;

    // Saved header for main loop — ISR copies rx_header here before starting
    // new DMA, because setup_header_dma() makes rx_header a live DMA target
    // that gets overwritten by subsequent ESP32 transactions (POLL_OP_READY etc.)
    panel_protocol_header_t saved_header;

    // IRQ suspend state — disabled during initiator SCSI bus operations
    bool irq_suspended;

    // Transaction tracking for non-debug logging
    bool first_transaction_logged;
    uint32_t transaction_count;
    uint32_t last_log_time;
} g_panel;

// Forward declarations
static void setup_header_dma(void);
static bool setup_payload_dma(size_t size, uint8_t *rx_buf, uint8_t *tx_buf);
static void setup_status_dma(void);
static void dma_irq_handler(void);

// True when the SCSI target bus is active OR a host selection is latched but
// not yet serviced. Panel write commands run their (potentially multi-ms) SD
// I/O in the main loop and block scsiPoll() while doing so, so we defer them
// until the bus is genuinely idle — not just during DATA phases. This narrows
// the window in which a panel SD operation can delay a host selection or
// command. (DATA_IN/DATA_OUT are a subset of phase != BUS_FREE.)
static inline bool panel_scsi_bus_busy(void) {
    return scsiDev.phase != BUS_FREE || scsiDev.selFlag;
}

bool panel_spi_init(void) {
    if (g_panel.initialized) {
        return true;
    }

    memset(&g_panel, 0, sizeof(g_panel));
    g_panel.spi = PANEL_SPI;
    g_panel.dma_rx_channel = -1;
    g_panel.dma_tx_channel = -1;

    logmsg("Panel SPI: Initializing on GPIO RX=", PANEL_SPI_RX,
           " TX=", PANEL_SPI_TX, " SCK=", PANEL_SPI_SCK, " CS=", PANEL_SPI_CS);

    // Configure GPIO pins for SPI function
    gpio_set_function(PANEL_SPI_RX, GPIO_FUNC_SPI);   // MOSI (RX for slave)
    gpio_set_function(PANEL_SPI_TX, GPIO_FUNC_SPI);   // MISO (TX for slave)
    gpio_set_function(PANEL_SPI_SCK, GPIO_FUNC_SPI);  // SCK
    gpio_set_function(PANEL_SPI_CS, GPIO_FUNC_SPI);   // CS

    // Disable pulls except CS (pull-up for idle high)
    gpio_disable_pulls(PANEL_SPI_RX);
    gpio_disable_pulls(PANEL_SPI_TX);
    gpio_disable_pulls(PANEL_SPI_SCK);
    gpio_pull_up(PANEL_SPI_CS);

    // Initialize SPI in slave mode
    // Mode 1: CPOL=0, CPHA=1 (matches PicoIDE/ESP32 master)
    spi_init(g_panel.spi, 10000000);  // 10 MHz (actual speed set by master)
    spi_set_format(g_panel.spi, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);
    spi_set_slave(g_panel.spi, true);

    // Claim DMA channels
    g_panel.dma_rx_channel = dma_claim_unused_channel(true);
    g_panel.dma_tx_channel = dma_claim_unused_channel(true);

    if (g_panel.dma_rx_channel < 0 || g_panel.dma_tx_channel < 0) {
        logmsg("Panel SPI: Failed to claim DMA channels");
        panel_spi_deinit();
        return false;
    }

    logmsg("Panel SPI: DMA channels RX=", g_panel.dma_rx_channel,
           " TX=", g_panel.dma_tx_channel);

    // Configure RX DMA channel
    dma_channel_config rx_config = dma_channel_get_default_config(g_panel.dma_rx_channel);
    channel_config_set_transfer_data_size(&rx_config, DMA_SIZE_8);
    channel_config_set_read_increment(&rx_config, false);  // Read from fixed SPI DR
    channel_config_set_write_increment(&rx_config, true);  // Write to buffer with increment
    channel_config_set_dreq(&rx_config, spi_get_dreq(g_panel.spi, false));  // RX dreq
    dma_channel_set_config(g_panel.dma_rx_channel, &rx_config, false);
    dma_channel_set_read_addr(g_panel.dma_rx_channel, &spi_get_hw(g_panel.spi)->dr, false);

    // Enable CRC16 sniffer on RX channel for payload validation
    dma_sniffer_enable(g_panel.dma_rx_channel, 0x2 /* CRC-16-CCITT */, true);

    // Configure TX DMA channel
    dma_channel_config tx_config = dma_channel_get_default_config(g_panel.dma_tx_channel);
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_8);
    channel_config_set_read_increment(&tx_config, true);   // Read from buffer with increment
    channel_config_set_write_increment(&tx_config, false); // Write to fixed SPI DR
    channel_config_set_dreq(&tx_config, spi_get_dreq(g_panel.spi, true));  // TX dreq
    dma_channel_set_config(g_panel.dma_tx_channel, &tx_config, false);
    dma_channel_set_write_addr(g_panel.dma_tx_channel, &spi_get_hw(g_panel.spi)->dr, false);

    // Setup DMA completion interrupt on RX channel
    dma_irqn_set_channel_enabled(PANEL_DMA_IRQ_IDX, g_panel.dma_rx_channel, true);
    irq_set_exclusive_handler(PANEL_DMA_IRQ_NUM, dma_irq_handler);
    irq_set_enabled(PANEL_DMA_IRQ_NUM, true);

    // Initialize protocol handler
    panel_protocol_init();

    // Start listening for header
    g_panel.phase = PHASE_HEADER;
    setup_header_dma();

    g_panel.initialized = true;
    logmsg("Panel SPI: Initialized successfully");
    return true;
}

void panel_spi_deinit(void) {
    if (g_panel.dma_rx_channel >= 0) {
        dma_irqn_set_channel_enabled(PANEL_DMA_IRQ_IDX, g_panel.dma_rx_channel, false);
        dma_channel_abort(g_panel.dma_rx_channel);
        dma_channel_unclaim(g_panel.dma_rx_channel);
        g_panel.dma_rx_channel = -1;
    }

    if (g_panel.dma_tx_channel >= 0) {
        dma_channel_abort(g_panel.dma_tx_channel);
        dma_channel_unclaim(g_panel.dma_tx_channel);
        g_panel.dma_tx_channel = -1;
    }

    if (g_panel.spi) {
        spi_deinit(g_panel.spi);
        g_panel.spi = NULL;
    }

    g_panel.initialized = false;
    logmsg("Panel SPI: Deinitialized");
}

// Helper to get command name for logging
static const char* panel_cmd_name(uint8_t cmd) {
    switch (cmd) {
        case PANEL_CMD_GET_DIR_ENTRY_COUNT: return "GET_DIR_ENTRY_COUNT";
        case PANEL_CMD_GET_ENTRY_INFO: return "GET_ENTRY_INFO";
        case PANEL_CMD_SELECT_ENTRY: return "SELECT_ENTRY";
        case PANEL_CMD_GET_CURRENT_PATH: return "GET_CURRENT_PATH";
        case PANEL_CMD_EJECT_IMAGE: return "EJECT_IMAGE";
        case PANEL_CMD_GET_LOADED_IMAGE_STATUS: return "GET_LOADED_IMAGE_STATUS";
        case PANEL_CMD_SELECT_PREV_IMAGE: return "SELECT_PREV_IMAGE";
        case PANEL_CMD_SELECT_NEXT_IMAGE: return "SELECT_NEXT_IMAGE";
        case PANEL_CMD_SELECT_IMAGE_BY_NAME: return "SELECT_IMAGE_BY_NAME";
        case PANEL_CMD_CHECK_FIRMWARE: return "CHECK_FIRMWARE";
        case PANEL_CMD_START_FIRMWARE_READ: return "START_FIRMWARE_READ";
        case PANEL_CMD_START_FILE_UPLOAD: return "START_FILE_UPLOAD";
        case PANEL_CMD_WRITE_FILE_CHUNK: return "WRITE_FILE_CHUNK";
        case PANEL_CMD_FINISH_FILE_UPLOAD: return "FINISH_FILE_UPLOAD";
        case PANEL_CMD_GET_RP2350_FW_STATUS: return "GET_RP2350_FW_STATUS";
        case PANEL_CMD_START_RP2350_UPDATE: return "START_RP2350_UPDATE";
        case PANEL_CMD_START_FILE_DOWNLOAD: return "START_FILE_DOWNLOAD";
        case PANEL_CMD_READ_FILE_CHUNK: return "READ_FILE_CHUNK";
        case PANEL_CMD_GET_INITIATOR_STATUS: return "GET_INITIATOR_STATUS";
        case PANEL_CMD_RESET: return "RESET";
        case PANEL_CMD_POLL_STATUS: return "POLL_STATUS";
        case PANEL_CMD_POLL_OP_READY: return "POLL_OP_READY";
        case PANEL_CMD_GET_DEVICE_STATUS: return "GET_DEVICE_STATUS";
        case PANEL_CMD_GET_FIRMWARE_INFO: return "GET_FIRMWARE_INFO";
        case PANEL_CMD_GET_PLAYBACK_STATUS: return "GET_PLAYBACK_STATUS";
        case PANEL_CMD_GET_COMMAND_STATUS: return "GET_COMMAND_STATUS";
        default: return "UNKNOWN";
    }
}

// Stable shadow for the most recently received write payload.
// The DMA IRQ copies rx_payload + the sniffer CRC into here as soon as a
// write payload completes, before signalling the main loop. This shadow
// is what the main loop reads from for both the immediate-dispatch and
// deferred (SCSI-busy) paths, so the next ESP32 transaction overwriting
// rx_payload cannot corrupt an in-flight write.
//
// Per protocol: write commands carrying payload are async. ESP32 must see
// READY via POLL_OP_READY before sending another write. POLL_OP_READY
// transfers only touch rx_header + status_response, not rx_payload, so
// this shadow stays intact across the polling window.
//
// `pending` flips on when SCSI bus is busy and we have to wait to dispatch.
static struct {
    bool pending;
    uint8_t command;
    uint16_t argument;
    uint16_t payload_size;
    uint16_t crc16;
    uint8_t payload[PANEL_PROTOCOL_MAX_PAYLOAD] __attribute__((aligned(4)));
} g_deferred_write;

static void panel_spi_dispatch_write(uint8_t cmd, uint16_t arg,
                                     uint8_t *payload, uint16_t payload_size,
                                     uint16_t crc16) {
    if (payload_size > 0) {
        if (PANEL_CMD_IS_ASYNC(cmd)) {
            g_panel.pending_async_command = cmd;
            g_panel.async_state = PANEL_ASYNC_PROCESSING;
        }
        panel_protocol_handle_write(cmd, arg, payload, payload_size, crc16);
    } else {
        // Note: async state already set in IRQ for no-payload commands
        panel_protocol_handle_write(cmd, arg, NULL, 0, 0);
    }
}

void panel_spi_poll(void) {
    if (!g_panel.initialized) {
        return;
    }

    // Refresh the device-status snapshot from the main loop so the IRQ-context
    // read handlers never touch img->file (which switchNextImage reassigns).
    panel_protocol_refresh_device_snapshot();

    // During initiator SCSI bus operations, suspend the DMA IRQ.
    // Resume cleanly when the bus is free.
    if (scsiInitiatorBusBusy()) {
        if (!g_panel.irq_suspended) {
            irq_set_enabled(PANEL_DMA_IRQ_NUM, false);
            g_panel.irq_suspended = true;
        }
        return;
    }

    if (g_panel.irq_suspended) {
        // SCSI bus now free — cleanly resume SPI.
        // 1. Abort stale DMA
        dma_channel_abort(g_panel.dma_rx_channel);
        dma_channel_abort(g_panel.dma_tx_channel);

        // 2. Wait for CS high (ESP32 not mid-transaction)
        for (int i = 0; i < 200 && !gpio_get(PANEL_SPI_CS); i++) {
            busy_wait_us_32(1);
        }

        // 3. Drain SPI RX FIFO — stale bytes from partial transactions
        while (spi_is_readable(g_panel.spi)) {
            (void)spi_get_hw(g_panel.spi)->dr;
        }

        // 4. Clear pending DMA IRQ
        dma_irqn_acknowledge_channel(PANEL_DMA_IRQ_IDX, g_panel.dma_rx_channel);

        // 5. Reset state and start fresh
        g_panel.dma_complete = false;
        g_panel.phase = PHASE_HEADER;
        setup_header_dma();
        g_panel.irq_suspended = false;
        irq_set_enabled(PANEL_DMA_IRQ_NUM, true);
    }

    // Process deferred write only when the SCSI bus is genuinely idle
    if (g_deferred_write.pending) {
        if (panel_scsi_bus_busy()) {
            return;
        }
        g_deferred_write.pending = false;
        dbgmsg("Panel: processing deferred cmd=", g_deferred_write.command,
               " (", panel_cmd_name(g_deferred_write.command), ")");
        panel_spi_dispatch_write(g_deferred_write.command,
                                 g_deferred_write.argument,
                                 g_deferred_write.payload,
                                 g_deferred_write.payload_size,
                                 g_deferred_write.crc16);
        return;
    }

    if (!g_panel.dma_complete) {
        return;
    }

    // Read from saved_header — rx_header is a live DMA target and may already
    // be overwritten by POLL_OP_READY transactions from the ESP32.
    uint8_t cmd = g_panel.saved_header.command;
    uint16_t arg = g_panel.saved_header.argument;
    uint16_t payload_size = g_panel.saved_header.payload_size;

    // Defer write commands whenever the SCSI bus is active (or a selection is
    // pending), not just during DATA phases, to avoid blocking scsiPoll() and
    // SD-card contention. ISR already set PROCESSING so ESP32 polls see the
    // command was received. The ISR has already snapshotted the payload + CRC
    // into g_deferred_write.payload / .crc16, so we only need to record the
    // command metadata and flip the pending flag.
    if (PANEL_CMD_IS_WRITE(cmd) && panel_scsi_bus_busy()) {
        g_deferred_write.pending = true;
        g_deferred_write.command = cmd;
        g_deferred_write.argument = arg;
        g_deferred_write.payload_size = payload_size;
        g_panel.dma_complete = false;  // Let ISR continue handling commands
        return;
    }

    g_panel.dma_complete = false;

    // Track transactions
    g_panel.transaction_count++;

    // Log first transaction to confirm connection
    if (!g_panel.first_transaction_logged) {
        g_panel.first_transaction_logged = true;
        logmsg("Panel SPI: First transaction received - front panel connected");
        dbgmsg("Panel SPI: cmd=", cmd, " (", panel_cmd_name(cmd), ") arg=", (int)arg, " payload=", (int)payload_size);
        g_panel.last_log_time = platform_millis();
    }

    // Periodic transaction summary (every 10 seconds), and drain any
    // events the IRQ recorded since logmsg is not safe in IRQ context.
    uint32_t now = platform_millis();
    if (now - g_panel.last_log_time >= 10000) {
        dbgmsg("Panel SPI: ", g_panel.transaction_count, " transactions processed");
        g_panel.last_log_time = now;
    }
    panel_protocol_drain_irq_log();

    // Log the command (DMA setup already done in IRQ)
    dbgmsg("Panel RX: cmd=", cmd, " (", panel_cmd_name(cmd), ") arg=", (int)arg, " payload=", (int)payload_size);

    // Handle write commands - call protocol handler
    // Read commands are fully handled in IRQ (response prepared and DMA started)
    // POLL_OP_READY is fully handled in IRQ
    if (PANEL_CMD_IS_WRITE(cmd)) {
        // ISR has already snapshotted the payload + CRC into g_deferred_write
        // before signalling dma_complete, so this read is race-free even if
        // the ESP32 sends another transaction while the handler is running.
        // (Protocol guarantees no back-to-back async writes without polling.)
        panel_spi_dispatch_write(cmd, arg,
                                 g_deferred_write.payload,
                                 payload_size,
                                 g_deferred_write.crc16);
    }
    // Read commands: response already prepared and sent in IRQ
}

bool panel_spi_is_initialized(void) {
    return g_panel.initialized;
}

void panel_spi_set_async_result(const uint8_t* data, size_t size) {
    // Never let a handler stage more than the TX buffer holds.
    if (size > PANEL_PROTOCOL_MAX_PAYLOAD) {
        size = PANEL_PROTOCOL_MAX_PAYLOAD;
    }
    if (size > 0 && data != NULL) {
        // Copy to TX buffer if not already there
        if (data != g_panel.tx_payload) {
            memcpy(g_panel.tx_payload, data, size);
        }
    }
    g_panel.async_response_size = size;
    g_panel.async_state = PANEL_ASYNC_READY;
    dbgmsg("Panel SPI: Async cmd ", g_panel.pending_async_command,
           " (", panel_cmd_name(g_panel.pending_async_command), ") completed, ", (int)size, " bytes");
}

void panel_spi_set_async_error(void) {
    g_panel.async_response_size = 0;
    g_panel.async_state = PANEL_ASYNC_ERROR;
    dbgmsg("Panel SPI: Async cmd ", g_panel.pending_async_command,
           " (", panel_cmd_name(g_panel.pending_async_command), ") failed");
}

uint8_t* panel_spi_get_rx_buffer(void) {
    return g_panel.rx_payload;
}

uint8_t* panel_spi_get_tx_buffer(void) {
    return g_panel.tx_payload;
}

// ============================================================================
// Internal functions
// ============================================================================

static void setup_header_dma(void) {
    // Clear TX header (we send zeros during header phase)
    memset(&g_panel.tx_header, 0, sizeof(g_panel.tx_header));

    // Setup RX DMA for header
    dma_channel_set_write_addr(g_panel.dma_rx_channel, &g_panel.rx_header, false);
    dma_channel_set_trans_count(g_panel.dma_rx_channel, PANEL_PROTOCOL_HEADER_SIZE, false);

    // Setup TX DMA for header (send zeros)
    dma_channel_set_read_addr(g_panel.dma_tx_channel, &g_panel.tx_header, false);
    dma_channel_set_trans_count(g_panel.dma_tx_channel, PANEL_PROTOCOL_HEADER_SIZE, false);

    // Start both channels simultaneously
    uint32_t channel_mask = (1u << g_panel.dma_rx_channel) | (1u << g_panel.dma_tx_channel);
    dma_start_channel_mask(channel_mask);
}

// Returns false (without arming any DMA) if size is out of range, so callers
// can recover to the header phase instead of wedging the state machine with no
// DMA armed (a malformed/oversized payload_size from the master must not be
// able to permanently stall the panel link).
static bool setup_payload_dma(size_t size, uint8_t *rx_buf, uint8_t *tx_buf) {
    if (size == 0 || size > PANEL_PROTOCOL_MAX_PAYLOAD) {
        return false;
    }

    // Reset CRC sniffer for payload validation
    dma_sniffer_set_data_accumulator(0xFFFF);

    // Setup RX DMA for payload. Caller picks the destination:
    //   - For read commands the master is reading from us; the bytes the
    //     master sends back are dummy and we land them in rx_payload.
    //   - For write commands, point straight at g_deferred_write.payload so
    //     the main loop reads from a stable buffer with zero IRQ memcpy.
    dma_channel_set_write_addr(g_panel.dma_rx_channel, rx_buf, false);
    dma_channel_set_trans_count(g_panel.dma_rx_channel, size, false);

    // Setup TX DMA for payload. Caller picks the source: sync read responses
    // come from g_panel.sync_response, async results from g_panel.tx_payload.
    dma_channel_set_read_addr(g_panel.dma_tx_channel, tx_buf, false);
    dma_channel_set_trans_count(g_panel.dma_tx_channel, size, false);

    // Start both channels
    uint32_t channel_mask = (1u << g_panel.dma_rx_channel) | (1u << g_panel.dma_tx_channel);
    dma_start_channel_mask(channel_mask);
    return true;
}

// Separate dummy buffer for status DMA receive — must not alias rx_payload,
// which may still hold write payload data being processed by the main loop.
static uint8_t status_rx_dummy[sizeof(panel_status_response_t)] __attribute__((aligned(4)));

static void setup_status_dma(void) {
    // Setup RX DMA for dummy bytes (into dedicated buffer, NOT rx_payload)
    dma_channel_set_write_addr(g_panel.dma_rx_channel, status_rx_dummy, false);
    dma_channel_set_trans_count(g_panel.dma_rx_channel, sizeof(panel_status_response_t), false);

    // Setup TX DMA for status response
    dma_channel_set_read_addr(g_panel.dma_tx_channel, &g_panel.status_response, false);
    dma_channel_set_trans_count(g_panel.dma_tx_channel, sizeof(panel_status_response_t), false);

    // Start both channels
    uint32_t channel_mask = (1u << g_panel.dma_rx_channel) | (1u << g_panel.dma_tx_channel);
    dma_start_channel_mask(channel_mask);
}

// DMA IRQ handler - handles ALL phase transitions and DMA setup in IRQ context
// This is critical for timing: ESP32 starts clocking immediately after sending header
static void dma_irq_handler(void) {
    if (!dma_irqn_get_channel_status(PANEL_DMA_IRQ_IDX, g_panel.dma_rx_channel)) return;
    dma_irqn_acknowledge_channel(PANEL_DMA_IRQ_IDX, g_panel.dma_rx_channel);

    switch (g_panel.phase) {
        case PHASE_HEADER: {
            // Header received - process IN IRQ and set up next DMA immediately
            uint8_t cmd = g_panel.rx_header.command;
            uint16_t payload_size = g_panel.rx_header.payload_size;

            if (payload_size > 0) {
                if (PANEL_CMD_IS_READ(cmd)) {
                    if (cmd == PANEL_CMD_POLL_OP_READY) {
                        // POLL_OP_READY: prepare status and send immediately
                        g_panel.status_response.ready_flag = g_panel.async_state;
                        g_panel.status_response.response_size =
                            (g_panel.async_state == PANEL_ASYNC_READY) ? g_panel.async_response_size : 0;
                        g_panel.phase = PHASE_PENDING;
                        setup_status_dma();
                        // Don't set dma_complete - status handled entirely in IRQ
                    } else {
                        // Other read command - prepare response and send it.
                        // Must call handler HERE (in IRQ) because DMA starts
                        // immediately. Write the response into the dedicated
                        // sync_response buffer (NOT tx_payload) so it cannot
                        // clobber an async result staged in tx_payload that is
                        // still awaiting POLL_OP_READY. Any legitimate sync read
                        // fits in sync_response; an oversized request (none are
                        // legitimate) falls back to tx_payload so the DMA source
                        // is always at least payload_size bytes.
                        uint8_t *resp_buf = (payload_size <= PANEL_SYNC_RESPONSE_SIZE)
                                            ? g_panel.sync_response : g_panel.tx_payload;
                        size_t resp_max = (payload_size <= PANEL_SYNC_RESPONSE_SIZE)
                                          ? PANEL_SYNC_RESPONSE_SIZE : PANEL_PROTOCOL_MAX_PAYLOAD;
                        panel_protocol_handle_read(cmd, g_panel.rx_header.argument,
                                                   resp_buf, resp_max);
                        g_panel.phase = PHASE_PAYLOAD;
                        // Master is reading from us; the bytes coming back from
                        // the master are dummy. Land them in rx_payload (we'll
                        // ignore them). On a bad payload_size, recover to header
                        // rather than wedging with no DMA armed.
                        if (!setup_payload_dma(payload_size, g_panel.rx_payload, resp_buf)) {
                            g_panel.phase = PHASE_HEADER;
                            setup_header_dma();
                        }
                        // No dma_complete - response already prepared and sending
                    }
                } else {
                    // Write command with payload - receive it directly into the
                    // shadow buffer the main loop reads from. No memcpy needed
                    // when payload completes; protocol guarantees no back-to-back
                    // writes without a POLL_OP_READY in between.
                    g_panel.phase = PHASE_PAYLOAD;
                    // Write phase: master sends data (RX -> deferred shadow);
                    // our TX bytes are ignored by the master, so tx_payload is
                    // fine as the (unused) TX source. On a bad payload_size,
                    // recover to header rather than wedging.
                    if (!setup_payload_dma(payload_size, g_deferred_write.payload, g_panel.tx_payload)) {
                        g_panel.phase = PHASE_HEADER;
                        setup_header_dma();
                    }
                    // Don't set dma_complete yet - wait for payload
                }
            } else {
                // No payload command
                if (PANEL_CMD_IS_ASYNC(cmd)) {
                    g_panel.pending_async_command = cmd;
                    g_panel.async_state = PANEL_ASYNC_PROCESSING;
                }
                g_panel.saved_header = g_panel.rx_header;  // Save before DMA overwrites
                g_panel.phase = PHASE_HEADER;
                setup_header_dma();
                g_panel.dma_complete = true;  // Main loop handles command
            }
            break;
        }

        case PHASE_PENDING:
            // Status response just sent - check if payload follows
            if (g_panel.rx_header.command == PANEL_CMD_POLL_OP_READY &&
                g_panel.status_response.ready_flag == PANEL_ASYNC_READY &&
                g_panel.status_response.response_size > 0) {
                // Ready with data - send payload immediately. We're transmitting
                // (master reading from us); RX bytes are dummy, land in rx_payload.
                // The async result lives in tx_payload (staged by the handler).
                g_panel.phase = PHASE_PAYLOAD;
                if (!setup_payload_dma(g_panel.status_response.response_size,
                                       g_panel.rx_payload, g_panel.tx_payload)) {
                    g_panel.phase = PHASE_HEADER;
                    setup_header_dma();
                }
                // Don't set dma_complete - wait for payload transfer
            } else {
                // Not ready or no data - back to header
                g_panel.phase = PHASE_HEADER;
                setup_header_dma();
                // Don't set dma_complete - nothing for main loop to do
            }
            break;

        case PHASE_PAYLOAD: {
            // Payload transfer complete
            uint8_t cmd = g_panel.rx_header.command;

            // Clean up async state if we just sent async result
            if (g_panel.async_state == PANEL_ASYNC_READY) {
                g_panel.async_state = PANEL_ASYNC_IDLE;
                g_panel.async_response_size = 0;
            }

            // Signal main loop to process the transaction
            // (for write commands with payload, or read commands that need handling)
            if (PANEL_CMD_IS_WRITE(cmd)) {
                // Set PROCESSING immediately so ESP32 polls see command was received
                // (matches PicoIDE pattern: ISR acknowledges, main loop executes)
                if (PANEL_CMD_IS_ASYNC(cmd)) {
                    g_panel.pending_async_command = cmd;
                    g_panel.async_state = PANEL_ASYNC_PROCESSING;
                }
                g_panel.saved_header = g_panel.rx_header;  // Save before DMA overwrites

                // The RX DMA wrote the payload directly into g_deferred_write.
                // payload, so no memcpy is needed here - we only sample the
                // sniffer accumulator before any subsequent transaction can
                // reset it. Keeping IRQ work minimal protects the SCSI hot
                // path: this handler shares the default Cortex-M priority
                // with the SCSI buffer-swap IRQ, and any time spent here
                // delays SCSI buffer swaps.
                if (g_panel.rx_header.payload_size > 0) {
                    g_deferred_write.crc16 =
                        (uint16_t)dma_sniffer_get_data_accumulator();
                } else {
                    g_deferred_write.crc16 = 0;
                }

                g_panel.dma_complete = true;  // Main loop processes write payload
            }

            // Set up for next header (after saving, so DMA doesn't overwrite saved data)
            g_panel.phase = PHASE_HEADER;
            setup_header_dma();
            break;
        }

        default:
            // Shouldn't happen - reset to header
            g_panel.phase = PHASE_HEADER;
            setup_header_dma();
            break;
    }
}

#endif // ENABLE_PANEL_SPI
