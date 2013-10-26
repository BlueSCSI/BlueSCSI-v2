#include <stdio.h>
#include <stdlib.h>

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
printf("read");
	return (hid_read(handle, data, count) >= 0 ? 0 : -1);
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
	//int result = hid_write(handle, buf, 65);
	int result = hid_send_feature_report(handle, buf, count + 1);
printf("write %d, %d", count, result);
	return (result >= 0) ? 0 : -1;
}


static void ProgressUpdate(unsigned char arrayId, unsigned short rowNum)
{
	printf("Completed array %d, row %d\n", arrayId, rowNum);
}

int main(int argc, char* argv[])
{
	int res;
	unsigned char buf[65];
	#define MAX_STR 255
	wchar_t wstr[MAX_STR];
	int i;

	CyBtldr_CommunicationsData cyComms =
	{
		&OpenConnection,
		&CloseConnection,
		&ReadData,
		&WriteData,
		64
	};

	// Enumerate and print the HID devices on the system
	struct hid_device_info *dev = NULL;

	printf("Waiting for a mate\n");
	while (dev == NULL)
	{
		dev = hid_enumerate(0x04B4, 0xB71D);
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
	handle = hid_open(0x04B4, 0xB71D, NULL);


	printf("Tryng to program\n");
	int result = CyBtldr_Program("/home/michael/projects/SCSI2SD/git/software/SCSI2SD/SCSI2SD.cydsn/CortexM3/ARM_GCC_473/Release/SCSI2SD.cyacd",
		&cyComms,
		&ProgressUpdate);
	printf("Possibly successful ? %d\n", result);

	return 0;
}

