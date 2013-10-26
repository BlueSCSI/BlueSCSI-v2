/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
********************************************************************************/

#include "cybtldr_command.h"
#include "cybtldr_api.h"

/* The highest number of flash array for any device */
#define MAX_FLASH_ARRAYS    4
/* The default value if a flash array has not yet received data */
#define NO_FLASH_ARRAY_DATA 0

unsigned long g_validRows[MAX_FLASH_ARRAYS];
static CyBtldr_CommunicationsData* g_comm;

int CyBtldr_TransferData(unsigned char* inBuf, int inSize, unsigned char* outBuf, int outSize)
{
    int err = g_comm->WriteData(inBuf, inSize);

    if (CYRET_SUCCESS == err)
        err = g_comm->ReadData(outBuf, outSize);

    if (CYRET_SUCCESS != err)
        err |= CYRET_ERR_COMM_MASK;

    return err;
}

int CyBtldr_ValidateRow(unsigned char arrayId, unsigned short rowNum)
{
    unsigned long inSize;
    unsigned long outSize;
    unsigned short minRow = 0;
    unsigned short maxRow = 0;
    unsigned char inBuf[MAX_COMMAND_SIZE];
    unsigned char outBuf[MAX_COMMAND_SIZE];
    unsigned char status = CYRET_SUCCESS;
    int err = CYRET_SUCCESS;

    if (arrayId < MAX_FLASH_ARRAYS)
    {
        if (NO_FLASH_ARRAY_DATA == g_validRows[arrayId])
        {
            err = CyBtldr_CreateGetFlashSizeCmd(arrayId, inBuf, &inSize, &outSize);
            if (CYRET_SUCCESS == err)
                err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
            if (CYRET_SUCCESS == err)
                err = CyBtldr_ParseGetFlashSizeCmdResult(outBuf, outSize, &minRow, &maxRow, &status);
            if (CYRET_SUCCESS != status)
                err = status | CYRET_ERR_BTLDR_MASK;

            if (CYRET_SUCCESS == err)
            {
                if (CYRET_SUCCESS == status)
                    g_validRows[arrayId] = (minRow << 16) + maxRow;
                else
                    err = status | CYRET_ERR_BTLDR_MASK;
            }
        }
        if (CYRET_SUCCESS == err)
        {
            minRow = (unsigned short)(g_validRows[arrayId] >> 16);
            maxRow = (unsigned short)g_validRows[arrayId];
            if (rowNum < minRow || rowNum > maxRow)
                err = CYRET_ERR_ROW;
        }
    }
    else
        err = CYRET_ERR_ARRAY;

    return err;
}


