/*
 * Copyright (c) 2026 Eric Helgeson
 *
 * This file is part of BlueSCSI.
 *
 * BlueSCSI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BlueSCSI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Apple LaserWriter IISC SCSI printer emulation.
 *
 * Spools raw printer SCSI commands and data into /PR<id>/print_NNNN.bin
 * for offline post-processing. The framebuffer rasterization is done by the
 * host driver; we just capture the byte stream coming over the bus.
 *
 * Spool file format: a stream of records, each
 *   uint8_t  cdb[6];           // raw 6-byte CDB
 *   uint32_t payload_len;      // big-endian byte count
 *   uint8_t  payload[payload_len];
 *
 * PRINT (0x0A) with bit 7 of cdb[5] set finalizes the file. Any later
 * command rotates to a new spool file (next sequence number).
 *
 * The SCSI command-set emulated here is based on demik's reverse-engineering
 * research of the LaserWriter IISC controller and the System 6 (6.0.5)
 * driver, posted at
 * https://web.archive.org/web/20260425223154/https://68kmla.org/bb/threads/laserwriter-iisc-scsi-protocol-reversed-for-emulator-implementation.52165/
 */

#include "BlueSCSI_printer.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include <BlueSCSI_platform.h>
#include <SdFat.h>
#include <minIni.h>
#include <strings.h>

extern "C" {
#include <scsi.h>
#include <scsiPhy.h>
}

extern SdFs SD;

// If the current spool file goes this long without seeing any band data
// (a 0x05 READ BLOCK LIMITS record), we treat it as abandoned — discard
// the file and recycle the sequence number. This handles the trailing
// SETUP the LaserWriter driver always sends after PRINT.
static constexpr uint32_t PRINTER_ABANDON_MS = 10 * 1000;

