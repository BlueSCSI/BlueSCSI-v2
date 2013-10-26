/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
********************************************************************************/

#include <string.h>
#include "cybtldr_parse.h"
#include "cybtldr_command.h"
#include "cybtldr_api.h"
#include "cybtldr_api2.h"

unsigned char g_abort;

int CyBtldr_RunAction(CyBtldr_Action action, const char* file, CyBtldr_CommunicationsData* comm, CyBtldr_ProgressUpdate* update)
{
    const unsigned long BL_VER_SUPPORT_VERIFY = 0x010214; /* Support for full flash verify added in v2.20 of cy_boot */
    const unsigned char INVALID_APP = 0xFF;

    unsigned long blVer = 0;
    unsigned long siliconId = 0;
    unsigned short rowNum = 0;
    unsigned short bufSize = 0;
    unsigned char siliconRev = 0;
    unsigned char chksumtype = SUM_CHECKSUM;
    unsigned char checksum = 0;
    unsigned char checksum2 = 0;
    unsigned char arrayId = 0;
    unsigned char appId;
    unsigned char isValid;
    unsigned char isActive;
    unsigned char buffer[MAX_BUFFER_SIZE];
    char line[MAX_BUFFER_SIZE];
    unsigned int lineLen;
    char * app;
    int err;
	
    g_abort = 0;

    err = CyBtldr_OpenDataFile(file);
    if (CYRET_SUCCESS == err)
    {
        err = CyBtldr_ReadLine(&lineLen, line);
        if (CYRET_SUCCESS == err)
            err = CyBtldr_ParseHeader(lineLen, line, &siliconId, &siliconRev, &chksumtype);

        if (CYRET_SUCCESS == err)
        {
            CyBtldr_SetCheckSumType(chksumtype);
            err = CyBtldr_StartBootloadOperation(comm, siliconId, siliconRev, &blVer);
        }

        app = strrchr(file, '_');
        appId = (app != NULL && '.' == app[2])
            ? (CyBtldr_FromHex(app[1]) - 1)
            : INVALID_APP;

        if (CYRET_SUCCESS == err && INVALID_APP != appId)
        {
			/* NB: This block of code will still run for single app if file */ 
			/* name format follows same as multi app (e.g. myfile_1.cyacd)  */

			/* This will return error if bootloader is for single app */
            err = CyBtldr_GetApplicationStatus(appId, &isValid, &isActive);

            /* Active app can be verified, but not programmed or erased */
            if (CYRET_SUCCESS == err && VERIFY != action && isActive)
			{
				/* This is multi app */
				err = CYRET_ERR_ACTIVE;
			}
			else if (CYBTLDR_STAT_ERR_CMD == (err ^ (int)CYRET_ERR_BTLDR_MASK))
			{
				/* Single app - restore previous CYRET_SUCCESS */
				err = CYRET_SUCCESS;
			}
        }

        if (CYRET_SUCCESS == err)
        {
            while (CYRET_SUCCESS == err)
            {
                if (g_abort)
                {
                    err = CYRET_ABORT;
                    break;
                }

                err = CyBtldr_ReadLine(&lineLen, line);
                if (CYRET_SUCCESS == err)
                    err = CyBtldr_ParseRowData(lineLen, line, &arrayId, &rowNum, buffer, &bufSize, &checksum);
                if (CYRET_SUCCESS == err)
                {
                    switch (action)
                    {
                        case ERASE:
                            err = CyBtldr_EraseRow(arrayId, rowNum);
                            break;
                        case PROGRAM:
                            err = CyBtldr_ProgramRow(arrayId, rowNum, buffer, bufSize);
                            if (CYRET_SUCCESS != err)
                                break;
                            /* Continue on to verify the row that was programmed */
                        case VERIFY:
                            checksum2 = (unsigned char)(checksum + arrayId + rowNum + (rowNum >> 8) + bufSize + (bufSize >> 8));
                            err = CyBtldr_VerifyRow(arrayId, rowNum, checksum2);
                            break;
                    }
                    if (CYRET_SUCCESS == err && NULL != update)
                        update(arrayId, rowNum);
                }
                else if (CYRET_ERR_EOF == err)
                {
                    err = CYRET_SUCCESS;
                    break;
                }
            }

            if (CYRET_SUCCESS == err)
            {
                /* Set the active application to what was just programmed */
                if (PROGRAM == action && INVALID_APP != appId)
                {
                    err = CyBtldr_GetApplicationStatus(appId, &isValid, &isActive);

                    if (CYRET_SUCCESS == err)
                    {
                        /* If valid set the active application to what was just programmed */
						/* This is multi app */
                        err = (0 == isValid)
                            ? CyBtldr_SetApplicationStatus(appId)
                            : CYRET_ERR_CHECKSUM;
                    }
					else if (CYBTLDR_STAT_ERR_CMD == (err ^ (int)CYRET_ERR_BTLDR_MASK))
					{
						/* Single app - restore previous CYRET_SUCCESS */
						err = CYRET_SUCCESS;
					}
                }

                /* Verify that the entire application is valid */
                else if ((PROGRAM == action || VERIFY == action) && (blVer >= BL_VER_SUPPORT_VERIFY))
                    err = CyBtldr_VerifyApplication();
            }

            CyBtldr_EndBootloadOperation();
        }
        else if (CYRET_ERR_COMM_MASK != (CYRET_ERR_COMM_MASK & err))
            CyBtldr_EndBootloadOperation();

        CyBtldr_CloseDataFile();
    }

    return err;
}

int CyBtldr_Program(const char* file, CyBtldr_CommunicationsData* comm, CyBtldr_ProgressUpdate* update)
{
    return CyBtldr_RunAction(PROGRAM, file, comm, update);
}

int CyBtldr_Erase(const char* file, CyBtldr_CommunicationsData* comm, CyBtldr_ProgressUpdate* update)
{
    return CyBtldr_RunAction(ERASE, file, comm, update);
}

int CyBtldr_Verify(const char* file, CyBtldr_CommunicationsData* comm, CyBtldr_ProgressUpdate* update)
{
    return CyBtldr_RunAction(VERIFY, file, comm, update);
}

int CyBtldr_Abort(void)
{
    g_abort = 1;
    return CYRET_SUCCESS;
}
