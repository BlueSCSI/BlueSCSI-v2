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

#include "Firmware.hh"

extern "C"
{
#include "cybtldr_parse.h"
}

#include <functional>
#include <sstream>
#include <stdexcept>

#include <string.h>

using namespace SCSI2SD;

namespace
{
	struct FirmwareFile
	{
		~FirmwareFile()
		{
			CyBtldr_CloseDataFile();
		}
	};
}

Firmware::Firmware(const std::string& path)
{
	if (CyBtldr_OpenDataFile(path.c_str()) != CYRET_SUCCESS)
	{
		std::stringstream msg;
		msg << "Could not open file: " << path;
		throw std::runtime_error(msg.str());
	}

	FirmwareFile closeOnReturn;

	uint8_t buffer[MAX_BUFFER_SIZE];
	unsigned int lineSize;
	if (
		CyBtldr_ReadLine(&lineSize, reinterpret_cast<char*>(buffer))
			!= CYRET_SUCCESS
		)
	{
		std::stringstream msg;
		msg << "Could not read file: " << path;
		throw std::runtime_error(msg.str());
	}

	{
		unsigned long silId;
		unsigned char silRev[MAX_BUFFER_SIZE];
		unsigned char chksum[MAX_BUFFER_SIZE];
		if (
			CyBtldr_ParseHeader(
				lineSize,
				buffer,
				&silId,
				silRev,
				chksum)
			!= CYRET_SUCCESS)
		{
			std::stringstream msg;
			msg << "Premature end of file: " << path;
			throw std::runtime_error(msg.str());
		}
		mySiliconId = silId;
		mySiliconRev =
			std::string(
				reinterpret_cast<char*>(silRev),
				strnlen(reinterpret_cast<char*>(silRev), MAX_BUFFER_SIZE)
				);
	}

	// Count the number of flash rows. This is used to show "percentage
	// complete" when uploading the firmware.
	myTotalFlashRows = 0;
	while (
		CyBtldr_ReadLine(&lineSize, reinterpret_cast<char*>(buffer))
			== CYRET_SUCCESS
		)
	{
		myTotalFlashRows++;
	}
}

Firmware::~Firmware()
{
}

