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

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// htonl/ntohl includes.
#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "hidapi.h"

#define MIN(a,b) (a < b ? a : b)

enum
{
	PARAM_ID,
	PARAM_PARITY,
	PARAM_NOPARITY,
	PARAM_UNITATT,
	PARAM_NOUNITATT,
	PARAM_MAXBLOCKS,
	PARAM_APPLE,
	PARAM_VENDOR,
	PARAM_PRODID,
	PARAM_REV,
	PARAM_BYTESPERSECTOR
};

// Must be consistent with the structure defined in the SCSI2SD config.h header.
// We always transfer data in network byte order.
typedef struct __attribute((packed))
{
	uint8_t scsiId;
	char vendor[8];
	char prodId[16];
	char revision[4];
	uint8_t enableParity;
	uint8_t enableUnitAttention;
	uint8_t reserved1; // Unused. Ensures maxBlocks is aligned.
	uint32_t maxSectors;
	uint16_t bytesPerSector;


	// Pad to 64 bytes, which is what we can fit into a USB HID packet.
	char reserved[26];
} ConfigPacket;

static void printConfig(ConfigPacket* packet)
{
	printf("SCSI ID:\t\t\t%d\n", packet->scsiId);
	printf("Vendor:\t\t\t\t\"%.*s\"\n", 8, packet->vendor);
	printf("Product ID:\t\t\t\"%.*s\"\n", 16, packet->prodId);
	printf("Revision:\t\t\t\"%.*s\"\n", 4, packet->revision);
	printf("\n");
	printf("Parity Checking:\t\t%s\n", packet->enableParity ? "enabled" : "disabled");
	printf("Unit Attention Condition:\t%s\n", packet->enableUnitAttention ? "enabled" : "disabled");
	printf("Bytes per sector:\t\t%d\n", packet->bytesPerSector);
	if (packet->maxSectors)
	{
		char sizeBuf[64];
		uint64_t maxBytes = packet->maxSectors * (uint64_t) packet->bytesPerSector;
		if (maxBytes > (1024*1024*1024))
		{
			sprintf(sizeBuf, "%.02fGB", maxBytes / (1024.0*1024.0*1024.0));
		}
		else if (maxBytes > (1024*1024))
		{
			sprintf(sizeBuf, "%.02fMB", maxBytes / (1024.0*1024.0));
		}
		else if (maxBytes > (1024))
		{
			sprintf(sizeBuf, "%.02fKB", maxBytes / (1024.0));
		}
		else
		{
			sprintf(sizeBuf, "%" PRIu64 " bytes", maxBytes);
		}

		printf("Maximum Size:\t\t\t%s (%d sectors)\n", sizeBuf, packet->maxSectors);
	}
	else
	{
		printf("Maximum Size:\t\t\tUnlimited\n");
	}
}

static int readConfig(hid_device* handle, ConfigPacket* packet)
{
	// First byte is the report ID (0)
	unsigned char buf[1 + sizeof(ConfigPacket)];
	memset(buf, 0, sizeof(buf));
	memset(packet, 0, sizeof(ConfigPacket));
	int result = hid_read(handle, buf, sizeof(buf));

	if (result < 0)
	{
		fprintf(stderr, "USB HID Read Failure: %ls\n", hid_error(handle));
	}

	memcpy(packet, buf, result);
	packet->maxSectors = ntohl(packet->maxSectors);
	packet->bytesPerSector = ntohs(packet->bytesPerSector);

	return result;
}

static int writeConfig(hid_device* handle, ConfigPacket* packet)
{
	unsigned char buf[1 + sizeof(ConfigPacket)];
	buf[0] = 0; // report ID

	packet->maxSectors = htonl(packet->maxSectors);
	packet->bytesPerSector = htons(packet->bytesPerSector);
	memcpy(buf + 1, packet, sizeof(ConfigPacket));
	packet->maxSectors = ntohl(packet->maxSectors);
	packet->bytesPerSector = ntohs(packet->bytesPerSector);

	int result = hid_write(handle, buf, sizeof(buf));

	if (result < 0)
	{
		fprintf(stderr, "USB HID Write Failure: %ls\n", hid_error(handle));
	}

	return result;
}

static void usage()
{
	printf("Usage: scsi2sd-config [options...]\n");
	printf("\n");
	printf("--id={0-7}\tSCSI device ID.\n\n");
	printf("--parity\tCheck the SCSI parity signal, and reject data where\n");
	printf("\t\tthe parity is bad.\n\n");
	printf("--no-parity\tDon't check the SCSI parity signal.\n");
	printf("\t\tThis is required for SCSI host controllers that do not provide\n");
	printf("\t\tparity.\n\n");
	printf("--attention\tRespond with a Unit Attention status on device reset.\n");
	printf("\t\tSome systems will fail on this response, even though it is\n");
	printf("\t\trequired by the SCSI-2 standard.\n\n");
	printf("--no-attention\tDisable Unit Attention responses.\n\n");
	printf("--blocks={0-4294967295}\n\t\tSet a limit to the reported device size.\n");
	printf("\t\tThe size of each block/sector is set by the --sector parameter.\n");
	printf("\t\tThe reported size will be the lower of this value and the SD\n");
	printf("\t\tcard size. 0 disables the limit.\n");
	printf("\t\tThe maximum possible size is 2TB.\n\n");
	printf("--sector={64-8192}\n\t\tSet the bytes-per-sector. Normally 512 bytes.\n");
	printf("\t\tCan also be set with a SCSI MODE SELECT command.\n\n");
	printf("--apple\t\tSet the vendor, product ID and revision fields to simulate an \n");
	printf("\t\tapple-suppled disk. Provides support for the Apple Drive Setup\n");
	printf("\t\tutility.\n\n");
	printf("--vendor={vendor}\tSets the reported device vendor. Up to 8 characters.\n\n");
	printf("--prod-id={prod-id}\tSets the reported product ID. Up to 16 characters.\n\n");
	printf("--rev={revision}\tSets the reported device revision. Up to 4 characters.\n\n");
	printf("\n");
	printf("\nThe current configuration settings are displayed if no options are supplied");
	printf("\n\n");
	exit(1);
}

