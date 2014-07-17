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

#ifndef SCSI2SD_Bootloader_H
#define SCSI2SD_Bootloader_H

#include "hidapi.h"

#if __cplusplus >= 201103L
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <string>

namespace SCSI2SD
{

class Bootloader
{
public:
	static const uint16_t VENDOR_ID = 0x04B4; // Cypress
	static const uint16_t PRODUCT_ID = 0xB71D; // Default PSoC3/5LP Bootloader

	static const size_t HID_PACKET_SIZE = 64;

	static Bootloader* Open();

	~Bootloader();

	struct HWInfo
	{
		std::string desc;
		std::string version;
		std::string firmwareName;
	};
	HWInfo getHWInfo() const;

	// USB HID data
	std::string getDevicePath() const;
	std::wstring getManufacturer() const;
	std::wstring getProductString() const;

	bool isCorrectFirmware(const std::string& path) const;

	// progress function accepts flash array ID and row Number
	void load(const std::string& path, void (*progress)(uint8_t, uint16_t));
private:
	Bootloader(hid_device_info* hidInfo);

	hid_device_info* myHidInfo;
	hid_device* myBootloaderHandle;
};

} // namespace
#endif
