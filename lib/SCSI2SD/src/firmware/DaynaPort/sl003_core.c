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

/* SL003 SCSI/Link protocol core.
 *
 * Behaviour and structure follow the Dayna SL003 v2.0 firmware
 * disassembly; ROM addresses appear in comments.
 */
#ifdef BLUESCSI_NETWORK
#include <string.h>
#include "sl003_core.h"

void sl003_init(sl003_t *s, const uint8_t mac[6],
                struct scsiNetworkPacketQueue *ring)
{
    memset(s, 0, sizeof *s);
    memcpy(s->mac, mac, 6);
    s->rx = ring;
    s->gap_header_us = SL003_GAP_AFTER_HEADER_US;
    s->gap_record_us = SL003_GAP_AFTER_RECORD_US;
}

void sl003_set_gaps(sl003_t *s, uint16_t header_us, uint16_t record_us)
{
    s->gap_header_us = header_us;
    s->gap_record_us = record_us;
}

uint16_t sl003_rx_count(const sl003_t *s)
{
    return (uint16_t)((s->rx->writeIndex - s->rx->readIndex
                       + NETWORK_PACKET_QUEUE_SIZE) % NETWORK_PACKET_QUEUE_SIZE);
}

uint16_t sl003_records_sent(const sl003_t *s) { return s->records_sent; }
bool     sl003_enabled(const sl003_t *s)      { return s->enabled; }
bool     sl003_mode_set(const sl003_t *s)     { return s->mode_set; }

void sl003_count_missed(sl003_t *s, uint32_t n) { s->ctr_missed_seen += n; }
void sl003_count_crc(sl003_t *s)     { s->ctr_crc++; }
void sl003_count_align(sl003_t *s)   { s->ctr_align++; }
void sl003_count_overrun(sl003_t *s) { s->ctr_overrun++; }

const uint8_t *sl003_mac(const sl003_t *s) { return s->mac; }

bool sl003_multicast_take(sl003_t *s)
{
    bool d = s->multicast_dirty;
    s->multicast_dirty = false;
    return d;
}

uint8_t        sl003_multicast_count(const sl003_t *s) { return s->multicast_count; }
const uint8_t *sl003_multicast_addr(const sl003_t *s, uint8_t i)
{
    return s->multicast[i];
}

/* Until 0x0e enables the interface only these four are accepted (1c60). */
static bool allowed_while_disabled(uint8_t op)
{
    return op == SL003_CMD_TEST_UNIT_READY ||
           op == SL003_CMD_REQUEST_SENSE   ||
           op == SL003_CMD_ENABLE          ||
           op == SL003_CMD_INQUIRY;
}

static uint8_t rx_next_index(const sl003_t *s)
{
    return (uint8_t)((s->rx->readIndex + 1) % NETWORK_PACKET_QUEUE_SIZE);
}

/* [len hi][len lo][0][0][0][flags]. len is the whole frame including its
 * FCS, never the clipped amount (0x0e8a). */
static void build_record_header(const sl003_t *s, uint8_t hdr[6],
                                uint16_t len, bool more, bool runt)
{
    memset(hdr, 0, SL003_RECORD_HDR_LEN);
    hdr[0] = (uint8_t)(len >> 8);
    hdr[1] = (uint8_t)len;
    hdr[5] = (uint8_t)((more ? SL003_FLAG_MORE : 0) |
                       ((runt && s->mode_set) ? SL003_FLAG_RUNT : 0));
}

/*
 * How many bytes we may ask fetch() to deposit in wbuf. CDB[3..4] is
 * sixteen bits of whatever the host felt like sending, and fetch fills
 * exactly the count we hand it -- an unclamped value is a buffer overflow
 * we asked for.
 */
static uint16_t clamp_want(uint16_t want)
{
    return want > SL003_PKT_MAX ? (uint16_t)SL003_PKT_MAX : want;
}

