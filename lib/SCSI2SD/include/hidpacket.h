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

// Library for sending packet data over a USB HID connection.
// Supports reassembly of packets larger than the HID packet,

#ifdef __cplusplus
extern "C" {
#endif

#define USBHID_LEN 64

// Maximum packet payload length. Must be large enough to support a SD sector
// + sector number
#define HIDPACKET_MAX_LEN 520

#include <stddef.h>
#include <stdint.h>

// The first byte of each HID packet contains the hid chunk number.
//   High-bit indicates a final chunk.
// The second byte of each HID packet contains the payload length.

// Call this with HID bytes received. len <= USBHID_LEN
void hidPacket_recv(const uint8_t* bytes, size_t len);

// Returns the received packet contents, or NULL if a complete packet isn't
// available.
const uint8_t* hidPacket_getPacket(size_t* len);

// Call this with packet data to send. len <= USBHID_LEN
// Overwrites any packet currently being sent.
void hidPacket_send(const uint8_t* bytes, size_t len);

// Returns USBHID_LEN bytes to send in the next HID packet, or
// NULL if there's nothing to send.
const uint8_t* hidPacket_getHIDBytes(uint8_t* hidBuffer);

#ifdef __cplusplus
} // extern "C"
#endif

