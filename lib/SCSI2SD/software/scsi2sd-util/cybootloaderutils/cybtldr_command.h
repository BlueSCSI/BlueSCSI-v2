/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
********************************************************************************/

#ifndef __CYBTLDR_COMMAND_H__
#define __CYBTLDR_COMMAND_H__

#include "cybtldr_utils.h"

/* Maximum number of bytes to allocate for a single command.  */
#define MAX_COMMAND_SIZE 512


//STANDARD PACKET FORMAT:
// Multi byte entries are encoded in LittleEndian.
/*******************************************************************************
* [1-byte] [1-byte ] [2-byte] [n-byte] [ 2-byte ] [1-byte]
* [ SOP  ] [Command] [ Size ] [ Data ] [Checksum] [ EOP  ]
*******************************************************************************/


/* The first byte of any boot loader command. */
#define CMD_START               0x01
/* The last byte of any boot loader command. */
#define CMD_STOP                0x17
/* The minimum number of bytes in a bootloader command. */
#define BASE_CMD_SIZE           0x07

/* Command identifier for verifying the checksum value of the bootloadable project. */
#define CMD_VERIFY_CHECKSUM     0x31
/* Command identifier for getting the number of flash rows in the target device. */
#define CMD_GET_FLASH_SIZE      0x32
/* Command identifier for getting info about the app status. This is only supported on multi app bootloader. */
#define CMD_GET_APP_STATUS      0x33
/* Command identifier for reasing a row of flash data from the target device. */
#define CMD_ERASE_ROW           0x34
/* Command identifier for making sure the bootloader host and bootloader are in sync. */
#define CMD_SYNC                0x35
/* Command identifier for setting the active application. This is only supported on multi app bootloader. */
#define CMD_SET_ACTIVE_APP      0x36
/* Command identifier for sending a block of data to the bootloader without doing anything with it yet. */
#define CMD_SEND_DATA           0x37
/* Command identifier for starting the boot loader.  All other commands ignored until this is sent. */
#define CMD_ENTER_BOOTLOADER    0x38
/* Command identifier for programming a single row of flash. */
#define CMD_PROGRAM_ROW         0x39
/* Command identifier for verifying the contents of a single row of flash. */
#define CMD_VERIFY_ROW          0x3A
/* Command identifier for exiting the bootloader and restarting the target program. */
#define CMD_EXIT_BOOTLOADER     0x3B

/*
 * This enum defines the different types of checksums that can be 
 * used by the bootloader for ensuring data integrety.
 */
typedef enum
{
    /* Checksum type is a basic inverted summation of all bytes */
    SUM_CHECKSUM = 0x00,
    /* 16-bit CRC checksum using the CCITT implementation */
    CRC_CHECKSUM = 0x01,
} CyBtldr_ChecksumType;

/*******************************************************************************
* Function Name: CyBtldr_ComputeChecksum
********************************************************************************
* Summary:
*   Computes the 2byte checksum for the provided command data.  The checksum is 
*   the 2's complement of the 1-byte sum of all bytes.
*
* Parameters:
*   buf  - The data to compute the checksum on
*   size - The number of bytes contained in buf.
*
* Returns:
*   The checksum for the provided data.
*
*******************************************************************************/
unsigned short CyBtldr_ComputeChecksum(unsigned char* buf, unsigned long size);

/*******************************************************************************
* Function Name: CyBtldr_SetCheckSumType
********************************************************************************
* Summary:
*   Updates what checksum algorithm is used when generating packets
*
* Parameters:
*   chksumType - The type of checksum to use when creating packets
*
* Returns:
*   NA
*
*******************************************************************************/
void CyBtldr_SetCheckSumType(CyBtldr_ChecksumType chksumType);

