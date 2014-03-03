/*******************************************************************************
* File Name: SCSI_CMD_TIMER_PM.c
* Version 2.50
*
*  Description:
*     This file provides the power management source code to API for the
*     Timer.
*
*   Note:
*     None
*
*******************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/

#include "SCSI_CMD_TIMER.h"
static SCSI_CMD_TIMER_backupStruct SCSI_CMD_TIMER_backup;


/*******************************************************************************
* Function Name: SCSI_CMD_TIMER_SaveConfig
********************************************************************************
*
* Summary:
*     Save the current user configuration
*
* Parameters:
*  void
*
* Return:
*  void
*
* Global variables:
*  SCSI_CMD_TIMER_backup:  Variables of this global structure are modified to
*  store the values of non retention configuration registers when Sleep() API is
*  called.
*
*******************************************************************************/
void SCSI_CMD_TIMER_SaveConfig(void) 
{
    #if (!SCSI_CMD_TIMER_UsingFixedFunction)
        /* Backup the UDB non-rentention registers for CY_UDB_V0 */
        #if (CY_UDB_V0)
            SCSI_CMD_TIMER_backup.TimerUdb = SCSI_CMD_TIMER_ReadCounter();
            SCSI_CMD_TIMER_backup.TimerPeriod = SCSI_CMD_TIMER_ReadPeriod();
            SCSI_CMD_TIMER_backup.InterruptMaskValue = SCSI_CMD_TIMER_STATUS_MASK;
            #if (SCSI_CMD_TIMER_UsingHWCaptureCounter)
                SCSI_CMD_TIMER_backup.TimerCaptureCounter = SCSI_CMD_TIMER_ReadCaptureCount();
            #endif /* Backup the UDB non-rentention register capture counter for CY_UDB_V0 */
        #endif /* Backup the UDB non-rentention registers for CY_UDB_V0 */

        #if (CY_UDB_V1)
            SCSI_CMD_TIMER_backup.TimerUdb = SCSI_CMD_TIMER_ReadCounter();
            SCSI_CMD_TIMER_backup.InterruptMaskValue = SCSI_CMD_TIMER_STATUS_MASK;
            #if (SCSI_CMD_TIMER_UsingHWCaptureCounter)
                SCSI_CMD_TIMER_backup.TimerCaptureCounter = SCSI_CMD_TIMER_ReadCaptureCount();
            #endif /* Back Up capture counter register  */
        #endif /* Backup non retention registers, interrupt mask and capture counter for CY_UDB_V1 */

        #if(!SCSI_CMD_TIMER_ControlRegRemoved)
            SCSI_CMD_TIMER_backup.TimerControlRegister = SCSI_CMD_TIMER_ReadControlRegister();
        #endif /* Backup the enable state of the Timer component */
    #endif /* Backup non retention registers in UDB implementation. All fixed function registers are retention */
}


