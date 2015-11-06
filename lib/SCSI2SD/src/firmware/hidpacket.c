//	Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.

#include "hidpacket.h"
#include <string.h>

enum STATE { IDLE, PARTIAL, COMPLETE };
typedef struct
{
	uint8_t buffer[HIDPACKET_MAX_LEN];

	int state;
	uint8_t chunk;
	size_t offset; // Current offset into buffer.
} HIDPacketState;
static HIDPacketState rx  __attribute__((aligned(USBHID_LEN))) = {{}, IDLE, 0, 0};
static HIDPacketState tx  __attribute__((aligned(USBHID_LEN))) = {{}, IDLE, 0, 0};

static void
rxReset()
{
	rx.state = IDLE;
	rx.chunk = 0;
	rx.offset = 0;
}

static void
txReset()
{
	tx.state = IDLE;
	tx.chunk = 0;
	tx.offset = 0;
}

void hidPacket_recv(const uint8_t* bytes, size_t len)
{
	if (len < 2)
	{
		// Invalid. We need at least a chunk number and payload length.
		rxReset();
		return;
	}

	uint8_t chunk = bytes[0] & 0x7F;
	int final = bytes[0] & 0x80;
	uint8_t payloadLen = bytes[1];
	if ((payloadLen > (len - 2)) || // short packet
		(payloadLen + rx.offset > sizeof(rx.buffer))) // buffer overflow
	{
		rxReset();
		return;
	}

	if (chunk == 0)
	{
		// Initial chunk
		rxReset();
		memcpy(rx.buffer, bytes + 2, payloadLen);
		rx.offset = payloadLen;
		rx.state = PARTIAL;
	}
	else if ((rx.state == PARTIAL) && (chunk == rx.chunk + 1))
	{
		memcpy(rx.buffer + rx.offset, bytes + 2, payloadLen);
		rx.offset += payloadLen;
		rx.chunk++;
	}
	else if (chunk == rx.chunk)
	{
		// duplicated packet. ignore.
	}
	else
	{
		// invalid. Maybe we missed some data.
		rxReset();
	}

	if ((rx.state == PARTIAL) && final)
	{
		rx.state = COMPLETE;
	}
}

const uint8_t*
hidPacket_getPacket(size_t* len)
{
	if (rx.state == COMPLETE)
	{
		*len = rx.offset;
		rxReset();
		return rx.buffer;
	}
	else
	{
		*len = 0;
		return NULL;
	}
}

void hidPacket_send(const uint8_t* bytes, size_t len)
{
	if (len <= sizeof(tx.buffer))
	{
		tx.state = PARTIAL;
		tx.chunk = 0;
		tx.offset = len;
		memcpy(tx.buffer, bytes, len);
	}
	else
	{
		txReset();
	}
}

const uint8_t*
hidPacket_getHIDBytes(uint8_t* hidBuffer)
{
	if ((tx.state != PARTIAL) || (tx.offset <= 0))
	{
		return NULL;
	}

	hidBuffer[0] = tx.chunk;
	tx.chunk++;
	uint8_t payload;
	if (tx.offset <= USBHID_LEN - 2)
	{
		hidBuffer[0] = hidBuffer[0] | 0x80;
		payload = tx.offset;
		tx.state = IDLE;
		memset(hidBuffer + 2, 0, USBHID_LEN - 2);
	}
	else
	{
		payload = USBHID_LEN - 2;
	}

	tx.offset -= payload;
	hidBuffer[1] = payload;
	memcpy(hidBuffer + 2, tx.buffer, payload);
	memmove(tx.buffer, tx.buffer + payload, sizeof(tx.buffer) - payload);

	return hidBuffer;
}


