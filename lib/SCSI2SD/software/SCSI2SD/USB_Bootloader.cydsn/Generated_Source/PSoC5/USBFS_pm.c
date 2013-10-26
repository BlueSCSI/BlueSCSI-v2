/*******************************************************************************
* File Name: USBFS_pm.c
* Version 2.60
*
* Description:
*  This file provides Suspend/Resume APIs functionality.
*
* Note:
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "project.h"
#include "USBFS.h"
#include "USBFS_pvt.h"


/***************************************
* Custom Declarations
***************************************/
/* `#START PM_CUSTOM_DECLARATIONS` Place your declaration here */

/* `#END` */


/***************************************
* Local data allocation
***************************************/

static USBFS_BACKUP_STRUCT  USBFS_backup;


#if(USBFS_DP_ISR_REMOVE == 0u)


    /*******************************************************************************
    * Function Name: USBFS_DP_Interrupt
    ********************************************************************************
    *
    * Summary:
    *  This Interrupt Service Routine handles DP pin changes for wake-up from
    *  the sleep mode.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    CY_ISR(USBFS_DP_ISR)
    {
        /* `#START DP_USER_CODE` Place your code here */

        /* `#END` */

        /* Clears active interrupt */
        CY_GET_REG8(USBFS_DP_INTSTAT_PTR);
    }

#endif /* (USBFS_DP_ISR_REMOVE == 0u) */


/*******************************************************************************
* Function Name: USBFS_SaveConfig
********************************************************************************
*
* Summary:
*  Saves the current user configuration.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void USBFS_SaveConfig(void) 
{

}


/*******************************************************************************
* Function Name: USBFS_RestoreConfig
********************************************************************************
*
* Summary:
*  Restores the current user configuration.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void USBFS_RestoreConfig(void) 
{
    if(USBFS_configuration != 0u)
    {
        USBFS_ConfigReg();
    }
}


/*******************************************************************************
* Function Name: USBFS_Suspend
********************************************************************************
*
* Summary:
*  This function disables the USBFS block and prepares for power donwn mode.
*
* Parameters:
*  None.
*
* Return:
*   None.
*
* Global variables:
*  USBFS_backup.enable:  modified.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void USBFS_Suspend(void) 
{
    uint8 enableInterrupts;
    enableInterrupts = CyEnterCriticalSection();

    if((CY_GET_REG8(USBFS_CR0_PTR) & USBFS_CR0_ENABLE) != 0u)
    {   /* USB block is enabled */
        USBFS_backup.enableState = 1u;

        #if(USBFS_EP_MM != USBFS__EP_MANUAL)
            USBFS_Stop_DMA(USBFS_MAX_EP);     /* Stop all DMAs */
        #endif   /* End USBFS_EP_MM != USBFS__EP_MANUAL */

        /* Ensure USB transmit enable is low (USB_USBIO_CR0.ten). - Manual Transmission - Disabled */
        USBFS_USBIO_CR0_REG &= (uint8)~USBFS_USBIO_CR0_TEN;
        CyDelayUs(0u);  /*~50ns delay */

        /* Disable the USBIO by asserting PM.USB_CR0.fsusbio_pd_n(Inverted) and pd_pullup_hv(Inverted) high. */
        USBFS_PM_USB_CR0_REG &=
                                (uint8)~(USBFS_PM_USB_CR0_PD_N | USBFS_PM_USB_CR0_PD_PULLUP_N);

        /* Disable the SIE */
        USBFS_CR0_REG &= (uint8)~USBFS_CR0_ENABLE;

        CyDelayUs(0u);  /*~50ns delay */
        /* Store mode and Disable VRegulator*/
        USBFS_backup.mode = USBFS_CR1_REG & USBFS_CR1_REG_ENABLE;
        USBFS_CR1_REG &= (uint8)~USBFS_CR1_REG_ENABLE;

        CyDelayUs(1u);  /* 0.5 us min delay */
        /* Disable the USBIO reference by setting PM.USB_CR0.fsusbio_ref_en.*/
        USBFS_PM_USB_CR0_REG &= (uint8)~USBFS_PM_USB_CR0_REF_EN;

        /* Switch DP and DM terminals to GPIO mode and disconnect 1.5k pullup*/
        USBFS_USBIO_CR1_REG |= USBFS_USBIO_CR1_IOMODE;

        /* Disable USB in ACT PM */
        USBFS_PM_ACT_CFG_REG &= (uint8)~USBFS_PM_ACT_EN_FSUSB;
        /* Disable USB block for Standby Power Mode */
        USBFS_PM_STBY_CFG_REG &= (uint8)~USBFS_PM_STBY_EN_FSUSB;
        CyDelayUs(1u); /* min  0.5us delay required */

    }
    else
    {
        USBFS_backup.enableState = 0u;
    }
    CyExitCriticalSection(enableInterrupts);

    /* Set the DP Interrupt for wake-up from sleep mode. */
    #if(USBFS_DP_ISR_REMOVE == 0u)
        (void) CyIntSetVector(USBFS_DP_INTC_VECT_NUM,   &USBFS_DP_ISR);
        CyIntSetPriority(USBFS_DP_INTC_VECT_NUM, USBFS_DP_INTC_PRIOR);
        CyIntClearPending(USBFS_DP_INTC_VECT_NUM);
        CyIntEnable(USBFS_DP_INTC_VECT_NUM);
    #endif /* (USBFS_DP_ISR_REMOVE == 0u) */

}


