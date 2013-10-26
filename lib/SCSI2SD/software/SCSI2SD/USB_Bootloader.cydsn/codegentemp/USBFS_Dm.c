/*******************************************************************************
* File Name: USBFS_Dm.c  
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
#include "USBFS_Dm.h"

/* APIs are not generated for P15[7:6] on PSoC 5 */
#if !(CY_PSOC5A &&\
	 USBFS_Dm__PORT == 15 && ((USBFS_Dm__MASK & 0xC0) != 0))


/*******************************************************************************
* Function Name: USBFS_Dm_Write
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
void USBFS_Dm_Write(uint8 value) 
{
    uint8 staticBits = (USBFS_Dm_DR & (uint8)(~USBFS_Dm_MASK));
    USBFS_Dm_DR = staticBits | ((uint8)(value << USBFS_Dm_SHIFT) & USBFS_Dm_MASK);
}


/*******************************************************************************
* Function Name: USBFS_Dm_SetDriveMode
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
void USBFS_Dm_SetDriveMode(uint8 mode) 
{
	CyPins_SetPinDriveMode(USBFS_Dm_0, mode);
}


/*******************************************************************************
* Function Name: USBFS_Dm_Read
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
*  Macro USBFS_Dm_ReadPS calls this function. 
*  
*******************************************************************************/
uint8 USBFS_Dm_Read(void) 
{
    return (USBFS_Dm_PS & USBFS_Dm_MASK) >> USBFS_Dm_SHIFT;
}


/*******************************************************************************
* Function Name: USBFS_Dm_ReadDataReg
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
uint8 USBFS_Dm_ReadDataReg(void) 
{
    return (USBFS_Dm_DR & USBFS_Dm_MASK) >> USBFS_Dm_SHIFT;
}


/* If Interrupts Are Enabled for this Pins component */ 
#if defined(USBFS_Dm_INTSTAT) 

    /*******************************************************************************
    * Function Name: USBFS_Dm_ClearInterrupt
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
    uint8 USBFS_Dm_ClearInterrupt(void) 
    {
        return (USBFS_Dm_INTSTAT & USBFS_Dm_MASK) >> USBFS_Dm_SHIFT;
    }

#endif /* If Interrupts Are Enabled for this Pins component */ 

#endif /* CY_PSOC5A... */

    
/* [] END OF FILE */