/*******************************************************************************
* Function Name: CyBtldr_ParseDefaultCmdResult
********************************************************************************
* Summary:
*   Parses the output from any command that returns the default result packet
*   data.  The default result is just a status byte
*
* Parameters:
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   status  - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
int CyBtldr_ParseDefaultCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateEnterBootLoaderCmd
********************************************************************************
* Summary:
*   Creates the command used to startup the bootloader.
*   NB: This command must be sent before the bootloader will respond to any
*       other command.
*
* Parameters:
*   protect - The flash protection settings.
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateEnterBootLoaderCmd(unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseEnterBootLoaderCmdResult
********************************************************************************
* Summary:
*   Parses the output from the EnterBootLoader command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf     - The buffer containing the output from the bootloader.
*   cmdSize    - The number of bytes in cmdBuf.
*   siliconId  - The silicon ID of the device being communicated with.
*   siliconRev - The silicon Revision of the device being communicated with.
*   blVersion  - The bootloader version being communicated with.
*   status     - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseEnterBootLoaderCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned long* siliconId, unsigned char* siliconRev, unsigned long* blVersion, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateExitBootLoaderCmd
********************************************************************************
* Summary:
*   Creates the command used to stop communicating with the boot loader and to
*   trigger the target device to restart, running the new bootloadable 
*   application.
*
* Parameters:
*   resetType - The type of reset to perform (0 = Reset, 1 = Direct Call).
*   cmdBuf    - The preallocated buffer to store command data in.
*   cmdSize   - The number of bytes in the command.
*   resSize   - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateExitBootLoaderCmd(unsigned char resetType, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_CreateProgramRowCmd
********************************************************************************
* Summary:
*   Creates the command used to program a single flash row.
*
* Parameters:
*   arrayId - The array id to program.
*   rowNum  - The row number to program.
*   buf     - The buffer of data to program into the flash row.
*   size    - The number of bytes in data for the row.
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateProgramRowCmd(unsigned char arrayId, unsigned short rowNum, unsigned char* buf, unsigned short size, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseProgramRowCmdResult
********************************************************************************
* Summary:
*   Parses the output from the ProgramRow command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   status  - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseProgramRowCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateVerifyRowCmd
********************************************************************************
* Summary:
*   Creates the command used to verify that the contents of flash match the
*   provided row data.
*
* Parameters:
*   arrayId - The array id to verify.
*   rowNum  - The row number to verify.
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateVerifyRowCmd(unsigned char arrayId, unsigned short rowNum, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseVerifyRowCmdResult
********************************************************************************
* Summary:
*   Parses the output from the VerifyRow command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf   - The preallocated buffer to store command data in.
*   cmdSize  - The number of bytes in the command.
*   checksum - The checksum from the row to verify.
*   status   - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseVerifyRowCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* checksum, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateEraseRowCmd
********************************************************************************
* Summary:
*   Creates the command used to erase a single flash row.
*
* Parameters:
*   arrayId - The array id to erase.
*   rowNum  - The row number to erase.
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateEraseRowCmd(unsigned char arrayId, unsigned short rowNum, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseEraseRowCmdResult
********************************************************************************
* Summary:
*   Parses the output from the EraseRow command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   status  - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseEraseRowCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateVerifyChecksumCmd
********************************************************************************
* Summary:
*   Creates the command used to verify that the checkusm value in flash matches
*   what is expected.
*
* Parameters:
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateVerifyChecksumCmd(unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseVerifyChecksumCmdResult
********************************************************************************
* Summary:
*   Parses the output from the VerifyChecksum command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf   - The preallocated buffer to store command data in.
*   cmdSize  - The number of bytes in the command.
*   checksumValid - Whether or not the full checksums match (1 = valid, 0 = invalid)
*   status   - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseVerifyChecksumCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* checksumValid, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateGetFlashSizeCmd
********************************************************************************
* Summary:
*   Creates the command used to retreive the number of flash rows in the device.
*
* Parameters:
*   arrayId - The array ID to get the flash size of.
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateGetFlashSizeCmd(unsigned char arrayId, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseGetFlashSizeCmdResult
********************************************************************************
* Summary:
*   Parses the output from the GetFlashSize command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf   - The preallocated buffer to store command data in.
*   cmdSize  - The number of bytes in the command.
*   startRow - The first available row number in the flash array.
*   endRow   - The last available row number in the flash array.
*   status   - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseGetFlashSizeCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned short* startRow, unsigned short* endRow, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateSendDataCmd
********************************************************************************
* Summary:
*   Creates the command used to send a block of data to the target.
*
* Parameters:
*   buf     - The buffer of data data to program into the flash row.
*   size    - The number of bytes in data for the row.
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateSendDataCmd(unsigned char* buf, unsigned short size, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseSendDataCmdResult
********************************************************************************
* Summary:
*   Parses the output from the SendData command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   status  - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseSendDataCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateSyncBootLoaderCmd
********************************************************************************
* Summary:
*   Creates the command used to ensure that the host application is in sync
*   with the bootloader application.
*
* Parameters:
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateSyncBootLoaderCmd(unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_CreateGetAppStatusCmd
********************************************************************************
* Summary:
*   Creates the command used to get information about the application.  This
*   command is only supported by the multi application bootloaader.
*
* Parameters:
*   appId   - The id for the application to get status for
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateGetAppStatusCmd(unsigned char appId, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseGetAppStatusCmdResult
********************************************************************************
* Summary:
*   Parses the output from the GetAppStatus command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   isValid - Is the application valid.
*   isActive- Is the application currently marked as active.
*   status  - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseGetAppStatusCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* isValid, unsigned char* isActive, unsigned char* status);

/*******************************************************************************
* Function Name: CyBtldr_CreateSetActiveAppCmd
********************************************************************************
* Summary:
*   Creates the command used to set the active application for the bootloader 
*   to run.  This command is only supported by the multi application 
*   bootloaader.
*
* Parameters:
*   appId   - The id for the application to get status for
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   resSize - The number of bytes expected in the bootloader's response packet.
*
* Returns:
*   CYRET_SUCCESS  - The command was constructed successfully
*
*******************************************************************************/
EXTERN int CyBtldr_CreateSetActiveAppCmd(unsigned char appId, unsigned char* cmdBuf, unsigned long* cmdSize, unsigned long* resSize);

/*******************************************************************************
* Function Name: CyBtldr_ParseSetActiveAppCmdResult
********************************************************************************
* Summary:
*   Parses the output from the SetActiveApp command to get the resultant
*   data.
*
* Parameters:
*   cmdBuf  - The preallocated buffer to store command data in.
*   cmdSize - The number of bytes in the command.
*   status  - The status code returned by the bootloader.
*
* Returns:
*   CYRET_SUCCESS    - The command was constructed successfully
*   CYRET_ERR_LENGTH - The packet does not contain enough data
*   CYRET_ERR_DATA   - The packet's contents are not correct
*
*******************************************************************************/
EXTERN int CyBtldr_ParseSetActiveAppCmdResult(unsigned char* cmdBuf, unsigned long cmdSize, unsigned char* status);

#endif
