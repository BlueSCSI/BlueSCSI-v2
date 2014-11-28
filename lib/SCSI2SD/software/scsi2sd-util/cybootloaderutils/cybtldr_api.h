/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
********************************************************************************/

#ifndef __CYBTLDR_API_H__
#define __CYBTLDR_API_H__

#include "cybtldr_utils.h"

/*
 * This struct defines all of the items necessary for the bootloader
 * host to communicate over an arbitrary communication protocol. The
 * caller must provide implementations of these items to use their
 * deisred communication protocol.
 */
typedef struct
{
    /* Function used to open the communications connection */
    int (*OpenConnection)(void);
    /* Function used to close the communications connection */
    int (*CloseConnection)(void);
    /* Function used to read data over the communications connection */
    int (*ReadData)(unsigned char*, int);
    /* Function used to write data over the communications connection */
    int (*WriteData)(unsigned char*, int);
    /* Value used to specify the maximum number of bytes that can be trasfered at a time */
    unsigned int MaxTransferSize;
} CyBtldr_CommunicationsData;



/*******************************************************************************
* Function Name: CyBtldr_TransferData
********************************************************************************
* Summary:
*   This function is responsible for transfering a buffer of data to the target
*   device and then reading a response packet back from the device.
*
* Parameters:
*   inBuf   - The buffer containing data to send to the target device
*   inSize  - The number of bytes to send to the target device
*   outBuf  - The buffer to store the data read from the device
*   outSize - The number of bytes to read from the target device
*
* Returns:
*   CYRET_SUCCESS  - The transfer completed successfully
*   CYRET_ERR_COMM - There was a communication error talking to the device
*
*******************************************************************************/
int CyBtldr_TransferData(unsigned char* inBuf, int inSize, unsigned char* outBuf, int outSize);

/*******************************************************************************
* Function Name: CyBtldr_ValidateRow
********************************************************************************
* Summary:
*   This function is responsible for verifying that the provided arrayId and
*   row number are valid for a bootload operation.
*
* Parameters:
*   arrayId - The array to check
*   rowNum  - The row number within the array to check
*
* Returns:
*   CYRET_SUCCESS   - The array and row are available for communication
*   CYRET_ERR_ARRAY - The array is not valid for communication
*   CYRET_ERR_ROW   - The array/row number is not valid for communication
*
*******************************************************************************/
int CyBtldr_ValidateRow(unsigned char arrayId, unsigned short rowNum);

/*******************************************************************************
* Function Name: CyBtldr_StartBootloadOperation
********************************************************************************
* Summary:
*   Initiates a new bootload operation.  This must be called before any other 
*   request to send data to the bootloader.  A corresponding call to 
*   CyBtldr_EndBootloadOperation() should be made once all transactions are 
*   complete.
*
* Parameters:
*   comm     – Communication struct used for communicating with the target device
*   expSiId  - The Silicon ID of the device we expect to communicate with
*   expSiRev - The Silicon Rev of the device we expect to communicate with
*   blVer    - The Bootloader version that is running on the device
*
* Returns:
*   CYRET_SUCCESS     - The start request was sent successfully
*   CYRET_ERR_DEVICE  - The detected device does not match the desired device
*   CYRET_ERR_VERSION - The detected bootloader version is not compatible
*   CYRET_ERR_BTLDR   - The bootloader experienced an error
*   CYRET_ERR_COMM    - There was a communication error talking to the device
*
*******************************************************************************/
EXTERN int CyBtldr_StartBootloadOperation(CyBtldr_CommunicationsData* comm, unsigned long expSiId, unsigned char expSiRev, unsigned long* blVer);

/*******************************************************************************
* Function Name: CyBtldr_EndBootloadOperation
********************************************************************************
* Summary:
*   Terminates the current bootload operation.  This should be called once all 
*   bootload commands have been sent and no more communication is desired.
*
* Parameters:
*   void.
*
* Returns:
*   CYRET_SUCCESS   - The end request was sent successfully
*   CYRET_ERR_BTLDR - The bootloader experienced an error
*   CYRET_ERR_COMM  - There was a communication error talking to the device
*
*******************************************************************************/
EXTERN int CyBtldr_EndBootloadOperation(void);

/*******************************************************************************
* Function Name: CyBtldr_GetApplicationStatus
********************************************************************************
* Summary:
*   Gets the status for the provided application id. The status includes whether
*   the application is valid and whether it is currently marked as active.  This
*   should be called immediatly after enter bootloader in order to determine if
*   the application is suitable for bootloading.
*   NOTE: This is only valid for multi application bootloaders.
*
* Parameters:
*   appID    - The application ID to get status information for
*   isValid  - Is the provided application valid to be executed
*   isActive - Is the provided application already marked as the active app
*
* Returns:
*   CYRET_SUCCESS   - The end request was sent successfully
*   CYRET_ERR_BTLDR - The bootloader experienced an error
*   CYRET_ERR_COMM  - There was a communication error talking to the device
*   CYRET_ERR_LENGTH- The result packet does not have enough data
*   CYRET_ERR_DATA  - The result packet does not contain valid data
*
*******************************************************************************/
EXTERN int CyBtldr_GetApplicationStatus(unsigned char appID, unsigned char* isValid, unsigned char* isActive);

