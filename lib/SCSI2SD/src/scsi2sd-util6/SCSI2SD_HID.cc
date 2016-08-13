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
	myFirmwareVersion(0),
	mySDCapacity(0)
{

	try
	{
		std::stringstream msg;
		msg << "Error opening HID device " << hidInfo->path << std::endl;

		myConfigHandle = hid_open_path(hidInfo->path);
		if (!myConfigHandle) throw std::runtime_error(msg.str());
		readNewDebugData();
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
	std::vector<uint8_t> out;
	std::vector<uint8_t> cmd { S2S_CMD_REBOOT };
	sendHIDPacket(cmd, out, 1);
}

void
HID::readSector(uint32_t sector, std::vector<uint8_t>& out)
{
	std::vector<uint8_t> cmd
	{
		S2S_CMD_SD_READ,
		static_cast<uint8_t>(sector >> 24),
		static_cast<uint8_t>(sector >> 16),
		static_cast<uint8_t>(sector >> 8),
		static_cast<uint8_t>(sector)
	};
	sendHIDPacket(cmd, out, HIDPACKET_MAX_LEN / 62);
}

void
HID::writeSector(uint32_t sector, const std::vector<uint8_t>& in)
{
	std::vector<uint8_t> cmd
	{
		S2S_CMD_SD_WRITE,
		static_cast<uint8_t>(sector >> 24),
		static_cast<uint8_t>(sector >> 16),
		static_cast<uint8_t>(sector >> 8),
		static_cast<uint8_t>(sector)
	};
	cmd.insert(cmd.end(), in.begin(), in.end());
	std::vector<uint8_t> out;
	sendHIDPacket(cmd, out, 1);
	if ((out.size() < 1) || (out[0] != S2S_CFG_STATUS_GOOD))
	{
		std::stringstream ss;
		ss << "Error writing sector " << sector;
		throw std::runtime_error(ss.str());
	}
}

bool
HID::readSCSIDebugInfo(std::vector<uint8_t>& buf)
{
	std::vector<uint8_t> cmd { S2S_CMD_DEBUG };
	sendHIDPacket(cmd, buf, 1);
	return buf.size() > 0;
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
HID::readNewDebugData()
{
	// Newer devices only have a single HID interface, and present
	// a command to obtain the data
	std::vector<uint8_t> cmd { S2S_CMD_DEVINFO, 0xDE, 0xAD, 0xBE, 0xEF };
	std::vector<uint8_t> out;
	try
	{
		sendHIDPacket(cmd, out, 6);
	}
	catch (std::runtime_error& e)
	{
		myFirmwareVersion = 0;
		mySDCapacity = 0;
		return;
	}

	out.resize(6);
	myFirmwareVersion = (out[0] << 8) | out[1];
	mySDCapacity =
		(((uint32_t)out[2]) << 24) |
		(((uint32_t)out[3]) << 16) |
		(((uint32_t)out[4]) << 8) |
		((uint32_t)out[5]);
}

std::string
HID::getFirmwareVersionStr() const
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


bool
HID::ping()
{
	std::vector<uint8_t> cmd { S2S_CMD_PING };
	std::vector<uint8_t> out;
	try
	{
		sendHIDPacket(cmd, out, 1);
	}
	catch (std::runtime_error& e)
	{
		return false;
	}

	return (out.size() >= 1) && (out[0] == S2S_CFG_STATUS_GOOD);
}

std::vector<uint8_t>
HID::getSD_CSD()
{
	std::vector<uint8_t> cmd { S2S_CMD_SDINFO };
	std::vector<uint8_t> out;
	try
	{
		sendHIDPacket(cmd, out, 16);
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
	std::vector<uint8_t> cmd { S2S_CMD_SDINFO };
	std::vector<uint8_t> out;
	try
	{
		sendHIDPacket(cmd, out, 16);
	}
	catch (std::runtime_error& e)
	{
		return std::vector<uint8_t>(16);
	}

	std::vector<uint8_t> result(16);
	for (size_t i = 0; i < 16; ++i) result[i] = out[16 + i];
	return result;
}

bool
HID::scsiSelfTest(int& code)
{
	std::vector<uint8_t> cmd { S2S_CMD_SCSITEST };
	std::vector<uint8_t> out;
	try
	{
		sendHIDPacket(cmd, out, 2);
	}
	catch (std::runtime_error& e)
	{
		return false;
	}
	code = out.size() >= 2 ? out[1] : -1;
	return (out.size() >= 1) && (out[0] == S2S_CFG_STATUS_GOOD);
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

	for (unsigned int retry = 0; retry < responseLength * 2 && !resp; ++retry)
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


