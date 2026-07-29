/*
 * DaynaPORT SCSI/Link personality.
 *
 * Copyright (c) 2023-2026 joshua stein <jcs@jcs.org>
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

/*
 * DaynaPORT personality: the sl003_io connecting sl003_core to the bus
 * and the radio. Dispatched from scsi.c on S2S_CFG_NETWORK, like
 * AmigaWIFI. INQUIRY and REQUEST SENSE are absent on purpose: scsi.c
 * answers both before any personality runs; the byte-36 trailer is in
 * inquiry.c.
 */

#ifdef BLUESCSI_NETWORK
#include <string.h>
#include "scsi.h"
#include "scsi2sd_time.h"
#include "scsiPhy.h"
#include "config.h"
#include "network.h"
#include "sl003_core.h"

#include "BlueSCSI_platform_network.h"
#include "hardware/sync.h"
#include "log.h"

extern long ini_getl(const char *section, const char *key, long defval,
                     const char *filename);
#ifndef CONFIGFILE
#define CONFIGFILE "bluescsi.ini"
#endif

static sl003_t g_sl003;
static bool    g_ready;

static void ensure_init(void)
{
	if (!g_ready) {
		sl003_init(&g_sl003, (const uint8_t *)scsiDev.boardCfg.wifiMACAddress,
		           &scsiNetworkInboundQueue);
		/* DaynaPortGapHeaderUs / DaynaPortGapRecordUs ([SCSI] in
		 * bluescsi.ini): inter-record pacing. Defaults match the real
		 * device; software-timed blind hosts (SE/30, Plus) need them,
		 * handshaked hosts can use 0. */
		sl003_set_gaps(&g_sl003,
			(uint16_t)ini_getl("SCSI", "DaynaPortGapHeaderUs",
			                   SL003_GAP_AFTER_HEADER_US, CONFIGFILE),
			(uint16_t)ini_getl("SCSI", "DaynaPortGapRecordUs",
			                   SL003_GAP_AFTER_RECORD_US, CONFIGFILE));
		g_ready = true;
	}
}

/*
 * Program the radio filter from the delivered multicast list; without
 * this the CYW43 drops those frames (EtherTalk, mDNS, IPv6 ND). The
 * platform has no removal call, so the filter only grows; addresses
 * already installed are skipped to keep re-sent lists from filling the
 * small table.
 */
static uint8_t g_filter[SL003_MULTICAST_MAX][6];
static uint8_t g_filter_count;

static void apply_multicast(void)
{
	uint8_t i, j;

	for (i = 0; i < sl003_multicast_count(&g_sl003); i++) {
		const uint8_t *a = sl003_multicast_addr(&g_sl003, i);

		for (j = 0; j < g_filter_count; j++)
			if (memcmp(g_filter[j], a, 6) == 0)
				break;
		if (j < g_filter_count)
			continue;

		platform_network_add_multicast_address((uint8_t *)a);
		if (g_filter_count < SL003_MULTICAST_MAX)
			memcpy(g_filter[g_filter_count++], a, 6);
	}
}

/* Byte 36 of INQUIRY, delivered by inquiry.c's generic path. */
uint8_t scsiNetworkInquiryStatus(void)
{
	ensure_init();
	return (uint8_t)((sl003_enabled(&g_sl003) ? SL003_INQ_ENABLED : 0) |
	                 (sl003_mode_set(&g_sl003) ? SL003_INQ_MODE_SET : 0));
}

/*
 * Batch-depth histogram, diagnostics builds only
 * (-DBLUESCSI_NETWORK_DEBUG=ON). Counts blind batches; polled READs
 * report depth 0.
 */
#ifdef NETWORK_DEBUG_LOGGING
#define DEPTH_BUCKETS 24
static uint32_t g_depth_hist[DEPTH_BUCKETS];
static uint32_t g_depth_reads;
static uint16_t g_depth_max;

