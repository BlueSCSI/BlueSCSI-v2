/*******************************************************************************
* File Name: USBFS_1_std.c
* Version 2.60
*
* Description:
*  USB Standard request handler.
*
* Note:
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "USBFS_1.h"
#include "USBFS_1_cdc.h"
#include "USBFS_1_pvt.h"
#if defined(USBFS_1_ENABLE_MIDI_STREAMING) 
    #include "USBFS_1_midi.h"
#endif /* End USBFS_1_ENABLE_MIDI_STREAMING*/


/***************************************
*   Static data allocation
***************************************/

#if defined(USBFS_1_ENABLE_FWSN_STRING)
    static volatile uint8 *USBFS_1_fwSerialNumberStringDescriptor;
    static volatile uint8 USBFS_1_snStringConfirm = USBFS_1_FALSE;
#endif  /* USBFS_1_ENABLE_FWSN_STRING */

#if defined(USBFS_1_ENABLE_FWSN_STRING)


    /*******************************************************************************
    * Function Name: USBFS_1_SerialNumString
    ********************************************************************************
    *
    * Summary:
    *  Application firmware may supply the source of the USB device descriptors
    *  serial number string during runtime.
    *
    * Parameters:
    *  snString:  pointer to string.
    *
    * Return:
    *  None.
    *
    * Reentrant:
    *  No.
    *
    *******************************************************************************/
    void  USBFS_1_SerialNumString(uint8 snString[]) 
    {
        USBFS_1_snStringConfirm = USBFS_1_FALSE;
        if(snString != NULL)
        {
            USBFS_1_fwSerialNumberStringDescriptor = snString;
            /* Check descriptor validation */
            if( (snString[0u] > 1u ) && (snString[1u] == USBFS_1_DESCR_STRING) )
            {
                USBFS_1_snStringConfirm = USBFS_1_TRUE;
            }
        }
    }

#endif  /* USBFS_1_ENABLE_FWSN_STRING */


