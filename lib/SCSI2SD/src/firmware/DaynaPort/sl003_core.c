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
                struct scsiNetworkPacketQueue *ring,
                uint8_t *wbuf, uint32_t wbuf_len)
{
    memset(s, 0, sizeof *s);
    memcpy(s->mac, mac, 6);
    s->rx = ring;
    s->wbuf = wbuf;
    s->wbuf_len = wbuf_len;
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

    /*
     * An allocation too small to carry payload is a pure peek: the host
     * sees the header alone and the packet stays queued, whatever
     * CDB[5] says.
     */
    if (alloc <= SL003_RECORD_HDR_LEN) {
        build_record_header(s, hdr, len,
                            rx_next_index(s) != s->rx->writeIndex, len < 60);
        io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN);
        return;
    }

    if (cdb[5] & SL003_READ_SEAMLESS_BIT) {
        /* Host opted in: header and payload staged into wbuf and sent
         * as ONE emit (see read_blind: seam pauses trigger a host-side
         * injection bug). wbuf is the caller's data-phase scratch and
         * is idle here, READ having no DATA OUT. */
        build_record_header(s, s->wbuf, len,
                            rx_next_index(s) != s->rx->writeIndex, len < 60);
        memcpy(s->wbuf + SL003_RECORD_HDR_LEN,
               s->rx->packets[s->rx->readIndex] + off, send);
        if (!io->emit(io->ctx, s->wbuf,
                      (uint16_t)(SL003_RECORD_HDR_LEN + send)))
            return;                     /* bus gone: keep the packet */
    } else {
        build_record_header(s, hdr, len,
                            rx_next_index(s) != s->rx->writeIndex, len < 60);
        if (!io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN))
            return;                     /* bus gone; the packet stays queued */
        io->gap(io->ctx, s->gap_header_us);
        if (send) {
            if (!io->emit(io->ctx,
                          s->rx->packets[s->rx->readIndex] + off, send))
                return;                 /* bus gone mid-payload: keep it */
        }
    }

    /* Dequeue iff this read finished the packet, or the host asked for
     * the remainder to be dropped (any ROM-defined CDB[5] bit set; the
     * seamless bit is an emission request, not a drop request, and
     * must not count). emit has returned, so the slot is no longer
     * being transmitted from and may go back. */
    if ((uint16_t)(off + send) >= len
     || (cdb[5] & (uint8_t)~SL003_READ_SEAMLESS_BIT) != 0)
        s->rx->readIndex = rx_next_index(s);
}

/* ------------------------------------------------------------------ */
/* READ(6), blind: a stream of records (0x0bfc-0x0d6a).                */
/* ------------------------------------------------------------------ */

/*
 * Seamless staging. The host's promise for a blind transfer is a data
 * phase with no REQ pause anywhere in it, and one emit per record does
 * not keep it: the pause BETWEEN records is a stall too, and the
 * Quadra-era SCSI Manager 4.3 SIM fabricates two bytes at any stall the
 * host did not declare. So a seamless batch accumulates whole and goes
 * out as ONE transfer, never split: a batch that would outgrow the
 * staging area ends early instead, which blind mode permits the device
 * anywhere, and the remaining records lead the next command.
 */
static bool stage_flush(sl003_t *s, const sl003_io *io, uint32_t *staged)
{
    uint32_t n = *staged;

    *staged = 0;
    if (n == 0)
        return true;
    return io->emit(io->ctx, s->wbuf, (uint16_t)n);
}

