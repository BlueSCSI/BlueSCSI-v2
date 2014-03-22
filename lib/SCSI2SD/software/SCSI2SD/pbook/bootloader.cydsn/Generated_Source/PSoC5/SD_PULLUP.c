/*******************************************************************************
* File Name: SD_PULLUP.c  
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
#include "SD_PULLUP.h"

/* APIs are not generated for P15[7:6] on PSoC 5 */
#if !(CY_PSOC5A &&\
	 SD_PULLUP__PORT == 15 && ((SD_PULLUP__MASK & 0xC0) != 0))


/*******************************************************************************
* Function Name: SD_PULLUP_Write
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
void SD_PULLUP_Write(uint8 value) 
{
    uint8 staticBits = (SD_PULLUP_DR & (uint8)(~SD_PULLUP_MASK));
    SD_PULLUP_DR = staticBits | ((uint8)(value << SD_PULLUP_SHIFT) & SD_PULLUP_MASK);
}


/*******************************************************************************
* Function Name: SD_PULLUP_SetDriveMode
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
void SD_PULLUP_SetDriveMode(uint8 mode) 
{
	CyPins_SetPinDriveMode(SD_PULLUP_0, mode);
	CyPins_SetPinDriveMode(SD_PULLUP_1, mode);
	CyPins_SetPinDriveMode(SD_PULLUP_2, mode);
	CyPins_SetPinDriveMode(SD_PULLUP_3, mode);
	CyPins_SetPinDriveMode(SD_PULLUP_4, mode);
}


/*******************************************************************************
* Function Name: SD_PULLUP_Read
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
*  Macro SD_PULLUP_ReadPS calls this function. 
*  
*******************************************************************************/
uint8 SD_PULLUP_Read(void) 
{
    return (SD_PULLUP_PS & SD_PULLUP_MASK) >> SD_PULLUP_SHIFT;
}


/*******************************************************************************
* Function Name: SD_PULLUP_ReadDataReg
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
uint8 SD_PULLUP_ReadDataReg(void) 
{
    return (SD_PULLUP_DR & SD_PULLUP_MASK) >> SD_PULLUP_SHIFT;
}


/* If Interrupts Are Enabled for this Pins component */ 
#if defined(SD_PULLUP_INTSTAT) 

    /*******************************************************************************
    * Function Name: SD_PULLUP_ClearInterrupt
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
    uint8 SD_PULLUP_ClearInterrupt(void) 
    {
        return (SD_PULLUP_INTSTAT & SD_PULLUP_MASK) >> SD_PULLUP_SHIFT;
    }

#endif /* If Interrupts Are Enabled for this Pins component */ 

#endif /* CY_PSOC5A... */

    
/* [] END OF FILE */
