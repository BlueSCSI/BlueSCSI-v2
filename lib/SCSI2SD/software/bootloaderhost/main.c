//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "hidapi.h"
#include "cybtldr_api.h"
#include "cybtldr_api2.h"

hid_device *handle = NULL;

static int OpenConnection(void)
{
	return 0;
}

static int CloseConnection(void)
{
	return 0;
}

static int ReadData(unsigned char* data, int count)
{
	unsigned char buf[65];
	buf[0] = 0; // Report ID

	int result = hid_read(handle, buf, count);

	if (result < 0)
	{
		fprintf(stderr, "USB HID Read Failure: %ls\n", hid_error(handle));
	}

	memcpy(data, buf, count);

	return (result >= 0) ? 0 : -1;
}

static int WriteData(unsigned char* data, int count)
{
	unsigned char buf[65];
	buf[0] = 0; // report ID
	int i;
	for (i = 0; i < count; ++i)
	{
		buf[i+1] = data[i];
	}
	int result = hid_write(handle, buf, count + 1);

	if (result < 0)
	{
		fprintf(stderr, "USB HID Write Failure: %ls\n", hid_error(handle));
	}

	return (result >= 0) ? 0 : -1;
}


static void ProgressUpdate(unsigned char arrayId, unsigned short rowNum)
{
	printf("Programmed flash array %d, row %d\n", arrayId, rowNum);
}

static void usage()
{
	printf("Usage: bootloaderhost [-v UsbVendorId] [-p UsbProductId] /path/to/firmware.cyacd\n");
	printf("\n\n");
}

int main(int argc, char* argv[])
{
	CyBtldr_CommunicationsData cyComms =
	{
		&OpenConnection,
		&CloseConnection,
		&ReadData,
		&WriteData,
		64
	};

	printf("PSoC 3/5LP USB HID Bootloader Host\n");
	printf("Copyright (C) 2013 Michael McMaster <michael@codesrc.com>\n\n");

	uint16_t vendorId = 0x04B4; // Cypress
	uint16_t productId = 0xB71D; // Default PSoC3/5LP Bootloader

	opterr = 0;
	int c;
	while ((c = getopt(argc, argv, "v:p:")) != -1)
	{
		switch (c)
		{
		case 'v':
			sscanf(optarg, "%hx", &vendorId);
			break;
		case 'p':
			sscanf(optarg, "%hx", &productId);
			break;
		case '?':
			usage();
			exit(1);
		}
	}

	const char* filename;
	if (optind < argc)
	{
		filename = argv[optind];
	}
	else
	{
		usage();
		exit(1);
	}

	printf(
		"USB device parameters\n\tVendor ID:\t0x%04X\n\tProduct ID:\t0x%04X\n",
		vendorId,
		productId);

	// Enumerate and print the HID devices on the system
	struct hid_device_info *dev = hid_enumerate(vendorId, productId);

	if (!dev)
	{
		printf("Waiting for device connection\n");
		printf("Connect USB cable to the bus-powered device now, or otherwise reset the device.\n");
	}

	while (!dev)
	{
		dev = hid_enumerate(vendorId, productId);
		usleep(10000); // 10ms
	}

	printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
		dev->vendor_id, dev->product_id, dev->path, dev->serial_number);
	printf("\n");
	printf("  Manufacturer: %ls\n", dev->manufacturer_string);
	printf("  Product:      %ls\n", dev->product_string);
	printf("\n");
	//hid_free_enumeration(devs);

	// Open the device using the VID, PID,
	// and optionally the Serial number.
	handle = hid_open(vendorId, productId, NULL);
	if (!handle)
	{
		fprintf(
			stderr,
			"Could not open device %s. Check permissions.\n", dev->path
			);
		exit(1);
	}


	printf("Starting firmware upload: %s\n", filename);
	int result = CyBtldr_Program(
		filename,
		&cyComms,
		&ProgressUpdate);
	if (result == 0)
	{
		printf("Firmware update complete\n");
	}
	else
	{
		printf("Firmware update failed\n");
	}

	return 0;
}