static void read_blind(sl003_t *s, const uint8_t cdb[6], uint16_t alloc,
                       const sl003_io *io)
{
    uint8_t  hdr[SL003_RECORD_HDR_LEN];
    uint32_t sent = 0;                  /* bytes actually emitted so far */
    uint32_t staged = 0;
    bool     seamless = (cdb[5] & SL003_READ_SEAMLESS_BIT) != 0;
    /* One transfer is one emit, and emit counts in 16 bits. */
    uint32_t stage_cap = s->wbuf_len > 0xFFFF ? 0xFFFF : s->wbuf_len;
    uint16_t cap;
    bool     bounded;

    /*
     * Batch bound. The ROM has none: the record cap is its only limit and
     * the allocation clips each record, not the total (0x0cb7-0x0cf2).
     * Bounded only when the host asks for it with SL003_READ_BOUNDED_BIT;
     * hosts learn the bit is honored from the INQUIRY capability bit.
     * Without the bit every allocation streams unbounded, exactly as the
     * ROM does. Fixed-length initiators with no residual reporting
     * (Atari SCSI_In) must set the bit.
     */
    bounded = (cdb[5] & SL003_READ_BOUNDED_BIT) != 0;

    /* A bounded batch never exceeds alloc, so with no room for even one
     * header there is nothing that can be said. */
    if (bounded && alloc < SL003_RECORD_HDR_LEN)
        return;

    /*
     * Per-record clip, the ROM's: the allocation bounds one record, and
     * the batch bound below is what keeps the total inside it. Taking
     * the terminator's reserve off this as well would clip a full-size
     * record while its header still declared the full length, and the
     * host's walk would run off the end of it.
     */
    cap = alloc > SL003_RECORD_HDR_LEN
        ? (uint16_t)(alloc - SL003_RECORD_HDR_LEN) : 0;

    for (;;) {
        uint16_t len, send;
        bool more;

        if (s->records_sent >= SL003_BLIND_RECORD_CAP) {
            if (bounded)
                goto term;
            /* Cap reached while more is pending: close the batch with a
             * zero-length record so the host knows to read again (0x0c1c). */
            goto close_empty;
        }

        if (sl003_rx_count(s) == 0) {
            /* Nothing left. If we already sent a record its flag said so
             * and the host has stopped; only an empty batch owes a reply.
             * Identical in bounded mode: a drained queue ends the batch
             * the ROM way, no terminator. */
            if (s->records_sent > 0)
                goto done;
            goto close_empty;
        }

        len = s->rx->sizes[s->rx->readIndex];

        /*
         * The batch bound, tested BEFORE adding the record: testing
         * after would overrun the host's transfer by up to one record.
         * Only whole records join a bounded batch, so the full length
         * is tested, not the clipped one, and one header is reserved
         * so the closing terminator always fits behind the records it
         * closes. Never applies to the first record, which the
         * per-record clip already fits; only there can the reserve go
         * unmet, when that record fills the whole allocation.
         */
        if (bounded && s->records_sent > 0
         && sent + 2 * SL003_RECORD_HDR_LEN + len > alloc)
            goto term;

        send = len < cap ? len : cap;

        /* Read live from the queue, so a frame arriving mid-batch is seen
         * (0x0ca6). */
        more = rx_next_index(s) != s->rx->writeIndex;

        if (seamless) {
            /* Append to the batch; it leaves as one transfer, ALWAYS:
             * a flush mid-batch would be the very stall the bit asks
             * to remove. A record that will not fit the staging area
             * (with the closing record's reserve) ends the batch
             * instead -- blind mode lets the device end a batch
             * anywhere, and the record leads the next one. Bounded
             * batches end with their depth-carrying terminator. */
            uint32_t need = (uint32_t)SL003_RECORD_HDR_LEN + send;

            if (s->records_sent > 0
             && staged + need + SL003_RECORD_HDR_LEN > stage_cap) {
                if (bounded)
                    goto term;
                goto close_empty;
            }
            if (need > stage_cap) {
                /* Caller under the documented wbuf minimum. Clip the
                 * first record rather than overflow; the header still
                 * declares the full length, as the ROM's own alloc
                 * clip does. */
                send = stage_cap > SL003_RECORD_HDR_LEN
                     ? (uint16_t)(stage_cap - SL003_RECORD_HDR_LEN) : 0;
                need = (uint32_t)SL003_RECORD_HDR_LEN + send;
            }
            build_record_header(s, s->wbuf + staged, len, more, len < 60);
            memcpy(s->wbuf + staged + SL003_RECORD_HDR_LEN,
                   s->rx->packets[s->rx->readIndex], send);
            staged += need;
        } else {
            /* Classic timing: split emit with the ROM's gaps, which
             * software-timed hosts (Plus, SE/30) calibrated their
             * blind loops against. */
            build_record_header(s, hdr, len, more, len < 60);
            if (!io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN))
                return;
            io->gap(io->ctx, s->gap_header_us);
            if (send) {
                if (!io->emit(io->ctx,
                              s->rx->packets[s->rx->readIndex], send)) {
                    s->rx->readIndex = rx_next_index(s);
                    return;
                }
                io->gap(io->ctx, s->gap_record_us);
            }
            /* Contiguous-emission hosts are hardware-handshaked and
             * get no pacing gaps at all: one gap configuration then
             * serves every machine on a shared card. */
        }

        /* The slot may go back to the producer: emit has returned, or
         * in a staged batch the bytes are copied out of it. */
        s->rx->readIndex = rx_next_index(s);
        s->records_sent++;
        sent += (uint32_t)SL003_RECORD_HDR_LEN + send;

        if (!more)
            goto done;
    }

term:
    /* Budget reached with records still queued: end the batch the way
     * the ROM ends one at its record cap -- the last record kept its
     * truthful more-flag, and this zero-length terminator says the
     * batch ends anyway. Byte 2 reports the queue depth (records,
     * clamped to 255) for adaptive pacing; hosts that ignore it see
     * ROM behavior exactly. */
    {
        uint16_t q = sl003_rx_count(s);

        build_record_header(s, hdr, 0, q > 0, false);
        hdr[2] = q > 255 ? 255 : (uint8_t)q;
        goto close;
    }

close_empty:
    build_record_header(s, hdr, 0, false, false);

close:
    /* Reached only when a first record filled the whole allocation:
     * the allocation comes first, and the batch ends on that record's
     * more-flag with just the depth hint lost. Every later record was
     * admitted with the terminator's reserve, so its room is
     * guaranteed. */
    if (bounded && sent + SL003_RECORD_HDR_LEN > alloc)
        goto done;

    /* The closing record rides out with the batch it ends: appended
     * when there is room, otherwise after a flush. */
    if (seamless) {
        if (staged + SL003_RECORD_HDR_LEN > stage_cap
         && !stage_flush(s, io, &staged))
            return;
        memcpy(s->wbuf + staged, hdr, SL003_RECORD_HDR_LEN);
        staged += SL003_RECORD_HDR_LEN;
    } else {
        io->emit(io->ctx, hdr, SL003_RECORD_HDR_LEN);
    }

done:
    if (seamless)
        stage_flush(s, io, &staged);
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
            read_blind(s, cdb, alloc, io);
        else
            read_polled(s, cdb, alloc, io);
        return SL003_OK;

    default:
        return SL003_CHECK_CONDITION;
    }
}

#endif /* BLUESCSI_NETWORK */
