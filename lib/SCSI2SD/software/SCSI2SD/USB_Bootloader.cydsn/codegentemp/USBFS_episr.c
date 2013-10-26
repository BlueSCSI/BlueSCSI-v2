/*******************************************************************************
* File Name: USBFS_episr.c
* Version 2.60
*
* Description:
*  Data endpoint Interrupt Service Routines
*
* Note:
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "USBFS.h"
#include "USBFS_pvt.h"
#if defined(USBFS_ENABLE_MIDI_STREAMING) && (USBFS_ENABLE_MIDI_API != 0u)
    #include "USBFS_midi.h"
#endif /* End USBFS_ENABLE_MIDI_STREAMING*/


/***************************************
* Custom Declarations
***************************************/
/* `#START CUSTOM_DECLARATIONS` Place your declaration here */

/* `#END` */


#if(USBFS_EP1_ISR_REMOVE == 0u)


    /******************************************************************************
    * Function Name: USBFS_EP_1_ISR
    *******************************************************************************
    *
    * Summary:
    *  Endpoint 1 Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    ******************************************************************************/
    CY_ISR(USBFS_EP_1_ISR)
    {
        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            uint8 int_en;
        #endif /* USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 */

        /* `#START EP1_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            int_en = EA;
            CyGlobalIntEnable;  /* Make sure nested interrupt is enabled */
        #endif /* USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 */

        CY_GET_REG8(USBFS_SIE_EP1_CR0_PTR); /* Must read the mode reg */
        /* Do not toggle ISOC endpoint */
        if((USBFS_EP[USBFS_EP1].attrib & USBFS_EP_TYPE_MASK) !=
                                                                                    USBFS_EP_TYPE_ISOC)
        {
            USBFS_EP[USBFS_EP1].epToggle ^= USBFS_EPX_CNT_DATA_TOGGLE;
        }
        USBFS_EP[USBFS_EP1].apiEpState = USBFS_EVENT_PENDING;
        CY_SET_REG8(USBFS_SIE_EP_INT_SR_PTR, CY_GET_REG8(USBFS_SIE_EP_INT_SR_PTR) &
                                                                    (uint8)~USBFS_SIE_EP_INT_EP1_MASK);

        #if( defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT )
            if(USBFS_midi_out_ep == USBFS_EP1)
            {
                USBFS_MIDI_OUT_EP_Service();
            }
        #endif /* End USBFS_ISR_SERVICE_MIDI_OUT */

        /* `#START EP1_END_USER_CODE` Place your code here */

        /* `#END` */

        #if ( defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 )
            EA = int_en;
        #endif /* USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 */
    }

#endif   /* End USBFS_EP1_ISR_REMOVE */


#if(USBFS_EP2_ISR_REMOVE == 0u)

    /*******************************************************************************
    * Function Name: USBFS_EP_2_ISR
    ********************************************************************************
    *
    * Summary:
    *  Endpoint 2 Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    CY_ISR(USBFS_EP_2_ISR)
    {
        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            uint8 int_en;
        #endif /* USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 */

        /* `#START EP2_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 )
            int_en = EA;
            CyGlobalIntEnable;  /* Make sure nested interrupt is enabled */
        #endif /* USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 */

        CY_GET_REG8(USBFS_SIE_EP2_CR0_PTR); /* Must read the mode reg */
        /* Do not toggle ISOC endpoint */
        if((USBFS_EP[USBFS_EP2].attrib & USBFS_EP_TYPE_MASK) !=
                                                                                    USBFS_EP_TYPE_ISOC)
        {
            USBFS_EP[USBFS_EP2].epToggle ^= USBFS_EPX_CNT_DATA_TOGGLE;
        }
        USBFS_EP[USBFS_EP2].apiEpState = USBFS_EVENT_PENDING;
        CY_SET_REG8(USBFS_SIE_EP_INT_SR_PTR, CY_GET_REG8(USBFS_SIE_EP_INT_SR_PTR)
                                                                        & (uint8)~USBFS_SIE_EP_INT_EP2_MASK);

        #if( defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT )
            if(USBFS_midi_out_ep == USBFS_EP2)
            {
                USBFS_MIDI_OUT_EP_Service();
            }
        #endif /* End USBFS_ISR_SERVICE_MIDI_OUT */

        /* `#START EP2_END_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            EA = int_en;
        #endif /* USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 */
    }

#endif   /* End USBFS_EP2_ISR_REMOVE */


