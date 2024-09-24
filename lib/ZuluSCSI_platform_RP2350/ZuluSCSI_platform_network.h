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

#pragma once

#ifdef ZULUSCSI_NETWORK

#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

bool platform_network_supported();
void platform_network_poll();
int platform_network_init(char *mac);
void platform_network_add_multicast_address(uint8_t *mac);
bool platform_network_wifi_join(char *ssid, char *password);
int platform_network_wifi_start_scan();
int platform_network_wifi_scan_finished();
void platform_network_wifi_dump_scan_list();
int platform_network_wifi_rssi();
char * platform_network_wifi_ssid();
char * platform_network_wifi_bssid();
int platform_network_wifi_channel();
int platform_network_send(uint8_t *buf, size_t len);

# ifdef __cplusplus
}
# endif

#endif // ZULUSCSI_NETWORK