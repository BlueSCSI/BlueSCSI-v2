//	Copyright (C) 2018 Michael McMaster <michael@codesrc.com>
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


// For compilers that support precompilation, includes "wx/wx.h".
#include "SCSI2SD_HID.hh"
#include "Dfu.hh"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>
#include <set>
#include <sstream>

#if __cplusplus >= 201103L
#include <cstdint>
#include <memory>
using std::shared_ptr;
#else
#include <stdint.h>
#include <tr1/memory>
using std::tr1::shared_ptr;
#endif

using namespace SCSI2SD;

int main()
{
	shared_ptr<HID> hid;
	try
	{
		hid.reset(HID::Open());
		if (hid)
		{
			std::cout << "SCSI2SD Ready, firmware version " <<
				hid->getFirmwareVersionStr() << "\n";

			std::vector<uint8_t> csd(hid->getSD_CSD());
			std::vector<uint8_t> cid(hid->getSD_CID());
			std::cout << "SD Capacity (512-byte sectors): " <<
				hid->getSDCapacity() << std::endl;

			int errcode;
			std::cout << "SCSI Self-Test: ";
			if (hid->scsiSelfTest(errcode))
			{
				std::cout << "Passed\n";
			}
			else
			{
				std::cout << "FAIL (" << errcode << ")\n";
			}
		}
		else
		{
			std::cerr << "Device not found" << std::endl;
		}
	}
	catch (std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
	}
}

