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
#include "SCSI2SD_HID.hh"

#include <cassert>
#include <stdexcept>
#include <sstream>

#include <iostream>
#include <string.h> // memcpy

using namespace SCSI2SD;

HID::HID(hid_device_info* hidInfo) :
	myHidInfo(hidInfo),
	myConfigHandle(NULL),
	myDebugHandle(NULL),
	myFirmwareVersion(0),
	mySDCapacity(0)
{
	// hidInfo->interface_number not supported on mac, and interfaces
	// are enumerated in a random order. :-(
	// We rely on the watchdog value of the debug interface changing on each
	// read to differentiate the interfaces.
	while (hidInfo && !(myConfigHandle && myDebugHandle))
	{
		if (hidInfo->interface_number == CONFIG_INTERFACE)
		{
			myConfigHandle = hid_open_path(hidInfo->path);
			hidInfo = hidInfo->next;
		}
		else if (hidInfo->interface_number == DEBUG_INTERFACE)
		{
			myDebugHandle = hid_open_path(hidInfo->path);
			readDebugData();
			hidInfo = hidInfo->next;
		}
		else if (hidInfo->interface_number == -1)
		{
			// hidInfo->interface_number not supported on mac, and
			// interfaces are enumerated in a random order. :-(
			// We rely on the watchdog value of the debug interface
			// changing on each read to differentiate the interfaces.
			hid_device* dev = hid_open_path(hidInfo->path);
			if (!dev)
			{
				hidInfo = hidInfo->next;
				continue;
			}

			uint8_t buf[HID_PACKET_SIZE];
			int watchVal = -1;
			int configIntFound = 1;
			for (int i = 0; i < 4; ++i)
			{
				buf[0] = 0; // report id
				hid_read(dev, buf, HID_PACKET_SIZE);
				if (watchVal == -1) watchVal = buf[25];
				configIntFound = configIntFound && (buf[25] == watchVal);
			}

			if (configIntFound)
			{
				myConfigHandle = dev;
			}
			else
			{
				myDebugHandle = dev;
				readDebugData();
			}
		}
	}
}

HID::~HID()
{
	if (myConfigHandle)
	{
		hid_close(myConfigHandle);
	}
	if (myDebugHandle)
	{
		hid_close(myDebugHandle);
	}

	hid_free_enumeration(myHidInfo);
}

HID*
HID::Open()
{
	hid_device_info* dev = hid_enumerate(VENDOR_ID, PRODUCT_ID);
	if (dev)
	{
		return new HID(dev);
	}
	else
	{
		return NULL;
	}
}

void
HID::enterBootloader()
{
	// Reboot commands added in firmware 3.5
	if (!myDebugHandle)
	{
		throw std::runtime_error(
			"Cannot enter SCSI2SD bootloader: debug interface not found");
	}
	else if (myFirmwareVersion == 0)
	{
		throw std::runtime_error(
			"Cannot enter SCSI2SD bootloader: old firmware version running.\n"
			"The SCSI2SD board cannot reset itself. Please disconnect and \n"
			"reconnect the USB cable.\n");
	}
	else
	{
		uint8_t hidBuf[HID_PACKET_SIZE + 1] =
		{
			0x00, // Report ID;
			0x01 // Reboot command
			// 63 bytes unused.
		};

		int result = hid_write(myDebugHandle, hidBuf, sizeof(hidBuf));
		if (result < 0)
		{
			const wchar_t* err = hid_error(myDebugHandle);
			std::stringstream ss;
			ss << "USB HID write failure: " << err;
			throw std::runtime_error(ss.str());
		}
	}
}

void
HID::readConfig(uint8_t* buffer, size_t len)
{
	assert(len >= 0);
	buffer[0] = 0; // report id

	int result = hid_read(myConfigHandle, buffer, len);

	if (result < 0)
	{
		const wchar_t* err = hid_error(myConfigHandle);
		std::stringstream ss;
		ss << "USB HID read failure: " << err;
		throw std::runtime_error(ss.str());
	}
}

void
HID::saveConfig(uint8_t* buffer, size_t len)
{
	assert(len >= 0 && len <= HID_PACKET_SIZE);

	uint8_t hidBuf[HID_PACKET_SIZE + 1] =
	{
		0x00, // Report ID;
	};
	memcpy(&hidBuf[1], buffer, len);

	int result = hid_write(myConfigHandle, hidBuf, len + 1);

	if (result < 0)
	{
		const wchar_t* err = hid_error(myConfigHandle);
		std::stringstream ss;
		ss << "USB HID write failure: " << err;
		throw std::runtime_error(ss.str());
	}


}

void
HID::readDebugData()
{
	uint8_t buf[HID_PACKET_SIZE];
	buf[0] = 0; // report id
	int result = hid_read(myDebugHandle, buf, HID_PACKET_SIZE);

	if (result < 0)
	{
		const wchar_t* err = hid_error(myDebugHandle);
		std::stringstream ss;
		ss << "USB HID read failure: " << err;
		throw std::runtime_error(ss.str());
	}
	myFirmwareVersion = (((uint16_t)buf[62]) << 8) | buf[63];

	mySDCapacity =
		(((uint32_t)buf[58]) << 24) |
		(((uint32_t)buf[59]) << 16) |
		(((uint32_t)buf[60]) << 8) |
		((uint32_t)buf[61]);
}

std::string
HID::getFirmwareVersionStr() const
{
	if (myFirmwareVersion == 0)
	{
		return "Unknown (3.0 - 3.4)";
	}
	else
	{
		std::stringstream ver;
		ver << std::hex <<
			(myFirmwareVersion >> 8) <<
			'.' << (myFirmwareVersion & 0xFF);
		return ver.str();
	}
}