/*******************************************************************************
* Function Name: USBFS_1_HandleStandardRqst
********************************************************************************
*
* Summary:
*  This Routine dispatches standard requests
*
* Parameters:
*  None.
*
* Return:
*  TRUE if request handled.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 USBFS_1_HandleStandardRqst(void) 
{
    uint8 requestHandled = USBFS_1_FALSE;
    uint8 interfaceNumber;
    #if defined(USBFS_1_ENABLE_STRINGS)
        volatile uint8 *pStr = 0u;
        #if defined(USBFS_1_ENABLE_DESCRIPTOR_STRINGS)
            uint8 nStr;
            uint8 descrLength;
        #endif /* USBFS_1_ENABLE_DESCRIPTOR_STRINGS */
    #endif /* USBFS_1_ENABLE_STRINGS */
    static volatile uint8 USBFS_1_tBuffer[USBFS_1_STATUS_LENGTH_MAX];
    const T_USBFS_1_LUT CYCODE *pTmp;
    USBFS_1_currentTD.count = 0u;

    if ((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_DIR_MASK) == USBFS_1_RQST_DIR_D2H)
    {
        /* Control Read */
        switch (CY_GET_REG8(USBFS_1_bRequest))
        {
            case USBFS_1_GET_DESCRIPTOR:
                if (CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_DESCR_DEVICE)
                {
                    pTmp = USBFS_1_GetDeviceTablePtr();
                    USBFS_1_currentTD.pData = (volatile uint8 *)pTmp->p_list;
                    USBFS_1_currentTD.count = USBFS_1_DEVICE_DESCR_LENGTH;
                    requestHandled  = USBFS_1_InitControlRead();
                }
                else if (CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_DESCR_CONFIG)
                {
                    pTmp = USBFS_1_GetConfigTablePtr(CY_GET_REG8(USBFS_1_wValueLo));
                    USBFS_1_currentTD.pData = (volatile uint8 *)pTmp->p_list;
                    USBFS_1_currentTD.count = ((uint16)(USBFS_1_currentTD.pData)[ \
                                      USBFS_1_CONFIG_DESCR_TOTAL_LENGTH_HI] << 8u) | \
                                     (USBFS_1_currentTD.pData)[USBFS_1_CONFIG_DESCR_TOTAL_LENGTH_LOW];
                    requestHandled  = USBFS_1_InitControlRead();
                }
                #if defined(USBFS_1_ENABLE_STRINGS)
                else if (CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_DESCR_STRING)
                {
                    /* Descriptor Strings*/
                    #if defined(USBFS_1_ENABLE_DESCRIPTOR_STRINGS)
                        nStr = 0u;
                        pStr = (volatile uint8 *)&USBFS_1_STRING_DESCRIPTORS[0u];
                        while ( (CY_GET_REG8(USBFS_1_wValueLo) > nStr) && (*pStr != 0u) )
                        {
                            /* Read descriptor length from 1st byte */
                            descrLength = *pStr;
                            /* Move to next string descriptor */
                            pStr = &pStr[descrLength];
                            nStr++;
                        }
                    #endif /* End USBFS_1_ENABLE_DESCRIPTOR_STRINGS */
                    /* Microsoft OS String*/
                    #if defined(USBFS_1_ENABLE_MSOS_STRING)
                        if( CY_GET_REG8(USBFS_1_wValueLo) == USBFS_1_STRING_MSOS )
                        {
                            pStr = (volatile uint8 *)&USBFS_1_MSOS_DESCRIPTOR[0u];
                        }
                    #endif /* End USBFS_1_ENABLE_MSOS_STRING*/
                    /* SN string */
                    #if defined(USBFS_1_ENABLE_SN_STRING)
                        if( (CY_GET_REG8(USBFS_1_wValueLo) != 0u) &&
                            (CY_GET_REG8(USBFS_1_wValueLo) ==
                            USBFS_1_DEVICE0_DESCR[USBFS_1_DEVICE_DESCR_SN_SHIFT]) )
                        {
                            pStr = (volatile uint8 *)&USBFS_1_SN_STRING_DESCRIPTOR[0u];
                            #if defined(USBFS_1_ENABLE_FWSN_STRING)
                                if(USBFS_1_snStringConfirm != USBFS_1_FALSE)
                                {
                                    pStr = USBFS_1_fwSerialNumberStringDescriptor;
                                }
                            #endif  /* USBFS_1_ENABLE_FWSN_STRING */
                            #if defined(USBFS_1_ENABLE_IDSN_STRING)
                                /* Read DIE ID and generate string descriptor in RAM */
                                USBFS_1_ReadDieID(USBFS_1_idSerialNumberStringDescriptor);
                                pStr = USBFS_1_idSerialNumberStringDescriptor;
                            #endif    /* End USBFS_1_ENABLE_IDSN_STRING */
                        }
                    #endif    /* End USBFS_1_ENABLE_SN_STRING */
                    if (*pStr != 0u)
                    {
                        USBFS_1_currentTD.count = *pStr;
                        USBFS_1_currentTD.pData = pStr;
                        requestHandled  = USBFS_1_InitControlRead();
                    }
                }
                #endif /* End USBFS_1_ENABLE_STRINGS */
                else
                {
                    requestHandled = USBFS_1_DispatchClassRqst();
                }
                break;
            case USBFS_1_GET_STATUS:
                switch ((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_RCPT_MASK))
                {
                    case USBFS_1_RQST_RCPT_EP:
                        USBFS_1_currentTD.count = USBFS_1_EP_STATUS_LENGTH;
                        USBFS_1_tBuffer[0u] = USBFS_1_EP[ \
                                        CY_GET_REG8(USBFS_1_wIndexLo) & USBFS_1_DIR_UNUSED].hwEpState;
                        USBFS_1_tBuffer[1u] = 0u;
                        USBFS_1_currentTD.pData = &USBFS_1_tBuffer[0u];
                        requestHandled  = USBFS_1_InitControlRead();
                        break;
                    case USBFS_1_RQST_RCPT_DEV:
                        USBFS_1_currentTD.count = USBFS_1_DEVICE_STATUS_LENGTH;
                        USBFS_1_tBuffer[0u] = USBFS_1_deviceStatus;
                        USBFS_1_tBuffer[1u] = 0u;
                        USBFS_1_currentTD.pData = &USBFS_1_tBuffer[0u];
                        requestHandled  = USBFS_1_InitControlRead();
                        break;
                    default:    /* requestHandled is initialized as FALSE by default */
                        break;
                }
                break;
            case USBFS_1_GET_CONFIGURATION:
                USBFS_1_currentTD.count = 1u;
                USBFS_1_currentTD.pData = (volatile uint8 *)&USBFS_1_configuration;
                requestHandled  = USBFS_1_InitControlRead();
                break;
            case USBFS_1_GET_INTERFACE:
                USBFS_1_currentTD.count = 1u;
                USBFS_1_currentTD.pData = (volatile uint8 *)&USBFS_1_interfaceSetting[ \
                                                                            CY_GET_REG8(USBFS_1_wIndexLo)];
                requestHandled  = USBFS_1_InitControlRead();
                break;
            default: /* requestHandled is initialized as FALSE by default */
                break;
        }
    }
    else {
        /* Control Write */
        switch (CY_GET_REG8(USBFS_1_bRequest))
        {
            case USBFS_1_SET_ADDRESS:
                USBFS_1_deviceAddress = CY_GET_REG8(USBFS_1_wValueLo);
                requestHandled = USBFS_1_InitNoDataControlTransfer();
                break;
            case USBFS_1_SET_CONFIGURATION:
                USBFS_1_configuration = CY_GET_REG8(USBFS_1_wValueLo);
                USBFS_1_configurationChanged = USBFS_1_TRUE;
                USBFS_1_Config(USBFS_1_TRUE);
                requestHandled = USBFS_1_InitNoDataControlTransfer();
                break;
            case USBFS_1_SET_INTERFACE:
                if (USBFS_1_ValidateAlternateSetting() != 0u)
                {
                    interfaceNumber = CY_GET_REG8(USBFS_1_wIndexLo);
                    USBFS_1_interfaceNumber = interfaceNumber;
                    USBFS_1_configurationChanged = USBFS_1_TRUE;
                    #if ((USBFS_1_EP_MA == USBFS_1__MA_DYNAMIC) && \
                         (USBFS_1_EP_MM == USBFS_1__EP_MANUAL) )
                        USBFS_1_Config(USBFS_1_FALSE);
                    #else
                        USBFS_1_ConfigAltChanged();
                    #endif /* End (USBFS_1_EP_MA == USBFS_1__MA_DYNAMIC) */
                    /* Update handled Alt setting changes status */
                    USBFS_1_interfaceSetting_last[interfaceNumber] =
                         USBFS_1_interfaceSetting[interfaceNumber];
                    requestHandled = USBFS_1_InitNoDataControlTransfer();
                }
                break;
            case USBFS_1_CLEAR_FEATURE:
                switch (CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_RCPT_MASK)
                {
                    case USBFS_1_RQST_RCPT_EP:
                        if (CY_GET_REG8(USBFS_1_wValueLo) == USBFS_1_ENDPOINT_HALT)
                        {
                            requestHandled = USBFS_1_ClearEndpointHalt();
                        }
                        break;
                    case USBFS_1_RQST_RCPT_DEV:
                        /* Clear device REMOTE_WAKEUP */
                        if (CY_GET_REG8(USBFS_1_wValueLo) == USBFS_1_DEVICE_REMOTE_WAKEUP)
                        {
                            USBFS_1_deviceStatus &= (uint8)~USBFS_1_DEVICE_STATUS_REMOTE_WAKEUP;
                            requestHandled = USBFS_1_InitNoDataControlTransfer();
                        }
                        break;
                    case USBFS_1_RQST_RCPT_IFC:
                        /* Validate interfaceNumber */
                        if (CY_GET_REG8(USBFS_1_wIndexLo) < USBFS_1_MAX_INTERFACES_NUMBER)
                        {
                            USBFS_1_interfaceStatus[CY_GET_REG8(USBFS_1_wIndexLo)] &=
                                                                (uint8)~(CY_GET_REG8(USBFS_1_wValueLo));
                            requestHandled = USBFS_1_InitNoDataControlTransfer();
                        }
                        break;
                    default:    /* requestHandled is initialized as FALSE by default */
                        break;
                }
                break;
            case USBFS_1_SET_FEATURE:
                switch (CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_RCPT_MASK)
                {
                    case USBFS_1_RQST_RCPT_EP:
                        if (CY_GET_REG8(USBFS_1_wValueLo) == USBFS_1_ENDPOINT_HALT)
                        {
                            requestHandled = USBFS_1_SetEndpointHalt();
                        }
                        break;
                    case USBFS_1_RQST_RCPT_DEV:
                        /* Set device REMOTE_WAKEUP */
                        if (CY_GET_REG8(USBFS_1_wValueLo) == USBFS_1_DEVICE_REMOTE_WAKEUP)
                        {
                            USBFS_1_deviceStatus |= USBFS_1_DEVICE_STATUS_REMOTE_WAKEUP;
                            requestHandled = USBFS_1_InitNoDataControlTransfer();
                        }
                        break;
                    case USBFS_1_RQST_RCPT_IFC:
                        /* Validate interfaceNumber */
                        if (CY_GET_REG8(USBFS_1_wIndexLo) < USBFS_1_MAX_INTERFACES_NUMBER)
                        {
                            USBFS_1_interfaceStatus[CY_GET_REG8(USBFS_1_wIndexLo)] &=
                                                                (uint8)~(CY_GET_REG8(USBFS_1_wValueLo));
                            requestHandled = USBFS_1_InitNoDataControlTransfer();
                        }
                        break;
                    default:    /* requestHandled is initialized as FALSE by default */
                        break;
                }
                break;
            default:    /* requestHandled is initialized as FALSE by default */
                break;
        }
    }
    return(requestHandled);
}


