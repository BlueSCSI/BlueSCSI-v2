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

/* SL003 protocol core. No I/O of its own, no allocation.
 *
 * Structured like the ROM: a dispatch (0x0b5e) into one handler per
 * opcode, data phases inline. The bus is injected as an sl003_io.
 */
#ifndef SL003_CORE_H
#define SL003_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "sl003_proto.h"
#include "../network.h"   /* the shared receive ring */

#define SL003_PKT_MAX       1520          /* frame incl FCS, even */
#define SL003_MULTICAST_MAX   16

/* Payload emission points straight into ring slots, so a slot must never be
 * larger than the biggest record the core emits. */
_Static_assert(NETWORK_PACKET_MAX_SIZE <= SL003_PKT_MAX,
               "ring slot larger than the maximum record the core emits");

typedef enum {
    SL003_OK = 0,
    SL003_CHECK_CONDITION = 1,   /* bad opcode, or the disabled gate */
    SL003_CHECK_FIELD = 2        /* opcode fine, a CDB field is not */
} sl003_result;

/*
 * The I/O the core drives. All calls are synchronous; a pointer passed to
 * emit (possibly into a ring slot) is done with when emit returns.
 *
 *   emit   DATA IN. false = bus gone; the handler stops.
 *   fetch  DATA OUT, exactly len bytes. false = bus gone or transfer
 *          failed; the handler stops, the caller records which.
 *   gap    inter-record pacing in microseconds; may be a no-op.
 *   tx     a completed outbound frame, valid for the call's duration.
 */
typedef struct sl003_io {
    bool (*emit)(void *ctx, const uint8_t *data, uint16_t len);
    bool (*fetch)(void *ctx, uint8_t *buf, uint16_t len);
    void (*gap)(void *ctx, uint16_t us);
    void (*tx)(void *ctx, const uint8_t *frame, uint16_t len);
    void *ctx;
} sl003_io;

typedef struct sl003 {
    uint8_t   mac[6];
    uint8_t   multicast[SL003_MULTICAST_MAX][6];
    uint8_t   multicast_count;
    bool      multicast_dirty;   /* a list arrived; the filter needs it */
    uint8_t   mode;
    bool      mode_set;
    bool      enabled;

    /* Not owned: filled by scsiNetworkEnqueue, also read by the Amiga
     * personality. Single producer / single consumer: the producer owns
     * writeIndex and never writes the slot at readIndex (full ring
     * drops); we own readIndex and advance it only after emit() has
     * returned for the payload pointing into the slot. */
    struct scsiNetworkPacketQueue *rx;

    uint32_t  ctr_missed_seen, ctr_crc, ctr_align, ctr_overrun;

    /* configuration */
    uint16_t  gap_header_us;     /* pacing, see sl003_set_gaps */
    uint16_t  gap_record_us;

    /* What the last READ emitted -- the batch depth, for instrumentation. */
    uint16_t  records_sent;

    /* DATA OUT landing area, at least SL003_PKT_MAX + 8 bytes, supplied
     * by the caller (the firmware passes scsiDev.data rather than
     * spending 1.5 KB of static RAM; the RP2040 targets have none to
     * spare). CDB[3..4] is sixteen untrusted bits and fetch fills
     * exactly the count we ask for, so every requested length is
     * clamped to SL003_PKT_MAX. */
    uint8_t  *wbuf;
} sl003_t;

void     sl003_init(sl003_t *s, const uint8_t mac[6],
                    struct scsiNetworkPacketQueue *ring, uint8_t *wbuf);

/*
 * Inter-record pacing, default 75/300 us as observed from the real
 * device. Required by hosts whose blind read is a software-timed loop
 * (Mac Plus, SE/30): their blind mode fails at 0/0. Handshaked
 * initiators may run 0.
 */
void     sl003_set_gaps(sl003_t *s, uint16_t header_us, uint16_t record_us);

uint16_t sl003_rx_count(const sl003_t *s);
/* Records emitted by the READ that just ran -- the batch depth. */
uint16_t sl003_records_sent(const sl003_t *s);

/*
 * One SCSI command, start to finish: gate, dispatch, and every data phase,
 * driven through io. The ROM's 0x0b5e dispatch, with the bus abstracted.
 */
sl003_result sl003_handle(sl003_t *s, const uint8_t cdb[6],
                          const sl003_io *io);

bool sl003_enabled(const sl003_t *s);
bool sl003_mode_set(const sl003_t *s);

/*
 * Receiver error counters; 0x09 reports and clears them.
 * sl003_count_missed accumulates a delta: hand each drop over exactly
 * once, not as a running total. Only the missed counter has a producer;
 * crc/align/overrun read as zero.
 */
void           sl003_count_missed(sl003_t *s, uint32_t n);
void           sl003_count_crc(sl003_t *s);
void           sl003_count_align(sl003_t *s);
void           sl003_count_overrun(sl003_t *s);
const uint8_t *sl003_mac(const sl003_t *s);

/* True after ADD MULTICAST delivered a list; cleared by the read. Guards
 * against re-programming the filter from a stale list when a command
 * never reached DATA OUT. */
bool           sl003_multicast_take(sl003_t *s);
uint8_t        sl003_multicast_count(const sl003_t *s);
const uint8_t *sl003_multicast_addr(const sl003_t *s, uint8_t i);

#endif
