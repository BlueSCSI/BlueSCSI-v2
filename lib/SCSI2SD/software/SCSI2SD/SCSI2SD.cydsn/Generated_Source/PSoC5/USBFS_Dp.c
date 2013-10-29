/*******************************************************************************
* File Name: USBFS_Dp.c  
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
#include "USBFS_Dp.h"

/* APIs are not generated for P15[7:6] on PSoC 5 */
#if !(CY_PSOC5A &&\
	 USBFS_Dp__PORT == 15 && ((USBFS_Dp__MASK & 0xC0) != 0))


/*******************************************************************************
* Function Name: USBFS_Dp_Write
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
void USBFS_Dp_Write(uint8 value) 
{
    uint8 staticBits = (USBFS_Dp_DR & (uint8)(~USBFS_Dp_MASK));
    USBFS_Dp_DR = staticBits | ((uint8)(value << USBFS_Dp_SHIFT) & USBFS_Dp_MASK);
}


/*******************************************************************************
* Function Name: USBFS_Dp_SetDriveMode
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
void USBFS_Dp_SetDriveMode(uint8 mode) 
{
	CyPins_SetPinDriveMode(USBFS_Dp_0, mode);
}


/*******************************************************************************
* Function Name: USBFS_Dp_Read
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
*  Macro USBFS_Dp_ReadPS calls this function. 
*  
*******************************************************************************/
uint8 USBFS_Dp_Read(void) 
{
    return (USBFS_Dp_PS & USBFS_Dp_MASK) >> USBFS_Dp_SHIFT;
}


/*******************************************************************************
* Function Name: USBFS_Dp_ReadDataReg
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
uint8 USBFS_Dp_ReadDataReg(void) 
{
    return (USBFS_Dp_DR & USBFS_Dp_MASK) >> USBFS_Dp_SHIFT;
}


/* If Interrupts Are Enabled for this Pins component */ 
#if defined(USBFS_Dp_INTSTAT) 

    /*******************************************************************************
    * Function Name: USBFS_Dp_ClearInterrupt
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
    uint8 USBFS_Dp_ClearInterrupt(void) 
    {
        return (USBFS_Dp_INTSTAT & USBFS_Dp_MASK) >> USBFS_Dp_SHIFT;
    }

#endif /* If Interrupts Are Enabled for this Pins component */ 

#endif /* CY_PSOC5A... */

    
/* [] END OF FILE */