/* ------------------------------------------------------------------ */
/* READ(6), polled: exactly one record, never a terminator (0x0d77).   */
/* ------------------------------------------------------------------ */

static void read_polled(sl003_t *s, const uint8_t cdb[6], uint16_t alloc,
                        const sl003_io *io)
{
    uint8_t  hdr[SL003_RECORD_HDR_LEN];
    uint16_t len, off, cap, send;

    if (sl003_rx_count(s) == 0) {
        /* The empty-queue answer is a real six-byte record of length zero. */
        build_record_header(s, hdr, 0, false, false);
        io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN);
        return;
    }

    len = s->rx->sizes[s->rx->readIndex];
    /* CDB[1..2] is a byte offset into the head packet: the stateless
     * peek/resume the ROM supports at 0x0e05-0x0f17. */
    off = (uint16_t)((cdb[1] << 8) | cdb[2]);
    if (off > len) off = len;
    cap  = alloc > SL003_RECORD_HDR_LEN
         ? (uint16_t)(alloc - SL003_RECORD_HDR_LEN) : 0;
    send = (uint16_t)(len - off);
    if (send > cap) send = cap;

    build_record_header(s, hdr, len,
                        rx_next_index(s) != s->rx->writeIndex, len < 60);
    if (!io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN))
        return;                         /* bus gone; the packet stays queued */

    /*
     * An allocation too small to carry payload is a pure peek: the host
     * sees the header and the packet stays queued, whatever CDB[5] says.
     */
    if (alloc <= SL003_RECORD_HDR_LEN)
        return;

    io->gap(io->ctx, s->gap_header_us);

    if (send) {
        if (!io->emit(io->ctx, s->rx->packets[s->rx->readIndex] + off, send))
            return;                     /* bus gone mid-payload: keep it */
    }

    /* Dequeue iff this read finished the packet, or the host asked for the
     * remainder to be dropped (CDB[5] != 0). emit has returned, so the
     * slot is no longer being transmitted from and may go back. */
    if ((uint16_t)(off + send) >= len || cdb[5] != 0)
        s->rx->readIndex = rx_next_index(s);
}

/* ------------------------------------------------------------------ */
/* READ(6), blind: a stream of records (0x0bfc-0x0d6a).                */
/* ------------------------------------------------------------------ */

