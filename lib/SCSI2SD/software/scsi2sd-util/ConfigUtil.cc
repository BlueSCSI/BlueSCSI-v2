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
#include <sstream>
#include <stdexcept>

#include <string.h>

#include <wx/xml/xml.h>


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
	config.deviceTypeModifier = 0;
	config.sdSectorStart = 0;

	// Default to 2GB. Many systems have trouble with > 2GB disks, and
	// a few start to complain at 1GB.
	config.scsiSectors = 4194303; // 2GB - 1 sector
	config.bytesPerSector = 512;
	config.sectorsPerTrack = 63;
	config.headsPerCylinder = 255;
	memcpy(config.vendor, " codesrc", 8);
	memcpy(config.prodId, "         SCSI2SD", 16);
	memcpy(config.revision, " 4.2", 4);
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

std::string
ConfigUtil::toXML(const TargetConfig& config)
{
	std::stringstream s;

	s <<
		"<SCSITarget id=\"" <<
			static_cast<int>(config.scsiId & CONFIG_TARGET_ID_BITS) << "\">\n" <<

		"	<enabled>" <<
			(config.scsiId & CONFIG_TARGET_ENABLED ? "true" : "false") <<
			"</enabled>\n" <<

		"	<unitAttention>" <<
			(config.flags & CONFIG_ENABLE_UNIT_ATTENTION ? "true" : "false") <<
			"</unitAttention>\n" <<

		"	<parity>" <<
			(config.flags & CONFIG_ENABLE_PARITY ? "true" : "false") <<
			"</parity>\n" <<

		"\n" <<
		"	<!-- ********************************************************\n" <<
		"	Space separated list. Available options:\n" <<
		"	apple\t\tReturns Apple-specific mode pages\n" <<
		"	********************************************************* -->\n" <<
		"	<quirks>" <<
			(config.quirks & CONFIG_QUIRKS_APPLE ? "apple" : "") <<
			"</quirks>\n" <<

		"\n\n" <<
		"	<!-- ********************************************************\n" <<
		"	0x0    Fixed hard drive.\n" <<
		"	0x1    Removable drive.\n" <<
		"	0x2    Optical drive  (ie. CD drive).\n" <<
		"	0x3    1.44MB Floppy Drive.\n" <<
		"	********************************************************* -->\n" <<
		"	<deviceType>0x" <<
				std::hex << static_cast<int>(config.deviceType) <<
			"</deviceType>\n" <<

		"\n\n" <<
		"	<!-- ********************************************************\n" <<
		"	Device type modifier is usually 0x00. Only change this if your\n" <<
		"	OS requires some special value.\n" <<
		"\n" <<
		"	0x4C    Data General Micropolis disk\n" <<
		"	********************************************************* -->\n" <<
		"	<deviceTypeModifier>0x" <<
				std::hex << static_cast<int>(config.deviceTypeModifier) <<
			"</deviceTypeModifier>\n" <<

		"\n\n" <<
		"	<!-- ********************************************************\n" <<
		"	SD card offset, as a sector number (always 512 bytes).\n" <<
		"	********************************************************* -->\n" <<
		"	<sdSectorStart>" << std::dec << config.sdSectorStart << "</sdSectorStart>\n" <<
		"\n\n" <<
		"	<!-- ********************************************************\n" <<
		"	Drive geometry settings.\n" <<
		"	********************************************************* -->\n" <<
		"\n"
		"	<scsiSectors>" << std::dec << config.scsiSectors << "</scsiSectors>\n" <<
		"	<bytesPerSector>" << std::dec << config.bytesPerSector << "</bytesPerSector>\n" <<
		"	<sectorsPerTrack>" << std::dec << config.sectorsPerTrack<< "</sectorsPerTrack>\n" <<
		"	<headsPerCylinder>" << std::dec << config.headsPerCylinder << "</headsPerCylinder>\n" <<
		"\n\n" <<
		"	<!-- ********************************************************\n" <<
		"	Drive identification information. The SCSI2SD doesn't\n" <<
		"	care what these are set to. Use these strings to trick a OS\n" <<
		"	thinking a specific hard drive model is attached.\n" <<
		"	********************************************************* -->\n" <<
		"\n"
		"	<!-- 8 character vendor string -->\n" <<
		"	<!-- For Apple HD SC Setup/Drive Setup, use ' SEAGATE' -->\n" <<
		"	<vendor>" << std::string(config.vendor, 8) << "</vendor>\n" <<
		"\n" <<
		"	<!-- 16 character produce identifier -->\n" <<
		"	<!-- For Apple HD SC Setup/Drive Setup, use ' ST225N' -->\n" <<
		"	<prodId>" << std::string(config.prodId, 16) << "</prodId>\n" <<
		"\n" <<
		"	<!-- 4 character product revision number -->\n" <<
		"	<!-- For Apple HD SC Setup/Drive Setup, use '1.0 ' -->\n" <<
		"	<revision>" << std::string(config.revision, 4) << "</revision>\n" <<
		"\n" <<
		"	<!-- 16 character serial number -->\n" <<
		"	<serial>" << std::string(config.serial, 16) << "</serial>\n" <<
		"</SCSITarget>\n";

	return s.str();
}

