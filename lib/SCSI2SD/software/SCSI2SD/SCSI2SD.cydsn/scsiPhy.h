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
#ifndef SCSIPHY_H
#define SCSIPHY_H

#define SCSI_SetPin(pin) \
	CyPins_SetPin((pin));

#define SCSI_ClearPin(pin) \
	CyPins_ClearPin((pin));

// Active low: we interpret a 0 as "true", and non-zero as "false"
#define SCSI_ReadPin(pin) \
	(CyPins_ReadPin((pin)) == 0)

// Contains the odd-parity flag for a given 8-bit value.
extern const uint8 Lookup_OddParity[256];

uint8 scsiRead(void);
void scsiWrite(uint8 value);

// Returns true if the ATN flag becomes set, indicating a parity error.
int scsiWriteMsg(uint8 msg);

void scsiEnterPhase(int phase);

#endif
