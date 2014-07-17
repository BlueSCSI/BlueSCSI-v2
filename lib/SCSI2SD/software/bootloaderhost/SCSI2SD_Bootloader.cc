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

#include "SCSI2SD_Bootloader.hh"

#include <iostream>
#include <sstream>
#include <stdexcept>


extern "C"
{
#include "hidapi.h"
#include "cybtldr_api.h"
#include "cybtldr_api2.h"
}
#include <string.h>

using namespace SCSI2SD;

hid_device* SCSI2SDHID_handle = NULL;

// cybtldr interface.
extern "C" int
SCSI2SDHID_OpenConnection(void)
{
	return 0;
}

extern "C" int
SCSI2SDHID_CloseConnection(void)
{
	return 0;
}

extern "C" int
SCSI2SDHID_ReadData(unsigned char* data, int count)
{
	uint8_t buf[65];
	buf[0] = 0; // Report ID

	int result = hid_read(SCSI2SDHID_handle, buf, count);

	if (result < 0)
	{
		std::cerr << "USB HID Read Failure: " <<
			hid_error(SCSI2SDHID_handle) << std::endl;
	}

	memcpy(data, buf, count);

	return (result >= 0) ? 0 : -1;
}

extern "C" int
SCSI2SDHID_WriteData(unsigned char* data, int count)
{
	uint8_t buf[65];
	buf[0] = 0; // report ID
	int i;
	for (i = 0; i < count; ++i)
	{
		buf[i+1] = data[i];
	}
	int result = hid_write(SCSI2SDHID_handle, buf, count + 1);

	if (result < 0)
	{
		std::cerr << "USB HID Write Failure: " <<
			hid_error(SCSI2SDHID_handle) << std::endl;
	}

	return (result >= 0) ? 0 : -1;
}

Bootloader::Bootloader(hid_device_info* hidInfo) :
	myHidInfo(hidInfo),
	myBootloaderHandle(NULL)
{
	myBootloaderHandle = hid_open_path(hidInfo->path);
	if (!myBootloaderHandle)
	{
		hid_free_enumeration(myHidInfo);
		myHidInfo = NULL;

		std::stringstream msg;
		msg << "Error opening HID device " << hidInfo->path << std::endl;
		throw std::runtime_error(msg.str());
	}
}

Bootloader::~Bootloader()
{
	if (myBootloaderHandle)
	{
		hid_close(myBootloaderHandle);
	}

	hid_free_enumeration(myHidInfo);
}

Bootloader*
Bootloader::Open()
{
	hid_device_info* dev = hid_enumerate(VENDOR_ID, PRODUCT_ID);
	if (dev)
	{
		return new Bootloader(dev);
	}
	else
	{
		return NULL;
	}
}

Bootloader::HWInfo
Bootloader::getHWInfo() const
{
	HWInfo info = {"unknown", "unknown", "unknown"};

	switch (myHidInfo->release_number)
	{
	case 0x3001:
		info.desc = "3.5\" SCSI2SD (green)";
		info.version = "V3.0";
		info.firmwareName = "SCSI2SD-V3.cyacd";
		break;
	case 0x3002:
		info.desc = "3.5\" SCSI2SD (yellow) or 2.5\" SCSI2SD for Apple Powerbook";
		info.version = "V4.1/V4.2";
		info.firmwareName = "SCSI2SD-V4.cyacd";
		break;
	}
	return info;
}

bool
Bootloader::isCorrectFirmware(const std::string& path) const
{
	HWInfo info = getHWInfo();
	return path.rfind(info.firmwareName) != std::string::npos;
}

std::string
Bootloader::getDevicePath() const
{
	return myHidInfo->path;
}

std::wstring
Bootloader::getManufacturer() const
{
	return myHidInfo->manufacturer_string;
}

std::wstring
Bootloader::getProductString() const
{
	return myHidInfo->product_string;
}

void
Bootloader::load(const std::string& path, void (*progress)(uint8_t, uint16_t))
{
	CyBtldr_CommunicationsData cyComms =
	{
		&SCSI2SDHID_OpenConnection,
		&SCSI2SDHID_CloseConnection,
		&SCSI2SDHID_ReadData,
		&SCSI2SDHID_WriteData,
		HID_PACKET_SIZE
	};

	SCSI2SDHID_handle = myBootloaderHandle;

	int result = CyBtldr_Program(
		path.c_str(),
		&cyComms,
		progress);

	if (result)
	{
		throw std::runtime_error("Firmware update failed");
	}
}

