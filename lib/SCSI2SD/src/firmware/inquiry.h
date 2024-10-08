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
#ifndef S2S_INQUIRY_H
#define S2S_INQUIRY_H

#define INQUIRY_STD_RESPONSE_LEN_OFFSET 4
#define INQUIRY_STD_RESPONSE_LEN 0x1f // 31

void s2s_scsiInquiry(void);
uint32_t s2s_getStandardInquiry(const S2S_TargetCfg* cfg, uint8_t* out, uint32_t maxlen);
uint8_t getDeviceTypeQualifier(void);


#endif
