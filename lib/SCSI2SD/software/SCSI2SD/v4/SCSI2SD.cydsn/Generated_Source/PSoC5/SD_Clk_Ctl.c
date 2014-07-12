/*******************************************************************************
* File Name: SD_Clk_Ctl.c  
* Version 1.70
*
* Description:
*  This file contains API to enable firmware control of a Control Register.
*
* Note:
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#include "SD_Clk_Ctl.h"

#if !defined(SD_Clk_Ctl_Sync_ctrl_reg__REMOVED) /* Check for removal by optimization */

/*******************************************************************************
* Function Name: SD_Clk_Ctl_Write
********************************************************************************
*
* Summary:
*  Write a byte to the Control Register.
*
* Parameters:
*  control:  The value to be assigned to the Control Register.
*
* Return:
*  None.
*
*******************************************************************************/
void SD_Clk_Ctl_Write(uint8 control) 
{
    SD_Clk_Ctl_Control = control;
}


/*******************************************************************************
* Function Name: SD_Clk_Ctl_Read
********************************************************************************
*
* Summary:
*  Reads the current value assigned to the Control Register.
*
* Parameters:
*  None.
*
* Return:
*  Returns the current value in the Control Register.
*
*******************************************************************************/
uint8 SD_Clk_Ctl_Read(void) 
{
    return SD_Clk_Ctl_Control;
}

#endif /* End check for removal by optimization */


/* [] END OF FILE */