static void read_blind(sl003_t *s, uint16_t alloc, const sl003_io *io)
{
    uint8_t  hdr[SL003_RECORD_HDR_LEN];
    uint32_t batch_bytes = 0;
    uint16_t limit;

    /*
     * Batch bound. The ROM has none: the record cap is its only limit and
     * the allocation clips each record, not the total (0x0cb7-0x0cf2).
     * An allocation of two or more maximum records is taken as the host's
     * buffer size and bounds the whole batch; ALLOC 1524 (all known Mac
     * drivers) streams unbounded as the ROM does. Fixed-length initiators
     * with no residual reporting (Atari SCSI_In) need the bound.
     */
    limit = 0;
    if (alloc >= 2 * SL003_MAX_RECORD)
        limit = alloc;

    for (;;) {
        uint16_t len, cap, send;
        bool more;

        if (s->records_sent >= SL003_BLIND_RECORD_CAP) {
            /* Cap reached while more is pending: close the batch with a
             * zero-length record so the host knows to read again (0x0c1c). */
            build_record_header(s, hdr, 0, false, false);
            io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN);
            return;
        }

        if (sl003_rx_count(s) == 0) {
            /* Nothing left. If we already sent a record its flag said so
             * and the host has stopped; only an empty batch owes a reply. */
            if (s->records_sent > 0)
                return;
            build_record_header(s, hdr, 0, false, false);
            io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN);
            return;
        }

        len = s->rx->sizes[s->rx->readIndex];

        /*
         * Tested BEFORE adding the record; testing after would overrun
         * the host's transfer by up to one record. Redundant with the
         * more-flag suppression below, kept as a backstop. Never applies
         * to the first record: the per-record clip fits it.
         */
        if (limit && s->records_sent > 0
         && batch_bytes + len + SL003_RECORD_HDR_LEN > limit)
            return;

        cap  = alloc > SL003_RECORD_HDR_LEN
             ? (uint16_t)(alloc - SL003_RECORD_HDR_LEN) : 0;
        send = len < cap ? len : cap;

        /* Counts the whole record, not the clipped payload. Clipping and
         * a limit cannot coexist (clip needs alloc < 1 record, the limit
         * needs >= 2), and overstating is the safe direction anyway. */
        batch_bytes += (uint32_t)len + SL003_RECORD_HDR_LEN;

        /* Read live from the queue, so a frame arriving mid-batch is seen
         * (0x0ca6). */
        more = rx_next_index(s) != s->rx->writeIndex;

        /* The flag is a promise of another record in THIS batch, so check
         * the record actually queued next; its length is fixed and the
         * producer can only append behind it. */
        if (more && limit) {
            uint16_t nlen = s->rx->sizes[rx_next_index(s)];
            if (batch_bytes + nlen + SL003_RECORD_HDR_LEN > limit)
                more = false;
        }

        build_record_header(s, hdr, len, more, len < 60);
        if (!io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN))
            return;
        io->gap(io->ctx, s->gap_header_us);

        if (send) {
            if (!io->emit(io->ctx,
                          s->rx->packets[s->rx->readIndex], send)) {
                s->rx->readIndex = rx_next_index(s);   /* spent either way */
                return;
            }
            io->gap(io->ctx, s->gap_record_us);
        }

        /* emit has returned; the slot may go back to the producer. */
        s->rx->readIndex = rx_next_index(s);
        s->records_sent++;

        if (!more)
            return;
    }
}

/* ------------------------------------------------------------------ */
/* READ MAC + statistics (0x09): 22 bytes, counters read-and-clear.    */
/* ------------------------------------------------------------------ */

/* The Z180 stored these little-endian and the ROM hands them out untouched,
 * which is why the Dayna driver runs each through swaplong. Byte-swapping
 * them here would silently break that driver. */
static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void read_stats(sl003_t *s, uint16_t alloc, const sl003_io *io)
{
    uint8_t  buf[SL003_STATS_LEN];
    uint16_t len;

    memcpy(buf, s->mac, 6);
    put_le32(buf + 6,  s->ctr_missed_seen);
    put_le32(buf + 10, s->ctr_crc);
    put_le32(buf + 14, s->ctr_align);
    put_le32(buf + 18, s->ctr_overrun);
    /* Cleared on every 0x09, regardless of how much the host takes: the
     * ROM's counters are read-and-clear. */
    s->ctr_missed_seen = s->ctr_crc = s->ctr_align = s->ctr_overrun = 0;

    len = alloc < SL003_STATS_LEN ? alloc : SL003_STATS_LEN;
    if (len)
        io->emit(io->ctx, buf, len);
}

/* ------------------------------------------------------------------ */
/* WRITE(6): raw packet or record stream, by CDB[5] bit 7 (0x1281).    */
/* ------------------------------------------------------------------ */

static void write_stream(sl003_t *s, const sl003_io *io)
{
    /* [len(2)][pad(2)][payload] repeated; a zero-length header ends the
     * stream and CDB[3..4] is ignored (0x1294). */
    for (;;) {
        uint16_t reclen;

        if (!io->fetch(io->ctx, s->wbuf, 4))
            return;                     /* bus gone; nothing leaks */
        reclen = (uint16_t)((s->wbuf[0] << 8) | s->wbuf[1]);
        if (reclen == 0)
            return;                     /* terminator */
        if (reclen > SL003_PKT_MAX)
            return;                     /* absurd length: abandon the stream */
        if (!io->fetch(io->ctx, s->wbuf, reclen))
            return;
        io->tx(io->ctx, s->wbuf, reclen);
    }
}

