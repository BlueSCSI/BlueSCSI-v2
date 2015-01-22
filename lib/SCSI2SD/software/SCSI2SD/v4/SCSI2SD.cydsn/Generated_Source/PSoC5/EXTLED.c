/*******************************************************************************
* File Name: EXTLED.c  
* Version 2.10
*
* Description:
*  This file contains API to enable firmware control of a Pins component.
*
* Note:
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#include "cytypes.h"
#include "EXTLED.h"

/* APIs are not generated for P15[7:6] on PSoC 5 */
#if !(CY_PSOC5A &&\
	 EXTLED__PORT == 15 && ((EXTLED__MASK & 0xC0) != 0))


/*******************************************************************************
* Function Name: EXTLED_Write
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
void EXTLED_Write(uint8 value) 
{
    uint8 staticBits = (EXTLED_DR & (uint8)(~EXTLED_MASK));
    EXTLED_DR = staticBits | ((uint8)(value << EXTLED_SHIFT) & EXTLED_MASK);
}


/*******************************************************************************
* Function Name: EXTLED_SetDriveMode
********************************************************************************
*
* Summary:
*  Change the drive mode on the pins of the port.
* 
* Parameters:  
*  mode:  Change the pins to one of the following drive modes.
*
*  EXTLED_DM_STRONG     Strong Drive 
*  EXTLED_DM_OD_HI      Open Drain, Drives High 
*  EXTLED_DM_OD_LO      Open Drain, Drives Low 
*  EXTLED_DM_RES_UP     Resistive Pull Up 
*  EXTLED_DM_RES_DWN    Resistive Pull Down 
*  EXTLED_DM_RES_UPDWN  Resistive Pull Up/Down 
*  EXTLED_DM_DIG_HIZ    High Impedance Digital 
*  EXTLED_DM_ALG_HIZ    High Impedance Analog 
*
* Return: 
*  None
*
*******************************************************************************/
void EXTLED_SetDriveMode(uint8 mode) 
{
	CyPins_SetPinDriveMode(EXTLED_0, mode);
}


/*******************************************************************************
* Function Name: EXTLED_Read
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
*  Macro EXTLED_ReadPS calls this function. 
*  
*******************************************************************************/
uint8 EXTLED_Read(void) 
{
    return (EXTLED_PS & EXTLED_MASK) >> EXTLED_SHIFT;
}


/*******************************************************************************
* Function Name: EXTLED_ReadDataReg
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
uint8 EXTLED_ReadDataReg(void) 
{
    return (EXTLED_DR & EXTLED_MASK) >> EXTLED_SHIFT;
}


/* If Interrupts Are Enabled for this Pins component */ 
#if defined(EXTLED_INTSTAT) 

    /*******************************************************************************
    * Function Name: EXTLED_ClearInterrupt
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
    uint8 EXTLED_ClearInterrupt(void) 
    {
        return (EXTLED_INTSTAT & EXTLED_MASK) >> EXTLED_SHIFT;
    }

#endif /* If Interrupts Are Enabled for this Pins component */ 

#endif /* CY_PSOC5A... */

    
/* [] END OF FILE */
