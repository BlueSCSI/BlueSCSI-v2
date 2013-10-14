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

#include "loopback.h"
#include "scsi.h"
#include "device.h"

// Return true if all inputs are un-asserted (1)
// Note that CyPins returns non-zero if pin is active. It does NOT
// necessarily return 1.
static int test_initial_inputs(void)
{
	uint8 dbx = SCSI_In_DBx_Read();
	int result =
		(dbx == 0xFF) &&
		CyPins_ReadPin(SCSI_In_DBP) &&
		CyPins_ReadPin(SCSI_ATN_INT) &&
		CyPins_ReadPin(SCSI_In_BSY)	&&
		CyPins_ReadPin(SCSI_In_ACK) &&
		CyPins_ReadPin(SCSI_RST_INT) &&
		CyPins_ReadPin(SCSI_In_MSG) &&
		CyPins_ReadPin(SCSI_In_SEL) &&
		CyPins_ReadPin(SCSI_In_CD) &&
		CyPins_ReadPin(SCSI_In_REQ) &&
		CyPins_ReadPin(SCSI_In_IO);

	return result;
}

static int test_data_lines(void)
{
	int result = 1;
	int i;
	for (i = 0; i < 8; ++i)
	{
		// We write using Active High
		SCSI_Out_DBx_Write(1 << i);
		CyDelay(1); // ms
		
		// And expect an Active Low response.
		uint8 dbx = SCSI_In_DBx_Read();
		result = result && (dbx == (0xFF ^ (1 << i)));
	}
	SCSI_Out_DBx_Write(0);
	return result;
}

static int test_data_10MHz(void)
{
	// 10MHz = 100ns period.
	// We'll try and go high -> low -> high in 100ns.
	// At 66MHz, 50ns ~= 3 cycles.
	
	int result = 1;
	int i;
	for (i = 0; i < 100; ++i)
	{
		// We write using Active High
		SCSI_Out_DBx_Write(0xFF);
		CyDelayCycles(3);
		// And expect an Active Low response.
		uint8 dbx = SCSI_In_DBx_Read();
		result = result && (dbx == 0);
		
		// We write using Active High
		SCSI_Out_DBx_Write(0);
		CyDelayCycles(3);
		// And expect an Active Low response.
		dbx = SCSI_In_DBx_Read();
		result = result && (dbx == 0xFF);
	}
	SCSI_Out_DBx_Write(0);
	return result;
}

static int test_ATN_interrupt(void)
{
	int result = 1;
	int i;
	
	scsiDev.atnFlag = 0;
	for (i = 0; i < 100 && result; ++i)
	{
		// We write using Active High
		CyPins_SetPin(SCSI_Out_ATN);
		CyDelayCycles(2);
		result &= scsiDev.atnFlag == 1;
		scsiDev.atnFlag = 0;
		CyPins_ClearPin(SCSI_Out_ATN);
		result &= scsiDev.atnFlag == 0;
	}
	return result;
}

static void test_error(void)
{
	// Toggle LED.
	while (1)
	{
		LED1_Write(0);
		CyDelay(250); // ms
		LED1_Write(1);
		CyDelay(250); // ms
	}
}

static void test_success(void)
{
	// Toggle LED.
	while (1)
	{
		LED1_Write(0);
		CyDelay(1000); // ms
		LED1_Write(1);
		CyDelay(1000); // ms
	}
}
void scsi2sd_test_loopback(void)
{
	if (!test_initial_inputs() ||
		!test_data_lines() ||
		!test_data_10MHz() ||
		!test_ATN_interrupt())
	{
		test_error();
	}
	else
	{
		test_success();
	}
}