/* ------------------------------------------------------------------ */
/* Dispatch (0x0b5e)                                                   */
/* ------------------------------------------------------------------ */

sl003_result sl003_handle(sl003_t *s, const uint8_t cdb[6],
                          const sl003_io *io)
{
    uint16_t alloc = (uint16_t)((cdb[3] << 8) | cdb[4]);

    if (!s->enabled && !allowed_while_disabled(cdb[0]))
        return SL003_CHECK_CONDITION;

    switch (cdb[0]) {
    case SL003_CMD_TEST_UNIT_READY:
        return SL003_OK;

    case SL003_CMD_INQUIRY:
        /* Answered by s2s_scsiInquiry before this personality runs; the
         * byte-36 status trailer lives in inquiry.c. */
        return SL003_OK;

    case SL003_CMD_REQUEST_SENSE:
        /* Served by s2s_scsiRequestSense from scsiDev.target->sense before
         * a personality is ever reached. Accepted here only so the
         * disabled gate agrees with what the host actually sees. */
        return SL003_OK;

    case SL003_CMD_ENABLE:
        if (cdb[5] & SL003_ENABLE_BIT) {
            s->enabled = true;
            /* Drop what is queued by moving OUR index up to the
             * producer's. Writing writeIndex here would race a producer
             * that runs concurrently with SCSI commands. */
            s->rx->readIndex = s->rx->writeIndex;
        } else {
            s->enabled = false;
        }
        return SL003_OK;

    case SL003_CMD_READ_STATS:
        read_stats(s, alloc, io);
        return SL003_OK;

    case SL003_CMD_SET_MODE:
        /* SET MAC (bit 6) is refused: the radio cannot change its receive
         * address, and accepting would report a MAC via 0x09 that never
         * receives. Refused before any DATA OUT; a combined mode+addr CDB
         * is refused whole. Wire a real implementation here if the
         * platform ever grows one. */
        if (cdb[5] & SL003_SETMODE_ADDR_BIT)
            return SL003_CHECK_FIELD;
        if (cdb[5] & SL003_SETMODE_MODE_BIT) {
            s->mode     = cdb[4];
            s->mode_set = true;
        }
        return SL003_OK;

    case SL003_CMD_ADD_MULTICAST: {
        /* 6-byte strides over the whole list, not just the first address:
         * the real device walks it all and rebuilds its hash filter. */
        uint16_t want = clamp_want(alloc);
        uint16_t off;
        if (want == 0)
            return SL003_OK;
        if (!io->fetch(io->ctx, s->wbuf, want))
            return SL003_OK;
        s->multicast_count = 0;
        for (off = 0; off + 6 <= want &&
                      s->multicast_count < SL003_MULTICAST_MAX; off += 6) {
            memcpy(s->multicast[s->multicast_count], s->wbuf + off, 6);
            s->multicast_count++;
        }
        s->multicast_dirty = true;
        return SL003_OK;
    }

    case SL003_CMD_WRITE:
        /* Nonzero CDB[1..2] is rejected by the real device (0x1281). */
        if (cdb[1] != 0 || cdb[2] != 0)
            return SL003_CHECK_FIELD;
        if (cdb[5] & SL003_WRITE_STREAM_BIT) {
            write_stream(s, io);
        } else {
            uint16_t want = clamp_want(alloc);
            if (want && io->fetch(io->ctx, s->wbuf, want))
                io->tx(io->ctx, s->wbuf, want);
        }
        return SL003_OK;

    case SL003_CMD_READ:
        s->records_sent = 0;
        if (cdb[5] & SL003_READ_BLIND_BIT)
            read_blind(s, alloc, io);
        else
            read_polled(s, cdb, alloc, io);
        return SL003_OK;

    default:
        return SL003_CHECK_CONDITION;
    }
}

#endif /* BLUESCSI_NETWORK */