int CyBtldr_StartBootloadOperation(CyBtldr_CommunicationsData* comm, unsigned long expSiId, unsigned char expSiRev, unsigned long* blVer)
{
    const unsigned long SUPPORTED_BOOTLOADER = 0x010000;
    const unsigned long BOOTLOADER_VERSION_MASK = 0xFF0000;
    unsigned long i;
    unsigned long inSize = 0;
    unsigned long outSize = 0;
    unsigned long siliconId = 0;
    unsigned char inBuf[MAX_COMMAND_SIZE];
    unsigned char outBuf[MAX_COMMAND_SIZE];
    unsigned char siliconRev = 0;
    unsigned char status = CYRET_SUCCESS;
    int err;

    g_comm = comm;
    for (i = 0; i < MAX_FLASH_ARRAYS; i++)
        g_validRows[i] = NO_FLASH_ARRAY_DATA;

    err = g_comm->OpenConnection();
    if (CYRET_SUCCESS != err)
        err |= CYRET_ERR_COMM_MASK;

    if (CYRET_SUCCESS == err)
        err = CyBtldr_CreateEnterBootLoaderCmd(inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_ParseEnterBootLoaderCmdResult(outBuf, outSize, &siliconId, &siliconRev, blVer, &status);


    if (CYRET_SUCCESS == err)
    {
        if (CYRET_SUCCESS != status)
            err = status | CYRET_ERR_BTLDR_MASK;
        if (expSiId != siliconId || expSiRev != siliconRev)
            err = CYRET_ERR_DEVICE;
        else if ((*blVer & BOOTLOADER_VERSION_MASK) != SUPPORTED_BOOTLOADER)
            err = CYRET_ERR_VERSION;
    }

    return err;
}

int CyBtldr_GetApplicationStatus(unsigned char appID, unsigned char* isValid, unsigned char* isActive)
{
    unsigned long inSize = 0;
    unsigned long outSize = 0;
    unsigned char inBuf[MAX_COMMAND_SIZE];
    unsigned char outBuf[MAX_COMMAND_SIZE];
    unsigned char status = CYRET_SUCCESS;
    int err;

    err = CyBtldr_CreateGetAppStatusCmd(appID, inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_ParseGetAppStatusCmdResult(outBuf, outSize, isValid, isActive, &status);

    if (CYRET_SUCCESS == err)
    {
        if (CYRET_SUCCESS != status)
            err = status | CYRET_ERR_BTLDR_MASK;
    }

    return err;
}

int CyBtldr_SetApplicationStatus(unsigned char appID)
{
    unsigned long inSize = 0;
    unsigned long outSize = 0;
    unsigned char inBuf[MAX_COMMAND_SIZE];
    unsigned char outBuf[MAX_COMMAND_SIZE];
    unsigned char status = CYRET_SUCCESS;
    int err;

    err = CyBtldr_CreateSetActiveAppCmd(appID, inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_ParseSetActiveAppCmdResult(outBuf, outSize, &status);

    if (CYRET_SUCCESS == err)
    {
        if (CYRET_SUCCESS != status)
            err = status | CYRET_ERR_BTLDR_MASK;
    }

    return err;
}

int CyBtldr_EndBootloadOperation(void)
{
    const unsigned char RESET = 0x00;
    unsigned long inSize;
    unsigned long outSize;
    unsigned char inBuf[MAX_COMMAND_SIZE];

    int err = CyBtldr_CreateExitBootLoaderCmd(RESET, inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err)
    {
        err = g_comm->WriteData(inBuf, inSize);

        if (CYRET_SUCCESS == err)
            err = g_comm->CloseConnection();

        if (CYRET_SUCCESS != err)
            err |= CYRET_ERR_COMM_MASK;
    }
    g_comm = NULL;

    return err;
}

int CyBtldr_ProgramRow(unsigned char arrayID, unsigned short rowNum, unsigned char* buf, unsigned short size)
{
    const int TRANSFER_HEADER_SIZE = 11;

    unsigned char inBuf[MAX_COMMAND_SIZE];
    unsigned char outBuf[MAX_COMMAND_SIZE];
    unsigned long inSize;
    unsigned long outSize;
    unsigned long offset = 0;
    unsigned short subBufSize;
    unsigned char status = CYRET_SUCCESS;

    int err = CyBtldr_ValidateRow(arrayID, rowNum);

    //Break row into pieces to ensure we don't send too much for the transfer protocol
    while ((CYRET_SUCCESS == err) && ((size - offset + TRANSFER_HEADER_SIZE) > g_comm->MaxTransferSize))
    {
        subBufSize = (unsigned short)(g_comm->MaxTransferSize - TRANSFER_HEADER_SIZE);

        err = CyBtldr_CreateSendDataCmd(&buf[offset], subBufSize, inBuf, &inSize, &outSize);
        if (CYRET_SUCCESS == err)
            err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
        if (CYRET_SUCCESS == err)
            err = CyBtldr_ParseSendDataCmdResult(outBuf, outSize, &status);
        if (CYRET_SUCCESS != status)
            err = status | CYRET_ERR_BTLDR_MASK;

        offset += subBufSize;
    }

    if (CYRET_SUCCESS == err)
    {
        subBufSize = (unsigned short)(size - offset);

        err = CyBtldr_CreateProgramRowCmd(arrayID, rowNum, &buf[offset], subBufSize, inBuf, &inSize, &outSize);
        if (CYRET_SUCCESS == err)
            err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
        if (CYRET_SUCCESS == err)
            err = CyBtldr_ParseProgramRowCmdResult(outBuf, outSize, &status);
        if (CYRET_SUCCESS != status)
            err = status | CYRET_ERR_BTLDR_MASK;
    }

    return err;
}

int CyBtldr_EraseRow(unsigned char arrayID, unsigned short rowNum)
{
    unsigned char inBuf[MAX_COMMAND_SIZE];
    unsigned char outBuf[MAX_COMMAND_SIZE];
    unsigned long inSize = 0;
    unsigned long outSize = 0;
    unsigned char status = CYRET_SUCCESS;

    int err = CyBtldr_ValidateRow(arrayID, rowNum);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_CreateEraseRowCmd(arrayID, rowNum, inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_ParseEraseRowCmdResult(outBuf, outSize, &status);
    if (CYRET_SUCCESS != status)
        err = status | CYRET_ERR_BTLDR_MASK;

    return err;
}

int CyBtldr_VerifyRow(unsigned char arrayID, unsigned short rowNum, unsigned char checksum)
{
    unsigned char inBuf[MAX_COMMAND_SIZE];
    unsigned char outBuf[MAX_COMMAND_SIZE];
    unsigned long inSize = 0;
    unsigned long outSize = 0;
    unsigned char rowChecksum = 0;
    unsigned char status = CYRET_SUCCESS;

    int err = CyBtldr_ValidateRow(arrayID, rowNum);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_CreateVerifyRowCmd(arrayID, rowNum, inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_ParseVerifyRowCmdResult(outBuf, outSize, &rowChecksum, &status);
    if (CYRET_SUCCESS != status)
        err = status | CYRET_ERR_BTLDR_MASK;
    if ((CYRET_SUCCESS == err) && (rowChecksum != checksum))
        err = CYRET_ERR_CHECKSUM;

    return err;
}

int CyBtldr_VerifyApplication()
{
    unsigned char inBuf[MAX_COMMAND_SIZE];
    unsigned char outBuf[MAX_COMMAND_SIZE];
    unsigned long inSize = 0;
    unsigned long outSize = 0;
    unsigned char checksumValid = 0;
    unsigned char status = CYRET_SUCCESS;

    int err = CyBtldr_CreateVerifyChecksumCmd(inBuf, &inSize, &outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_TransferData(inBuf, inSize, outBuf, outSize);
    if (CYRET_SUCCESS == err)
        err = CyBtldr_ParseVerifyChecksumCmdResult(outBuf, outSize, &checksumValid, &status);
    if (CYRET_SUCCESS != status)
        err = status | CYRET_ERR_BTLDR_MASK;
    if ((CYRET_SUCCESS == err) && (!checksumValid))
        err = CYRET_ERR_CHECKSUM;

    return err;
}