// Single shared printer state. Multiple targets configured as printers
// share this slot.
namespace {
struct PrinterState
{
    FsFile   spool_file;
    uint32_t next_seq        = 0;
    uint32_t last_write_ms   = 0;
    int8_t   owner_id        = -1;     // SCSI target ID owning the slot, -1 = unowned
    bool     seq_initialized = false;
    bool     dir_ready       = false;
    bool     has_band_data   = false;  // any 0x05 record written to current file?
    char     spool_dir[8]    = {0};    // "/PR<N>" — fits up to /PR15 + NUL
};

PrinterState g_printer_state;

// Pending stage-2 work for the 0x05 READ BLOCK LIMITS command. The header
// is received via DATA_OUT + postDataOutHook; the hook then synchronously
// pulls the mask bytes off the bus and writes them straight to the spool.
struct PrinterPendingMask
{
    uint8_t target_id;
    bool active;
};

PrinterPendingMask g_printer_pending = {0, false};

bool printerEnsureDir(PrinterState &ps, int target_id)
{
    if (ps.dir_ready) return true;

    // PR<id>/ folder doubles as both the device-registration trigger (in
    // findHDDImages) and the spool target. scsiDiskOpenHDDImage creates it
    // if the user only supplied a placeholder file, so it should normally
    // exist by the time we get here.
    snprintf(ps.spool_dir, sizeof(ps.spool_dir), "/PR%d", target_id);

    FsFile dir;
    if (!dir.open(ps.spool_dir))
    {
        if (!SD.mkdir(ps.spool_dir) || !dir.open(ps.spool_dir))
        {
            logmsg("PRINTER: could not open or create spool dir ", ps.spool_dir);
            return false;
        }
    }
    dir.close();
    ps.dir_ready = true;
    return true;
}

// Walk the spool dir once to find the highest existing print_NNNN.bin
// sequence number, so a reboot doesn't overwrite previous jobs.
void printerInitSeq(PrinterState &ps)
{
    if (ps.seq_initialized) return;
    ps.seq_initialized = true;
    ps.next_seq = 0;

    FsFile dir;
    if (!dir.open(ps.spool_dir)) return;

    FsFile entry;
    char name[64];
    while (entry.openNext(&dir, O_RDONLY))
    {
        if (entry.getName(name, sizeof(name)))
        {
            // Match "print_NNNN.bin" (case-insensitive).
            if (strncasecmp(name, "print_", 6) == 0)
            {
                uint32_t n = (uint32_t)strtoul(&name[6], NULL, 10);
                if (n + 1 > ps.next_seq) ps.next_seq = n + 1;
            }
        }
        entry.close();
    }
    dir.close();
}

// If the current file is open, has no band data, and has gone idle past
// PRINTER_ABANDON_MS, delete it and recycle the sequence number. This is
// what cleans up the trailing-SETUP-after-PRINT case for single-page
// jobs. Returns true iff a file was discarded.
bool printerDiscardIfAbandoned(PrinterState &ps)
{
    if (!ps.spool_file.isOpen()) return false;
    if (ps.has_band_data) return false;
    if ((uint32_t)(platform_millis() - ps.last_write_ms) < PRINTER_ABANDON_MS)
        return false;

    // The currently-open file was assigned next_seq-1 when opened.
    char path[64];
    snprintf(path, sizeof(path), "%s/print_%04lu.bin",
             ps.spool_dir, (unsigned long)(ps.next_seq - 1));
    ps.spool_file.close();
    SD.remove(path);
    ps.next_seq--;
    logmsg("PRINTER: discarded abandoned spool ", path);
    return true;
}

// Switch ownership of the shared spool slot to `target_id`. If a different
// target was using it, finalize that target's spool first.
void printerSwitchOwner(int target_id)
{
    PrinterState &ps = g_printer_state;
    if (ps.owner_id == target_id) return;

    if (ps.spool_file.isOpen())
    {
        ps.spool_file.sync();
        ps.spool_file.close();
    }
    ps.owner_id = (int8_t)target_id;
    ps.next_seq = 0;
    ps.last_write_ms = 0;
    ps.seq_initialized = false;
    ps.dir_ready = false;
    ps.has_band_data = false;
    ps.spool_dir[0] = 0;
}

bool printerOpenSpoolIfNeeded(int target_id)
{
    printerSwitchOwner(target_id);
    PrinterState &ps = g_printer_state;

    // Reclaim the slot if the previous file was an unused trailing-SETUP
    // orphan. Must run before the isOpen() short-circuit below.
    printerDiscardIfAbandoned(ps);

    if (ps.spool_file.isOpen()) return true;

    if (!printerEnsureDir(ps, target_id)) return false;
    printerInitSeq(ps);

    char path[64];
    snprintf(path, sizeof(path), "%s/print_%04lu.bin",
             ps.spool_dir, (unsigned long)ps.next_seq);

    // Use SD.open() (returns FsFile by value) instead of reusing the closed
    // member; SdFat doesn't fully reset internal state on close() and a second
    // .open() on the same FsFile fails silently for a different path.
    ps.spool_file = SD.open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (!ps.spool_file.isOpen())
    {
        logmsg("PRINTER: failed to open spool file ", path);
        return false;
    }
    logmsg("PRINTER: spooling to ", path);
    ps.next_seq++;
    ps.has_band_data = false;
    ps.last_write_ms = platform_millis();
    return true;
}

void printerCloseSpool(int target_id)
{
    PrinterState &ps = g_printer_state;
    if (ps.spool_file.isOpen())
    {
        ps.spool_file.sync();
        ps.spool_file.close();
    }
}

void printerBuildRecordHeader(uint8_t out[10], uint32_t payload_len)
{
    memcpy(out, scsiDev.cdb, 6);
    out[6] = (uint8_t)(payload_len >> 24);
    out[7] = (uint8_t)(payload_len >> 16);
    out[8] = (uint8_t)(payload_len >> 8);
    out[9] = (uint8_t)(payload_len);
}

// Record a command (CDB + optional payload) into the spool stream.
// Opens the spool file (creating a new one if needed) before writing.
void printerRecord(int target_id, const uint8_t *payload, uint32_t payload_len)
{
    if (!printerOpenSpoolIfNeeded(target_id)) return;

    PrinterState &ps = g_printer_state;
    uint8_t hdr[10];
    printerBuildRecordHeader(hdr, payload_len);

    ps.spool_file.write(hdr, sizeof(hdr));
    if (payload && payload_len > 0)
    {
        ps.spool_file.write(payload, payload_len);
    }
    ps.last_write_ms = platform_millis();
}

// Streams `count` bytes from the SCSI bus into the spool file, in chunks
// sized so they fit in scsiDev.data[] without consuming extra RAM.
void printerStreamMaskFromBus(int target_id, uint32_t count)
{
    PrinterState &ps = g_printer_state;
    scsiEnterPhase(DATA_OUT);
    while (count > 0)
    {
        uint32_t chunk = count;
        if (chunk > sizeof(scsiDev.data)) chunk = sizeof(scsiDev.data);

        int parityError = 0;
        scsiRead(scsiDev.data, chunk, &parityError);

        if (ps.spool_file.isOpen())
        {
            ps.spool_file.write(scsiDev.data, chunk);
        }
        count -= chunk;
    }
}

void printerHandleReadBlockLimitsHeader();

// Hook fires once the 10-byte header has been received in scsiDev.data[].
void printerHandleReadBlockLimitsHeader()
{
    scsiDev.postDataOutHook = NULL;
    if (!g_printer_pending.active) return;
    int target_id = g_printer_pending.target_id;
    g_printer_pending.active = false;

    // Header layout (big-endian):
    //   bytes 0..1 = x1, 2..3 = y1, 4..5 = x2, 6..7 = y2,
    //   bytes 8..9 = padding | mask-type bits.
    uint16_t x1 = ((uint16_t)scsiDev.data[0] << 8) | scsiDev.data[1];
    uint16_t y1 = ((uint16_t)scsiDev.data[2] << 8) | scsiDev.data[3];
    uint16_t x2 = ((uint16_t)scsiDev.data[4] << 8) | scsiDev.data[5];
    uint16_t y2 = ((uint16_t)scsiDev.data[6] << 8) | scsiDev.data[7];

    uint32_t width  = (x2 > x1) ? (uint32_t)(x2 - x1) : 0;
    uint32_t height = (y2 > y1) ? (uint32_t)(y2 - y1) : 0;
    uint32_t mask_bytes = ((width + 7) / 8) * height;

    dbgmsg("PRINTER: band ", (int)x1, ",", (int)y1, " -> ",
           (int)x2, ",", (int)y2, " mask=", (int)mask_bytes, "B");

    if (printerOpenSpoolIfNeeded(target_id))
    {
        // Write the band record header + the 10-byte band header payload
        // directly; the mask streams in via printerStreamMaskFromBus below.
        uint8_t hdr[10];
        printerBuildRecordHeader(hdr, 10 + mask_bytes);
        PrinterState &ps = g_printer_state;
        ps.spool_file.write(hdr, sizeof(hdr));
        ps.spool_file.write(scsiDev.data, 10);
        ps.has_band_data = true;            // protects file from abandonment
        ps.last_write_ms = platform_millis();
    }

    // Stage 3: blind data write of mask_bytes. We must not reselect or the
    // host's "blind" write is lost.
    if (mask_bytes > 0)
    {
        printerStreamMaskFromBus(target_id, mask_bytes);
        g_printer_state.last_write_ms = platform_millis();
    }

    scsiDev.phase = STATUS;
    scsiDev.status = GOOD;
}

// Hook for FORMAT/SETUP-style data-out: record the CDB + payload now that
// scsiDev.data[] holds the parameter list.
void printerHandlePayloadReceived()
{
    int target_id = scsiDev.target->targetId;
    scsiDev.postDataOutHook = NULL;
    printerRecord(target_id, scsiDev.data, scsiDev.dataLen);
    scsiDev.phase = STATUS;
    scsiDev.status = GOOD;
}

// SETUP (0x06) completion hook. Logs a human-readable paper name on a
// full-page setup (x_off == y_off == 0) using the LaserWriter IISC Service
// Manual printable-area table, then defers to the generic record/spool
// path. Per-region setups (non-zero offsets) are spooled silently.
void printerHandleSetupReceived()
{
    int target_id = scsiDev.target->targetId;
    scsiDev.postDataOutHook = NULL;

    if (scsiDev.dataLen >= 8)
    {
        uint16_t y_off = ((uint16_t)scsiDev.data[0] << 8) | scsiDev.data[1];
        uint16_t x_off = ((uint16_t)scsiDev.data[2] << 8) | scsiDev.data[3];
        uint16_t w     = ((uint16_t)scsiDev.data[4] << 8) | scsiDev.data[5];
        uint16_t h     = ((uint16_t)scsiDev.data[6] << 8) | scsiDev.data[7];

        if (x_off == 0 && y_off == 0)
        {
            // Printable-area dimensions from the LaserWriter IISC Service
            // Manual. These are NOT full-paper sizes (host-side renderers
            // pick paper independently); they're what the driver sends.
            const char *paper = "Custom";
            if      (w == 2400 && h == 3175) paper = "US Letter (8.0\"x10.6\")";
            else if (w == 2000 && h == 3750) paper = "US Legal (6.72\"x12.5\")";
            else if (w == 2400 && h == 3375) paper = "A4 (8.0\"x11.27\")";
            else if (w == 2000 && h == 2825) paper = "B5 (6.67\"x9.43\")";
            else if (w == 1136 && h == 2725) paper = "#10 Envelope (3.84\"x9.1\")";

            logmsg("PRINTER: page setup ", (int)w, "x", (int)h,
                   " printable area: ", paper);
        }
    }

    printerRecord(target_id, scsiDev.data, scsiDev.dataLen);
    scsiDev.phase = STATUS;
    scsiDev.status = GOOD;
}

void printerStartDataOut(uint32_t len)
{
    scsiDev.dataLen = len;
    scsiDev.dataPtr = 0;
    scsiDev.phase = DATA_OUT;
    scsiDev.postDataOutHook = printerHandlePayloadReceived;
}

} // namespace

