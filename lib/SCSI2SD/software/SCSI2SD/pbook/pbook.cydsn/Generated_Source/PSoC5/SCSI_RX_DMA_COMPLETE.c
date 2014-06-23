/*******************************************************************************
* File Name: SCSI_RX_DMA_COMPLETE.c  
* Version 1.70
*
*  Description:
*   API for controlling the state of an interrupt.
*
*
*  Note:
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/


#include <cydevice_trm.h>
#include <CyLib.h>
#include <SCSI_RX_DMA_COMPLETE.h>

#if !defined(SCSI_RX_DMA_COMPLETE__REMOVED) /* Check for removal by optimization */

/*******************************************************************************
*  Place your includes, defines and code here 
********************************************************************************/
/* `#START SCSI_RX_DMA_COMPLETE_intc` */

/* `#END` */

#ifndef CYINT_IRQ_BASE
#define CYINT_IRQ_BASE      16
#endif /* CYINT_IRQ_BASE */
#ifndef CYINT_VECT_TABLE
#define CYINT_VECT_TABLE    ((cyisraddress **) CYREG_NVIC_VECT_OFFSET)
#endif /* CYINT_VECT_TABLE */

/* Declared in startup, used to set unused interrupts to. */
CY_ISR_PROTO(IntDefaultHandler);


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_Start
********************************************************************************
*
* Summary:
*  Set up the interrupt and enable it.
*
* Parameters:  
*   None
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_Start(void)
{
    /* For all we know the interrupt is active. */
    SCSI_RX_DMA_COMPLETE_Disable();

    /* Set the ISR to point to the SCSI_RX_DMA_COMPLETE Interrupt. */
    SCSI_RX_DMA_COMPLETE_SetVector(&SCSI_RX_DMA_COMPLETE_Interrupt);

    /* Set the priority. */
    SCSI_RX_DMA_COMPLETE_SetPriority((uint8)SCSI_RX_DMA_COMPLETE_INTC_PRIOR_NUMBER);

    /* Enable it. */
    SCSI_RX_DMA_COMPLETE_Enable();
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_StartEx
********************************************************************************
*
* Summary:
*  Set up the interrupt and enable it.
*
* Parameters:  
*   address: Address of the ISR to set in the interrupt vector table.
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_StartEx(cyisraddress address)
{
    /* For all we know the interrupt is active. */
    SCSI_RX_DMA_COMPLETE_Disable();

    /* Set the ISR to point to the SCSI_RX_DMA_COMPLETE Interrupt. */
    SCSI_RX_DMA_COMPLETE_SetVector(address);

    /* Set the priority. */
    SCSI_RX_DMA_COMPLETE_SetPriority((uint8)SCSI_RX_DMA_COMPLETE_INTC_PRIOR_NUMBER);

    /* Enable it. */
    SCSI_RX_DMA_COMPLETE_Enable();
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_Stop
********************************************************************************
*
* Summary:
*   Disables and removes the interrupt.
*
* Parameters:  
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_Stop(void)
{
    /* Disable this interrupt. */
    SCSI_RX_DMA_COMPLETE_Disable();

    /* Set the ISR to point to the passive one. */
    SCSI_RX_DMA_COMPLETE_SetVector(&IntDefaultHandler);
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_Interrupt
********************************************************************************
*
* Summary:
*   The default Interrupt Service Routine for SCSI_RX_DMA_COMPLETE.
*
*   Add custom code between the coments to keep the next version of this file
*   from over writting your code.
*
* Parameters:  
*
* Return:
*   None
*
*******************************************************************************/
CY_ISR(SCSI_RX_DMA_COMPLETE_Interrupt)
{
    /*  Place your Interrupt code here. */
    /* `#START SCSI_RX_DMA_COMPLETE_Interrupt` */

    /* `#END` */
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_SetVector
********************************************************************************
*
* Summary:
*   Change the ISR vector for the Interrupt. Note calling SCSI_RX_DMA_COMPLETE_Start
*   will override any effect this method would have had. To set the vector 
*   before the component has been started use SCSI_RX_DMA_COMPLETE_StartEx instead.
*
* Parameters:
*   address: Address of the ISR to set in the interrupt vector table.
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_SetVector(cyisraddress address)
{
    cyisraddress * ramVectorTable;

    ramVectorTable = (cyisraddress *) *CYINT_VECT_TABLE;

    ramVectorTable[CYINT_IRQ_BASE + (uint32)SCSI_RX_DMA_COMPLETE__INTC_NUMBER] = address;
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_GetVector
********************************************************************************
*
* Summary:
*   Gets the "address" of the current ISR vector for the Interrupt.
*
* Parameters:
*   None
*
* Return:
*   Address of the ISR in the interrupt vector table.
*
*******************************************************************************/
cyisraddress SCSI_RX_DMA_COMPLETE_GetVector(void)
{
    cyisraddress * ramVectorTable;

    ramVectorTable = (cyisraddress *) *CYINT_VECT_TABLE;

    return ramVectorTable[CYINT_IRQ_BASE + (uint32)SCSI_RX_DMA_COMPLETE__INTC_NUMBER];
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_SetPriority
********************************************************************************
*
* Summary:
*   Sets the Priority of the Interrupt. Note calling SCSI_RX_DMA_COMPLETE_Start
*   or SCSI_RX_DMA_COMPLETE_StartEx will override any effect this method 
*   would have had. This method should only be called after 
*   SCSI_RX_DMA_COMPLETE_Start or SCSI_RX_DMA_COMPLETE_StartEx has been called. To set 
*   the initial priority for the component use the cydwr file in the tool.
*
* Parameters:
*   priority: Priority of the interrupt. 0 - 7, 0 being the highest.
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_SetPriority(uint8 priority)
{
    *SCSI_RX_DMA_COMPLETE_INTC_PRIOR = priority << 5;
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_GetPriority
********************************************************************************
*
* Summary:
*   Gets the Priority of the Interrupt.
*
* Parameters:
*   None
*
* Return:
*   Priority of the interrupt. 0 - 7, 0 being the highest.
*
*******************************************************************************/
uint8 SCSI_RX_DMA_COMPLETE_GetPriority(void)
{
    uint8 priority;


    priority = *SCSI_RX_DMA_COMPLETE_INTC_PRIOR >> 5;

    return priority;
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_Enable
********************************************************************************
*
* Summary:
*   Enables the interrupt.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_Enable(void)
{
    /* Enable the general interrupt. */
    *SCSI_RX_DMA_COMPLETE_INTC_SET_EN = SCSI_RX_DMA_COMPLETE__INTC_MASK;
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_GetState
********************************************************************************
*
* Summary:
*   Gets the state (enabled, disabled) of the Interrupt.
*
* Parameters:
*   None
*
* Return:
*   1 if enabled, 0 if disabled.
*
*******************************************************************************/
uint8 SCSI_RX_DMA_COMPLETE_GetState(void)
{
    /* Get the state of the general interrupt. */
    return ((*SCSI_RX_DMA_COMPLETE_INTC_SET_EN & (uint32)SCSI_RX_DMA_COMPLETE__INTC_MASK) != 0u) ? 1u:0u;
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_Disable
********************************************************************************
*
* Summary:
*   Disables the Interrupt.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_Disable(void)
{
    /* Disable the general interrupt. */
    *SCSI_RX_DMA_COMPLETE_INTC_CLR_EN = SCSI_RX_DMA_COMPLETE__INTC_MASK;
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_SetPending
********************************************************************************
*
* Summary:
*   Causes the Interrupt to enter the pending state, a software method of
*   generating the interrupt.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_SetPending(void)
{
    *SCSI_RX_DMA_COMPLETE_INTC_SET_PD = SCSI_RX_DMA_COMPLETE__INTC_MASK;
}


/*******************************************************************************
* Function Name: SCSI_RX_DMA_COMPLETE_ClearPending
********************************************************************************
*
* Summary:
*   Clears a pending interrupt.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
void SCSI_RX_DMA_COMPLETE_ClearPending(void)
{
    *SCSI_RX_DMA_COMPLETE_INTC_CLR_PD = SCSI_RX_DMA_COMPLETE__INTC_MASK;
}

#endif /* End check for removal by optimization */


/* [] END OF FILE */