#if defined(USBFS_1_ENABLE_IDSN_STRING)

    /***************************************************************************
    * Function Name: USBFS_1_ReadDieID
    ****************************************************************************
    *
    * Summary:
    *  This routine read Die ID and generate Serial Number string descriptor.
    *
    * Parameters:
    *  descr:  pointer on string descriptor.
    *
    * Return:
    *  None.
    *
    * Reentrant:
    *  No.
    *
    ***************************************************************************/
    void USBFS_1_ReadDieID(uint8 descr[]) 
    {
        uint8 i;
        uint8 j = 0u;
        uint8 value;
        const char8 CYCODE hex[16u] = "0123456789ABCDEF";


        /* Check descriptor validation */
        if( descr != NULL)
        {
            descr[0u] = USBFS_1_IDSN_DESCR_LENGTH;
            descr[1u] = USBFS_1_DESCR_STRING;

            /* fill descriptor */
            for(i = 2u; i < USBFS_1_IDSN_DESCR_LENGTH; i += 4u)
            {
                value = CY_GET_XTND_REG8((void CYFAR *)(USBFS_1_DIE_ID + j));
                j++;
                descr[i] = (uint8)hex[value >> 4u];
                descr[i + 2u] = (uint8)hex[value & 0x0Fu];
            }
        }
    }

#endif /* End USBFS_1_ENABLE_IDSN_STRING */


/*******************************************************************************
* Function Name: USBFS_1_ConfigReg
********************************************************************************
*
* Summary:
*  This routine configures hardware registers from the variables.
*  It is called from USBFS_1_Config() function and from RestoreConfig
*  after Wakeup.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void USBFS_1_ConfigReg(void) 
{
    uint8 ep;
    uint8 i;
    #if(USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO)
        uint8 ep_type = 0u;
    #endif /* End USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO */

    /* Set the endpoint buffer addresses */
    ep = USBFS_1_EP1;
    for (i = 0u; i < 0x80u; i+= 0x10u)
    {
        CY_SET_REG8((reg8 *)(USBFS_1_ARB_EP1_CFG_IND + i), USBFS_1_ARB_EPX_CFG_CRC_BYPASS |
                                                          USBFS_1_ARB_EPX_CFG_RESET);

        #if(USBFS_1_EP_MM != USBFS_1__EP_MANUAL)
            /* Enable all Arbiter EP Interrupts : err, buf under, buf over, dma gnt(mode2 only), in buf full */
            CY_SET_REG8((reg8 *)(USBFS_1_ARB_EP1_INT_EN_IND + i), USBFS_1_ARB_EPX_INT_MASK);
        #endif   /* End USBFS_1_EP_MM != USBFS_1__EP_MANUAL */

        if(USBFS_1_EP[ep].epMode != USBFS_1_MODE_DISABLE)
        {
            if((USBFS_1_EP[ep].addr & USBFS_1_DIR_IN) != 0u )
            {
                CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + i), USBFS_1_MODE_NAK_IN);
            }
            else
            {
                CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + i), USBFS_1_MODE_NAK_OUT);
                /* Prepare EP type mask for automatic memory allocation */
                #if(USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO)
                    ep_type |= (uint8)(0x01u << (ep - USBFS_1_EP1));
                #endif /* End USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO */
            }
        }
        else
        {
            CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + i), USBFS_1_MODE_STALL_DATA_EP);
        }

        #if(USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO)
            CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CNT0_IND + i),   USBFS_1_EP[ep].bufferSize >> 8u);
            CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CNT1_IND + i),   USBFS_1_EP[ep].bufferSize & 0xFFu);

            CY_SET_REG8((reg8 *)(USBFS_1_ARB_RW1_RA_IND + i),     USBFS_1_EP[ep].buffOffset & 0xFFu);
            CY_SET_REG8((reg8 *)(USBFS_1_ARB_RW1_RA_MSB_IND + i), USBFS_1_EP[ep].buffOffset >> 8u);
            CY_SET_REG8((reg8 *)(USBFS_1_ARB_RW1_WA_IND + i),     USBFS_1_EP[ep].buffOffset & 0xFFu);
            CY_SET_REG8((reg8 *)(USBFS_1_ARB_RW1_WA_MSB_IND + i), USBFS_1_EP[ep].buffOffset >> 8u);
        #endif /* End USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO */

        ep++;
    }

    #if(USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO)
         /* BUF_SIZE depend on DMA_THRESS value: 55-32 bytes  44-16 bytes 33-8 bytes 22-4 bytes 11-2 bytes */
        USBFS_1_BUF_SIZE_REG = USBFS_1_DMA_BUF_SIZE;
        USBFS_1_DMA_THRES_REG = USBFS_1_DMA_BYTES_PER_BURST;   /* DMA burst threshold */
        USBFS_1_DMA_THRES_MSB_REG = 0u;
        USBFS_1_EP_ACTIVE_REG = USBFS_1_ARB_INT_MASK;
        USBFS_1_EP_TYPE_REG = ep_type;
        /* Cfg_cmp bit set to 1 once configuration is complete. */
        USBFS_1_ARB_CFG_REG = USBFS_1_ARB_CFG_AUTO_DMA | USBFS_1_ARB_CFG_AUTO_MEM |
                                       USBFS_1_ARB_CFG_CFG_CPM;
        /* Cfg_cmp bit set to 0 during configuration of PFSUSB Registers. */
        USBFS_1_ARB_CFG_REG = USBFS_1_ARB_CFG_AUTO_DMA | USBFS_1_ARB_CFG_AUTO_MEM;
    #endif /* End USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO */

    CY_SET_REG8(USBFS_1_SIE_EP_INT_EN_PTR, 0xFFu);
}


