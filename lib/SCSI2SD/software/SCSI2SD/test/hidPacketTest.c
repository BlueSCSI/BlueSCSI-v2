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

#include <assert.h>
#include <stdio.h>

void test(uint8_t* in, size_t inLen)
{
	printf("Testing packet of size %ld\n", inLen);

	hidPacket_send(in, inLen);
	uint8_t hidBuffer[USBHID_LEN];
	const uint8_t* toSend = hidPacket_getHIDBytes(hidBuffer);
	while (toSend)
	{
		printf("Transferring packet\n");
		hidPacket_recv(toSend, USBHID_LEN);
		toSend = hidPacket_getHIDBytes(hidBuffer);
	}

	size_t len;
	const uint8_t* rxPacket = hidPacket_getPacket(&len);
	printf("Received length = %ld\n", len);
	assert(len == inLen);
	assert(rxPacket);
	assert(memcmp(in, rxPacket, len) == 0);
	printf("OK\n\n");
}


int main()
{
	uint8_t testPacketSmall[] = {1,2,3,4,5,6,7,8,9,0};
	uint8_t testPacketMed[] =
	{
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4
	};
	uint8_t testPacketBig[] =
	{
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0
	};
	uint8_t testPacketHuge[] =
	{
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6,7,8,9,0,
		1,2,3,4,5,6
	};

	test(testPacketSmall, sizeof(testPacketSmall));
	test(testPacketMed, sizeof(testPacketMed));
	test(testPacketBig, sizeof(testPacketBig));
	test(testPacketHuge, sizeof(testPacketHuge));
	return 0;
}
