/*******************************************************************************
* File Name: SCSI_Out_Bits_PM.c
* Version 1.80
*
* Description:
*  This file contains the setup, control, and status commands to support 
*  the component operation in the low power mode. 
*
* Note:
*
********************************************************************************
* Copyright 2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#include "SCSI_Out_Bits.h"

/* Check for removal by optimization */
#if !defined(SCSI_Out_Bits_Sync_ctrl_reg__REMOVED)

static SCSI_Out_Bits_BACKUP_STRUCT  SCSI_Out_Bits_backup = {0u};

    
/*******************************************************************************
* Function Name: SCSI_Out_Bits_SaveConfig
********************************************************************************
*
* Summary:
*  Saves the control register value.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void SCSI_Out_Bits_SaveConfig(void) 
{
    SCSI_Out_Bits_backup.controlState = SCSI_Out_Bits_Control;
}


/*******************************************************************************
* Function Name: SCSI_Out_Bits_RestoreConfig
********************************************************************************
*
* Summary:
*  Restores the control register value.
*
* Parameters:
*  None
*
* Return:
*  None
*
*
*******************************************************************************/
void SCSI_Out_Bits_RestoreConfig(void) 
{
     SCSI_Out_Bits_Control = SCSI_Out_Bits_backup.controlState;
}


/*******************************************************************************
* Function Name: SCSI_Out_Bits_Sleep
********************************************************************************
*
* Summary:
*  Prepares the component for entering the low power mode.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void SCSI_Out_Bits_Sleep(void) 
{
    SCSI_Out_Bits_SaveConfig();
}


/*******************************************************************************
* Function Name: SCSI_Out_Bits_Wakeup
********************************************************************************
*
* Summary:
*  Restores the component after waking up from the low power mode.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void SCSI_Out_Bits_Wakeup(void)  
{
    SCSI_Out_Bits_RestoreConfig();
}

#endif /* End check for removal by optimization */


/* [] END OF FILE */
