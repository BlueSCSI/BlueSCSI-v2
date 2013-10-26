/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
********************************************************************************/

#include "cybtldr_command.h"


/* Variable used to store the currently selected packet checksum type */
CyBtldr_ChecksumType CyBtldr_Checksum = SUM_CHECKSUM;

unsigned short CyBtldr_ComputeChecksum(unsigned char* buf, unsigned long size)
{
    if (CyBtldr_Checksum == CRC_CHECKSUM)
    {
	    unsigned short crc = 0xffff;
	    unsigned short tmp;
	    int i;
           
	    if (size == 0)
		    return (~crc);
           
	    do
	    {
		    for (i = 0, tmp = 0x00ff & *buf++; i < 8; i++, tmp >>= 1)
		    {
			    if ((crc & 0x0001) ^ (tmp & 0x0001))
				    crc = (crc >> 1) ^ 0x8408;
			    else
			        crc >>= 1;
		    }
	    }
	    while (--size);

	    crc = ~crc;
        tmp = crc;
	    crc = (crc << 8) | (tmp >> 8 & 0xFF);
           
	    return crc;
    }
    else /* SUM_CHECKSUM */
    {
        unsigned short sum = 0;
	    while (size-- > 0)
		    sum += *buf++;
    	
	    return (1 + ~sum);
    }
}

void CyBtldr_SetCheckSumType(CyBtldr_ChecksumType chksumType)
{
    CyBtldr_Checksum = chksumType;
}

int CyBtldr_ParseDefaultCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status)
{
    int err = CYRET_SUCCESS;
    if (cmdSize != BASE_CMD_SIZE)
        err = CYRET_ERR_LENGTH;
    else if (cmdBuf[1] != CYRET_SUCCESS)
        err = CYRET_ERR_BTLDR_MASK | (*status = cmdBuf[1]);
    else if (cmdBuf[0] != CMD_START || cmdBuf[2] != 0 || cmdBuf[3] != 0 || cmdBuf[6] != CMD_STOP)
        err = CYRET_ERR_DATA;
    else
        *status = cmdBuf[1];

    return err;
}

int CyBtldr_CreateEnterBootLoaderCmd(unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long RESULT_DATA_SIZE = 8;
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    *cmdSize = BASE_CMD_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_ENTER_BOOTLOADER;
    cmdBuf[2] = 0;
    cmdBuf[3] = 0;
    checksum = CyBtldr_ComputeChecksum(cmdBuf, BASE_CMD_SIZE - 3);
    cmdBuf[4] = (unsigned char)checksum;
    cmdBuf[5] = (unsigned char)(checksum >> 8);
    cmdBuf[6] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseEnterBootLoaderCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned long* siliconId, unsigned char* siliconRev, unsigned long* blVersion, unsigned char* status)
{
    const unsigned long RESULT_DATA_SIZE = 8;
    const unsigned long RESULT_SIZE = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    int err = CYRET_SUCCESS;

    if (cmdSize != RESULT_SIZE)
        err = CYRET_ERR_LENGTH;
    else if (cmdBuf[1] != CYRET_SUCCESS)
        err = CYRET_ERR_BTLDR_MASK | (*status = cmdBuf[1]);
    else if (cmdBuf[0] != CMD_START || cmdBuf[2] != RESULT_DATA_SIZE || cmdBuf[3] != (RESULT_DATA_SIZE >> 8) || cmdBuf[RESULT_SIZE - 1] != CMD_STOP)
        err = CYRET_ERR_DATA;
    else
    {
        *siliconId = (cmdBuf[7] << 24) | (cmdBuf[6] << 16) | (cmdBuf[5] << 8) | cmdBuf[4];
        *siliconRev = cmdBuf[8];
        *blVersion = (cmdBuf[11] << 16) | (cmdBuf[10] << 8) | cmdBuf[9];
        *status = cmdBuf[1];
    }

    return err;
}

