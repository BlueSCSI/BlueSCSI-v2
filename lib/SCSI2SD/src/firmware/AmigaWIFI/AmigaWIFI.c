/*
 * AmigaWIFI Module, based on the work by joshua stein <jcs@jcs.org> Copyright (c) 2023 
 * Copyright (C) 2026 RobSmithDev
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

#ifdef BLUESCSI_NETWORK
#include <string.h>

#include "scsi.h"
#include "scsi2sd_time.h"
#include "scsiPhy.h"
#include "config.h"
#include "network.h"
#include "AmigaWIFI.h"

// Share with the original network code
extern bool scsiNetworkEnabled;
extern struct scsiNetworkPacketQueue scsiNetworkInboundQueue;

#define AMIGASCSI_PATCH_24BYTE_BLOCKSIZE 	0xA8   // In this mode, data written is rounded up to the nearest 24-byte boundary
#define AMIGASCSI_PATCH_SINGLEWRITE_ONLY 	0xA9   // In this mode, data written is always ONLY as one single write command
#define AMIGASCSI_BATCHMODE                 0x40   // Enable batch mode

#define SCSI_CMD_READ   			0x08
#define SCSI_CMD_WRITE  			0x0A
#define SCSI_CMD_ADDMULTOCAST 		0x0D
#define SCSI_CMD_TOGGLEINTERFACE	0x0E
#define SCSI_CMD_MODESENSE          0x1A  
#define SCSI_CMD_WIFI  				0x1C
#define SCSI_NETWORK_WIFI_CMD_SCAN			0x01	// cdb[2]
#define SCSI_NETWORK_WIFI_CMD_COMPLETE		0x02
#define SCSI_NETWORK_WIFI_CMD_SCAN_RESULTS	0x03
#define SCSI_NETWORK_WIFI_CMD_INFO			0x04
#define SCSI_NETWORK_WIFI_CMD_JOIN			0x05
#define SCSI_NETWORK_WIFI_CMD_ALTREAD       0x08
#define SCSI_NETWORK_WIFI_CMD_GETMACADDRESS 0x09
#define SCSI_NETWORK_WIFI_CMD_ALTWRITE      0x0A
#define SCSI_NETWORK_WIFI_CMD_AMIGANET_INFO 0x0B

// Special command to fetch info about the config

int amigaWifiCommand()
{
	int handled = 1;
	int parityError = 0;
	long psize;	
	uint32_t size;
	uint8_t command = scsiDev.cdb[0];

	// Rather than duplicating code, this just diverts a 'fake' read request to make the gvpscsi.device happy on the Amiga
	if ((scsiDev.cdb[0] == SCSI_CMD_WIFI) && (scsiDev.cdb[1] == SCSI_NETWORK_WIFI_CMD_ALTREAD)) {
		// Redirect the command as a READ.
		command = 0x08;
	}

	/*// Rather than duplicating code, this just diverts a 'fake' read/write request to make the gvpscsi.device happy on the Amiga
	if (scsiDev.cdb[0] == SCSI_CMD_WIFI) 
		switch (scsiDev.cdb[1]) {
			case SCSI_NETWORK_WIFI_CMD_ALTREAD: command = SCSI_CMD_READ; break;
			case SCSI_NETWORK_WIFI_CMD_ALTWRITE: command = SCSI_CMD_WRITE; break;
		}
