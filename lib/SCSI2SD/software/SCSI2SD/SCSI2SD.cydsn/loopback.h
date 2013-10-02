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

#ifndef SCSI2SD_LOOPBACK_H
#define SCSI2SD_LOOPBACK_H

// Loopback test
// Ensure we can read-back whatever we write to the SCSI bus.
// This testing should be performed in isolation, with the
// terminator jumper and terminator power jumper installed.
// ie. do not connect a SCSI cable and plug us in to another
// device.
void scsi2sd_test_loopback(void);


#endif // SCSI2SD_POST_H
