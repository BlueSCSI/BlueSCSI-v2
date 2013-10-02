/*******************************************************************************
* File Name: SCSI_In_DBx.c  
* Version 1.90
*
* Description:
*  This file contains API to enable firmware control of a Pins component.
*
* Note:
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#include "cytypes.h"
#include "SCSI_In_DBx.h"

/* APIs are not generated for P15[7:6] on PSoC 5 */
#if !(CY_PSOC5A &&\
	 SCSI_In_DBx__PORT == 15 && ((SCSI_In_DBx__MASK & 0xC0) != 0))


/*******************************************************************************
* Function Name: SCSI_In_DBx_Write
********************************************************************************
*
* Summary:
*  Assign a new value to the digital port's data output register.  
*
* Parameters:  
*  prtValue:  The value to be assigned to the Digital Port. 
*
* Return: 
*  None
*  
*******************************************************************************/
void SCSI_In_DBx_Write(uint8 value) 
{
    uint8 staticBits = (SCSI_In_DBx_DR & (uint8)(~SCSI_In_DBx_MASK));
    SCSI_In_DBx_DR = staticBits | ((uint8)(value << SCSI_In_DBx_SHIFT) & SCSI_In_DBx_MASK);
}


/*******************************************************************************
* Function Name: SCSI_In_DBx_SetDriveMode
********************************************************************************
*
* Summary:
*  Change the drive mode on the pins of the port.
* 
* Parameters:  
*  mode:  Change the pins to this drive mode.
*
* Return: 
*  None
*
*******************************************************************************/
void SCSI_In_DBx_SetDriveMode(uint8 mode) 
{
	CyPins_SetPinDriveMode(SCSI_In_DBx_0, mode);
	CyPins_SetPinDriveMode(SCSI_In_DBx_1, mode);
	CyPins_SetPinDriveMode(SCSI_In_DBx_2, mode);
	CyPins_SetPinDriveMode(SCSI_In_DBx_3, mode);
	CyPins_SetPinDriveMode(SCSI_In_DBx_4, mode);
	CyPins_SetPinDriveMode(SCSI_In_DBx_5, mode);
	CyPins_SetPinDriveMode(SCSI_In_DBx_6, mode);
	CyPins_SetPinDriveMode(SCSI_In_DBx_7, mode);
}


/*******************************************************************************
* Function Name: SCSI_In_DBx_Read
********************************************************************************
*
* Summary:
*  Read the current value on the pins of the Digital Port in right justified 
*  form.
*
* Parameters:  
*  None
*
* Return: 
*  Returns the current value of the Digital Port as a right justified number
*  
* Note:
*  Macro SCSI_In_DBx_ReadPS calls this function. 
*  
*******************************************************************************/
uint8 SCSI_In_DBx_Read(void) 
{
    return (SCSI_In_DBx_PS & SCSI_In_DBx_MASK) >> SCSI_In_DBx_SHIFT;
}


/*******************************************************************************
* Function Name: SCSI_In_DBx_ReadDataReg
********************************************************************************
*
* Summary:
*  Read the current value assigned to a Digital Port's data output register
*
* Parameters:  
*  None 
*
* Return: 
*  Returns the current value assigned to the Digital Port's data output register
*  
*******************************************************************************/
uint8 SCSI_In_DBx_ReadDataReg(void) 
{
    return (SCSI_In_DBx_DR & SCSI_In_DBx_MASK) >> SCSI_In_DBx_SHIFT;
}


/* If Interrupts Are Enabled for this Pins component */ 
#if defined(SCSI_In_DBx_INTSTAT) 

    /*******************************************************************************
    * Function Name: SCSI_In_DBx_ClearInterrupt
    ********************************************************************************
    * Summary:
    *  Clears any active interrupts attached to port and returns the value of the 
    *  interrupt status register.
    *
    * Parameters:  
    *  None 
    *
    * Return: 
    *  Returns the value of the interrupt status register
    *  
    *******************************************************************************/
    uint8 SCSI_In_DBx_ClearInterrupt(void) 
    {
        return (SCSI_In_DBx_INTSTAT & SCSI_In_DBx_MASK) >> SCSI_In_DBx_SHIFT;
    }

#endif /* If Interrupts Are Enabled for this Pins component */ 

#endif /* CY_PSOC5A... */

    
/* [] END OF FILE */