#if(USBFS_EP3_ISR_REMOVE == 0u)

    /*******************************************************************************
    * Function Name: USBFS_EP_3_ISR
    ********************************************************************************
    *
    * Summary:
    *  Endpoint 3 Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    CY_ISR(USBFS_EP_3_ISR)
    {
        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            uint8 int_en;
        #endif /* USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3 */

        /* `#START EP3_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            int_en = EA;
            CyGlobalIntEnable;  /* Make sure nested interrupt is enabled */
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        CY_GET_REG8(USBFS_SIE_EP3_CR0_PTR); /* Must read the mode reg */
        /* Do not toggle ISOC endpoint */
        if((USBFS_EP[USBFS_EP3].attrib & USBFS_EP_TYPE_MASK) !=
                                                                                    USBFS_EP_TYPE_ISOC)
        {
            USBFS_EP[USBFS_EP3].epToggle ^= USBFS_EPX_CNT_DATA_TOGGLE;
        }
        USBFS_EP[USBFS_EP3].apiEpState = USBFS_EVENT_PENDING;
        CY_SET_REG8(USBFS_SIE_EP_INT_SR_PTR, CY_GET_REG8(USBFS_SIE_EP_INT_SR_PTR)
                                                                        & (uint8)~USBFS_SIE_EP_INT_EP3_MASK);

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT)
            if(USBFS_midi_out_ep == USBFS_EP3)
            {
                USBFS_MIDI_OUT_EP_Service();
            }
        #endif /* End USBFS_ISR_SERVICE_MIDI_OUT */

        /* `#START EP3_END_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            EA = int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */
    }

#endif   /* End USBFS_EP3_ISR_REMOVE */


#if(USBFS_EP4_ISR_REMOVE == 0u)

    /*******************************************************************************
    * Function Name: USBFS_EP_4_ISR
    ********************************************************************************
    *
    * Summary:
    *  Endpoint 4 Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    CY_ISR(USBFS_EP_4_ISR)
    {
        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            uint8 int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        /* `#START EP4_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            int_en = EA;
            CyGlobalIntEnable;  /* Make sure nested interrupt is enabled */
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        CY_GET_REG8(USBFS_SIE_EP4_CR0_PTR); /* Must read the mode reg */
        /* Do not toggle ISOC endpoint */
        if((USBFS_EP[USBFS_EP4].attrib & USBFS_EP_TYPE_MASK) !=
                                                                                    USBFS_EP_TYPE_ISOC)
        {
            USBFS_EP[USBFS_EP4].epToggle ^= USBFS_EPX_CNT_DATA_TOGGLE;
        }
        USBFS_EP[USBFS_EP4].apiEpState = USBFS_EVENT_PENDING;
        CY_SET_REG8(USBFS_SIE_EP_INT_SR_PTR, CY_GET_REG8(USBFS_SIE_EP_INT_SR_PTR)
                                                                        & (uint8)~USBFS_SIE_EP_INT_EP4_MASK);

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT)
            if(USBFS_midi_out_ep == USBFS_EP4)
            {
                USBFS_MIDI_OUT_EP_Service();
            }
        #endif /* End USBFS_ISR_SERVICE_MIDI_OUT */

        /* `#START EP4_END_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            EA = int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */
    }

#endif   /* End USBFS_EP4_ISR_REMOVE */


#if(USBFS_EP5_ISR_REMOVE == 0u)

    /*******************************************************************************
    * Function Name: USBFS_EP_5_ISR
    ********************************************************************************
    *
    * Summary:
    *  Endpoint 5 Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    CY_ISR(USBFS_EP_5_ISR)
    {
        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            uint8 int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        /* `#START EP5_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            int_en = EA;
            CyGlobalIntEnable;  /* Make sure nested interrupt is enabled */
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        CY_GET_REG8(USBFS_SIE_EP5_CR0_PTR); /* Must read the mode reg */
        /* Do not toggle ISOC endpoint */
        if((USBFS_EP[USBFS_EP5].attrib & USBFS_EP_TYPE_MASK) !=
                                                                                    USBFS_EP_TYPE_ISOC)
        {
            USBFS_EP[USBFS_EP5].epToggle ^= USBFS_EPX_CNT_DATA_TOGGLE;
        }
        USBFS_EP[USBFS_EP5].apiEpState = USBFS_EVENT_PENDING;
        CY_SET_REG8(USBFS_SIE_EP_INT_SR_PTR, CY_GET_REG8(USBFS_SIE_EP_INT_SR_PTR)
                                                                        & (uint8)~USBFS_SIE_EP_INT_EP5_MASK);

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT)
            if(USBFS_midi_out_ep == USBFS_EP5)
            {
                USBFS_MIDI_OUT_EP_Service();
            }
        #endif /* End USBFS_ISR_SERVICE_MIDI_OUT */

        /* `#START EP5_END_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            EA = int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */
    }
#endif   /* End USBFS_EP5_ISR_REMOVE */


