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
#ifdef BLUESCSI_NETWORK
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NETWORK_PACKET_QUEUE_SIZE
# define NETWORK_PACKET_QUEUE_SIZE   20		// must be <= 255
#endif

#define NETWORK_PACKET_MAX_SIZE     1520

struct scsiNetworkPacketQueue {
	uint8_t packets[NETWORK_PACKET_QUEUE_SIZE][NETWORK_PACKET_MAX_SIZE];
	uint16_t sizes[NETWORK_PACKET_QUEUE_SIZE];
	uint8_t writeIndex;
	uint8_t readIndex;
};

struct __attribute__((packed)) wifi_network_entry {
	char ssid[64];
	char bssid[6];
	int8_t rssi;
	uint8_t channel;
	uint8_t flags;
#define WIFI_NETWORK_FLAG_AUTH 0x1
	uint8_t _padding;
};

#define WIFI_NETWORK_LIST_ENTRY_COUNT 10
extern struct wifi_network_entry wifi_network_list[WIFI_NETWORK_LIST_ENTRY_COUNT];

struct __attribute__((packed)) wifi_join_request {
	char ssid[64];
	char key[64];
	uint8_t channel;
	uint8_t _padding;
};

int scsiNetworkCommand(void);
int scsiNetworkEnqueue(const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif
#endif // BLUESCSI_NETWORK
#endif // NETWORK_H
