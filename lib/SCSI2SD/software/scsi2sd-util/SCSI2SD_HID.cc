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
#include "scsi2sd.h"
#include "hidpacket.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/utils.h>

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
	try
	{
		while (hidInfo && !(myConfigHandle && myDebugHandle))
		{
			std::stringstream msg;
			msg << "Error opening HID device " << hidInfo->path << std::endl;

			if ((hidInfo->interface_number == CONFIG_INTERFACE) ||
				(hidInfo->usage_page == 0xFF00))
			{
				myConfigHandle = hid_open_path(hidInfo->path);
				if (!myConfigHandle) throw std::runtime_error(msg.str());

				hidInfo = hidInfo->next;
			}
			else if ((hidInfo->interface_number == DEBUG_INTERFACE) ||
				(hidInfo->usage_page == 0xFF01))
			{
				myDebugHandle = hid_open_path(hidInfo->path);
				if (!myDebugHandle) throw std::runtime_error(msg.str());
				readDebugData();
				hidInfo = hidInfo->next;
			}
			else if (hidInfo->interface_number == -1)
			{
				// hidInfo->interface_number not supported on mac, and
				// interfaces are enumerated in a random order. :-(
				// We rely on the watchdog value of the debug interface
				// changing on each read to differentiate the interfaces.
				// Not necessary since firmware 3.5.2 as the usage_page can
				// be used instead.
				hid_device* dev = hid_open_path(hidInfo->path);
				if (!dev)
				{
					throw std::runtime_error(msg.str());
				}

				uint8_t buf[HID_PACKET_SIZE];
				int watchVal = -1;
				int configIntFound = 1;
				for (int i = 0; i < 4; ++i)
				{
					buf[0] = 0; // report id
					hid_read_timeout(dev, buf, HID_PACKET_SIZE, HID_TIMEOUT_MS);
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
	catch (std::runtime_error& e)
	{
		destroy();
		throw e;
	}
}

void
HID::destroy()
{
	if (myConfigHandle)
	{
		hid_close(myConfigHandle);
		myConfigHandle = NULL;
	}
	if (myDebugHandle)
	{
		hid_close(myDebugHandle);
		myDebugHandle = NULL;
	}

	hid_free_enumeration(myHidInfo);
	myHidInfo = NULL;
}

HID::~HID()
{
	destroy();
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
		if (result <= 0)
		{
			const wchar_t* err = hid_error(myDebugHandle);
			std::stringstream ss;
			ss << "USB HID write failure: " << err;
			throw std::runtime_error(ss.str());
		}
	}
}

void
HID::readFlashRow(int array, int row, std::vector<uint8_t>& out)
{
	std::vector<uint8_t> cmd
	{
		CONFIG_READFLASH,
		static_cast<uint8_t>(array),
		static_cast<uint8_t>(row)
	};
	sendHIDPacket(cmd, out, HIDPACKET_MAX_LEN / 62);
}

void
HID::writeFlashRow(int array, int row, const std::vector<uint8_t>& in)
{
	std::vector<uint8_t> cmd;
	cmd.push_back(CONFIG_WRITEFLASH);
	cmd.insert(cmd.end(), in.begin(), in.end());
	cmd.push_back(static_cast<uint8_t>(array));
	cmd.push_back(static_cast<uint8_t>(row));
	std::vector<uint8_t> out;
	sendHIDPacket(cmd, out, 1);
	if ((out.size() < 1) || (out[0] != CONFIG_STATUS_GOOD))
	{
		std::stringstream ss;
		ss << "Error writing flash " << array << "/" << row;
		throw std::runtime_error(ss.str());
	}
}

bool
HID::readSCSIDebugInfo(std::vector<uint8_t>& buf)
{
	buf[0] = 0; // report id
	hid_set_nonblocking(myDebugHandle, 1);
	int result =
		hid_read_timeout(
			myDebugHandle,
			&buf[0],
			HID_PACKET_SIZE,
			HID_TIMEOUT_MS);
	hid_set_nonblocking(myDebugHandle, 0);

	if (result <= 0)
	{
		const wchar_t* err = hid_error(myDebugHandle);
		std::stringstream ss;
		ss << "USB HID read failure: " << err;
		throw std::runtime_error(ss.str());
	}
	return result > 0;
}


void
HID::readHID(uint8_t* buffer, size_t len)
{
	assert(len >= 0);
	buffer[0] = 0; // report id

	int result = -1;
	for (int retry = 0; retry < 3 && result <= 0; ++retry)
	{
		result = hid_read_timeout(myConfigHandle, buffer, len, HID_TIMEOUT_MS);
	}

	if (result < 0)
	{
		const wchar_t* err = hid_error(myConfigHandle);
		std::stringstream ss;
		ss << "USB HID read failure: " << err;
		throw std::runtime_error(ss.str());
	}
}

void
HID::readDebugData()
{
	uint8_t buf[HID_PACKET_SIZE];
	buf[0] = 0; // report id
	int result =
		hid_read_timeout(
			myDebugHandle,
			buf,
			HID_PACKET_SIZE,
			HID_TIMEOUT_MS);

	if (result <= 0)
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
			'.' << ((myFirmwareVersion & 0xF0) >> 4);

		int rev = myFirmwareVersion & 0xF;
		if (rev)
		{
			ver << "." << rev;
		}
		return ver.str();
	}
}


bool
HID::ping()
{
	std::vector<uint8_t> cmd { CONFIG_PING };
	std::vector<uint8_t> out;
	try
	{
		sendHIDPacket(cmd, out, 1);
	}
	catch (std::runtime_error& e)
	{
		return false;
	}

	return (out.size() >= 1) && (out[0] == CONFIG_STATUS_GOOD);
}

std::vector<uint8_t>
HID::getSD_CSD()
{
	std::vector<uint8_t> cmd { CONFIG_SDINFO };
	std::vector<uint8_t> out;
	try
	{
		sendHIDPacket(cmd, out, 1);
	}
	catch (std::runtime_error& e)
	{
		return std::vector<uint8_t>(16);
	}

	out.resize(16);
	return out;
}

std::vector<uint8_t>
HID::getSD_CID()
{
	std::vector<uint8_t> cmd { CONFIG_SDINFO };
	std::vector<uint8_t> out;
	try
	{
		sendHIDPacket(cmd, out, 1);
	}
	catch (std::runtime_error& e)
	{
		return std::vector<uint8_t>(16);
	}

	std::vector<uint8_t> result(16);
	for (size_t i = 0; i < 16; ++i) result[i] = out[16 + i];
	return result;
}

void
HID::sendHIDPacket(
	const std::vector<uint8_t>& cmd,
	std::vector<uint8_t>& out,
	size_t responseLength)
{
	assert(cmd.size() <= HIDPACKET_MAX_LEN);
	hidPacket_send(&cmd[0], cmd.size());

	uint8_t hidBuf[HID_PACKET_SIZE];
	const uint8_t* chunk = hidPacket_getHIDBytes(hidBuf);

	while (chunk)
	{
		uint8_t reportBuf[HID_PACKET_SIZE + 1] = { 0x00 }; // Report ID
		memcpy(&reportBuf[1], chunk, HID_PACKET_SIZE);
		int result = -1;
		for (int retry = 0; retry < 10 && result <= 0; ++retry)
		{
			result = hid_write(myConfigHandle, reportBuf, sizeof(reportBuf));
		}

		if (result <= 0)
		{
			const wchar_t* err = hid_error(myConfigHandle);
			std::stringstream ss;
			ss << "USB HID write failure: " << err;
			throw std::runtime_error(ss.str());
		}
		chunk = hidPacket_getHIDBytes(hidBuf);
	}

	const uint8_t* resp = NULL;
	size_t respLen;
	resp = hidPacket_getPacket(&respLen);

	for (int retry = 0; retry < responseLength * 2 && !resp; ++retry)
	{
		readHID(hidBuf, sizeof(hidBuf)); // Will block
		hidPacket_recv(hidBuf, HID_PACKET_SIZE);
		resp = hidPacket_getPacket(&respLen);
	}

	if (!resp)
	{
		throw std::runtime_error("SCSI2SD config protocol error");
	}
	out.insert(
		out.end(),
		resp,
		resp + respLen);
}


