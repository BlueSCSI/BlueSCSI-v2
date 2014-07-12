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

#ifndef SCSI2SD_HID_H
#define SCSI2SD_HID_H

#include "hidapi.h"

#include <cstdint>
#include <string>

namespace SCSI2SD
{

class HID
{
public:
	static const uint16_t VENDOR_ID = 0x04B4; // Cypress
	static const uint16_t PRODUCT_ID = 0x1337; // SCSI2SD application firmware

	static const int CONFIG_INTERFACE = 0;
	static const int DEBUG_INTERFACE = 1;

	static const size_t HID_PACKET_SIZE = 64;

	static HID* Open();

	~HID();

	uint16_t getFirmwareVersion() const { return myFirmwareVersion; }
	std::string getFirmwareVersionStr() const;
	uint32_t getSDCapacity() const { return mySDCapacity; }


	void enterBootloader();

	void readConfig(uint8_t* buffer, size_t len);
	void saveConfig(uint8_t* buffer, size_t len);
private:
	HID(hid_device_info* hidInfo);
	void readDebugData();

	hid_device_info* myHidInfo;
	hid_device* myConfigHandle;
	hid_device* myDebugHandle;

	// Read-only data from the debug interface.
	uint16_t myFirmwareVersion;
	uint32_t mySDCapacity;
};

} // namespace

#endif