/*******************************************************************************
* Function Name: USBFS_1_Config
********************************************************************************
*
* Summary:
*  This routine configures endpoints for the entire configuration by scanning
*  the configuration descriptor.
*
* Parameters:
*  clearAltSetting: It configures the bAlternateSetting 0 for each interface.
*
* Return:
*  None.
*
* USBFS_1_interfaceClass - Initialized class array for each interface.
*   It is used for handling Class specific requests depend on interface class.
*   Different classes in multiple Alternate settings does not supported.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void USBFS_1_Config(uint8 clearAltSetting) 
{
    uint8 ep;
    uint8 cur_ep;
    uint8 i;
    uint8 ep_type;
    const uint8 *pDescr;
    #if(USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO)
        uint16 buffCount = 0u;
    #endif /* End USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO */

    const T_USBFS_1_LUT CYCODE *pTmp;
    const T_USBFS_1_EP_SETTINGS_BLOCK CYCODE *pEP;

    /* Clear all of the endpoints */
    for (ep = 0u; ep < USBFS_1_MAX_EP; ep++)
    {
        USBFS_1_EP[ep].attrib = 0u;
        USBFS_1_EP[ep].hwEpState = 0u;
        USBFS_1_EP[ep].apiEpState = USBFS_1_NO_EVENT_PENDING;
        USBFS_1_EP[ep].epToggle = 0u;
        USBFS_1_EP[ep].epMode = USBFS_1_MODE_DISABLE;
        USBFS_1_EP[ep].bufferSize = 0u;
        USBFS_1_EP[ep].interface = 0u;

    }

    /* Clear Alternate settings for all interfaces */
    if(clearAltSetting != 0u)
    {
        for (i = 0u; i < USBFS_1_MAX_INTERFACES_NUMBER; i++)
        {
            USBFS_1_interfaceSetting[i] = 0x00u;
            USBFS_1_interfaceSetting_last[i] = 0x00u;
        }
    }

    /* Init Endpoints and Device Status if configured */
    if(USBFS_1_configuration > 0u)
    {
        pTmp = USBFS_1_GetConfigTablePtr(USBFS_1_configuration - 1u);
        /* Set Power status for current configuration */
        pDescr = (const uint8 *)pTmp->p_list;
        if((pDescr[USBFS_1_CONFIG_DESCR_ATTRIB] & USBFS_1_CONFIG_DESCR_ATTRIB_SELF_POWERED) != 0u)
        {
            USBFS_1_deviceStatus |=  USBFS_1_DEVICE_STATUS_SELF_POWERED;
        }
        else
        {
            USBFS_1_deviceStatus &=  (uint8)~USBFS_1_DEVICE_STATUS_SELF_POWERED;
        }
        /* Move to next element */
        pTmp = &pTmp[1u];
        ep = pTmp->c;  /* For this table, c is the number of endpoints configurations  */

        #if ((USBFS_1_EP_MA == USBFS_1__MA_DYNAMIC) && \
             (USBFS_1_EP_MM == USBFS_1__EP_MANUAL) )
            /* Configure for dynamic EP memory allocation */
            /* p_list points the endpoint setting table. */
            pEP = (T_USBFS_1_EP_SETTINGS_BLOCK *) pTmp->p_list;
            for (i = 0u; i < ep; i++)
            {
                /* Compare current Alternate setting with EP Alt*/
                if(USBFS_1_interfaceSetting[pEP->interface] == pEP->altSetting)
                {
                    cur_ep = pEP->addr & USBFS_1_DIR_UNUSED;
                    ep_type = pEP->attributes & USBFS_1_EP_TYPE_MASK;
                    if (pEP->addr & USBFS_1_DIR_IN)
                    {
                        /* IN Endpoint */
                        USBFS_1_EP[cur_ep].apiEpState = USBFS_1_EVENT_PENDING;
                        USBFS_1_EP[cur_ep].epMode = (ep_type == USBFS_1_EP_TYPE_ISOC) ?
                                                        USBFS_1_MODE_ISO_IN : USBFS_1_MODE_ACK_IN;
                        #if defined(USBFS_1_ENABLE_CDC_CLASS)
                            if(((pEP->bMisc == USBFS_1_CLASS_CDC_DATA) ||
                                (pEP->bMisc == USBFS_1_CLASS_CDC)) &&
                                (ep_type != USBFS_1_EP_TYPE_INT))
                            {
                                USBFS_1_cdc_data_in_ep = cur_ep;
                            }
                        #endif  /* End USBFS_1_ENABLE_CDC_CLASS*/
                        #if ( defined(USBFS_1_ENABLE_MIDI_STREAMING) && \
                                             (USBFS_1_MIDI_IN_BUFF_SIZE > 0) )
                            if((pEP->bMisc == USBFS_1_CLASS_AUDIO) &&
                               (ep_type == USBFS_1_EP_TYPE_BULK))
                            {
                                USBFS_1_midi_in_ep = cur_ep;
                            }
                        #endif  /* End USBFS_1_ENABLE_MIDI_STREAMING*/
                    }
                    else
                    {
                        /* OUT Endpoint */
                        USBFS_1_EP[cur_ep].apiEpState = USBFS_1_NO_EVENT_PENDING;
                        USBFS_1_EP[cur_ep].epMode = (ep_type == USBFS_1_EP_TYPE_ISOC) ?
                                                    USBFS_1_MODE_ISO_OUT : USBFS_1_MODE_ACK_OUT;
                        #if defined(USBFS_1_ENABLE_CDC_CLASS)
                            if(((pEP->bMisc == USBFS_1_CLASS_CDC_DATA) ||
                                (pEP->bMisc == USBFS_1_CLASS_CDC)) &&
                                (ep_type != USBFS_1_EP_TYPE_INT))
                            {
                                USBFS_1_cdc_data_out_ep = cur_ep;
                            }
                        #endif  /* End USBFS_1_ENABLE_CDC_CLASS*/
                        #if ( defined(USBFS_1_ENABLE_MIDI_STREAMING) && \
                                     (USBFS_1_MIDI_OUT_BUFF_SIZE > 0) )
                            if((pEP->bMisc == USBFS_1_CLASS_AUDIO) &&
                               (ep_type == USBFS_1_EP_TYPE_BULK))
                            {
                                USBFS_1_midi_out_ep = cur_ep;
                            }
                        #endif  /* End USBFS_1_ENABLE_MIDI_STREAMING*/
                    }
                    USBFS_1_EP[cur_ep].bufferSize = pEP->bufferSize;
                    USBFS_1_EP[cur_ep].addr = pEP->addr;
                    USBFS_1_EP[cur_ep].attrib = pEP->attributes;
                }
                pEP = &pEP[1u];
            }
        #else /* Config for static EP memory allocation  */
            for (i = USBFS_1_EP1; i < USBFS_1_MAX_EP; i++)
            {
                /* p_list points the endpoint setting table. */
                pEP = (const T_USBFS_1_EP_SETTINGS_BLOCK CYCODE *) pTmp->p_list;
                /* Find max length for each EP and select it (length could be different in different Alt settings) */
                /* but other settings should be correct with regards to Interface alt Setting */
                for (cur_ep = 0u; cur_ep < ep; cur_ep++)
                {
                    /* EP count is equal to EP # in table and we found larger EP length than have before*/
                    if(i == (pEP->addr & USBFS_1_DIR_UNUSED))
                    {
                        if(USBFS_1_EP[i].bufferSize < pEP->bufferSize)
                        {
                            USBFS_1_EP[i].bufferSize = pEP->bufferSize;
                        }
                        /* Compare current Alternate setting with EP Alt*/
                        if(USBFS_1_interfaceSetting[pEP->interface] == pEP->altSetting)
                        {
                            ep_type = pEP->attributes & USBFS_1_EP_TYPE_MASK;
                            if ((pEP->addr & USBFS_1_DIR_IN) != 0u)
                            {
                                /* IN Endpoint */
                                USBFS_1_EP[i].apiEpState = USBFS_1_EVENT_PENDING;
                                USBFS_1_EP[i].epMode = (ep_type == USBFS_1_EP_TYPE_ISOC) ?
                                                        USBFS_1_MODE_ISO_IN : USBFS_1_MODE_ACK_IN;
                                /* Find and init CDC IN endpoint number */
                                #if defined(USBFS_1_ENABLE_CDC_CLASS)
                                    if(((pEP->bMisc == USBFS_1_CLASS_CDC_DATA) ||
                                        (pEP->bMisc == USBFS_1_CLASS_CDC)) &&
                                        (ep_type != USBFS_1_EP_TYPE_INT))
                                    {
                                        USBFS_1_cdc_data_in_ep = i;
                                    }
                                #endif  /* End USBFS_1_ENABLE_CDC_CLASS*/
                                #if ( defined(USBFS_1_ENABLE_MIDI_STREAMING) && \
                                             (USBFS_1_MIDI_IN_BUFF_SIZE > 0) )
                                    if((pEP->bMisc == USBFS_1_CLASS_AUDIO) &&
                                       (ep_type == USBFS_1_EP_TYPE_BULK))
                                    {
                                        USBFS_1_midi_in_ep = i;
                                    }
                                #endif  /* End USBFS_1_ENABLE_MIDI_STREAMING*/
                            }
                            else
                            {
                                /* OUT Endpoint */
                                USBFS_1_EP[i].apiEpState = USBFS_1_NO_EVENT_PENDING;
                                USBFS_1_EP[i].epMode = (ep_type == USBFS_1_EP_TYPE_ISOC) ?
                                                    USBFS_1_MODE_ISO_OUT : USBFS_1_MODE_ACK_OUT;
                                /* Find and init CDC IN endpoint number */
                                #if defined(USBFS_1_ENABLE_CDC_CLASS)
                                    if(((pEP->bMisc == USBFS_1_CLASS_CDC_DATA) ||
                                        (pEP->bMisc == USBFS_1_CLASS_CDC)) &&
                                        (ep_type != USBFS_1_EP_TYPE_INT))
                                    {
                                        USBFS_1_cdc_data_out_ep = i;
                                    }
                                #endif  /* End USBFS_1_ENABLE_CDC_CLASS*/
                                #if ( defined(USBFS_1_ENABLE_MIDI_STREAMING) && \
                                             (USBFS_1_MIDI_OUT_BUFF_SIZE > 0) )
                                    if((pEP->bMisc == USBFS_1_CLASS_AUDIO) &&
                                       (ep_type == USBFS_1_EP_TYPE_BULK))
                                    {
                                        USBFS_1_midi_out_ep = i;
                                    }
                                #endif  /* End USBFS_1_ENABLE_MIDI_STREAMING*/
                            }
                            USBFS_1_EP[i].addr = pEP->addr;
                            USBFS_1_EP[i].attrib = pEP->attributes;

                            #if(USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO)
                                break;      /* use first EP setting in Auto memory managment */
                            #endif /* End USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO */
                        }
                    }
                    pEP = &pEP[1u];
                }
            }
        #endif /* End (USBFS_1_EP_MA == USBFS_1__MA_DYNAMIC) */

        /* Init class array for each interface and interface number for each EP.
        *  It is used for handling Class specific requests directed to either an
        *  interface or the endpoint.
        */
        /* p_list points the endpoint setting table. */
        pEP = (const T_USBFS_1_EP_SETTINGS_BLOCK CYCODE *) pTmp->p_list;
        for (i = 0u; i < ep; i++)
        {
            /* Configure interface number for each EP*/
            USBFS_1_EP[pEP->addr & USBFS_1_DIR_UNUSED].interface = pEP->interface;
            pEP = &pEP[1u];
        }
        /* Init pointer on interface class table*/
        USBFS_1_interfaceClass = USBFS_1_GetInterfaceClassTablePtr();
        /* Set the endpoint buffer addresses */

        #if(USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO)
            for (ep = USBFS_1_EP1; ep < USBFS_1_MAX_EP; ep++)
            {
                USBFS_1_EP[ep].buffOffset = buffCount;
                 buffCount += USBFS_1_EP[ep].bufferSize;
            }
        #endif /* End USBFS_1_EP_MM != USBFS_1__EP_DMAAUTO */

        /* Configure hardware registers */
        USBFS_1_ConfigReg();
    } /* USBFS_1_configuration > 0 */
}


