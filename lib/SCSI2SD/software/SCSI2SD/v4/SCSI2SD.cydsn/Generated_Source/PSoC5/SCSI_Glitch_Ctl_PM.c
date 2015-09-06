/*******************************************************************************
* File Name: SCSI_Glitch_Ctl_PM.c
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

#include "SCSI_Glitch_Ctl.h"

/* Check for removal by optimization */
#if !defined(SCSI_Glitch_Ctl_Sync_ctrl_reg__REMOVED)

static SCSI_Glitch_Ctl_BACKUP_STRUCT  SCSI_Glitch_Ctl_backup = {0u};

    
/*******************************************************************************
* Function Name: SCSI_Glitch_Ctl_SaveConfig
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
void SCSI_Glitch_Ctl_SaveConfig(void) 
{
    SCSI_Glitch_Ctl_backup.controlState = SCSI_Glitch_Ctl_Control;
}


/*******************************************************************************
* Function Name: SCSI_Glitch_Ctl_RestoreConfig
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
void SCSI_Glitch_Ctl_RestoreConfig(void) 
{
     SCSI_Glitch_Ctl_Control = SCSI_Glitch_Ctl_backup.controlState;
}


/*******************************************************************************
* Function Name: SCSI_Glitch_Ctl_Sleep
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
void SCSI_Glitch_Ctl_Sleep(void) 
{
    SCSI_Glitch_Ctl_SaveConfig();
}


/*******************************************************************************
* Function Name: SCSI_Glitch_Ctl_Wakeup
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
void SCSI_Glitch_Ctl_Wakeup(void)  
{
    SCSI_Glitch_Ctl_RestoreConfig();
}

#endif /* End check for removal by optimization */


/* [] END OF FILE */
