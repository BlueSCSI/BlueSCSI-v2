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
#include <project.h>

static void resetSCSI()
{
	CyPins_ClearPin(SCSI_Out_IO_raw);	
	CyPins_ClearPin(SCSI_Out_ATN);
	CyPins_ClearPin(SCSI_Out_BSY);
	CyPins_ClearPin(SCSI_Out_ACK);
	CyPins_ClearPin(SCSI_Out_RST);
	CyPins_ClearPin(SCSI_Out_SEL);
	CyPins_ClearPin(SCSI_Out_REQ);
	CyPins_ClearPin(SCSI_Out_MSG);
	CyPins_ClearPin(SCSI_Out_CD);
	CyPins_ClearPin(SCSI_Out_DBx_DB0);
	CyPins_ClearPin(SCSI_Out_DBx_DB1);
	CyPins_ClearPin(SCSI_Out_DBx_DB2);
	CyPins_ClearPin(SCSI_Out_DBx_DB3);
	CyPins_ClearPin(SCSI_Out_DBx_DB4);
	CyPins_ClearPin(SCSI_Out_DBx_DB5);
	CyPins_ClearPin(SCSI_Out_DBx_DB6);
	CyPins_ClearPin(SCSI_Out_DBx_DB7);
	CyPins_ClearPin(SCSI_Out_DBP_raw);
}

void main()
{
    resetSCSI();
	
	// The call to the bootloader should not return
    CyBtldr_Start();

    /* CyGlobalIntEnable; */ /* Uncomment this line to enable global interrupts. */
    for(;;)
    {
        /* Place your application code here. */
    }
}

