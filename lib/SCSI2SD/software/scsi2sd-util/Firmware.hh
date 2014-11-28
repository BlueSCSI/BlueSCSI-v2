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

#ifndef Firmware_HH
#define Firmware_HH

#if __cplusplus >= 201103L
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <string>

namespace SCSI2SD
{

//
// Warning: The Cypress API (used by the Firmware class) uses global data and
// is NOT thread safe.
//
class Firmware
{
public:
	Firmware(const std::string& path);
	~Firmware();

	uint64_t siliconId() const { return mySiliconId; }
	int siliconRev() const { return mySiliconRev; }

	int totalFlashRows() const { return myTotalFlashRows; }

private:
	uint64_t mySiliconId;
	int mySiliconRev;
	int myTotalFlashRows;
};

} // namespace
#endif