int CyBtldr_CreateExitBootLoaderCmd(unsigned char resetType, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long COMMAND_DATA_SIZE = 1;
    const unsigned int COMMAND_SIZE = BASE_CMD_SIZE + COMMAND_DATA_SIZE;
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE;
    *cmdSize = COMMAND_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_EXIT_BOOTLOADER;
    cmdBuf[2] = (unsigned char)COMMAND_DATA_SIZE;
    cmdBuf[3] = (unsigned char)(COMMAND_DATA_SIZE >> 8);
    cmdBuf[4] = resetType;
    checksum = CyBtldr_ComputeChecksum(cmdBuf, COMMAND_SIZE - 3);
    cmdBuf[5] = (unsigned char)checksum;
    cmdBuf[6] = (unsigned char)(checksum >> 8);
    cmdBuf[7] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_CreateProgramRowCmd(unsigned char arrayId, unsigned short rowNum, unsigned char* buf, unsigned short size, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long COMMAND_DATA_SIZE = 3;
    unsigned int checksum;
    unsigned long i;

    *resSize = BASE_CMD_SIZE;
    *cmdSize = BASE_CMD_SIZE + COMMAND_DATA_SIZE + size;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_PROGRAM_ROW;
    cmdBuf[2] = (unsigned char)(size + COMMAND_DATA_SIZE);
    cmdBuf[3] = (unsigned char)((size + COMMAND_DATA_SIZE) >> 8);
    cmdBuf[4] = arrayId;
    cmdBuf[5] = (unsigned char)rowNum;
    cmdBuf[6] = (unsigned char)(rowNum >> 8);
    for (i = 0; i < size; i++)
        cmdBuf[i + 7] = buf[i];
    checksum = CyBtldr_ComputeChecksum(cmdBuf, (*cmdSize) - 3);
    cmdBuf[*cmdSize - 3] = (unsigned char)checksum;
    cmdBuf[*cmdSize - 2] = (unsigned char)(checksum >> 8);
    cmdBuf[*cmdSize - 1] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseProgramRowCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status)
{
    return CyBtldr_ParseDefaultCmdResult(cmdBuf, cmdSize, status);
}

int CyBtldr_CreateVerifyRowCmd(unsigned char arrayId, unsigned short rowNum, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long RESULT_DATA_SIZE = 1;
    const unsigned long COMMAND_DATA_SIZE = 3;
    const unsigned int COMMAND_SIZE = BASE_CMD_SIZE + COMMAND_DATA_SIZE;
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    *cmdSize = COMMAND_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_VERIFY_ROW;
    cmdBuf[2] = (unsigned char)COMMAND_DATA_SIZE;
    cmdBuf[3] = (unsigned char)(COMMAND_DATA_SIZE >> 8);
    cmdBuf[4] = arrayId;
    cmdBuf[5] = (unsigned char)rowNum;
    cmdBuf[6] = (unsigned char)(rowNum >> 8);
    checksum = CyBtldr_ComputeChecksum(cmdBuf, COMMAND_SIZE - 3);
    cmdBuf[7] = (unsigned char)checksum;
    cmdBuf[8] = (unsigned char)(checksum >> 8);
    cmdBuf[9] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseVerifyRowCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* checksum, unsigned char* status)
{
    const unsigned long RESULT_DATA_SIZE = 1;
    const unsigned long RESULT_SIZE = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    int err = CYRET_SUCCESS;

    if (cmdSize != RESULT_SIZE)
        err = CYRET_ERR_LENGTH;
    else if (cmdBuf[1] != CYRET_SUCCESS)
        err = CYRET_ERR_BTLDR_MASK | (*status = cmdBuf[1]);
    else if (cmdBuf[0] != CMD_START || cmdBuf[2] != RESULT_DATA_SIZE || cmdBuf[3] != (RESULT_DATA_SIZE >> 8) || cmdBuf[RESULT_SIZE - 1] != CMD_STOP)
        err = CYRET_ERR_DATA;
    else
    {
        *checksum = cmdBuf[4];
        *status = cmdBuf[1];
    }

    return err;
}

int CyBtldr_CreateEraseRowCmd(unsigned char arrayId, unsigned short rowNum, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long COMMAND_DATA_SIZE = 3;
    const unsigned int COMMAND_SIZE = BASE_CMD_SIZE + COMMAND_DATA_SIZE;
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE;
    *cmdSize = COMMAND_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_ERASE_ROW;
    cmdBuf[2] = (unsigned char)COMMAND_DATA_SIZE;
    cmdBuf[3] = (unsigned char)(COMMAND_DATA_SIZE >> 8);
    cmdBuf[4] = arrayId;
    cmdBuf[5] = (unsigned char)rowNum;
    cmdBuf[6] = (unsigned char)(rowNum >> 8);
    checksum = CyBtldr_ComputeChecksum(cmdBuf, COMMAND_SIZE - 3);
    cmdBuf[7] = (unsigned char)checksum;
    cmdBuf[8] = (unsigned char)(checksum >> 8);
    cmdBuf[9] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseEraseRowCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status)
{
    return CyBtldr_ParseDefaultCmdResult(cmdBuf, cmdSize, status);
}

int CyBtldr_CreateVerifyChecksumCmd(unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long RESULT_DATA_SIZE = 1;
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    *cmdSize = BASE_CMD_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_VERIFY_CHECKSUM;
    cmdBuf[2] = 0;
    cmdBuf[3] = 0;
    checksum = CyBtldr_ComputeChecksum(cmdBuf, BASE_CMD_SIZE - 3);
    cmdBuf[4] = (unsigned char)checksum;
    cmdBuf[5] = (unsigned char)(checksum >> 8);
    cmdBuf[6] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseVerifyChecksumCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* checksumValid, unsigned char* status)
{
    const unsigned long RESULT_DATA_SIZE = 1;
    const unsigned long RESULT_SIZE = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    int err = CYRET_SUCCESS;

    if (cmdSize != RESULT_SIZE)
        err = CYRET_ERR_LENGTH;
    else if (cmdBuf[1] != CYRET_SUCCESS)
        err = CYRET_ERR_BTLDR_MASK | (*status = cmdBuf[1]);
    else if (cmdBuf[0] != CMD_START || cmdBuf[2] != RESULT_DATA_SIZE || cmdBuf[3] != (RESULT_DATA_SIZE >> 8) || cmdBuf[RESULT_SIZE - 1] != CMD_STOP)
        err = CYRET_ERR_DATA;
    else
    {
        *checksumValid = cmdBuf[4];
        *status = cmdBuf[1];
    }

    return err;
}

int CyBtldr_CreateGetFlashSizeCmd(unsigned char arrayId, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long RESULT_DATA_SIZE = 4;
    const unsigned long COMMAND_DATA_SIZE = 1;
    const unsigned int COMMAND_SIZE = BASE_CMD_SIZE + COMMAND_DATA_SIZE;
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    *cmdSize = COMMAND_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_GET_FLASH_SIZE;
    cmdBuf[2] = (unsigned char)COMMAND_DATA_SIZE;
    cmdBuf[3] = (unsigned char)(COMMAND_DATA_SIZE >> 8);
    cmdBuf[4] = arrayId;
    checksum = CyBtldr_ComputeChecksum(cmdBuf, COMMAND_SIZE - 3);
    cmdBuf[5] = (unsigned char)checksum;
    cmdBuf[6] = (unsigned char)(checksum >> 8);
    cmdBuf[7] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseGetFlashSizeCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned short* startRow, unsigned short* endRow, unsigned char* status)
{
    const unsigned long RESULT_DATA_SIZE = 4;
    const unsigned long RESULT_SIZE = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    int err = CYRET_SUCCESS;

    if (cmdSize != RESULT_SIZE)
        err = CYRET_ERR_LENGTH;
    else if (cmdBuf[1] != CYRET_SUCCESS)
        err = CYRET_ERR_BTLDR_MASK | (*status = cmdBuf[1]);
    else if (cmdBuf[0] != CMD_START || cmdBuf[2] != RESULT_DATA_SIZE || cmdBuf[3] != (RESULT_DATA_SIZE >> 8) || cmdBuf[RESULT_SIZE - 1] != CMD_STOP)
        err = CYRET_ERR_DATA;
    else
    {
        *startRow = (cmdBuf[5] << 8) | cmdBuf[4];
        *endRow = (cmdBuf[7] << 8) | cmdBuf[6];
        *status = cmdBuf[1];
    }

    return err;
}

int CyBtldr_CreateSendDataCmd(unsigned char* buf, unsigned short size, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    unsigned short checksum;
    unsigned long i;

    *resSize = BASE_CMD_SIZE;
    *cmdSize = size + BASE_CMD_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_SEND_DATA;
    cmdBuf[2] = (unsigned char)size;
    cmdBuf[3] = (unsigned char)(size >> 8);
    for (i = 0; i < size; i++)
        cmdBuf[i + 4] = buf[i];
    checksum = CyBtldr_ComputeChecksum(cmdBuf, (*cmdSize) - 3);
    cmdBuf[(*cmdSize) - 3] = (unsigned char)checksum;
    cmdBuf[(*cmdSize) - 2] = (unsigned char)(checksum >> 8);
    cmdBuf[(*cmdSize) - 1] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseSendDataCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status)
{
    return CyBtldr_ParseDefaultCmdResult(cmdBuf, cmdSize, status);
}

int CyBtldr_CreateSyncBootLoaderCmd(unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE;
    *cmdSize = BASE_CMD_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_SYNC;
    cmdBuf[2] = 0;
    cmdBuf[3] = 0;
    checksum = CyBtldr_ComputeChecksum(cmdBuf, BASE_CMD_SIZE - 3);
    cmdBuf[4] = (unsigned char)checksum;
    cmdBuf[5] = (unsigned char)(checksum >> 8);
    cmdBuf[6] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_CreateGetAppStatusCmd(unsigned char appId, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long RESULT_DATA_SIZE = 2;
    const unsigned long COMMAND_DATA_SIZE = 1;
    const unsigned int COMMAND_SIZE = BASE_CMD_SIZE + COMMAND_DATA_SIZE;
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    *cmdSize = COMMAND_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_GET_APP_STATUS;
    cmdBuf[2] = (unsigned char)COMMAND_DATA_SIZE;
    cmdBuf[3] = (unsigned char)(COMMAND_DATA_SIZE >> 8);
    cmdBuf[4] = appId;
    checksum = CyBtldr_ComputeChecksum(cmdBuf, COMMAND_SIZE - 3);
    cmdBuf[5] = (unsigned char)checksum;
    cmdBuf[6] = (unsigned char)(checksum >> 8);
    cmdBuf[7] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseGetAppStatusCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* isValid, unsigned char* isActive, unsigned char* status)
{
    const unsigned long RESULT_DATA_SIZE = 2;
    const unsigned long RESULT_SIZE = BASE_CMD_SIZE + RESULT_DATA_SIZE;
    int err = CYRET_SUCCESS;

    if (cmdSize != RESULT_SIZE)
        err = CYRET_ERR_LENGTH;
    else if (cmdBuf[1] != CYRET_SUCCESS)
        err = CYRET_ERR_BTLDR_MASK | (*status = cmdBuf[1]);
    else if (cmdBuf[0] != CMD_START || cmdBuf[2] != RESULT_DATA_SIZE || cmdBuf[3] != (RESULT_DATA_SIZE >> 8) || cmdBuf[RESULT_SIZE - 1] != CMD_STOP)
        err = CYRET_ERR_DATA;
    else
    {
        *isValid = cmdBuf[4];
        *isActive = cmdBuf[5];
        *status = cmdBuf[1];
    }

    return err;
}

int CyBtldr_CreateSetActiveAppCmd(unsigned char appId, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize)
{
    const unsigned long COMMAND_DATA_SIZE = 1;
    const unsigned int COMMAND_SIZE = BASE_CMD_SIZE + COMMAND_DATA_SIZE;
    unsigned short checksum;

    *resSize = BASE_CMD_SIZE;
    *cmdSize = COMMAND_SIZE;
    cmdBuf[0] = CMD_START;
    cmdBuf[1] = CMD_SET_ACTIVE_APP;
    cmdBuf[2] = (unsigned char)COMMAND_DATA_SIZE;
    cmdBuf[3] = (unsigned char)(COMMAND_DATA_SIZE >> 8);
    cmdBuf[4] = appId;
    checksum = CyBtldr_ComputeChecksum(cmdBuf, COMMAND_SIZE - 3);
    cmdBuf[5] = (unsigned char)checksum;
    cmdBuf[6] = (unsigned char)(checksum >> 8);
    cmdBuf[7] = CMD_STOP;

    return CYRET_SUCCESS;
}

int CyBtldr_ParseSetActiveAppCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status)
{
    return CyBtldr_ParseDefaultCmdResult(cmdBuf, cmdSize, status);
}