*/
	DBGMSG_F("------ in amigaWifiCommand with command 0x%02x", command);

	switch (command) {
		case SCSI_CMD_READ: {
			size = scsiDev.cdb[4] + (scsiDev.cdb[3] << 8);

			if (unlikely(size < NETWORK_PACKET_MAX_SIZE)) {
				DBGMSG_F("%s: SCSI_CMD_READ Data too small %ld", __func__, size);
				scsiDev.target->sense.code = ILLEGAL_REQUEST;
				scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			if (scsiDev.cdb[2] & AMIGASCSI_BATCHMODE) {
				if (scsiNetworkInboundQueue.readIndex == scsiNetworkInboundQueue.writeIndex) {
					// No data
					memset(scsiDev.data, 0, 4);
					scsiDev.dataLen = 4;
				} else {
					uint16_t packets = 0;
					uint8_t* dataPos = scsiDev.data;
					uint32_t bufferUsed = 4; dataPos += 4; // skip header
					psize = scsiNetworkInboundQueue.sizes[scsiNetworkInboundQueue.readIndex];
					if (psize>4) psize-=4; // remove checksum

					while (size-bufferUsed>psize+2) {
						bufferUsed += psize + 2;
						dataPos[0] = psize >> 8;
						dataPos[1] = psize & 0xff;
						memcpy(&dataPos[2], scsiNetworkInboundQueue.packets[scsiNetworkInboundQueue.readIndex], psize);

						dataPos += psize+2;
						packets++;

						// Next packet
						if (scsiNetworkInboundQueue.readIndex == NETWORK_PACKET_QUEUE_SIZE - 1)
							scsiNetworkInboundQueue.readIndex = 0;
						else scsiNetworkInboundQueue.readIndex++;
						if (scsiNetworkInboundQueue.readIndex == scsiNetworkInboundQueue.writeIndex) break;
						psize = scsiNetworkInboundQueue.sizes[scsiNetworkInboundQueue.readIndex];
						if (psize>4) psize-=4; // remove checksum
					}

					// Encode the header
					scsiDev.data[0] = packets >> 8;
					scsiDev.data[1] = packets & 0xFF;
					scsiDev.data[2] = (scsiNetworkInboundQueue.readIndex == scsiNetworkInboundQueue.writeIndex) ? 0 : 1;
					scsiDev.data[3] = 0;
						
					scsiDev.dataLen = bufferUsed;
					//DBGMSG_BUF(scsiDev.data, scsiDev.dataLen);
				}				
			} else {
				if (scsiNetworkInboundQueue.readIndex == scsiNetworkInboundQueue.writeIndex) {
					memset(scsiDev.data, 0, 6);
					scsiDev.dataLen = 6;
				} else {
					psize = scsiNetworkInboundQueue.sizes[scsiNetworkInboundQueue.readIndex];
					if (psize < 64) psize = 64;
					else if (psize + 6 > size) {
						LOGMSG_F("%s: packet size too big (%d)", __func__, psize);
						psize = size - 6;
					}
					DBGMSG_F("%s: sending packet[%d] to host of size %zu + 6", __func__, scsiNetworkInboundQueue.readIndex, psize);
					scsiDev.dataLen = psize + 6; // 2-byte length + 4-byte flag + packet
					memcpy(scsiDev.data + 6, scsiNetworkInboundQueue.packets[scsiNetworkInboundQueue.readIndex], psize);
					scsiDev.data[0] = (psize >> 8) & 0xff;
					scsiDev.data[1] = psize & 0xff;
					if (scsiNetworkInboundQueue.readIndex == NETWORK_PACKET_QUEUE_SIZE - 1)
						scsiNetworkInboundQueue.readIndex = 0;
					else scsiNetworkInboundQueue.readIndex++;
					scsiDev.data[2] = 0; scsiDev.data[3] = 0; scsiDev.data[4] = 0;
					// more data to read?
					scsiDev.data[5] = (scsiNetworkInboundQueue.readIndex == scsiNetworkInboundQueue.writeIndex ? 0 : 0x10);
					DBGMSG_BUF(scsiDev.data, scsiDev.dataLen);
				}
			}
			
			// Patches around the weirdness on the Amiga SCSI devices
			if ((scsiDev.cdb[0] == SCSI_CMD_WIFI) && (scsiDev.cdb[1] == SCSI_NETWORK_WIFI_CMD_ALTREAD)) {
				//scsiDev.data[2] = scsiDev.cdb[2];    // for me really
				int extra = 0;
				if ( (scsiDev.cdb[2]&0xBF) == AMIGASCSI_PATCH_24BYTE_BLOCKSIZE) {
					if (scsiDev.dataLen<90) scsiDev.dataLen = 90;
					int missing = (scsiDev.dataLen-90) % 24;
					if (missing) {
						scsiDev.dataLen += 24 - missing;
						// TODO!
						if (scsiDev.dataLen>size) {
							extra = scsiDev.dataLen - size;
							scsiDev.dataLen = size;
						}
					}
					scsiEnterPhase(DATA_IN);
					scsiWrite(scsiDev.data, scsiDev.dataLen);
					while (!scsiIsWriteFinished(NULL)) platform_poll();
					scsiFinishWrite();
				} else {
					extra = scsiDev.dataLen;     // F9 means send in ONE transaction
					if (extra) scsiEnterPhase(DATA_IN);
				}

				if (extra) {
					// Just write the extra data to make the padding work for such a large packet
					scsiWrite(scsiDev.data, extra);
					while (!scsiIsWriteFinished(NULL)) platform_poll();
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
		}

	case SCSI_CMD_WRITE:
		size = scsiDev.cdb[4] + (scsiDev.cdb[3] << 8);
		if (scsiDev.cdb[2] & AMIGASCSI_BATCHMODE) {
			if (unlikely(size < 4)) {
				DBGMSG_F("%s: SCSI_CMD_WRITE Data too small %ld", __func__, size);
				scsiDev.target->sense.code = ILLEGAL_REQUEST;
				scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
				scsiDev.status = CHECK_CONDITION;
				scsiDev.phase = STATUS;
				break;
			}

			// Send multiple packets to the WIFI
			scsiEnterPhase(DATA_OUT);
			parityError = 0;
			scsiRead(scsiDev.data, size, &parityError);
			if (parityError) {
				DBGMSG_F("%s: read packets block from host of size %zu (parity error %d)", __func__, size, parityError);
			}
			else {
				DBGMSG_F("------ %s: read packets block from host of size %zu", __func__, size);
			}

			// How many packets?
			uint8_t* bufferPosition = scsiDev.data;
			const uint16_t numPackets = (scsiDev.data[0] << 8) | scsiDev.data[1];
			bufferPosition+=2;

			for (uint16_t packet=0; packet<numPackets; packet++) {
				// Enough room left?
				if (size<2) {
					DBGMSG_F("------ More packets sent than data allowed", __func__);
					break;
				}

				uint16_t packetSize = (bufferPosition[0] << 8) | bufferPosition[1];
				bufferPosition += 2;
				size-=2;

				if (packetSize <= size) {
					size -= packetSize;
					platform_network_send(bufferPosition, packetSize);	
					bufferPosition += packetSize;
				} else {
					DBGMSG_F("------ Packet size %d larger than remaining buffer %d", __func__, packetSize, size);
					break;
				}
			}
			scsiDev.status = GOOD;
			scsiDev.phase = STATUS;
			break;		
		} else {
			scsiEnterPhase(DATA_OUT);
			parityError = 0;
			scsiRead(scsiDev.data, size, &parityError);
			if (parityError) { DBGMSG_F("%s: read packet from host of size %zu (parity error %d)", __func__, size, parityError); }
				else DBGMSG_F("------ %s: read packet from host of size %zu", __func__, size);
			platform_network_send(scsiDev.data, size);	
			scsiDev.status = GOOD;
			scsiDev.phase = STATUS;
		}
		break;

	

	//////////////// The commands below this point all behave the same was as the original in network.c
	case SCSI_CMD_ADDMULTOCAST: {
		size = scsiDev.cdb[4] + (scsiDev.cdb[3] << 8);
		// add multicast addr to network filter
		memset(scsiDev.data, 0, sizeof(scsiDev.data));
		scsiEnterPhase(DATA_OUT);
		parityError = 0;
		scsiRead(scsiDev.data, size, &parityError);
		DBGMSG_F("%s: adding multicast address %02x:%02x:%02x:%02x:%02x:%02x", __func__, scsiDev.data[0], scsiDev.data[1], scsiDev.data[2], scsiDev.data[3], scsiDev.data[4], scsiDev.data[5]);

		platform_network_add_multicast_address(scsiDev.data);

		scsiDev.status = GOOD;
		scsiDev.phase = STATUS;
		break;
	}

	case SCSI_CMD_MODESENSE:
		// mode sense (ignored)
		break;

	case SCSI_CMD_TOGGLEINTERFACE:
		// toggle interface
		if (scsiDev.cdb[5] & 0x80) {
			DBGMSG_F("%s: enable interface", __func__);
			scsiNetworkEnabled = true;
			memset(&scsiNetworkInboundQueue, 0, sizeof(scsiNetworkInboundQueue));
		}
		else {
			DBGMSG_F("%s: disable interface", __func__);
			scsiNetworkEnabled = false;
		}
		scsiDev.status = GOOD;
		scsiDev.phase = STATUS;
		break;


	// custom wifi commands all using the same opcode, with a sub-command in cdb[2] - same as the standard daynaport ones
	case SCSI_CMD_WIFI: {
		size = scsiDev.cdb[4] + (scsiDev.cdb[3] << 8);

		DBGMSG_F("in amigaWifiNetworkCommand with wi-fi command 0x%02x (size %d)", scsiDev.cdb[1], size);
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
					scsiDev.target->sense.code = ILLEGAL_REQUEST;
					scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
					scsiDev.status = CHECK_CONDITION;
					scsiDev.phase = STATUS;
					break;
				}

				if (unlikely(size < 2))
				{
					scsiDev.target->sense.code = ILLEGAL_REQUEST;
					scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
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
					unsigned int netsize = sizeof(struct wifi_network_entry) * nets;
					if (netsize + 2 > sizeof(scsiDev.data))
					{
						LOGMSG_F("WARNING: wifi_network_list is bigger than scsiDev.data, truncating", 0);
						netsize = sizeof(scsiDev.data) - 2;
						netsize -= (netsize % (sizeof(struct wifi_network_entry)));
					}
					if (netsize + 2 > size)
					{
						LOGMSG_F("WARNING: wifi_network_list is bigger than requested dataLen, truncating", 0);
						netsize = size - 2;
						netsize -= (netsize % (sizeof(struct wifi_network_entry)));
					}
					scsiDev.data[0] = (netsize >> 8) & 0xff;
					scsiDev.data[1] = netsize & 0xff;
					memcpy(scsiDev.data + 2, wifi_network_list, netsize);
					scsiDev.dataLen = netsize + 2;
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
					LOGMSG_F("wifi_join_request bad size (%zu != %zu), ignoring", size, sizeof(req));
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
				break;

			// Fetches some info, and the MAC address
			case SCSI_NETWORK_WIFI_CMD_AMIGANET_INFO: {
					uint32_t maxData = sizeof(scsiDev.data);
					if (maxData>0xFFFC) maxData = 0xFFFC;  // Even number that fits into two bytes and is a multiple of 4 bytes					
					const uint32_t maxPackets = NETWORK_PACKET_QUEUE_SIZE-4;  // Wanna leave some space
					scsiDev.data[0] = (uint8_t)((maxData>>8) & 0xFF);   // maximum buffer size that can be received
					scsiDev.data[1] = (uint8_t)(maxData & 0xFF);
					scsiDev.data[2] = (uint8_t)(maxPackets >> 8); // maximum packets in one go
					scsiDev.data[3] = (uint8_t)(maxPackets & 0xFF); 
					scsiDev.data[4] = 0; // RFU
					scsiDev.data[5] = 0; // RFU
					memcpy(&scsiDev.data[6], scsiDev.boardCfg.wifiMACAddress, sizeof(scsiDev.boardCfg.wifiMACAddress));
					scsiDev.dataLen = 12;
					scsiDev.phase = DATA_IN;
					break;		
				}
			}					
		}
		break;
		
	default:
		handled = 0;
		break;
	}


	return handled;
}


#endif
