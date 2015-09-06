/*******************************************************************************
* File Name: SDCard_PM.c
* Version 2.50
*
* Description:
*  This file contains the setup, control and status commands to support
*  component operations in low power mode.
*
* Note:
*
********************************************************************************
* Copyright 2008-2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "SDCard_PVT.h"

static SDCard_BACKUP_STRUCT SDCard_backup =
{
    SDCard_DISABLED,
    SDCard_BITCTR_INIT,
};


/*******************************************************************************
* Function Name: SDCard_SaveConfig
********************************************************************************
*
* Summary:
*  Empty function. Included for consistency with other components.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void SDCard_SaveConfig(void) 
{

}


/*******************************************************************************
* Function Name: SDCard_RestoreConfig
********************************************************************************
*
* Summary:
*  Empty function. Included for consistency with other components.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void SDCard_RestoreConfig(void) 
{

}


/*******************************************************************************
* Function Name: SDCard_Sleep
********************************************************************************
*
* Summary:
*  Prepare SPIM Component goes to sleep.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global Variables:
*  SDCard_backup - modified when non-retention registers are saved.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_Sleep(void) 
{
    /* Save components enable state */
    SDCard_backup.enableState = ((uint8) SDCard_IS_ENABLED);

    SDCard_Stop();
}


/*******************************************************************************
* Function Name: SDCard_Wakeup
********************************************************************************
*
* Summary:
*  Prepare SPIM Component to wake up.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global Variables:
*  SDCard_backup - used when non-retention registers are restored.
*  SDCard_txBufferWrite - modified every function call - resets to
*  zero.
*  SDCard_txBufferRead - modified every function call - resets to
*  zero.
*  SDCard_rxBufferWrite - modified every function call - resets to
*  zero.
*  SDCard_rxBufferRead - modified every function call - resets to
*  zero.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_Wakeup(void) 
{
    #if(SDCard_RX_SOFTWARE_BUF_ENABLED)
        SDCard_rxBufferFull  = 0u;
        SDCard_rxBufferRead  = 0u;
        SDCard_rxBufferWrite = 0u;
    #endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */

    #if(SDCard_TX_SOFTWARE_BUF_ENABLED)
        SDCard_txBufferFull  = 0u;
        SDCard_txBufferRead  = 0u;
        SDCard_txBufferWrite = 0u;
    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

    /* Clear any data from the RX and TX FIFO */
    SDCard_ClearFIFO();

    /* Restore components block enable state */
    if(0u != SDCard_backup.enableState)
    {
        SDCard_Enable();
    }
}


/* [] END OF FILE */
