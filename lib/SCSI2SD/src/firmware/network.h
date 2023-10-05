/*
 * Copyright (c) 2023 joshua stein <jcs@jcs.org>
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

#ifndef NETWORK_H
#define NETWORK_H
#ifdef ZULUSCSI_NETWORK
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCSI_NETWORK_WIFI_CMD				0x1c	// cdb opcode
#define SCSI_NETWORK_WIFI_CMD_SCAN			0x01	// cdb[2]
#define SCSI_NETWORK_WIFI_CMD_COMPLETE		0x02
#define SCSI_NETWORK_WIFI_CMD_SCAN_RESULTS	0x03
#define SCSI_NETWORK_WIFI_CMD_INFO			0x04
#define SCSI_NETWORK_WIFI_CMD_JOIN			0x05

#ifndef NETWORK_PACKET_QUEUE_SIZE
# define NETWORK_PACKET_QUEUE_SIZE   10		// must be <= 255
#endif

#define NETWORK_PACKET_MAX_SIZE     1520

int scsiNetworkCommand(void);
int scsiNetworkEnqueue(const uint8_t *buf, size_t len);
int scsiNetworkPurge(void);

#ifdef __cplusplus
}
#endif
#endif // ZULUSCSI_NETWORK
#endif // NETWORK_H
