//	Copyright (C) 2016 Michael McMaster <michael@codesrc.com>
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

#include <libusb.h>

#include <string>

#ifndef SCSI2SD_DFU_H
#define SCSI2SD_DFU_H

namespace SCSI2SD
{

class Dfu
{
public:
	Dfu();

	~Dfu();

	bool hasDevice();

private:
	enum { Vendor = 0x0483, Product = 0xdf11 };

	libusb_context* m_usbctx;

	std::string m_filename;
};

} // namespace

#endif

