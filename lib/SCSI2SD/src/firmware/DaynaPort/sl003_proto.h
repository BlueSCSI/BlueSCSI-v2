/*
 * Copyright (c) 2026 Ingo Paschke <ipaschke@lpclabs.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* SL003 SCSI/Link wire protocol constants.
 *
 * Values come from the disassembly of the Dayna SL003 v2.0 firmware; ROM
 * addresses are cited. Suitable for host drivers too.
 */
#ifndef SL003_PROTO_H
#define SL003_PROTO_H

/* Opcodes. The ROM dispatches these from a table at 0x0b5e; anything else
 * reaches thunk_illegal_default and is rejected. */
#define SL003_CMD_TEST_UNIT_READY   0x00
#define SL003_CMD_REQUEST_SENSE     0x03
#define SL003_CMD_READ              0x08
#define SL003_CMD_READ_STATS        0x09
#define SL003_CMD_WRITE             0x0a
#define SL003_CMD_SET_MODE          0x0c
#define SL003_CMD_ADD_MULTICAST     0x0d
#define SL003_CMD_ENABLE            0x0e
#define SL003_CMD_INQUIRY           0x12

/* CDB[5] selectors */
#define SL003_READ_BLIND_BIT        0x40  /* clear = polled (0x0f3b)      */
#define SL003_WRITE_STREAM_BIT      0x80  /* clear = one raw packet       */
#define SL003_ENABLE_BIT            0x80  /* 0x0e: set = enable           */
#define SL003_SETMODE_MODE_BIT      0x80  /* 0x0c: mode byte in CDB[4]    */
#define SL003_SETMODE_ADDR_BIT      0x40  /* 0x0c: 6-byte MAC in DATA OUT */

/* Record: [len hi][len lo][0][0][0][flags]. len includes the 4-byte FCS. */
#define SL003_RECORD_HDR_LEN        6
#define SL003_FRAME_MAX             1518
#define SL003_READ_ALLOC            1524  /* header + max frame           */

/* Flags byte (header byte 5) */
#define SL003_FLAG_MORE             0x10  /* more packets queued (0x0ca6) */
#define SL003_FLAG_RUNT             0x80  /* only once a mode is set      */

/* Blind batches stop here and emit a zero-length terminator (0x0d5c,
 * 0x0c1c). v1.3 had no cap at all. Overridable so tests can reach the cap
 * without queueing 200 packets. */
#ifndef SL003_BLIND_RECORD_CAP
#define SL003_BLIND_RECORD_CAP      200
#endif

/* Inter-record gaps measured on a bus analyser. The ROM contains no
 * programmed delays; these are emergent Z180/DMA turnaround, and a
 * software-timed blind host depends on them. */
/* The largest record on the wire: a full frame with its FCS, plus the
 * six-byte header. Not SL003_PKT_MAX, which sizes a ring slot. */
#define SL003_MAX_RECORD            (1518 + SL003_RECORD_HDR_LEN)

#define SL003_GAP_AFTER_HEADER_US    75
#define SL003_GAP_AFTER_RECORD_US   300

/* Replies */
#define SL003_INQUIRY_LEN           37    /* clamp at 0x151c              */
#define SL003_STATS_LEN             22    /* MAC + four 32-bit counters   */
#define SL003_SENSE_LEN              9

/* INQUIRY byte 36 status bits (0x1500, 0x150e) */
#define SL003_INQ_ENABLED           0x80
#define SL003_INQ_MODE_SET          0x40

#define SL003_SENSE_ILLEGAL_REQUEST  5

#endif
