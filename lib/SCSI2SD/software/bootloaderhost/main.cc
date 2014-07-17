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

#include "SCSI2SD_HID.hh"
#include "SCSI2SD_Bootloader.hh"
#include "Firmware.hh"

#if __cplusplus >= 201103L
#include <cstdint>
#include <memory>
using std::shared_ptr;
#else
#include <stdint.h>
#include <tr1/memory>
using std::tr1::shared_ptr;
#endif

#include <iomanip>
#include <iostream>
#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace SCSI2SD;

extern "C"
void ProgressUpdate(unsigned char arrayId, unsigned short rowNum)
{
	//std::cerr <<
		//"Programmed flash array " << static_cast<int>(arrayId) <<
		//	", row " << rowNum << std::endl;
	std::cout << "." << std::flush;
}

static void usage()
{
	std::cout << "Usage: bootloaderhost [-f] "
		"/path/to/firmware.cyacd\n" <<
		"\t-f\tForce, even if the firmware doesn't match the target board.\n\n" <<
		std::endl;
}


int main(int argc, char* argv[])
{
	std::cout <<
		"PSoC 3/5LP USB HID Bootloader Host\n" <<
		"Copyright (C) 2013 Michael McMaster <michael@codesrc.com>\n" <<
		std::endl;

	int force = 0;

	opterr = 0;
	int c;
	while ((c = getopt(argc, argv, "v:p:f")) != -1)
	{
		switch (c)
		{
		case 'f':
			force = 1;
			break;
		case '?':
			usage();
			exit(1);
		}
	}

	std::string filename;
	if (optind < argc)
	{
		filename = argv[optind];
	}
	else
	{
		usage();
		exit(1);
	}

	// Enumerate and print the HID devices on the system
	shared_ptr<Bootloader> bootloader(Bootloader::Open());
	shared_ptr<HID> hid(HID::Open());

	if (hid)
	{
		try
		{
			hid->enterBootloader();
		}
		catch (std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			hid.reset();
		}
	}

	if (!hid)
	{
		std::cout <<
			"Waiting for device connection" << std::endl <<
			"Connect USB cable to the bus-powered device now, or otherwise "
				"reset the device." << std::endl;
	}


	while (!bootloader)
	{
		bootloader.reset(Bootloader::Open());

		if (!bootloader && !hid)
		{
			hid.reset(HID::Open());
			if (hid)
			{
				try
				{
					hid->enterBootloader();
				}
				catch (std::exception& e)
				{
					std::cerr << e.what();
				}
			}
		}

		if (!bootloader)
		{
			usleep(100000); // 100ms
		}
	}

	std::stringstream foundMsg;
	foundMsg <<
		"Device Found\n" <<
		"  type:\t\t\t" << std::setw(4) << std::hex <<
			Bootloader::VENDOR_ID << " " <<
			Bootloader::PRODUCT_ID <<
			"\n" <<
		"  path:\t\t\t" << bootloader->getDevicePath() << "\n";
	std::cout << foundMsg.str() << std::endl;

	Bootloader::HWInfo hwInfo(bootloader->getHWInfo());
	std::cout <<
		"  Board:\t\t" << hwInfo.desc << "\n" <<
		"  Revision:\t\t" << hwInfo.version << std::endl;

	if (hid)
	{
		std::cout <<
			"  Existing firmware:\t" <<
			hid->getFirmwareVersionStr() << std::endl;
	}


	if (!bootloader->isCorrectFirmware(filename) && !force)
	{
		std::cerr <<
			"ERROR: Unexpected firmware file. Expected: \""
				<< hwInfo.firmwareName << "\"\n\n" <<
			"Using firmware design for a different board will destroy your " <<
			"hardware.\n" <<
			"If you still wish to proceed, try again with the \"-f\" flag.\n" <<
			std::endl;
		exit(1);
	}

	Firmware firmware(filename);

	std::stringstream firmMsg;
	firmMsg <<
		"  Firmware Silicon ID:\t" <<  std::hex << firmware.siliconId() <<
			"\n";
	std::cout << firmMsg.str() << std::endl;

	std::cout << "Starting firmware upload: " << filename << std::endl;

	try
	{
		bootloader->load(filename, &ProgressUpdate);
		std::cout << "Firmware upload complete." << std::endl;
	}
	catch (std::exception& e)
	{
		std::cerr << "ERROR: Firmware update failed.\n" << e.what() << std::endl;
		exit(1);
	}

	return 0;
}