/*******************************************************************************
* Function Name: USBFS_Resume
********************************************************************************
*
* Summary:
*  This function enables the USBFS block after power down mode.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global variables:
*  USBFS_backup - checked.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void USBFS_Resume(void) 
{
    uint8 enableInterrupts;
    enableInterrupts = CyEnterCriticalSection();

    if(USBFS_backup.enableState != 0u)
    {
        #if(USBFS_DP_ISR_REMOVE == 0u)
            CyIntDisable(USBFS_DP_INTC_VECT_NUM);
        #endif /* End USBFS_DP_ISR_REMOVE */

        /* Enable USB block */
        USBFS_PM_ACT_CFG_REG |= USBFS_PM_ACT_EN_FSUSB;
        /* Enable USB block for Standby Power Mode */
        USBFS_PM_STBY_CFG_REG |= USBFS_PM_STBY_EN_FSUSB;
        /* Enable core clock */
        USBFS_USB_CLK_EN_REG |= USBFS_USB_CLK_ENABLE;

        /* Enable the USBIO reference by setting PM.USB_CR0.fsusbio_ref_en.*/
        USBFS_PM_USB_CR0_REG |= USBFS_PM_USB_CR0_REF_EN;
        /* The reference will be available ~40us after power restored */
        CyDelayUs(40u);
        /* Return VRegulator*/
        USBFS_CR1_REG |= USBFS_backup.mode;
        CyDelayUs(0u);  /*~50ns delay */
        /* Enable USBIO */
        USBFS_PM_USB_CR0_REG |= USBFS_PM_USB_CR0_PD_N;
        CyDelayUs(2u);
        /* Set the USBIO pull-up enable */
        USBFS_PM_USB_CR0_REG |= USBFS_PM_USB_CR0_PD_PULLUP_N;

        /* Reinit Arbiter configuration for DMA transfers */
        #if(USBFS_EP_MM != USBFS__EP_MANUAL)
            /* usb arb interrupt enable */
            USBFS_ARB_INT_EN_REG = USBFS_ARB_INT_MASK;
            #if(USBFS_EP_MM == USBFS__EP_DMAMANUAL)
                USBFS_ARB_CFG_REG = USBFS_ARB_CFG_MANUAL_DMA;
            #endif   /* End USBFS_EP_MM == USBFS__EP_DMAMANUAL */
            #if(USBFS_EP_MM == USBFS__EP_DMAAUTO)
                /*Set cfg cmplt this rises DMA request when the full configuration is done */
                USBFS_ARB_CFG_REG = USBFS_ARB_CFG_AUTO_DMA | USBFS_ARB_CFG_AUTO_MEM;
            #endif   /* End USBFS_EP_MM == USBFS__EP_DMAAUTO */
        #endif   /* End USBFS_EP_MM != USBFS__EP_MANUAL */

        /* STALL_IN_OUT */
        CY_SET_REG8(USBFS_EP0_CR_PTR, USBFS_MODE_STALL_IN_OUT);
        /* Enable the SIE with a last address */
        USBFS_CR0_REG |= USBFS_CR0_ENABLE;
        CyDelayCycles(1u);
        /* Finally, Enable d+ pullup and select iomode to USB mode*/
        CY_SET_REG8(USBFS_USBIO_CR1_PTR, USBFS_USBIO_CR1_USBPUEN);

        /* Restore USB register settings */
        USBFS_RestoreConfig();

    }
    CyExitCriticalSection(enableInterrupts);
}


/* [] END OF FILE */