/*******************************************************************************
* Function Name: USBFS_1_ConfigAltChanged
********************************************************************************
*
* Summary:
*  This routine update configuration for the required endpoints only.
*  It is called after SET_INTERFACE request when Static memory allocation used.
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
void USBFS_1_ConfigAltChanged(void) 
{
    uint8 ep;
    uint8 cur_ep;
    uint8 i;
    uint8 ep_type;
    uint8 ri;

    const T_USBFS_1_LUT CYCODE *pTmp;
    const T_USBFS_1_EP_SETTINGS_BLOCK CYCODE *pEP;


    /* Init Endpoints and Device Status if configured */
    if(USBFS_1_configuration > 0u)
    {
        pTmp = USBFS_1_GetConfigTablePtr(USBFS_1_configuration - 1u);
        pTmp = &pTmp[1u];
        ep = pTmp->c;  /* For this table, c is the number of endpoints configurations  */

        /* Do not touch EP which doesn't need reconfiguration */
        /* When Alt setting changed, the only required endpoints need to be reconfigured */
        /* p_list points the endpoint setting table. */
        pEP = (const T_USBFS_1_EP_SETTINGS_BLOCK CYCODE *) pTmp->p_list;
        for (i = 0u; i < ep; i++)
        {
            /*If Alt setting changed and new is same with EP Alt */
            if((USBFS_1_interfaceSetting[pEP->interface] !=
                USBFS_1_interfaceSetting_last[pEP->interface] ) &&
               (USBFS_1_interfaceSetting[pEP->interface] == pEP->altSetting) &&
               (pEP->interface == CY_GET_REG8(USBFS_1_wIndexLo)))
            {
                cur_ep = pEP->addr & USBFS_1_DIR_UNUSED;
                ri = ((cur_ep - USBFS_1_EP1) << USBFS_1_EPX_CNTX_ADDR_SHIFT);
                ep_type = pEP->attributes & USBFS_1_EP_TYPE_MASK;
                if ((pEP->addr & USBFS_1_DIR_IN) != 0u)
                {
                    /* IN Endpoint */
                    USBFS_1_EP[cur_ep].apiEpState = USBFS_1_EVENT_PENDING;
                    USBFS_1_EP[cur_ep].epMode = (ep_type == USBFS_1_EP_TYPE_ISOC) ?
                                                USBFS_1_MODE_ISO_IN : USBFS_1_MODE_ACK_IN;
                }
                else
                {
                    /* OUT Endpoint */
                    USBFS_1_EP[cur_ep].apiEpState = USBFS_1_NO_EVENT_PENDING;
                    USBFS_1_EP[cur_ep].epMode = (ep_type == USBFS_1_EP_TYPE_ISOC) ?
                                                USBFS_1_MODE_ISO_OUT : USBFS_1_MODE_ACK_OUT;
                }
                 /* Change the SIE mode for the selected EP to NAK ALL */
                 CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_NAK_IN_OUT);
                USBFS_1_EP[cur_ep].bufferSize = pEP->bufferSize;
                USBFS_1_EP[cur_ep].addr = pEP->addr;
                USBFS_1_EP[cur_ep].attrib = pEP->attributes;

                /* Clear the data toggle */
                USBFS_1_EP[cur_ep].epToggle = 0u;

                /* Dynamic reconfiguration for mode 3 transfer */
            #if(USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO)
                /* In_data_rdy for selected EP should be set to 0 */
                * (reg8 *)(USBFS_1_ARB_EP1_CFG_IND + ri) &= (uint8)~USBFS_1_ARB_EPX_CFG_IN_DATA_RDY;

                /* write the EP number for which reconfiguration is required */
                USBFS_1_DYN_RECONFIG_REG = (cur_ep - USBFS_1_EP1) <<
                                                    USBFS_1_DYN_RECONFIG_EP_SHIFT;
                /* Set the dyn_config_en bit in dynamic reconfiguration register */
                USBFS_1_DYN_RECONFIG_REG |= USBFS_1_DYN_RECONFIG_ENABLE;
                /* wait for the dyn_config_rdy bit to set by the block,
                *  this bit will be set to 1 when block is ready for reconfiguration.
                */
                while((USBFS_1_DYN_RECONFIG_REG & USBFS_1_DYN_RECONFIG_RDY_STS) == 0u)
                {
                    ;
                }
                /* Once dyn_config_rdy bit is set, FW can change the EP configuration. */
                /* Change EP Type with new direction */
                if((pEP->addr & USBFS_1_DIR_IN) == 0u)
                {
                    USBFS_1_EP_TYPE_REG |= (uint8)(0x01u << (cur_ep - USBFS_1_EP1));
                }
                else
                {
                    USBFS_1_EP_TYPE_REG &= (uint8)~(uint8)(0x01u << (cur_ep - USBFS_1_EP1));
                }
                /* dynamic reconfiguration enable bit cleared, pointers and control/status
                *  signals for the selected EP is cleared/re-initialized on negative edge
                *  of dynamic reconfiguration enable bit).
                */
                USBFS_1_DYN_RECONFIG_REG &= (uint8)~USBFS_1_DYN_RECONFIG_ENABLE;
                /* The main loop has to re-enable DMA and OUT endpoint*/
            #else
                CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CNT0_IND + ri),
                                                                USBFS_1_EP[cur_ep].bufferSize >> 8u);
                CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CNT1_IND + ri),
                                                                USBFS_1_EP[cur_ep].bufferSize & 0xFFu);
                CY_SET_REG8((reg8 *)(USBFS_1_ARB_RW1_RA_IND + ri),
                                                                USBFS_1_EP[cur_ep].buffOffset & 0xFFu);
                CY_SET_REG8((reg8 *)(USBFS_1_ARB_RW1_RA_MSB_IND + ri),
                                                                USBFS_1_EP[cur_ep].buffOffset >> 8u);
                CY_SET_REG8((reg8 *)(USBFS_1_ARB_RW1_WA_IND + ri),
                                                                USBFS_1_EP[cur_ep].buffOffset & 0xFFu);
                CY_SET_REG8((reg8 *)(USBFS_1_ARB_RW1_WA_MSB_IND + ri),
                                                                USBFS_1_EP[cur_ep].buffOffset >> 8u);
            #endif /* End USBFS_1_EP_MM == USBFS_1__EP_DMAAUTO */
            }
            /* Get next EP element */
            pEP = &pEP[1u];
        }
    }   /* USBFS_1_configuration > 0 */
}


