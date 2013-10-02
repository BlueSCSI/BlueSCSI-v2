/*******************************************************************************
* File Name: SD_MISO.c  
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
#include "SD_MISO.h"

/* APIs are not generated for P15[7:6] on PSoC 5 */
#if !(CY_PSOC5A &&\
	 SD_MISO__PORT == 15 && ((SD_MISO__MASK & 0xC0) != 0))


/*******************************************************************************
* Function Name: SD_MISO_Write
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
void SD_MISO_Write(uint8 value) 
{
    uint8 staticBits = (SD_MISO_DR & (uint8)(~SD_MISO_MASK));
    SD_MISO_DR = staticBits | ((uint8)(value << SD_MISO_SHIFT) & SD_MISO_MASK);
}


/*******************************************************************************
* Function Name: SD_MISO_SetDriveMode
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
void SD_MISO_SetDriveMode(uint8 mode) 
{
	CyPins_SetPinDriveMode(SD_MISO_0, mode);
}


/*******************************************************************************
* Function Name: SD_MISO_Read
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
*  Macro SD_MISO_ReadPS calls this function. 
*  
*******************************************************************************/
uint8 SD_MISO_Read(void) 
{
    return (SD_MISO_PS & SD_MISO_MASK) >> SD_MISO_SHIFT;
}


/*******************************************************************************
* Function Name: SD_MISO_ReadDataReg
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
uint8 SD_MISO_ReadDataReg(void) 
{
    return (SD_MISO_DR & SD_MISO_MASK) >> SD_MISO_SHIFT;
}


/* If Interrupts Are Enabled for this Pins component */ 
#if defined(SD_MISO_INTSTAT) 

    /*******************************************************************************
    * Function Name: SD_MISO_ClearInterrupt
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
    uint8 SD_MISO_ClearInterrupt(void) 
    {
        return (SD_MISO_INTSTAT & SD_MISO_MASK) >> SD_MISO_SHIFT;
    }

#endif /* If Interrupts Are Enabled for this Pins component */ 

#endif /* CY_PSOC5A... */

    
/* [] END OF FILE */