static void depth_tally(uint16_t records)
{
	uint32_t half, run;
	uint16_t i, median;

	if (records >= DEPTH_BUCKETS)
		g_depth_hist[DEPTH_BUCKETS - 1]++;
	else
		g_depth_hist[records]++;
	if (records > g_depth_max)
		g_depth_max = records;

	if (++g_depth_reads % 512 != 0)
		return;

	half = g_depth_reads / 2;
	run = 0;
	median = 0;
	for (i = 0; i < DEPTH_BUCKETS; i++) {
		run += g_depth_hist[i];
		if (run >= half) { median = i; break; }
	}
	LOGMSG_F("SL003 batch depth over %lu reads: median %u, max %u, "
	         "empty %lu, 1 %lu, 2 %lu, 3 %lu, 4-7 %lu, 8+ %lu",
	         (unsigned long)g_depth_reads, median, g_depth_max,
	         (unsigned long)g_depth_hist[0], (unsigned long)g_depth_hist[1],
	         (unsigned long)g_depth_hist[2], (unsigned long)g_depth_hist[3],
	         (unsigned long)(g_depth_hist[4] + g_depth_hist[5] +
	                         g_depth_hist[6] + g_depth_hist[7]),
	         (unsigned long)(g_depth_reads - g_depth_hist[0] -
	                         g_depth_hist[1] - g_depth_hist[2] -
	                         g_depth_hist[3] - g_depth_hist[4] -
	                         g_depth_hist[5] - g_depth_hist[6] -
	                         g_depth_hist[7]));
}
#else
#define depth_tally(records) ((void)0)
#endif

/*
 * The sl003_io over the real bus. Data phases are entered lazily, on the
 * first byte each direction moves.
 *
 * Do not poll the radio from emit: cyw43 work is a millisecond-scale SPI
 * conversation and would run with the initiator waiting mid-phase. The
 * radio IRQ refills the ring instead (threadsafe-background arch), at no
 * bus cost. That also means a batch is NOT bounded by ring depth; the
 * bounds are the core's batch ceiling and the record cap.
 */
struct bus_io {
	bool in_data_in;
	bool in_data_out;
	bool parity;
};

static bool bus_emit(void *ctx, const uint8_t *data, uint16_t len)
{
	struct bus_io *b = ctx;

	if (scsiDev.resetFlag)
		return false;
	if (!b->in_data_in) {
		scsiEnterPhase(DATA_IN);
		b->in_data_in = true;
	}
	/* scsiWrite blocks; on return the bytes are on the bus and the
	 * pointer (possibly into a ring slot) is finished with. */
	scsiWrite(data, len);
	return !scsiDev.resetFlag;
}

static bool bus_fetch(void *ctx, uint8_t *buf, uint16_t len)
{
	struct bus_io *b = ctx;
	int parityError = 0;

	if (scsiDev.resetFlag)
		return false;
	if (!b->in_data_out) {
		scsiEnterPhase(DATA_OUT);
		b->in_data_out = true;
	}
	scsiRead(buf, len, &parityError);
	/* scsiRead returns without transferring once resetFlag is set;
	 * treating that as data would transmit garbage or parse stale bytes
	 * as a record length. */
	if (scsiDev.resetFlag)
		return false;
	if (parityError) {
		b->parity = true;
		return false;
	}
	return true;
}

static void bus_gap(void *ctx, uint16_t us)
{
	(void)ctx;
	if (us)
		sleep_us(us);
}

static void bus_tx(void *ctx, const uint8_t *frame, uint16_t len)
{
	(void)ctx;
	platform_network_send((uint8_t *)frame, len);
}

int scsiNetworkCommand(void)
{
	struct bus_io bus = { false, false, false };
	const sl003_io io = { bus_emit, bus_fetch, bus_gap, bus_tx, &bus };
	sl003_result r;

	ensure_init();

	if (scsiDev.cdb[0] == SCSI_NETWORK_WIFI_CMD)
		return scsiNetworkWifiCommand();

	/* Ring overflow drops feed the missed-packet counter. Read and clear
	 * as one step: the producer increments from an IRQ, and a drop
	 * landing between the two would be lost. */
	{
		uint32_t save = save_and_disable_interrupts();
		uint32_t missed = scsiNetworkMissed;
		scsiNetworkMissed = 0;
		restore_interrupts(save);
		sl003_count_missed(&g_sl003, missed);
	}

	r = sl003_handle(&g_sl003, scsiDev.cdb, &io);
	scsiNetworkEnabled = sl003_enabled(&g_sl003);

	if (scsiDev.cdb[0] == SL003_CMD_READ)
		depth_tally(sl003_records_sent(&g_sl003));

	/* Only when a list actually arrived (see sl003_multicast_take). */
	if (sl003_multicast_take(&g_sl003))
		apply_multicast();

	if (bus.parity) {
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ABORTED_COMMAND;
		scsiDev.target->sense.asc = SCSI_PARITY_ERROR;
		scsiDev.phase = STATUS;
		return 1;
	}
	if (r != SL003_OK) {
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = (r == SL003_CHECK_FIELD)
			? INVALID_FIELD_IN_CDB
			: INVALID_COMMAND_OPERATION_CODE;
		scsiDev.phase = STATUS;
		return 1;
	}

	scsiDev.status = GOOD;
	scsiDev.phase = STATUS;
	return 1;
}

#endif /* BLUESCSI_NETWORK */