/*******************************************************************************
* Function Name: SCSI_CMD_TIMER_RestoreConfig
********************************************************************************
*
* Summary:
*  Restores the current user configuration.
*
* Parameters:
*  void
*
* Return:
*  void
*
* Global variables:
*  SCSI_CMD_TIMER_backup:  Variables of this global structure are used to
*  restore the values of non retention registers on wakeup from sleep mode.
*
*******************************************************************************/
void SCSI_CMD_TIMER_RestoreConfig(void) 
{   
    #if (!SCSI_CMD_TIMER_UsingFixedFunction)
        /* Restore the UDB non-rentention registers for CY_UDB_V0 */
        #if (CY_UDB_V0)
            /* Interrupt State Backup for Critical Region*/
            uint8 SCSI_CMD_TIMER_interruptState;

            SCSI_CMD_TIMER_WriteCounter(SCSI_CMD_TIMER_backup.TimerUdb);
            SCSI_CMD_TIMER_WritePeriod(SCSI_CMD_TIMER_backup.TimerPeriod);
            /* CyEnterCriticalRegion and CyExitCriticalRegion are used to mark following region critical*/
            /* Enter Critical Region*/
            SCSI_CMD_TIMER_interruptState = CyEnterCriticalSection();
            /* Use the interrupt output of the status register for IRQ output */
            SCSI_CMD_TIMER_STATUS_AUX_CTRL |= SCSI_CMD_TIMER_STATUS_ACTL_INT_EN_MASK;
            /* Exit Critical Region*/
            CyExitCriticalSection(SCSI_CMD_TIMER_interruptState);
            SCSI_CMD_TIMER_STATUS_MASK =SCSI_CMD_TIMER_backup.InterruptMaskValue;
            #if (SCSI_CMD_TIMER_UsingHWCaptureCounter)
                SCSI_CMD_TIMER_SetCaptureCount(SCSI_CMD_TIMER_backup.TimerCaptureCounter);
            #endif /* Restore the UDB non-rentention register capture counter for CY_UDB_V0 */
        #endif /* Restore the UDB non-rentention registers for CY_UDB_V0 */

        #if (CY_UDB_V1)
            SCSI_CMD_TIMER_WriteCounter(SCSI_CMD_TIMER_backup.TimerUdb);
            SCSI_CMD_TIMER_STATUS_MASK =SCSI_CMD_TIMER_backup.InterruptMaskValue;
            #if (SCSI_CMD_TIMER_UsingHWCaptureCounter)
                SCSI_CMD_TIMER_SetCaptureCount(SCSI_CMD_TIMER_backup.TimerCaptureCounter);
            #endif /* Restore Capture counter register*/
        #endif /* Restore up non retention registers, interrupt mask and capture counter for CY_UDB_V1 */

        #if(!SCSI_CMD_TIMER_ControlRegRemoved)
            SCSI_CMD_TIMER_WriteControlRegister(SCSI_CMD_TIMER_backup.TimerControlRegister);
        #endif /* Restore the enable state of the Timer component */
    #endif /* Restore non retention registers in the UDB implementation only */
}


/*******************************************************************************
* Function Name: SCSI_CMD_TIMER_Sleep
********************************************************************************
*
* Summary:
*     Stop and Save the user configuration
*
* Parameters:
*  void
*
* Return:
*  void
*
* Global variables:
*  SCSI_CMD_TIMER_backup.TimerEnableState:  Is modified depending on the
*  enable state of the block before entering sleep mode.
*
*******************************************************************************/
void SCSI_CMD_TIMER_Sleep(void) 
{
    #if(!SCSI_CMD_TIMER_ControlRegRemoved)
        /* Save Counter's enable state */
        if(SCSI_CMD_TIMER_CTRL_ENABLE == (SCSI_CMD_TIMER_CONTROL & SCSI_CMD_TIMER_CTRL_ENABLE))
        {
            /* Timer is enabled */
            SCSI_CMD_TIMER_backup.TimerEnableState = 1u;
        }
        else
        {
            /* Timer is disabled */
            SCSI_CMD_TIMER_backup.TimerEnableState = 0u;
        }
    #endif /* Back up enable state from the Timer control register */
    SCSI_CMD_TIMER_Stop();
    SCSI_CMD_TIMER_SaveConfig();
}


/*******************************************************************************
* Function Name: SCSI_CMD_TIMER_Wakeup
********************************************************************************
*
* Summary:
*  Restores and enables the user configuration
*
* Parameters:
*  void
*
* Return:
*  void
*
* Global variables:
*  SCSI_CMD_TIMER_backup.enableState:  Is used to restore the enable state of
*  block on wakeup from sleep mode.
*
*******************************************************************************/
void SCSI_CMD_TIMER_Wakeup(void) 
{
    SCSI_CMD_TIMER_RestoreConfig();
    #if(!SCSI_CMD_TIMER_ControlRegRemoved)
        if(SCSI_CMD_TIMER_backup.TimerEnableState == 1u)
        {     /* Enable Timer's operation */
                SCSI_CMD_TIMER_Enable();
        } /* Do nothing if Timer was disabled before */
    #endif /* Remove this code section if Control register is removed */
}


/* [] END OF FILE */
