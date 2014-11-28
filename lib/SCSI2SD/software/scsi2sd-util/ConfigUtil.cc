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
#include <arpa/inet.h>


using namespace SCSI2SD;

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
	config.flags = CONFIG_ENABLE_PARITY | CONFIG_ENABLE_UNIT_ATTENTION;
	config.pad0 = 0;
	config.sdSectorStart = 0;
	config.scsiSectors = std::numeric_limits<uint32_t>::max();
	config.bytesPerSector = 512;
	config.sectorsPerTrack = 63;
	config.headsPerCylinder = 255;
	memcpy(config.vendor, " codesrc", 8);
	memcpy(config.prodId, "         SCSI2SD", 16);
	memcpy(config.revision, " 3.6", 4);
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
	result.sdSectorStart = ntohl(result.sdSectorStart);
	result.scsiSectors = ntohl(result.scsiSectors);
	result.bytesPerSector = ntohs(result.bytesPerSector);
	result.sectorsPerTrack = ntohs(result.sectorsPerTrack);
	result.headsPerCylinder = ntohs(result.headsPerCylinder);
	return result;
}


std::vector<uint8_t>
ConfigUtil::toBytes(const TargetConfig& _config)
{
	TargetConfig config(_config);
	config.sdSectorStart = htonl(config.sdSectorStart);
	config.scsiSectors = htonl(config.scsiSectors);
	config.bytesPerSector = htons(config.bytesPerSector);
	config.sectorsPerTrack = htons(config.sectorsPerTrack);
	config.headsPerCylinder = htons(config.headsPerCylinder);

	const uint8_t* begin = reinterpret_cast<const uint8_t*>(&config);
	return std::vector<uint8_t>(begin, begin + sizeof(config));
}