int main(int argc, char* argv[])
{
	printf("SCSI2SD Configuration Utility.\n");
	printf("Copyright (C) 2013 Michael McMaster <michael@codesrc.com>\n\n");

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

	printf("USB Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
		dev->vendor_id, dev->product_id, dev->path, dev->serial_number);
	printf("\n");
	printf("  Manufacturer: %ls\n", dev->manufacturer_string);
	printf("  Product:      %ls\n", dev->product_string);
	printf("\n");

	// Open the device using the VID, PID,
	// and optionally the Serial number.
	hid_device* handle = hid_open(vendorId, productId, NULL);
	if (!handle)
	{
		fprintf(
			stderr,
			"ERROR: Could not open device %s. Check permissions.\n", dev->path
			);
		exit(1);
	}

	ConfigPacket packet;
	if (readConfig(handle, &packet) <= 0)
	{
		fprintf(stderr, "ERROR: Invalid data received from device.\n");
		exit(1);
	}

	struct option options[] =
	{
		{
			"id", required_argument, NULL, PARAM_ID
		},
		{
			"parity", no_argument, NULL, PARAM_PARITY
		},
		{
			"no-parity", no_argument, NULL, PARAM_NOPARITY
		},
		{
			"attention", no_argument, NULL, PARAM_UNITATT
		},
		{
			"no-attention", no_argument, NULL, PARAM_NOUNITATT
		},
		{
			"blocks", required_argument, NULL, PARAM_MAXBLOCKS
		},
		{
			"apple", no_argument, NULL, PARAM_APPLE
		},
		{
			"vendor", required_argument, NULL, PARAM_VENDOR
		},
		{
			"prod-id", required_argument, NULL, PARAM_PRODID
		},
		{
			"rev", required_argument, NULL, PARAM_REV
		},
		{
			"sector", required_argument, NULL, PARAM_BYTESPERSECTOR
		},
		{
			NULL, 0, NULL, 0
		}
	};

	int doWrite = 0;
	int optIdx = 0;
	int c;
	while ((c = getopt_long(argc, argv, "", options, &optIdx)) != -1)
	{
		doWrite = 1;
		switch (c)
		{
		case PARAM_ID:
		{
			int id = -1;
			if (sscanf(optarg, "%d", &id) == 1 && id >= 0 && id <= 7)
			{
				packet.scsiId = id;
			}
			else
			{
				usage();
			}
			break;
		}

		case PARAM_PARITY:
			packet.enableParity = 1;
			break;

		case PARAM_NOPARITY:
			packet.enableParity = 0;
			break;

		case PARAM_UNITATT:
			packet.enableUnitAttention = 1;
			break;

		case PARAM_NOUNITATT:
			packet.enableUnitAttention = 0;
			break;

		case PARAM_MAXBLOCKS:
		{
			int64_t maxSectors = -1;
			if (sscanf(optarg, "%" PRId64, &maxSectors) == 1 &&
				maxSectors >= 0 && maxSectors <= UINT32_MAX)
			{
				packet.maxSectors = maxSectors;
			}
			else
			{
				usage();
			}
			break;
		}

		case PARAM_APPLE:
			memcpy(packet.vendor, " SEAGATE", 8);
			memcpy(packet.prodId, "          ST225N", 16);
			memcpy(packet.revision, "1.0 ", 4);
			break;

		case PARAM_VENDOR:
			memset(packet.vendor, ' ', 8);
			memcpy(packet.vendor, optarg, MIN(strlen(optarg), 8));
			break;

		case PARAM_PRODID:
			memset(packet.prodId, ' ', 16);
			memcpy(packet.prodId, optarg, MIN(strlen(optarg), 16));
			break;

		case PARAM_REV:
			memset(packet.revision, ' ', 4);
			memcpy(packet.revision, optarg, MIN(strlen(optarg), 4));
			break;

		case PARAM_BYTESPERSECTOR:
		{
			int64_t bytesPerSector = -1;
			if (sscanf(optarg, "%" PRId64, &bytesPerSector) == 1 &&
				bytesPerSector >= 64 && bytesPerSector <= 8192)
			{
				packet.bytesPerSector = bytesPerSector;
			}
			else
			{
				usage();
			}
			break;
		}
		case '?':
			usage();
		}
	}

	if (doWrite)
	{
		printf("Saving configuration...");
		if (writeConfig(handle, &packet) <= 0)
		{
			printf(" Fail.\n");
			fprintf(stderr, "ERROR: Failed to save config.\n");
			exit(1);
		}
		printf(" Done.\n");

		// Clear outstanding stale data
		readConfig(handle, &packet);

		// Proper update
		if (readConfig(handle, &packet) <= 0)
		{
			fprintf(stderr, "ERROR: Invalid data received from device.\n");
			exit(1);
		}
	}

	printf("\nCurrent Device Settings:\n");
	printConfig(&packet);

	return 0;
}

