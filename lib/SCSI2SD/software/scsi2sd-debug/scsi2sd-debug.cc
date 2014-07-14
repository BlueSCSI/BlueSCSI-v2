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

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// htonl/ntohl includes.
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "hidapi.h"

#define MIN(a,b) (a < b ? a : b)

FILE* logfile = NULL;

static void readConfig(hid_device* handle)
{
	// First byte is the report ID (0)
	unsigned char buf[65];
	memset(buf, 0, sizeof(buf));
	int result = hid_read(handle, buf, sizeof(buf));

	if (result < 0)
	{
		fprintf(stderr, "USB HID Read Failure: %ls\n", hid_error(handle));
	}
	int i;
	for (i = 0; i < 32; ++i)
	{
		fprintf(logfile, "%02x ", buf[i]);
	}
	fprintf(logfile, "\n");
	fflush(logfile);
}

static void usage()
{
	printf("Usage: scsi2sd-debug outputfile\n");
	printf("\n");
	printf("outputfile\tPath to the output log file.\n\n");
	printf("\n\n");
	exit(1);
}


int main(int argc, char* argv[])
{
	printf("SCSI2SD Debug Utility.\n");
	printf("Copyright (C) 2014 Michael McMaster <michael@codesrc.com>\n\n");

	if (argc != 2)
	{
		usage();
		exit(1);
	}

	logfile = fopen(argv[1], "w");
	if (!logfile)
	{
		fprintf(stderr, "Could not write to file %s.\n", argv[1]);
		exit(1);
	}


	uint16_t vendorId = 0x04B4; // Cypress
	uint16_t productId = 0x1337; // SCSI2SD

	printf(
		"USB device parameters\n\tVendor ID:\t0x%04X\n\tProduct ID:\t0x%04X\n",
		vendorId,
		productId);

	// Enumerate and print the HID devices on the system
	struct hid_device_info *dev = hid_enumerate(vendorId, productId);

	if (!dev)
	{
		fprintf(stderr, "ERROR: SCSI2SD USB device not found.\n");
		exit(1);
	}

	// We need the SECOND interface for debug data
	while (dev && dev->interface_number != 1)
	{
		dev = dev->next;
	}
	if (!dev)
	{
		fprintf(stderr, "ERROR: SCSI2SD Debug firmware not enabled.\n");
		exit(1);
	}

	printf("USB Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
		dev->vendor_id, dev->product_id, dev->path, dev->serial_number);
	printf("\n");
	printf("  Manufacturer: %ls\n", dev->manufacturer_string);
	printf("  Product:      %ls\n", dev->product_string);
	printf("\n");

	hid_device* handle = hid_open_path(dev->path);
	if (!handle)
	{
		fprintf(
			stderr,
			"ERROR: Could not open device %s. Check permissions.\n", dev->path
			);
		exit(1);
	}


	while (1)
	{
		readConfig(handle);
	}

	return 0;
}