#if(USBFS_EP6_ISR_REMOVE == 0u)

    /*******************************************************************************
    * Function Name: USBFS_EP_6_ISR
    ********************************************************************************
    *
    * Summary:
    *  Endpoint 6 Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    CY_ISR(USBFS_EP_6_ISR)
    {
        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            uint8 int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        /* `#START EP6_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            int_en = EA;
            CyGlobalIntEnable;  /* Make sure nested interrupt is enabled */
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        CY_GET_REG8(USBFS_SIE_EP6_CR0_PTR); /* Must read the mode reg */
        /* Do not toggle ISOC endpoint */
        if((USBFS_EP[USBFS_EP6].attrib & USBFS_EP_TYPE_MASK) !=
                                                                                    USBFS_EP_TYPE_ISOC)
        {
            USBFS_EP[USBFS_EP6].epToggle ^= USBFS_EPX_CNT_DATA_TOGGLE;
        }
        USBFS_EP[USBFS_EP6].apiEpState = USBFS_EVENT_PENDING;
        CY_SET_REG8(USBFS_SIE_EP_INT_SR_PTR, CY_GET_REG8(USBFS_SIE_EP_INT_SR_PTR)
                                                                        & (uint8)~USBFS_SIE_EP_INT_EP6_MASK);

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT)
            if(USBFS_midi_out_ep == USBFS_EP6)
            {
                USBFS_MIDI_OUT_EP_Service();
            }
        #endif /* End USBFS_ISR_SERVICE_MIDI_OUT  */

        /* `#START EP6_END_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            EA = int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */
    }

#endif   /* End USBFS_EP6_ISR_REMOVE */


#if(USBFS_EP7_ISR_REMOVE == 0u)

    /*******************************************************************************
    * Function Name: USBFS_EP_7_ISR
    ********************************************************************************
    *
    * Summary:
    *  Endpoint 7 Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    CY_ISR(USBFS_EP_7_ISR)
    {
        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            uint8 int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        /* `#START EP7_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            int_en = EA;
            CyGlobalIntEnable;  /* Make sure nested interrupt is enabled */
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        CY_GET_REG8(USBFS_SIE_EP7_CR0_PTR); /* Must read the mode reg */
        /* Do not toggle ISOC endpoint */
        if((USBFS_EP[USBFS_EP7].attrib & USBFS_EP_TYPE_MASK) !=
                                                                                    USBFS_EP_TYPE_ISOC)
        {
            USBFS_EP[USBFS_EP7].epToggle ^= USBFS_EPX_CNT_DATA_TOGGLE;
        }
        USBFS_EP[USBFS_EP7].apiEpState = USBFS_EVENT_PENDING;
        CY_SET_REG8(USBFS_SIE_EP_INT_SR_PTR, CY_GET_REG8(USBFS_SIE_EP_INT_SR_PTR)
                                                                        & (uint8)~USBFS_SIE_EP_INT_EP7_MASK);

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT)
            if(USBFS_midi_out_ep == USBFS_EP7)
            {
                USBFS_MIDI_OUT_EP_Service();
            }
        #endif /* End USBFS_ISR_SERVICE_MIDI_OUT  */

        /* `#START EP7_END_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            EA = int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */
    }

#endif   /* End USBFS_EP7_ISR_REMOVE */


#if(USBFS_EP8_ISR_REMOVE == 0u)

    /*******************************************************************************
    * Function Name: USBFS_EP_8_ISR
    ********************************************************************************
    *
    * Summary:
    *  Endpoint 8 Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    CY_ISR(USBFS_EP_8_ISR)
    {
        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            uint8 int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        /* `#START EP8_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            int_en = EA;
            CyGlobalIntEnable;  /* Make sure nested interrupt is enabled */
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */

        CY_GET_REG8(USBFS_SIE_EP8_CR0_PTR); /* Must read the mode reg */
        /* Do not toggle ISOC endpoint */
        if((USBFS_EP[USBFS_EP8].attrib & USBFS_EP_TYPE_MASK) !=
                                                                                    USBFS_EP_TYPE_ISOC)
        {
            USBFS_EP[USBFS_EP8].epToggle ^= USBFS_EPX_CNT_DATA_TOGGLE;
        }
        USBFS_EP[USBFS_EP8].apiEpState = USBFS_EVENT_PENDING;
        CY_SET_REG8(USBFS_SIE_EP_INT_SR_PTR, CY_GET_REG8(USBFS_SIE_EP_INT_SR_PTR)
                                                                        & (uint8)~USBFS_SIE_EP_INT_EP8_MASK);

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT)
            if(USBFS_midi_out_ep == USBFS_EP8)
            {
                USBFS_MIDI_OUT_EP_Service();
            }
        #endif /* End USBFS_ISR_SERVICE_MIDI_OUT */

        /* `#START EP8_END_USER_CODE` Place your code here */

        /* `#END` */

        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_OUT && CY_PSOC3)
            EA = int_en;
        #endif /* CY_PSOC3 & USBFS_ISR_SERVICE_MIDI_OUT  */
    }

