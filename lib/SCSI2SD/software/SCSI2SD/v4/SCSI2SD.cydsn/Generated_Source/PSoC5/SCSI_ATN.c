/*******************************************************************************
* File Name: SCSI_ATN.c  
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
#include "SCSI_ATN.h"

/* APIs are not generated for P15[7:6] on PSoC 5 */
#if !(CY_PSOC5A &&\
	 SCSI_ATN__PORT == 15 && ((SCSI_ATN__MASK & 0xC0) != 0))


/*******************************************************************************
* Function Name: SCSI_ATN_Write
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
void SCSI_ATN_Write(uint8 value) 
{
    uint8 staticBits = (SCSI_ATN_DR & (uint8)(~SCSI_ATN_MASK));
    SCSI_ATN_DR = staticBits | ((uint8)(value << SCSI_ATN_SHIFT) & SCSI_ATN_MASK);
}


/*******************************************************************************
* Function Name: SCSI_ATN_SetDriveMode
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
void SCSI_ATN_SetDriveMode(uint8 mode) 
{
	CyPins_SetPinDriveMode(SCSI_ATN_0, mode);
}


/*******************************************************************************
* Function Name: SCSI_ATN_Read
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
*  Macro SCSI_ATN_ReadPS calls this function. 
*  
*******************************************************************************/
uint8 SCSI_ATN_Read(void) 
{
    return (SCSI_ATN_PS & SCSI_ATN_MASK) >> SCSI_ATN_SHIFT;
}


/*******************************************************************************
* Function Name: SCSI_ATN_ReadDataReg
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
uint8 SCSI_ATN_ReadDataReg(void) 
{
    return (SCSI_ATN_DR & SCSI_ATN_MASK) >> SCSI_ATN_SHIFT;
}


/* If Interrupts Are Enabled for this Pins component */ 
#if defined(SCSI_ATN_INTSTAT) 

    /*******************************************************************************
    * Function Name: SCSI_ATN_ClearInterrupt
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
    uint8 SCSI_ATN_ClearInterrupt(void) 
    {
        return (SCSI_ATN_INTSTAT & SCSI_ATN_MASK) >> SCSI_ATN_SHIFT;
    }

#endif /* If Interrupts Are Enabled for this Pins component */ 

#endif /* CY_PSOC5A... */

    
/* [] END OF FILE */
