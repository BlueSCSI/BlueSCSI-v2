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
#ifndef ConfigUtil_hh
#define ConfigUtil_hh

#include "scsi2sd.h"

#include <cstddef>
#include <string>
#include <vector>
#include <utility>

namespace SCSI2SD
{
	class ConfigUtil
	{
	public:

		static BoardConfig DefaultBoardConfig();
		static TargetConfig Default(size_t targetIdx);

		static TargetConfig fromBytes(const uint8_t* data);
		static std::vector<uint8_t> toBytes(const TargetConfig& config);

		static BoardConfig boardConfigFromBytes(const uint8_t* data);
		static std::vector<uint8_t> boardConfigToBytes(const BoardConfig& config);

		static std::string toXML(const TargetConfig& config);
		static std::string toXML(const BoardConfig& config);
		static std::pair<BoardConfig, std::vector<TargetConfig>> fromXML(const std::string& filename);
	};
}

#endif