/*******************************************************************************
* Function Name: USBFS_1_GetConfigTablePtr
********************************************************************************
*
* Summary:
*  This routine returns a pointer a configuration table entry
*
* Parameters:
*  c:  Configuration Index
*
* Return:
*  Device Descriptor pointer.
*
*******************************************************************************/
const T_USBFS_1_LUT CYCODE *USBFS_1_GetConfigTablePtr(uint8 c)
                                                        
{
    /* Device Table */
    const T_USBFS_1_LUT CYCODE *pTmp;

    pTmp = (const T_USBFS_1_LUT CYCODE *) USBFS_1_TABLE[USBFS_1_device].p_list;

    /* The first entry points to the Device Descriptor,
    *  the rest configuration entries.
	*/
    return( (const T_USBFS_1_LUT CYCODE *) pTmp[c + 1u].p_list );
}


/*******************************************************************************
* Function Name: USBFS_1_GetDeviceTablePtr
********************************************************************************
*
* Summary:
*  This routine returns a pointer to the Device table
*
* Parameters:
*  None.
*
* Return:
*  Device Table pointer
*
*******************************************************************************/
const T_USBFS_1_LUT CYCODE *USBFS_1_GetDeviceTablePtr(void)
                                                            
{
    /* Device Table */
    return( (const T_USBFS_1_LUT CYCODE *) USBFS_1_TABLE[USBFS_1_device].p_list );
}