#endif   /* End USBFS_EP8_ISR_REMOVE */


/*******************************************************************************
* Function Name: USBFS_SOF_ISR
********************************************************************************
*
* Summary:
*  Start of Frame Interrupt Service Routine
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
CY_ISR(USBFS_SOF_ISR)
{
    /* `#START SOF_USER_CODE` Place your code here */

    /* `#END` */
}


/*******************************************************************************
* Function Name: USBFS_BUS_RESET_ISR
********************************************************************************
*
* Summary:
*  USB Bus Reset Interrupt Service Routine.  Calls _Start with the same
*  parameters as the last USER call to _Start
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
CY_ISR(USBFS_BUS_RESET_ISR)
{
    /* `#START BUS_RESET_USER_CODE` Place your code here */

    /* `#END` */

    USBFS_ReInitComponent();
}


#if((USBFS_EP_MM != USBFS__EP_MANUAL) && (USBFS_ARB_ISR_REMOVE == 0u))


    /*******************************************************************************
    * Function Name: USBFS_ARB_ISR
    ********************************************************************************
    *
    * Summary:
    *  Arbiter Interrupt Service Routine
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    * Side effect:
    *  Search for EP8 int_status will be much slower than search for EP1 int_status.
    *
    *******************************************************************************/
    CY_ISR(USBFS_ARB_ISR)
    {
        uint8 int_status;
        uint8 ep_status;
        uint8 ep = USBFS_EP1;
        uint8 ptr = 0u;

        /* `#START ARB_BEGIN_USER_CODE` Place your code here */

        /* `#END` */

        int_status = USBFS_ARB_INT_SR_REG;                   /* read Arbiter Status Register */
        USBFS_ARB_INT_SR_REG = int_status;                   /* Clear Serviced Interrupts */

        while(int_status != 0u)
        {
            if((int_status & 1u) != 0u)  /* If EpX interrupt present */
            {   /* read Endpoint Status Register */
                ep_status  = CY_GET_REG8((reg8 *)(USBFS_ARB_EP1_SR_IND + ptr));
                /* If In Buffer Full */
                if((ep_status & USBFS_ARB_EPX_SR_IN_BUF_FULL) != 0u)
                {
                    if((USBFS_EP[ep].addr & USBFS_DIR_IN) != 0u)
                    {
                        /* Clear Data ready status */
                        *(reg8 *)(USBFS_ARB_EP1_CFG_IND + ptr) &=
                                                                    (uint8)~USBFS_ARB_EPX_CFG_IN_DATA_RDY;
                        /* Write the Mode register */
                        CY_SET_REG8((reg8 *)(USBFS_SIE_EP1_CR0_IND + ptr), USBFS_EP[ep].epMode);
                        #if (defined(USBFS_ENABLE_MIDI_STREAMING) && USBFS_ISR_SERVICE_MIDI_IN)
                            if(ep == USBFS_midi_in_ep)
                            {   /* Clear MIDI input pointer */
                                USBFS_midiInPointer = 0u;
                            }
                        #endif /* End USBFS_ENABLE_MIDI_STREAMING*/
                    }
                }
                /* (re)arm Out EP only for mode2 */
                #if(USBFS_EP_MM != USBFS__EP_DMAAUTO)
                    /* If DMA Grant */
                    if((ep_status & USBFS_ARB_EPX_SR_DMA_GNT) != 0u)
                    {
                        if((USBFS_EP[ep].addr & USBFS_DIR_IN) == 0u)
                        {
                                USBFS_EP[ep].apiEpState = USBFS_NO_EVENT_PENDING;
                                /* Write the Mode register */
                                CY_SET_REG8((reg8 *)(USBFS_SIE_EP1_CR0_IND + ptr),
                                                                                    USBFS_EP[ep].epMode);
                        }
                    }
                #endif /* End USBFS_EP_MM */

                /* `#START ARB_USER_CODE` Place your code here for handle Buffer Underflow/Overflow */

                /* `#END` */

                CY_SET_REG8((reg8 *)(USBFS_ARB_EP1_SR_IND + ptr), ep_status);   /* Clear Serviced events */
            }
            ptr += USBFS_EPX_CNTX_ADDR_OFFSET;               /* prepare pointer for next EP */
            ep++;
            int_status >>= 1u;
        }

        /* `#START ARB_END_USER_CODE` Place your code here */

        /* `#END` */
    }

#endif /* End USBFS_EP_MM */


/* [] END OF FILE */