static uint64_t parseInt(wxXmlNode* node, uint64_t limit)
{
	std::string str(node->GetNodeContent().mb_str());
	if (str.empty())
	{
		throw std::runtime_error("Empty " + node->GetName());
	}

	std::stringstream s;
	if (str.find("0x") == 0)
	{
		s << std::hex << str.substr(2);
	}
	else
	{
		s << str;
	}

	uint64_t result;
	s >> result;
	if (!s)
	{
		throw std::runtime_error("Invalid value for " + node->GetName());
	}

	if (result > limit)
	{
		std::stringstream msg;
		msg << "Invalid value for " << node->GetName() <<
			" (max=" << limit << ")";
		throw std::runtime_error(msg.str());
	}
	return result;
}

static TargetConfig
parseTarget(wxXmlNode* node)
{
	int id;
	{
		std::stringstream s;
		s << node->GetAttribute("id", "7");
		s >> id;
		if (!s) throw std::runtime_error("Could not parse SCSITarget id attr");
	}
	TargetConfig result = ConfigUtil::Default(id & 0x7);

	wxXmlNode *child = node->GetChildren();
	while (child)
	{
		if (child->GetName() == "enabled")
		{
			std::string s(child->GetNodeContent().mb_str());
			if (s == "true")
			{
				result.scsiId |= CONFIG_TARGET_ENABLED;
			}
			else
			{
				result.scsiId = result.scsiId & ~CONFIG_TARGET_ENABLED;
			}
		}
		if (child->GetName() == "unitAttention")
		{
			std::string s(child->GetNodeContent().mb_str());
			if (s == "true")
			{
				result.flags |= CONFIG_ENABLE_UNIT_ATTENTION;
			}
			else
			{
				result.flags = result.flags & ~CONFIG_ENABLE_UNIT_ATTENTION;
			}
		}
		if (child->GetName() == "parity")
		{
			std::string s(child->GetNodeContent().mb_str());
			if (s == "true")
			{
				result.flags |= CONFIG_ENABLE_PARITY;
			}
			else
			{
				result.flags = result.flags & ~CONFIG_ENABLE_PARITY;
			}
		}
		else if (child->GetName() == "quirks")
		{
			std::stringstream s(std::string(child->GetNodeContent().mb_str()));
			std::string quirk;
			while (s >> quirk)
			{
				if (quirk == "apple")
				{
					result.quirks |= CONFIG_QUIRKS_APPLE;
				}
			}
		}
		else if (child->GetName() == "deviceType")
		{
			result.deviceType = parseInt(child, 0xFF);
		}
		else if (child->GetName() == "deviceTypeModifier")
		{
			result.deviceTypeModifier = parseInt(child, 0xFF);
		}
		else if (child->GetName() == "sdSectorStart")
		{
			result.sdSectorStart = parseInt(child, 0xFFFFFFFF);
		}
		else if (child->GetName() == "scsiSectors")
		{
			result.scsiSectors = parseInt(child, 0xFFFFFFFF);
		}
		else if (child->GetName() == "bytesPerSector")
		{
			result.bytesPerSector = parseInt(child, 8192);
		}
		else if (child->GetName() == "sectorsPerTrack")
		{
			result.sectorsPerTrack = parseInt(child, 255);
		}
		else if (child->GetName() == "headsPerCylinder")
		{
			result.headsPerCylinder = parseInt(child, 255);
		}
		else if (child->GetName() == "vendor")
		{
			std::string s(child->GetNodeContent().mb_str());
			s = s.substr(0, sizeof(result.vendor));
			memset(result.vendor, ' ', sizeof(result.vendor));
			memcpy(result.vendor, s.c_str(), s.size());
		}
		else if (child->GetName() == "prodId")
		{
			std::string s(child->GetNodeContent().mb_str());
			s = s.substr(0, sizeof(result.prodId));
			memset(result.prodId, ' ', sizeof(result.prodId));
			memcpy(result.prodId, s.c_str(), s.size());
		}
		else if (child->GetName() == "revision")
		{
			std::string s(child->GetNodeContent().mb_str());
			s = s.substr(0, sizeof(result.revision));
			memset(result.revision, ' ', sizeof(result.revision));
			memcpy(result.revision, s.c_str(), s.size());
		}
		else if (child->GetName() == "serial")
		{
			std::string s(child->GetNodeContent().mb_str());
			s = s.substr(0, sizeof(result.serial));
			memset(result.serial, ' ', sizeof(result.serial));
			memcpy(result.serial, s.c_str(), s.size());
		}

		child = child->GetNext();
	}
	return result;
}

std::vector<TargetConfig>
ConfigUtil::fromXML(const std::string& filename)
{
	wxXmlDocument doc;
	if (!doc.Load(filename))
	{
		throw std::runtime_error("Could not load XML file");
	}

	// start processing the XML file
	if (doc.GetRoot()->GetName() != "SCSI2SD")
	{
		throw std::runtime_error("Invalid root node, expected <SCSI2SD>");
	}

	std::vector<TargetConfig> result;
	wxXmlNode *child = doc.GetRoot()->GetChildren();
	while (child)
	{
		if (child->GetName() == "SCSITarget")
		{
			result.push_back(parseTarget(child));
		}
		child = child->GetNext();
	}
	return result;
}

