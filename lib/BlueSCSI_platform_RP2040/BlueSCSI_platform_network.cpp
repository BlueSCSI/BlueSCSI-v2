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

#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include <scsi.h>
#include <network.h>

extern "C" {

#include <cyw43.h>
#include <pico/cyw43_arch.h>

#ifndef CYW43_IOCTL_GET_RSSI
#define CYW43_IOCTL_GET_RSSI (0xfe)
#endif

// A default DaynaPort-compatible MAC
static const char defaultMAC[] = { 0x00, 0x80, 0x19, 0xc0, 0xff, 0xee };

static bool network_in_use = false;

bool platform_network_supported()
{
	/* from cores/rp2040/RP2040Support.h */
#if !defined(ARDUINO_RASPBERRY_PI_PICO_W)
	return false;
#else
	extern bool __isPicoW;
	return __isPicoW;
#endif
}

int platform_network_init(char *mac)
{
	pico_unique_board_id_t board_id;
	uint8_t set_mac[6], read_mac[6];

	if (!platform_network_supported())
		return -1;

	log(" ");
	log("=== Network Initialization ===");

	memset(wifi_network_list, 0, sizeof(wifi_network_list));

	cyw43_deinit(&cyw43_state);

	if (mac == NULL || (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0 && mac[5] == 0))
	{
		mac = (char *)&set_mac;
		memcpy(mac, defaultMAC, sizeof(set_mac));

		// retain Dayna vendor but use a device id specific to this board
		pico_get_unique_board_id(&board_id);
		if (g_log_debug)
			log_f("Unique board id: %02x %02x %02x %02x  %02x %02x %02x %02x",
				board_id.id[0], board_id.id[1], board_id.id[2], board_id.id[3],
				board_id.id[4], board_id.id[5], board_id.id[6], board_id.id[7]);

		if (board_id.id[3] != 0 && board_id.id[4] != 0 && board_id.id[5] != 0)
		{
			mac[3] = board_id.id[3];
			mac[4] = board_id.id[4];
			mac[5] = board_id.id[5];
		}

		memcpy(scsiDev.boardCfg.wifiMACAddress, mac, sizeof(scsiDev.boardCfg.wifiMACAddress));
	}

	// setting the MAC requires libpico to be compiled with CYW43_USE_OTP_MAC=0
	memcpy(cyw43_state.mac, mac, sizeof(cyw43_state.mac));
	cyw43_arch_enable_sta_mode();

	cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, read_mac);
	log_f("Wi-Fi MAC: %02X:%02X:%02X:%02X:%02X:%02X",
		read_mac[0], read_mac[1], read_mac[2], read_mac[3], read_mac[4], read_mac[5]);
	if (memcmp(mac, read_mac, sizeof(read_mac)) != 0)
		log("WARNING: Wi-Fi MAC is not what was requested (%02x:%02x:%02x:%02x:%02x:%02x), is libpico not compiled with CYW43_USE_OTP_MAC=0?",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	network_in_use = true;

	return 0;
}

void platform_network_add_multicast_address(uint8_t *mac)
{
	int ret;

	if ((ret = cyw43_wifi_update_multicast_filter(&cyw43_state, mac, true)) != 0)
		log_f("%s: cyw43_wifi_update_multicast_filter: %d", __func__, ret);
}

bool platform_network_wifi_join(char *ssid, char *password)
{
	int ret;

	if (!platform_network_supported())
		return false;

	if (password == NULL || password[0] == 0)
	{
		log_f("Connecting to Wi-Fi SSID \"%s\" with no authentication", ssid);
		ret = cyw43_arch_wifi_connect_async(ssid, NULL, CYW43_AUTH_OPEN);
	}
	else
	{
		log_f("Connecting to Wi-Fi SSID \"%s\" with WPA/WPA2 PSK", ssid);
		ret = cyw43_arch_wifi_connect_async(ssid, password, CYW43_AUTH_WPA2_MIXED_PSK);
	}
	if (ret != 0)
		log_f("Wi-Fi connection failed: %d", ret);
	
	return (ret == 0);
}

void platform_network_poll()
{
	if (network_in_use)
		cyw43_arch_poll();
}

int platform_network_send(uint8_t *buf, size_t len)
{
	int ret = cyw43_send_ethernet(&cyw43_state, 0, len, buf, 0);
	if (ret != 0)
		log_f("cyw43_send_ethernet failed: %d", ret);

	return ret;
}

static int platform_network_wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result)
{
	struct wifi_network_entry *entry = NULL;

	if (!result || !result->ssid_len || !result->ssid[0])
		return 0;

	for (int i = 0; i < WIFI_NETWORK_LIST_ENTRY_COUNT; i++)
	{
		// take first available
		if (wifi_network_list[i].ssid[0] == '\0')
		{
			entry = &wifi_network_list[i];
			break;
		}
		// or if we've seen this network before, use this slot
		else if (strcmp((char *)result->ssid, wifi_network_list[i].ssid) == 0)
		{
			entry = &wifi_network_list[i];
			break;
		}
	}

	if (!entry)
	{
		// no available slots, insert according to our RSSI
		for (int i = 0; i < WIFI_NETWORK_LIST_ENTRY_COUNT; i++)
		{
			if (result->rssi > wifi_network_list[i].rssi)
			{
				// shift everything else down
				for (int j = WIFI_NETWORK_LIST_ENTRY_COUNT - 1; j > i; j--)
					wifi_network_list[j] = wifi_network_list[j - 1];

				entry = &wifi_network_list[i];
				memset(entry, 0, sizeof(struct wifi_network_entry));
				break;
			}
		}
	}

	if (entry == NULL)
		return 0;

	if (entry->rssi == 0 || result->rssi > entry->rssi)
	{
		entry->channel = result->channel;
		entry->rssi = result->rssi;
	}
	if (result->auth_mode & 7)
		entry->flags = WIFI_NETWORK_FLAG_AUTH;
	strncpy(entry->ssid, (const char *)result->ssid, sizeof(entry->ssid));
	entry->ssid[sizeof(entry->ssid) - 1] = '\0';
	memcpy(entry->bssid, result->bssid, sizeof(entry->bssid));

	return 0;
}