/*******************************************************************************
* Function Name: USB_GetInterfaceClassTablePtr
********************************************************************************
*
* Summary:
*  This routine returns Interface Class table pointer, which contains
*  the relation between interface number and interface class.
*
* Parameters:
*  None.
*
* Return:
*  Interface Class table pointer.
*
*******************************************************************************/
const uint8 CYCODE *USBFS_1_GetInterfaceClassTablePtr(void)
                                                        
{
    const T_USBFS_1_LUT CYCODE *pTmp;
    uint8 currentInterfacesNum;

    pTmp = USBFS_1_GetConfigTablePtr(USBFS_1_configuration - 1u);
    currentInterfacesNum  = ((const uint8 *) pTmp->p_list)[USBFS_1_CONFIG_DESCR_NUM_INTERFACES];
    /* Third entry in the LUT starts the Interface Table pointers */
    /* The INTERFACE_CLASS table is located after all interfaces */
    pTmp = &pTmp[currentInterfacesNum + 2u];
    return( (const uint8 CYCODE *) pTmp->p_list );
}


/*******************************************************************************
* Function Name: USBFS_1_TerminateEP
********************************************************************************
*
* Summary:
*  This function terminates the specified USBFS endpoint.
*  This function should be used before endpoint reconfiguration.
*
* Parameters:
*  Endpoint number.
*
* Return:
*  None.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void USBFS_1_TerminateEP(uint8 ep) 
{
    uint8 ri;

    ep &= USBFS_1_DIR_UNUSED;
    ri = ((ep - USBFS_1_EP1) << USBFS_1_EPX_CNTX_ADDR_SHIFT);

    if ((ep > USBFS_1_EP0) && (ep < USBFS_1_MAX_EP))
    {
        /* Set the endpoint Halt */
        USBFS_1_EP[ep].hwEpState |= (USBFS_1_ENDPOINT_STATUS_HALT);

        /* Clear the data toggle */
        USBFS_1_EP[ep].epToggle = 0u;
        USBFS_1_EP[ep].apiEpState = USBFS_1_NO_EVENT_ALLOWED;

        if ((USBFS_1_EP[ep].addr & USBFS_1_DIR_IN) != 0u)
        {
            /* IN Endpoint */
            CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_NAK_IN);
        }
        else
        {
            /* OUT Endpoint */
            CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_NAK_OUT);
        }
    }
}


