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

#include <string.h>

#include "scsi.h"
#include "scsi2sd_time.h"
#include "scsiPhy.h"
#include "config.h"
#include "network.h"

static bool scsiNetworkEnabled = false;

struct scsiNetworkPacketQueue {
	uint8_t packets[NETWORK_PACKET_QUEUE_SIZE][NETWORK_PACKET_MAX_SIZE];
	uint16_t sizes[NETWORK_PACKET_QUEUE_SIZE];
	uint8_t writeIndex;
	uint8_t readIndex;
};

static struct scsiNetworkPacketQueue scsiNetworkInboundQueue, scsiNetworkOutboundQueue;

struct __attribute__((packed)) wifi_network_entry wifi_network_list[WIFI_NETWORK_LIST_ENTRY_COUNT] = { 0 };

static const uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t crc32(const void *buf, size_t size)
{
	const uint8_t *p = buf;
	uint32_t crc;

	crc = ~0U;
	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	return crc ^ ~0U;
}

int scsiNetworkCommand()
{
	int handled = 1;
	int off = 0;
	int parityError = 0;
	long psize;
	uint32_t size = scsiDev.cdb[4] + (scsiDev.cdb[3] << 8);
	uint8_t command = scsiDev.cdb[0];
	uint8_t cont = (scsiDev.cdb[5] == 0x80);

	DBGMSG_F("------ in scsiNetworkCommand with command 0x%02x (size %d)", command, size);

	// Rather than duplicating code, this just diverts a 'fake' read request to make the gvpscsi.device happy on the Amiga
	if ((command == SCSI_NETWORK_WIFI_CMD) && (scsiDev.cdb[1] == SCSI_METWORK_WIFI_CMD_ALTREAD)) {
		// Redirect the command as a READ.
		command = 0x08;
	}

	switch (command) {
	case 0x08:
		// read(6)
		if (unlikely(size == 1))
		{
			scsiDev.status = CHECK_CONDITION;
			scsiDev.phase = STATUS;
			break;
		}

		if (scsiNetworkInboundQueue.readIndex == scsiNetworkInboundQueue.writeIndex)
		{
			// nothing available
			memset(scsiDev.data, 0, 6);
			scsiDev.dataLen = 6;
		}
		else
		{
			psize = scsiNetworkInboundQueue.sizes[scsiNetworkInboundQueue.readIndex];

			// pad smaller packets
			if (psize < 64)
			{
				psize = 64;
			}
			else if (psize + 6 > size)
			{
				log_f("%s: packet size too big (%d)", __func__, psize);
				psize = size - 6;
			}

			DBGMSG_F("%s: sending packet[%d] to host of size %zu + 6", __func__, scsiNetworkInboundQueue.readIndex, psize);

			scsiDev.dataLen = psize + 6; // 2-byte length + 4-byte flag + packet
			memcpy(scsiDev.data + 6, scsiNetworkInboundQueue.packets[scsiNetworkInboundQueue.readIndex], psize);
			scsiDev.data[0] = (psize >> 8) & 0xff;
			scsiDev.data[1] = psize & 0xff;

			if (scsiNetworkInboundQueue.readIndex == NETWORK_PACKET_QUEUE_SIZE - 1)
				scsiNetworkInboundQueue.readIndex = 0;
			else
				scsiNetworkInboundQueue.readIndex++;

			// flags
			scsiDev.data[2] = 0;
			scsiDev.data[3] = 0;  
			scsiDev.data[4] = 0;
			// more data to read?
			scsiDev.data[5] = (scsiNetworkInboundQueue.readIndex == scsiNetworkInboundQueue.writeIndex ? 0 : 0x10);

			DBGMSG_BUF(scsiDev.data, scsiDev.dataLen);
		}
		// Patches around the weirdness on the Amiga SCSI devices
		if ((scsiDev.cdb[2] == AMIGASCSI_PATCH_24BYTE_BLOCKSIZE) || (scsiDev.cdb[2] == AMIGASCSI_PATCH_SINGLEWRITE_ONLY)) {
			scsiDev.data[2] = scsiDev.cdb[2];    // for me really
			int extra = 0;
			if (scsiDev.cdb[2] == AMIGASCSI_PATCH_24BYTE_BLOCKSIZE) {        
				if (scsiDev.dataLen<90) scsiDev.dataLen = 90;
				int missing = (scsiDev.dataLen-90) % 24;
				if (missing) {
					scsiDev.dataLen += 24 - missing;
					if (scsiDev.dataLen>NETWORK_PACKET_MAX_SIZE) {
						extra = scsiDev.dataLen - NETWORK_PACKET_MAX_SIZE;
						scsiDev.dataLen = NETWORK_PACKET_MAX_SIZE;
					}
				}
				scsiEnterPhase(DATA_IN);		
				scsiWrite(scsiDev.data, scsiDev.dataLen);
				while (!scsiIsWriteFinished(NULL))
				{
					platform_poll();
				}
				scsiFinishWrite();
			} else {
				extra = scsiDev.dataLen;     // F9 means send in ONE transaction
				if (extra) scsiEnterPhase(DATA_IN);		
			}

			if (extra) {
				// Just write the extra data to make the padding work for such a large packet
				scsiWrite(scsiDev.data, extra);
				while (!scsiIsWriteFinished(NULL))
				{
					platform_poll();
				}
				scsiFinishWrite();
			}
		} else {
			// DaynaPort driver needs a delay between reading the initial packet size and the data so manually do two transfers
			scsiEnterPhase(DATA_IN);
			scsiWrite(scsiDev.data, 6);
			while (!scsiIsWriteFinished(NULL))
			{
				platform_poll();
			}
			scsiFinishWrite();

			if (scsiDev.dataLen > 6)
			{
				s2s_delay_us(80);

				scsiWrite(scsiDev.data + 6, scsiDev.dataLen - 6);
				while (!scsiIsWriteFinished(NULL))
				{
					platform_poll();
				}
				scsiFinishWrite();
			}
		}

		scsiDev.status = GOOD;
		scsiDev.phase = STATUS;
		break;

	case 0x09:
		// read mac address and stats
		memcpy(scsiDev.data, scsiDev.boardCfg.wifiMACAddress, sizeof(scsiDev.boardCfg.wifiMACAddress));
		memset(scsiDev.data + sizeof(scsiDev.boardCfg.wifiMACAddress), 0, sizeof(scsiDev.data) - sizeof(scsiDev.boardCfg.wifiMACAddress));

		// three 32-bit counters expected to follow, just return zero for all
		scsiDev.dataLen = 18;
		scsiDev.phase = DATA_IN;
		break;

	case 0x0a:
		// write(6)
		off = 0;
		if (cont)
		{
			size += 8;
		}

		memset(scsiDev.data, 0, sizeof(scsiDev.data));

		scsiEnterPhase(DATA_OUT);
		parityError = 0;
		scsiRead(scsiDev.data, size, &parityError);

		if (parityError)
		{
			DBGMSG_F("%s: read packet from host of size %zu - %d (parity error %d)", __func__, size, (cont ? 4 : 0), parityError);
			DBGMSG_F(scsiDev.data, size);
		}
		else
		{
			DBGMSG_F("------ %s: read packet from host of size %zu - %d", __func__, size, (cont ? 4 : 0));
		}

		if (cont)
		{
			size = (scsiDev.data[0] << 8) | scsiDev.data[1];
			off = 4;
		}

		memcpy(&scsiNetworkOutboundQueue.packets[scsiNetworkOutboundQueue.writeIndex], scsiDev.data + off, size);
		scsiNetworkOutboundQueue.sizes[scsiNetworkOutboundQueue.writeIndex] = size;

		if (scsiNetworkOutboundQueue.writeIndex == NETWORK_PACKET_QUEUE_SIZE - 1)
			scsiNetworkOutboundQueue.writeIndex = 0;
		else
			scsiNetworkOutboundQueue.writeIndex++;

		scsiDev.status = GOOD;
		scsiDev.phase = STATUS;
		break;

	case 0x0c:
		// set interface mode (ignored)
		//broadcasts = (scsiDev.cdb[4] == 0x04);
		break;

	case 0x0d:
		// add multicast addr to network filter
		memset(scsiDev.data, 0, sizeof(scsiDev.data));
		scsiEnterPhase(DATA_OUT);
		parityError = 0;
		scsiRead(scsiDev.data, size, &parityError);

		DBGMSG_F("%s: adding multicast address %02x:%02x:%02x:%02x:%02x:%02x", __func__,
			  scsiDev.data[0], scsiDev.data[1], scsiDev.data[2], scsiDev.data[3], scsiDev.data[4], scsiDev.data[5]);

		platform_network_add_multicast_address(scsiDev.data);

		scsiDev.status = GOOD;
		scsiDev.phase = STATUS;
		break;

	case 0x0e:
		// toggle interface
		if (scsiDev.cdb[5] & 0x80)
		{

			DBGMSG_F("%s: enable interface", __func__);
			scsiNetworkEnabled = true;
			memset(&scsiNetworkInboundQueue, 0, sizeof(scsiNetworkInboundQueue));
			memset(&scsiNetworkOutboundQueue, 0, sizeof(scsiNetworkOutboundQueue));
		}
		else
		{

			DBGMSG_F("%s: disable interface", __func__);
			scsiNetworkEnabled = false;
		}
		break;

	case 0x1a:
		// mode sense (ignored)
		break;

	case 0x40:
		// set MAC (ignored)
		scsiDev.dataLen = 6;
		scsiDev.phase = DATA_OUT;
		break;

	case 0x80:
		// set mode (ignored)
		break;

	// custom wifi commands all using the same opcode, with a sub-command in cdb[2]
	case SCSI_NETWORK_WIFI_CMD:
		DBGMSG_F("in scsiNetworkCommand with wi-fi command 0x%02x (size %d)", scsiDev.cdb[1], size);

		switch (scsiDev.cdb[1]) {
		case SCSI_NETWORK_WIFI_CMD_SCAN:
			// initiate wi-fi scan
			scsiDev.dataLen = 1;
			int ret = platform_network_wifi_start_scan();
			scsiDev.data[0] = (ret < 0 ? ret : 1);
			scsiDev.phase = DATA_IN;
			break;

		case SCSI_NETWORK_WIFI_CMD_COMPLETE:
			// check for wi-fi scan completion
			scsiDev.dataLen = 1;
			scsiDev.data[0] = (platform_network_wifi_scan_finished() ? 1 : 0);
			scsiDev.phase = DATA_IN;
			break;

		case SCSI_NETWORK_WIFI_CMD_SCAN_RESULTS:
			// return wi-fi scan results
			if (!platform_network_wifi_scan_finished())
			{
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			int nets = 0;
			for (int i = 0; i < WIFI_NETWORK_LIST_ENTRY_COUNT; i++)
			{
				if (wifi_network_list[i].ssid[0] == '\0')
					break;
				nets++;
			}

			if (nets) {
				int size = sizeof(struct wifi_network_entry) * nets;
				if (size + 2 > sizeof(scsiDev.data))
				{
					log_f("WARNING: wifi_network_list is bigger than scsiDev.data, truncating");
					size = sizeof(scsiDev.data) - 2;
					size -= (size % (sizeof(struct wifi_network_entry)));
				}
				scsiDev.data[0] = (size >> 8) & 0xff;
				scsiDev.data[1] = size & 0xff;
				memcpy(scsiDev.data + 2, wifi_network_list, size);
				scsiDev.dataLen = size + 2;
			}
			else
			{
				scsiDev.data[0] = 0;
				scsiDev.data[1] = 0;
				scsiDev.dataLen = 2;
			}

			scsiDev.phase = DATA_IN;
			break;

		case SCSI_NETWORK_WIFI_CMD_INFO: {
			// return current wi-fi information
			struct wifi_network_entry wifi_cur = { 0 };
			int size = sizeof(wifi_cur);

			char *ssid = platform_network_wifi_ssid();
			if (ssid != NULL)
				strlcpy(wifi_cur.ssid, ssid, sizeof(wifi_cur.ssid));

			char *bssid = platform_network_wifi_bssid();
			if (bssid != NULL)
				memcpy(wifi_cur.bssid, bssid, sizeof(wifi_cur.bssid));

			wifi_cur.rssi = platform_network_wifi_rssi();

			wifi_cur.channel = platform_network_wifi_channel();

			scsiDev.data[0] = (size >> 8) & 0xff;
			scsiDev.data[1] = size & 0xff;
			memcpy(scsiDev.data + 2, (char *)&wifi_cur, size);
			scsiDev.dataLen = size + 2;
			scsiDev.phase = DATA_IN;
			break;
		}

		case SCSI_NETWORK_WIFI_CMD_JOIN: {
			// set current wi-fi network
			struct wifi_join_request req = { 0 };

			if (size != sizeof(req)) {
				log_f("wifi_join_request bad size (%zu != %zu), ignoring", size, sizeof(req));
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			scsiEnterPhase(DATA_OUT);
			parityError = 0;
			scsiRead((uint8_t *)&req, sizeof(req), &parityError);

			DBGMSG_F("%s: read join request from host:", __func__);
			DBGMSG_BUF(scsiDev.data, size);

			platform_network_wifi_join(req.ssid, req.key);

			scsiDev.status = GOOD;
			scsiDev.phase = STATUS;
			break;
		}

		case SCSI_NETWORK_WIFI_CMD_GETMACADDRESS: 
			// Update for the gvpscsi.device on the Amiga as it doesn't like 0x09 command being called! - NOTE this only sends 6 bytes back
			memcpy(scsiDev.data, scsiDev.boardCfg.wifiMACAddress, sizeof(scsiDev.boardCfg.wifiMACAddress));
			memset(scsiDev.data + sizeof(scsiDev.boardCfg.wifiMACAddress), 0, sizeof(scsiDev.data) - sizeof(scsiDev.boardCfg.wifiMACAddress));

			scsiDev.dataLen = 6;
			scsiDev.phase = DATA_IN;
		}
		break;

	default:
		handled = 0;
	}
	

	return handled;
}

int scsiNetworkEnqueue(const uint8_t *buf, size_t len)
{
	if (!scsiNetworkEnabled)
		return 0;

	if (len + 4 > sizeof(scsiNetworkInboundQueue.packets[0]))
	{
		DBGMSG_F("%s: dropping incoming network packet, too large (%zu > %zu)", __func__, len, sizeof(scsiNetworkInboundQueue.packets[0]));
		return 0;
	}

	memcpy(scsiNetworkInboundQueue.packets[scsiNetworkInboundQueue.writeIndex], buf, len);
	uint32_t crc = crc32(buf, len);
	scsiNetworkInboundQueue.packets[scsiNetworkInboundQueue.writeIndex][len] = crc & 0xff;
	scsiNetworkInboundQueue.packets[scsiNetworkInboundQueue.writeIndex][len + 1] = (crc >> 8) & 0xff;
	scsiNetworkInboundQueue.packets[scsiNetworkInboundQueue.writeIndex][len + 2] = (crc >> 16) & 0xff;
	scsiNetworkInboundQueue.packets[scsiNetworkInboundQueue.writeIndex][len + 3] = (crc >> 24) & 0xff;

	scsiNetworkInboundQueue.sizes[scsiNetworkInboundQueue.writeIndex] = len + 4;

	if (scsiNetworkInboundQueue.writeIndex == NETWORK_PACKET_QUEUE_SIZE - 1)
		scsiNetworkInboundQueue.writeIndex = 0;
	else
		scsiNetworkInboundQueue.writeIndex++;

	if (scsiNetworkInboundQueue.writeIndex == scsiNetworkInboundQueue.readIndex)
	{
		DBGMSG_F("%s: dropping packets in ring, write index caught up to read index", __func__);
	}
	
	return 1;
}

int scsiNetworkPurge(void)
{
	int sent = 0;

	if (!scsiNetworkEnabled)
		return 0;

	while (scsiNetworkOutboundQueue.readIndex != scsiNetworkOutboundQueue.writeIndex)
	{
		platform_network_send(scsiNetworkOutboundQueue.packets[scsiNetworkOutboundQueue.readIndex], scsiNetworkOutboundQueue.sizes[scsiNetworkOutboundQueue.readIndex]);

		if (scsiNetworkOutboundQueue.readIndex == NETWORK_PACKET_QUEUE_SIZE - 1)
			scsiNetworkOutboundQueue.readIndex = 0;
		else
			scsiNetworkOutboundQueue.readIndex++;
		
		sent++;
	}

	return sent;
}
