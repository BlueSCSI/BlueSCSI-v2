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
//
// This work incorporates work from the following
//  Copyright (c) 2023 joshua stein <jcs@jcs.org>

#ifndef S2S_Config_H
#define S2S_Config_H

#include "scsi2sd.h"
#include "log.h"

void s2s_configInit(S2S_BoardCfg* config);
void s2s_debugInit(void);
void s2s_configPoll(void);
void s2s_configSave(int scsiId, uint16_t byesPerSector);

const S2S_TargetCfg* s2s_getConfigByIndex(int index);
const S2S_TargetCfg* s2s_getConfigById(int scsiId);

#endif
