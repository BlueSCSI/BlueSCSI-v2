/*******************************************************************************
* File Name: SD_DAT1.c  
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
#include "SD_DAT1.h"

/* APIs are not generated for P15[7:6] on PSoC 5 */
#if !(CY_PSOC5A &&\
	 SD_DAT1__PORT == 15 && ((SD_DAT1__MASK & 0xC0) != 0))


/*******************************************************************************
* Function Name: SD_DAT1_Write
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
void SD_DAT1_Write(uint8 value) 
{
    uint8 staticBits = (SD_DAT1_DR & (uint8)(~SD_DAT1_MASK));
    SD_DAT1_DR = staticBits | ((uint8)(value << SD_DAT1_SHIFT) & SD_DAT1_MASK);
}


/*******************************************************************************
* Function Name: SD_DAT1_SetDriveMode
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
void SD_DAT1_SetDriveMode(uint8 mode) 
{
	CyPins_SetPinDriveMode(SD_DAT1_0, mode);
}


/*******************************************************************************
* Function Name: SD_DAT1_Read
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
*  Macro SD_DAT1_ReadPS calls this function. 
*  
*******************************************************************************/
uint8 SD_DAT1_Read(void) 
{
    return (SD_DAT1_PS & SD_DAT1_MASK) >> SD_DAT1_SHIFT;
}


/*******************************************************************************
* Function Name: SD_DAT1_ReadDataReg
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
uint8 SD_DAT1_ReadDataReg(void) 
{
    return (SD_DAT1_DR & SD_DAT1_MASK) >> SD_DAT1_SHIFT;
}


/* If Interrupts Are Enabled for this Pins component */ 
#if defined(SD_DAT1_INTSTAT) 

    /*******************************************************************************
    * Function Name: SD_DAT1_ClearInterrupt
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
    uint8 SD_DAT1_ClearInterrupt(void) 
    {
        return (SD_DAT1_INTSTAT & SD_DAT1_MASK) >> SD_DAT1_SHIFT;
    }

#endif /* If Interrupts Are Enabled for this Pins component */ 

#endif /* CY_PSOC5A... */

    
/* [] END OF FILE */