extern "C" int scsiPrinterCommand(void)
{
    uint8_t command = scsiDev.cdb[0];
    int target_id = scsiDev.target->targetId;

    switch (command)
    {
        case 0x04:
        {
            // FORMAT. Driver passes 4 bytes of timing data which we capture
            // for the spool stream; the bytes themselves don't matter for
            // emulation.
            uint32_t len = scsiDev.cdb[4];
            if (len == 0)
            {
                printerRecord(target_id, NULL, 0);
                scsiDev.phase = STATUS;
                scsiDev.status = GOOD;
            }
            else
            {
                printerStartDataOut(len);
            }
            return 1;
        }

        case 0x05:
        {
            // READ BLOCK LIMITS — vendor 2-stage. Stage 1 receives the
            // 10-byte band header; the postDataOutHook does the blind read
            // of the mask data.
            g_printer_pending.target_id = (uint8_t)target_id;
            g_printer_pending.active = true;
            scsiDev.dataLen = 10;
            scsiDev.dataPtr = 0;
            scsiDev.phase = DATA_OUT;
            scsiDev.postDataOutHook = printerHandleReadBlockLimitsHeader;
            return 1;
        }

        case 0x06:
        {
            // SETUP — page setup. cdb[4] is parameter list length (8).
            // Always opens a spool file. If no bands follow within
            // PRINTER_ABANDON_MS, the orphan is reaped on the next command.
            // Use a SETUP-specific completion hook so we can log the
            // recognised paper size on a full-page setup.
            uint32_t len = scsiDev.cdb[4];
            if (len == 0)
            {
                printerRecord(target_id, NULL, 0);
                scsiDev.phase = STATUS;
                scsiDev.status = GOOD;
            }
            else
            {
                scsiDev.dataLen = len;
                scsiDev.dataPtr = 0;
                scsiDev.phase = DATA_OUT;
                scsiDev.postDataOutHook = printerHandleSetupReceived;
            }
            return 1;
        }

        case 0x0A:
        {
            // PRINT. Bit 7 of cdb[5] = "actually print this page".
            // Only record/finalize if real band data has been written —
            // PRINT with no preceding bands is just the driver acking the
            // engine and has nothing to print.
            PrinterState &ps = g_printer_state;
            bool finalize = (scsiDev.cdb[5] & 0x80) != 0;

            if (ps.has_band_data)
            {
                printerRecord(target_id, NULL, 0);
                if (finalize)
                {
                    logmsg("PRINTER: page complete, closing spool (control=",
                           bytearray(&scsiDev.cdb[5], 1), ")");
                    printerCloseSpool(target_id);
                }
            }
            scsiDev.phase = STATUS;
            scsiDev.status = GOOD;
            return 1;
        }

        case 0x08:  // READ(6)
        case 0x28:  // READ(10)
        case 0x2A:  // WRITE(10)
        case 0x25:  // READ CAPACITY
            // The LaserWriter is print-only and uses group 0 only. Hosts
            // probing the bus may still issue these — reject silently with
            // INVALID_COMMAND_OPERATION_CODE rather than letting them fall
            // through to scsiDiskCommand and log a misleading "exceeding
            // image size" warning against the zero-size placeholder file.
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = ILLEGAL_REQUEST;
            scsiDev.target->sense.asc = INVALID_COMMAND_OPERATION_CODE;
            scsiDev.phase = STATUS;
            return 1;

        default:
            // Let the standard handlers deal with INQUIRY (0x12),
            // REQUEST SENSE (0x03), TEST UNIT READY (0x00),
            // MODE SELECT (0x15), MODE SENSE (0x1A), etc.
            return 0;
    }
}