/*******************************************************************************
* Function Name: CyBtldr_SetApplicationStatus
********************************************************************************
* Summary:
*   Sets the application that the bootloader will run.  This should be called
*   after a new application has been programmed in and verified 
*   NOTE: This is only valid for multi application bootloaders.
*
* Parameters:
*   appID    - The application ID to set as the active application 
*
* Returns:
*   CYRET_SUCCESS   - The end request was sent successfully
*   CYRET_ERR_BTLDR - The bootloader experienced an error
*   CYRET_ERR_COMM  - There was a communication error talking to the device
*   CYRET_ERR_LENGTH- The result packet does not have enough data
*   CYRET_ERR_DATA  - The result packet does not contain valid data
*   CYRET_ERR_APP   - The application is not valid and cannot be set as active
*
*******************************************************************************/
EXTERN int CyBtldr_SetApplicationStatus(unsigned char appID);

/*******************************************************************************
* Function Name: CyBtldr_ProgramRow
********************************************************************************
* Summary:
*   Sends a single row of data to the bootloader to be programmed into flash
*
* Parameters:
*   arrayID – The flash array that is to be reprogrammed
*   rowNum  – The row number within the array that is to be reprogrammed
*   buf     – The buffer of data to program into the devices flash
*   size    – The number of bytes in data that need to be sent to the bootloader
*
* Returns:
*   CYRET_SUCCESS    - The row was programmed successfully
*   CYRET_ERR_LENGTH - The result packet does not have enough data
*   CYRET_ERR_DATA   - The result packet does not contain valid data
*   CYRET_ERR_ARRAY  - The array is not valid for programming
*   CYRET_ERR_ROW    - The array/row number is not valid for programming
*   CYRET_ERR_BTLDR  - The bootloader experienced an error
*   CYRET_ERR_ACTIVE - The application is currently marked as active
*
*******************************************************************************/
EXTERN int CyBtldr_ProgramRow(unsigned char arrayID, unsigned short rowNum, unsigned char* buf, unsigned short size);

/*******************************************************************************
* Function Name: CyBtldr_EraseRow
********************************************************************************
* Summary:
*   Erases a single row of flash data from the device.
*
* Parameters:
*   arrayID – The flash array that is to have a row erased
*   rowNum  – The row number within the array that is to be erased
*
* Returns:
*   CYRET_SUCCESS    - The row was erased successfully
*   CYRET_ERR_LENGTH - The result packet does not have enough data
*   CYRET_ERR_DATA   - The result packet does not contain valid data
*   CYRET_ERR_ARRAY  - The array is not valid for programming
*   CYRET_ERR_ROW    - The array/row number is not valid for programming
*   CYRET_ERR_BTLDR  - The bootloader experienced an error
*   CYRET_ERR_COMM   - There was a communication error talking to the device
*   CYRET_ERR_ACTIVE - The application is currently marked as active
*
*******************************************************************************/
EXTERN int CyBtldr_EraseRow(unsigned char arrayID, unsigned short rowNum);

/*******************************************************************************
* Function Name: CyBtldr_VerifyRow
********************************************************************************
* Summary:
*   Verifies that the data contained within the specified flash array and row 
*   matches the expected value.
*
* Parameters:
*   arrayID  – The flash array that is to be verified
*   rowNum   – The row number within the array that is to be verified
*   checksum – The expected checksum value for the row
*
* Returns:
*   CYRET_SUCCESS      - The row was verified successfully
*   CYRET_ERR_LENGTH   - The result packet does not have enough data
*   CYRET_ERR_DATA     - The result packet does not contain valid data
*   CYRET_ERR_ARRAY	   - The array is not valid for programming
*   CYRET_ERR_ROW      - The array/row number is not valid for programming
*   CYRET_ERR_CHECKSUM - The checksum does not match the expected value
*   CYRET_ERR_BTLDR    - The bootloader experienced an error
*   CYRET_ERR_COMM     - There was a communication error talking to the device
*
*******************************************************************************/
EXTERN int CyBtldr_VerifyRow(unsigned char arrayID, unsigned short rowNum, unsigned char checksum);

/*******************************************************************************
* Function Name: CyBtldr_VerifyApplication
********************************************************************************
* Summary:
*   Verifies that the checksum for the entire bootloadable application matches 
*   the expected value.  This is used to verify that the entire bootloadable
*   image is valid and ready to execute.
*
* Parameters:
*   void
*
* Returns:
*   CYRET_SUCCESS      - The application was verified successfully
*   CYRET_ERR_LENGTH   - The result packet does not have enough data
*   CYRET_ERR_DATA     - The result packet does not contain valid data
*   CYRET_ERR_CHECKSUM - The checksum does not match the expected value
*   CYRET_ERR_BTLDR    - The bootloader experienced an error
*   CYRET_ERR_COMM     - There was a communication error talking to the device
*
*******************************************************************************/
EXTERN int CyBtldr_VerifyApplication();

#endif