/*******************************************************************************
* Function Name: USBFS_1_SetEndpointHalt
********************************************************************************
*
* Summary:
*  This routine handles set endpoint halt.
*
* Parameters:
*  None.
*
* Return:
*  requestHandled.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 USBFS_1_SetEndpointHalt(void) 
{
    uint8 ep;
    uint8 ri;
    uint8 requestHandled = USBFS_1_FALSE;

    /* Set endpoint halt */
    ep = CY_GET_REG8(USBFS_1_wIndexLo) & USBFS_1_DIR_UNUSED;
    ri = ((ep - USBFS_1_EP1) << USBFS_1_EPX_CNTX_ADDR_SHIFT);

    if ((ep > USBFS_1_EP0) && (ep < USBFS_1_MAX_EP))
    {
        /* Set the endpoint Halt */
        USBFS_1_EP[ep].hwEpState |= (USBFS_1_ENDPOINT_STATUS_HALT);

        /* Clear the data toggle */
        USBFS_1_EP[ep].epToggle = 0u;
        USBFS_1_EP[ep].apiEpState |= USBFS_1_NO_EVENT_ALLOWED;

        if ((USBFS_1_EP[ep].addr & USBFS_1_DIR_IN) != 0u)
        {
            /* IN Endpoint */
            CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_STALL_DATA_EP |
                                                               USBFS_1_MODE_ACK_IN);
        }
        else
        {
            /* OUT Endpoint */
            CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_STALL_DATA_EP |
                                                               USBFS_1_MODE_ACK_OUT);
        }
        requestHandled = USBFS_1_InitNoDataControlTransfer();
    }

    return(requestHandled);
}


/*******************************************************************************
* Function Name: USBFS_1_ClearEndpointHalt
********************************************************************************
*
* Summary:
*  This routine handles clear endpoint halt.
*
* Parameters:
*  None.
*
* Return:
*  requestHandled.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 USBFS_1_ClearEndpointHalt(void) 
{
    uint8 ep;
    uint8 ri;
    uint8 requestHandled = USBFS_1_FALSE;

    /* Clear endpoint halt */
    ep = CY_GET_REG8(USBFS_1_wIndexLo) & USBFS_1_DIR_UNUSED;
    ri = ((ep - USBFS_1_EP1) << USBFS_1_EPX_CNTX_ADDR_SHIFT);

    if ((ep > USBFS_1_EP0) && (ep < USBFS_1_MAX_EP))
    {
        /* Clear the endpoint Halt */
        USBFS_1_EP[ep].hwEpState &= (uint8)~(USBFS_1_ENDPOINT_STATUS_HALT);

        /* Clear the data toggle */
        USBFS_1_EP[ep].epToggle = 0u;
        /* Clear toggle bit for already armed packet */
        CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CNT0_IND + ri), CY_GET_REG8(
                    (reg8 *)(USBFS_1_SIE_EP1_CNT0_IND + ri)) & (uint8)~USBFS_1_EPX_CNT_DATA_TOGGLE);
        /* Return API State as it was defined before */
        USBFS_1_EP[ep].apiEpState &= (uint8)~USBFS_1_NO_EVENT_ALLOWED;

        if ((USBFS_1_EP[ep].addr & USBFS_1_DIR_IN) != 0u)
        {
            /* IN Endpoint */
            if(USBFS_1_EP[ep].apiEpState == USBFS_1_IN_BUFFER_EMPTY)
            {       /* Wait for next packet from application */
                CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_NAK_IN);
            }
            else    /* Continue armed transfer */
            {
                CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_ACK_IN);
            }
        }
        else
        {
            /* OUT Endpoint */
            if(USBFS_1_EP[ep].apiEpState == USBFS_1_OUT_BUFFER_FULL)
            {       /* Allow application to read full buffer */
                CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_NAK_OUT);
            }
            else    /* Mark endpoint as empty, so it will be reloaded */
            {
                CY_SET_REG8((reg8 *)(USBFS_1_SIE_EP1_CR0_IND + ri), USBFS_1_MODE_ACK_OUT);
            }
        }
        requestHandled = USBFS_1_InitNoDataControlTransfer();
    }

    return(requestHandled);
}


/*******************************************************************************
* Function Name: USBFS_1_ValidateAlternateSetting
********************************************************************************
*
* Summary:
*  Validates (and records) a SET INTERFACE request.
*
* Parameters:
*  None.
*
* Return:
*  requestHandled.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 USBFS_1_ValidateAlternateSetting(void) 
{
    uint8 requestHandled = USBFS_1_TRUE;
    uint8 interfaceNum;
    const T_USBFS_1_LUT CYCODE *pTmp;
    uint8 currentInterfacesNum;

    interfaceNum = CY_GET_REG8(USBFS_1_wIndexLo);
    /* Validate interface setting, stall if invalid. */
    pTmp = USBFS_1_GetConfigTablePtr(USBFS_1_configuration - 1u);
    currentInterfacesNum  = ((const uint8 *) pTmp->p_list)[USBFS_1_CONFIG_DESCR_NUM_INTERFACES];

    if((interfaceNum >= currentInterfacesNum) || (interfaceNum >= USBFS_1_MAX_INTERFACES_NUMBER))
    {   /* Wrong interface number */
        requestHandled = USBFS_1_FALSE;
    }
    else
    {
        /* Save current Alt setting to find out the difference in Config() function */
        USBFS_1_interfaceSetting_last[interfaceNum] = USBFS_1_interfaceSetting[interfaceNum];
        USBFS_1_interfaceSetting[interfaceNum] = CY_GET_REG8(USBFS_1_wValueLo);
    }

    return (requestHandled);
}


/* [] END OF FILE */
