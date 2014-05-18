/*******************************************************************************
* File Name: Debug_Timer_PM.c
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

#include "Debug_Timer.h"
static Debug_Timer_backupStruct Debug_Timer_backup;


/*******************************************************************************
* Function Name: Debug_Timer_SaveConfig
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
*  Debug_Timer_backup:  Variables of this global structure are modified to
*  store the values of non retention configuration registers when Sleep() API is
*  called.
*
*******************************************************************************/
void Debug_Timer_SaveConfig(void) 
{
    #if (!Debug_Timer_UsingFixedFunction)
        /* Backup the UDB non-rentention registers for CY_UDB_V0 */
        #if (CY_UDB_V0)
            Debug_Timer_backup.TimerUdb = Debug_Timer_ReadCounter();
            Debug_Timer_backup.TimerPeriod = Debug_Timer_ReadPeriod();
            Debug_Timer_backup.InterruptMaskValue = Debug_Timer_STATUS_MASK;
            #if (Debug_Timer_UsingHWCaptureCounter)
                Debug_Timer_backup.TimerCaptureCounter = Debug_Timer_ReadCaptureCount();
            #endif /* Backup the UDB non-rentention register capture counter for CY_UDB_V0 */
        #endif /* Backup the UDB non-rentention registers for CY_UDB_V0 */

        #if (CY_UDB_V1)
            Debug_Timer_backup.TimerUdb = Debug_Timer_ReadCounter();
            Debug_Timer_backup.InterruptMaskValue = Debug_Timer_STATUS_MASK;
            #if (Debug_Timer_UsingHWCaptureCounter)
                Debug_Timer_backup.TimerCaptureCounter = Debug_Timer_ReadCaptureCount();
            #endif /* Back Up capture counter register  */
        #endif /* Backup non retention registers, interrupt mask and capture counter for CY_UDB_V1 */

        #if(!Debug_Timer_ControlRegRemoved)
            Debug_Timer_backup.TimerControlRegister = Debug_Timer_ReadControlRegister();
        #endif /* Backup the enable state of the Timer component */
    #endif /* Backup non retention registers in UDB implementation. All fixed function registers are retention */
}


/*******************************************************************************
* Function Name: Debug_Timer_RestoreConfig
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
*  Debug_Timer_backup:  Variables of this global structure are used to
*  restore the values of non retention registers on wakeup from sleep mode.
*
*******************************************************************************/
void Debug_Timer_RestoreConfig(void) 
{   
    #if (!Debug_Timer_UsingFixedFunction)
        /* Restore the UDB non-rentention registers for CY_UDB_V0 */
        #if (CY_UDB_V0)
            /* Interrupt State Backup for Critical Region*/
            uint8 Debug_Timer_interruptState;

            Debug_Timer_WriteCounter(Debug_Timer_backup.TimerUdb);
            Debug_Timer_WritePeriod(Debug_Timer_backup.TimerPeriod);
            /* CyEnterCriticalRegion and CyExitCriticalRegion are used to mark following region critical*/
            /* Enter Critical Region*/
            Debug_Timer_interruptState = CyEnterCriticalSection();
            /* Use the interrupt output of the status register for IRQ output */
            Debug_Timer_STATUS_AUX_CTRL |= Debug_Timer_STATUS_ACTL_INT_EN_MASK;
            /* Exit Critical Region*/
            CyExitCriticalSection(Debug_Timer_interruptState);
            Debug_Timer_STATUS_MASK =Debug_Timer_backup.InterruptMaskValue;
            #if (Debug_Timer_UsingHWCaptureCounter)
                Debug_Timer_SetCaptureCount(Debug_Timer_backup.TimerCaptureCounter);
            #endif /* Restore the UDB non-rentention register capture counter for CY_UDB_V0 */
        #endif /* Restore the UDB non-rentention registers for CY_UDB_V0 */

        #if (CY_UDB_V1)
            Debug_Timer_WriteCounter(Debug_Timer_backup.TimerUdb);
            Debug_Timer_STATUS_MASK =Debug_Timer_backup.InterruptMaskValue;
            #if (Debug_Timer_UsingHWCaptureCounter)
                Debug_Timer_SetCaptureCount(Debug_Timer_backup.TimerCaptureCounter);
            #endif /* Restore Capture counter register*/
        #endif /* Restore up non retention registers, interrupt mask and capture counter for CY_UDB_V1 */

        #if(!Debug_Timer_ControlRegRemoved)
            Debug_Timer_WriteControlRegister(Debug_Timer_backup.TimerControlRegister);
        #endif /* Restore the enable state of the Timer component */
    #endif /* Restore non retention registers in the UDB implementation only */
}


/*******************************************************************************
* Function Name: Debug_Timer_Sleep
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
*  Debug_Timer_backup.TimerEnableState:  Is modified depending on the
*  enable state of the block before entering sleep mode.
*
*******************************************************************************/
void Debug_Timer_Sleep(void) 
{
    #if(!Debug_Timer_ControlRegRemoved)
        /* Save Counter's enable state */
        if(Debug_Timer_CTRL_ENABLE == (Debug_Timer_CONTROL & Debug_Timer_CTRL_ENABLE))
        {
            /* Timer is enabled */
            Debug_Timer_backup.TimerEnableState = 1u;
        }
        else
        {
            /* Timer is disabled */
            Debug_Timer_backup.TimerEnableState = 0u;
        }
    #endif /* Back up enable state from the Timer control register */
    Debug_Timer_Stop();
    Debug_Timer_SaveConfig();
}


/*******************************************************************************
* Function Name: Debug_Timer_Wakeup
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
*  Debug_Timer_backup.enableState:  Is used to restore the enable state of
*  block on wakeup from sleep mode.
*
*******************************************************************************/
void Debug_Timer_Wakeup(void) 
{
    Debug_Timer_RestoreConfig();
    #if(!Debug_Timer_ControlRegRemoved)
        if(Debug_Timer_backup.TimerEnableState == 1u)
        {     /* Enable Timer's operation */
                Debug_Timer_Enable();
        } /* Do nothing if Timer was disabled before */
    #endif /* Remove this code section if Control register is removed */
}


/* [] END OF FILE */