int platform_network_wifi_start_scan()
{
	if (cyw43_wifi_scan_active(&cyw43_state))
		return -1;

	cyw43_wifi_scan_options_t scan_options = { 0 };
	memset(wifi_network_list, 0, sizeof(wifi_network_list));
	return cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, platform_network_wifi_scan_result);
}

int platform_network_wifi_scan_finished()
{
	return !cyw43_wifi_scan_active(&cyw43_state);
}

void platform_network_wifi_dump_scan_list()
{
	struct wifi_network_entry *entry = NULL;
	
	for (int i = 0; i < WIFI_NETWORK_LIST_ENTRY_COUNT; i++)
	{
		entry = &wifi_network_list[i];

		if (entry->ssid[0] == '\0')
			break;
			
		log_f("wifi[%d] = %s, channel %d, rssi %d, bssid %02x:%02x:%02x:%02x:%02x:%02x, flags %d",
			i, entry->ssid, entry->channel, entry->rssi,
			entry->bssid[0], entry->bssid[1], entry->bssid[2],
			entry->bssid[3], entry->bssid[4], entry->bssid[5],
			entry->flags);
	}
}

int platform_network_wifi_rssi()
{
	int32_t rssi = 0;

    cyw43_ioctl(&cyw43_state, CYW43_IOCTL_GET_RSSI, sizeof(rssi), (uint8_t *)&rssi, CYW43_ITF_STA);
	return rssi;
}

char * platform_network_wifi_ssid()
{
	struct ssid_t {
		uint32_t ssid_len;
		uint8_t ssid[32 + 1];
	} ssid;
	static char cur_ssid[32 + 1];

	memset(cur_ssid, 0, sizeof(cur_ssid));

	int ret = cyw43_ioctl(&cyw43_state, CYW43_IOCTL_GET_SSID, sizeof(ssid), (uint8_t *)&ssid, CYW43_ITF_STA);
	if (ret)
	{
		log_f("Failed getting Wi-Fi SSID: %d", ret);
		return NULL;
	}

	ssid.ssid[sizeof(ssid.ssid) - 1] = '\0';
	if (ssid.ssid_len < sizeof(ssid.ssid))
		ssid.ssid[ssid.ssid_len] = '\0';
	
	strlcpy(cur_ssid, (char *)ssid.ssid, sizeof(cur_ssid));
	return cur_ssid;
}

char * platform_network_wifi_bssid()
{
	static char bssid[6];

	memset(bssid, 0, sizeof(bssid));

	/* TODO */

	return bssid;
}

int platform_network_wifi_channel()
{
	int32_t channel = 0;

    cyw43_ioctl(&cyw43_state, CYW43_IOCTL_GET_CHANNEL, sizeof(channel), (uint8_t *)&channel, CYW43_ITF_STA);
	return channel;
}

// these override weakly-defined functions in pico-sdk

void cyw43_cb_process_ethernet(void *cb_data, int itf, size_t len, const uint8_t *buf)
{
	scsiNetworkEnqueue(buf, len);
}

void cyw43_cb_tcpip_set_link_down(cyw43_t *self, int itf)
{
	log_f("Disassociated from Wi-Fi SSID \"%s\"", self->ap_ssid);
}

void cyw43_cb_tcpip_set_link_up(cyw43_t *self, int itf)
{
	char *ssid = platform_network_wifi_ssid();

	if (ssid)
		log_f("Successfully connected to Wi-Fi SSID \"%s\"", ssid);
}

}
