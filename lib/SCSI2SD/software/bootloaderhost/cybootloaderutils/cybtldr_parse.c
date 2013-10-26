/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
********************************************************************************/

#include <string.h>
#include "cybtldr_parse.h"

/* Pointer to the *.cyacd file containing the data that is to be read */
static FILE* dataFile;

unsigned char CyBtldr_FromHex(char value)
{
    if ('0' <= value && value <= '9')
        return (unsigned char)(value - '0');
    if ('a' <= value && value <= 'f')
        return (unsigned char)(10 + value - 'a');
    if ('A' <= value && value <= 'F')
        return (unsigned char)(10 + value - 'A');
    return 0;
}

int CyBtldr_FromAscii(unsigned int bufSize, unsigned char* buffer, unsigned short* rowSize, unsigned char* rowData)
{
    unsigned short i;
    int err = CYRET_SUCCESS;

    if (bufSize & 1) // Make sure even number of bytes
        err = CYRET_ERR_LENGTH;
    else
    {
        for (i = 0; i < bufSize / 2; i++)
        {
            rowData[i] = (CyBtldr_FromHex(buffer[i * 2]) << 4) | CyBtldr_FromHex(buffer[i * 2 + 1]);
        }
        *rowSize = i;
    }

    return err;
}

int CyBtldr_ReadLine(unsigned int* size, char* buffer)
{
    int err = CYRET_SUCCESS;
    unsigned int len = 0;

    if (NULL != dataFile && !feof(dataFile))
    {
        if (NULL != fgets(buffer, MAX_BUFFER_SIZE, dataFile))
        {
            len = strlen(buffer);

            while (len > 0 && ('\n' == buffer[len - 1] || '\r' == buffer[len - 1]))
                --len;
        }
        else
            err = CYRET_ERR_EOF;
    }
    else
        err = CYRET_ERR_FILE;

    *size = len;
    return err;
}

int CyBtldr_OpenDataFile(const char* file)
{
    dataFile = fopen(file, "r");

    return (NULL == dataFile)
        ? CYRET_ERR_FILE
        : CYRET_SUCCESS;
}

int CyBtldr_ParseHeader(unsigned int bufSize, unsigned char* buffer, unsigned long* siliconId, unsigned char* siliconRev, unsigned char* chksum)
{
    const unsigned int LENGTH_ID     = 5;            //4-silicon id, 1-silicon rev
    const unsigned int LENGTH_CHKSUM = LENGTH_ID + 1; //1-checksum type

    unsigned short rowSize;
    unsigned char rowData[MAX_BUFFER_SIZE];

    int err = CyBtldr_FromAscii(bufSize, buffer, &rowSize, rowData);

    if (CYRET_SUCCESS == err)
    {
        if (rowSize >= LENGTH_CHKSUM)
            *chksum = rowData[5];
        if (rowSize >= LENGTH_ID)
        {
            *siliconId = (rowData[0] << 24) | (rowData[1] << 16) | (rowData[2] << 8) | (rowData[3]);
            *siliconRev = rowData[4];
        }
        else
            err = CYRET_ERR_LENGTH;
    }

    return err;
}

int CyBtldr_ParseRowData(unsigned int bufSize, unsigned char* buffer, unsigned char* arrayId, unsigned short* rowNum, unsigned char* rowData, unsigned short* size, unsigned char* checksum)
{
    const unsigned short MIN_SIZE = 6; //1-array, 2-addr, 2-size, 1-checksum
    const int DATA_OFFSET = 5;

    unsigned int i;
    unsigned short hexSize;
    unsigned char hexData[MAX_BUFFER_SIZE];
    int err = CYRET_SUCCESS;

    if (bufSize <= MIN_SIZE)
        err = CYRET_ERR_LENGTH;
    else if (buffer[0] == ':')
    {
        err = CyBtldr_FromAscii(bufSize - 1, &buffer[1], &hexSize, hexData);

        *arrayId = hexData[0];
        *rowNum = (hexData[1] << 8) | (hexData[2]);
        *size = (hexData[3] << 8) | (hexData[4]);
        *checksum = (hexData[hexSize - 1]);

        if ((*size + MIN_SIZE) == hexSize)
        {
            for (i = 0; i < *size; i++)
            {
                rowData[i] = (hexData[DATA_OFFSET + i]);
            }
        }
        else
            err = CYRET_ERR_DATA;
    }
    else
        err = CYRET_ERR_CMD;

    return err;
}

int CyBtldr_CloseDataFile(void)
{
    int err = 0;
    if (NULL != dataFile)
    {
        err = fclose(dataFile);
        dataFile = NULL;
    }
    return (0 == err)
        ? CYRET_SUCCESS
        : CYRET_ERR_FILE;
}
