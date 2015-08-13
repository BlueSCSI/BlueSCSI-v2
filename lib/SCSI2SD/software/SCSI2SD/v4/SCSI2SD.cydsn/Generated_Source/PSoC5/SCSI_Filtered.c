/*******************************************************************************
* File Name: SCSI_Filtered.c  
* Version 1.90
*
* Description:
*  This file contains API to enable firmware to read the value of a Status 
*  Register.
*
* Note:
*
********************************************************************************
* Copyright 2008-2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#include "SCSI_Filtered.h"

#if !defined(SCSI_Filtered_sts_sts_reg__REMOVED) /* Check for removal by optimization */


/*******************************************************************************
* Function Name: SCSI_Filtered_Read
********************************************************************************
*
* Summary:
*  Reads the current value assigned to the Status Register.
*
* Parameters:
*  None.
*
* Return:
*  The current value in the Status Register.
*
*******************************************************************************/
uint8 SCSI_Filtered_Read(void) 
{ 
    return SCSI_Filtered_Status;
}


/*******************************************************************************
* Function Name: SCSI_Filtered_InterruptEnable
********************************************************************************
*
* Summary:
*  Enables the Status Register interrupt.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void SCSI_Filtered_InterruptEnable(void) 
{
    uint8 interruptState;
    interruptState = CyEnterCriticalSection();
    SCSI_Filtered_Status_Aux_Ctrl |= SCSI_Filtered_STATUS_INTR_ENBL;
    CyExitCriticalSection(interruptState);
}


/*******************************************************************************
* Function Name: SCSI_Filtered_InterruptDisable
********************************************************************************
*
* Summary:
*  Disables the Status Register interrupt.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void SCSI_Filtered_InterruptDisable(void) 
{
    uint8 interruptState;
    interruptState = CyEnterCriticalSection();
    SCSI_Filtered_Status_Aux_Ctrl &= (uint8)(~SCSI_Filtered_STATUS_INTR_ENBL);
    CyExitCriticalSection(interruptState);
}


/*******************************************************************************
* Function Name: SCSI_Filtered_WriteMask
********************************************************************************
*
* Summary:
*  Writes the current mask value assigned to the Status Register.
*
* Parameters:
*  mask:  Value to write into the mask register.
*
* Return:
*  None.
*
*******************************************************************************/
void SCSI_Filtered_WriteMask(uint8 mask) 
{
    #if(SCSI_Filtered_INPUTS < 8u)
    	mask &= ((uint8)(1u << SCSI_Filtered_INPUTS) - 1u);
	#endif /* End SCSI_Filtered_INPUTS < 8u */
    SCSI_Filtered_Status_Mask = mask;
}


/*******************************************************************************
* Function Name: SCSI_Filtered_ReadMask
********************************************************************************
*
* Summary:
*  Reads the current interrupt mask assigned to the Status Register.
*
* Parameters:
*  None.
*
* Return:
*  The value of the interrupt mask of the Status Register.
*
*******************************************************************************/
uint8 SCSI_Filtered_ReadMask(void) 
{
    return SCSI_Filtered_Status_Mask;
}

#endif /* End check for removal by optimization */


/* [] END OF FILE */
