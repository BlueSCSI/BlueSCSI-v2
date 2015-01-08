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

#include "ConfigUtil.hh"

#include <limits>

#include <string.h>


using namespace SCSI2SD;

namespace
{
	// Endian conversion routines.
	// The Cortex-M3 inside the Cypress PSoC 5LP is a
	// little-endian device.

	bool isHostLE()
	{
		union
		{
			int i;
			char c[sizeof(int)];
		} x;
		x.i = 1;
		return (x.c[0] == 1);
	}

	uint16_t toLE16(uint16_t in)
	{
		if (isHostLE())
		{
			return in;
		}
		else
		{
			return (in >> 8) | (in << 8);
		}
	}
	uint16_t fromLE16(uint16_t in)
	{
		return toLE16(in);
	}

	uint32_t toLE32(uint32_t in)
	{
		if (isHostLE())
		{
			return in;
		}
		else
		{
			return (in >> 24) |
				((in >> 8) & 0xff00) |
				((in << 8) & 0xff0000) |
				(in << 24);
		}
	}
	uint32_t fromLE32(uint32_t in)
	{
		return toLE32(in);
	}

}

TargetConfig
ConfigUtil::Default(size_t targetIdx)
{
	TargetConfig config;
	memset(&config, 0, sizeof(config));

	config.scsiId = targetIdx;
	if (targetIdx == 0)
	{
		config.scsiId = config.scsiId | CONFIG_TARGET_ENABLED;
	}
	config.deviceType = CONFIG_FIXED;

	// Default to maximum fail-safe options.
	config.flags = 0;// CONFIG_ENABLE_PARITY | CONFIG_ENABLE_UNIT_ATTENTION;
	config.pad0 = 0;
	config.sdSectorStart = 0;

	// Default to 2GB. Many systems have trouble with > 2GB disks, and
	// a few start to complain at 1GB.
	config.scsiSectors = 4194303; // 2GB - 1 sector
	config.bytesPerSector = 512;
	config.sectorsPerTrack = 63;
	config.headsPerCylinder = 255;
	memcpy(config.vendor, " codesrc", 8);
	memcpy(config.prodId, "         SCSI2SD", 16);
	memcpy(config.revision, " 4.0", 4);
	memcpy(config.serial, "1234567812345678", 16);

	// Reserved fields, already set to 0
	// config.reserved

	// not supported yet.
	// config.vpd

	return config;
}


TargetConfig
ConfigUtil::fromBytes(const uint8_t* data)
{
	TargetConfig result;
	memcpy(&result, data, sizeof(TargetConfig));
	result.sdSectorStart = toLE32(result.sdSectorStart);
	result.scsiSectors = toLE32(result.scsiSectors);
	result.bytesPerSector = toLE16(result.bytesPerSector);
	result.sectorsPerTrack = toLE16(result.sectorsPerTrack);
	result.headsPerCylinder = toLE16(result.headsPerCylinder);
	return result;
}


std::vector<uint8_t>
ConfigUtil::toBytes(const TargetConfig& _config)
{
	TargetConfig config(_config);
	config.sdSectorStart = fromLE32(config.sdSectorStart);
	config.scsiSectors = fromLE32(config.scsiSectors);
	config.bytesPerSector = fromLE16(config.bytesPerSector);
	config.sectorsPerTrack = fromLE16(config.sectorsPerTrack);
	config.headsPerCylinder = fromLE16(config.headsPerCylinder);

	const uint8_t* begin = reinterpret_cast<const uint8_t*>(&config);
	return std::vector<uint8_t>(begin, begin + sizeof(config));
}

