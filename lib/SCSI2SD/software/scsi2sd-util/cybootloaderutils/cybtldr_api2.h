/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
********************************************************************************/

#ifndef __CYBTLDR_API2_H__
#define __CYBTLDR_API2_H__

#include "cybtldr_utils.h"

/*
 * This enum defines the different operations that can be performed
 * by the bootloader host.
 */
typedef enum
{
    /* Perform a Program operation*/
    PROGRAM,
    /* Perform an Erase operation */
    ERASE,
    /* Perform a Verify operation */
    VERIFY,
} CyBtldr_Action;

/* Function used to notify caller that a row was finished */
typedef void CyBtldr_ProgressUpdate(unsigned char arrayId, unsigned short rowNum);


/*******************************************************************************
* Function Name: CyBtldr_RunAction
********************************************************************************
* Summary:
*   
*
* Parameters:
*   action - The action to execute
*   file   – The full canonical path to the *.cyacd file to open
*   comm   – Communication struct used for communicating with the target device
*   update - Optional function pointer to use to notify of progress updates
*
* Returns:
*   CYRET_SUCCESS	    - The device was programmed successfully
*   CYRET_ERR_DEVICE	- The detected device does not match the desired device
*   CYRET_ERR_VERSION	- The detected bootloader version is not compatible
*   CYRET_ERR_LENGTH	- The result packet does not have enough data
*   CYRET_ERR_DATA	    - The result packet does not contain valid data
*   CYRET_ERR_ARRAY	    - The array is not valid for programming
*   CYRET_ERR_ROW	    - The array/row number is not valid for programming
*   CYRET_ERR_CHECKSUM  - The checksum does not match the expected value
*   CYRET_ERR_BTLDR	    - The bootloader experienced an error
*   CYRET_ERR_COMM	    - There was a communication error talking to the device
*   CYRET_ABORT		    - The operation was aborted
*
*******************************************************************************/
int CyBtldr_RunAction(CyBtldr_Action action, const char* file, 
                      CyBtldr_CommunicationsData* comm, CyBtldr_ProgressUpdate* update);

/*******************************************************************************
* Function Name: CyBtldr_Program
********************************************************************************
* Summary:
*   This function reprograms the bootloadable portion of the PSoC’s flash with 
*   the contents of the provided *.cyacd file.
*
* Parameters:
*   file   – The full canonical path to the *.cyacd file to open
*   comm   – Communication struct used for communicating with the target device
*   update - Optional function pointer to use to notify of progress updates
*
* Returns:
*   CYRET_SUCCESS	    - The device was programmed successfully
*   CYRET_ERR_DEVICE	- The detected device does not match the desired device
*   CYRET_ERR_VERSION	- The detected bootloader version is not compatible
*   CYRET_ERR_LENGTH	- The result packet does not have enough data
*   CYRET_ERR_DATA	    - The result packet does not contain valid data
*   CYRET_ERR_ARRAY	    - The array is not valid for programming
*   CYRET_ERR_ROW	    - The array/row number is not valid for programming
*   CYRET_ERR_BTLDR	    - The bootloader experienced an error
*   CYRET_ERR_COMM	    - There was a communication error talking to the device
*   CYRET_ABORT		    - The operation was aborted
*
*******************************************************************************/
EXTERN int CyBtldr_Program(const char* file, CyBtldr_CommunicationsData* comm, CyBtldr_ProgressUpdate* update);

/*******************************************************************************
* Function Name: CyBtldr_Erase
********************************************************************************
* Summary:
*   This function erases the bootloadable portion of the PSoC’s flash contained 
*   within the specified *.cyacd file.
*   
*
* Parameters:
*   file   – The full canonical path to the *.cyacd file to open
*   comm   – Communication struct used for communicating with the target device
*   update - Optional function pointer to use to notify of progress updates
*
* Returns:
*   CYRET_SUCCESS	    - The device was erased successfully
*   CYRET_ERR_DEVICE	- The detected device does not match the desired device
*   CYRET_ERR_VERSION	- The detected bootloader version is not compatible
*   CYRET_ERR_LENGTH	- The result packet does not have enough data
*   CYRET_ERR_DATA	    - The result packet does not contain valid data
*   CYRET_ERR_ARRAY	    - The array is not valid for programming
*   CYRET_ERR_ROW	    - The array/row number is not valid for programming
*   CYRET_ERR_BTLDR	    - The bootloader experienced an error
*   CYRET_ERR_COMM	    - There was a communication error talking to the device
*   CYRET_ABORT		    - The operation was aborted
*
*******************************************************************************/
EXTERN int CyBtldr_Erase(const char* file, CyBtldr_CommunicationsData* comm, CyBtldr_ProgressUpdate* update);

/*******************************************************************************
* Function Name: CyBtldr_Verify
********************************************************************************
* Summary:
*   This function verifies the contents of bootloadable portion of the PSoC’s 
*   flash with the contents of the provided *.cyacd file.
*
* Parameters:
*   file   – The full canonical path to the *.cyacd file to open
*   comm   – Communication struct used for communicating with the target device
*   update - Optional function pointer to use to notify of progress updates
*
* Returns:
*   CYRET_SUCCESS	    - The device’s flash image was verified successfully
*   CYRET_ERR_DEVICE	- The detected device does not match the desired device
*   CYRET_ERR_VERSION	- The detected bootloader version is not compatible
*   CYRET_ERR_LENGTH	- The result packet does not have enough data
*   CYRET_ERR_DATA	    - The result packet does not contain valid data
*   CYRET_ERR_ARRAY	    - The array is not valid for programming
*   CYRET_ERR_ROW	    - The array/row number is not valid for programming
*   CYRET_ERR_CHECKSUM  - The checksum does not match the expected value
*   CYRET_ERR_BTLDR	    - The bootloader experienced an error
*   CYRET_ERR_COMM	    - There was a communication error talking to the device
*   CYRET_ABORT		    - The operation was aborted
*
*******************************************************************************/
EXTERN int CyBtldr_Verify(const char* file, CyBtldr_CommunicationsData* comm, CyBtldr_ProgressUpdate* update);

/*******************************************************************************
* Function Name: CyBtldr_Abort
********************************************************************************
* Summary:
*  This function aborts the current operation, whether it be Programming, 
*  Erasing, or Verifying.  This is done by setting a global flag that the 
*  Program, Erase & Verify operations check at the end of each row operation.  
*  Since all calls are blocking, this will need to be called from a different 
*  execution thread.
*
* Parameters:
*   void.
*
* Returns:
*   CYRET_SUCCESS	    - The abort was sent successfully
*
*******************************************************************************/
EXTERN int CyBtldr_Abort(void);

#endif
