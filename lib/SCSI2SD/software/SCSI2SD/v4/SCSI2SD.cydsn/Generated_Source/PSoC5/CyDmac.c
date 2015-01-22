/*******************************************************************************
* File Name: CyDmac.c
* Version 4.20
*
* Description:
*  Provides an API for the DMAC component. The API includes functions for the
*  DMA controller, DMA channels and Transfer Descriptors.
*
*  This API is the library version not the auto generated code that gets
*  generated when the user places a DMA component on the schematic.
*
*  The auto generated code would use the APi's in this module.
*
* Note:
*  This code is endian agnostic.
*
*  The Transfer Descriptor memory can be used as regular memory if the TD's are
*  not being used.
*
*  This code uses the first byte of each TD to manage the free list of TD's.
*  The user can overwrite this once the TD is allocated.
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "CyDmac.h"


/*******************************************************************************
* The following variables are initialized from CyDmacConfigure() function that
* is executed from initialize_psoc() at the early initialization stage.
* In case of IAR EW IDE, initialize_psoc() is executed before the data sections
* are initialized. To avoid zeroing, these variables should be initialized
* properly during segments initialization as well.
*******************************************************************************/
static uint8  CyDmaTdCurrentNumber = CY_DMA_NUMBEROF_TDS;           /* Current Number of free elements on list */
static uint8  CyDmaTdFreeIndex = (uint8)(CY_DMA_NUMBEROF_TDS - 1u); /* Index of first available TD */
static uint32 CyDmaChannels = DMA_CHANNELS_USED__MASK0;              /* Bit map of DMA channel ownership */


/*******************************************************************************
* Function Name: CyDmacConfigure
********************************************************************************
*
* Summary:
*  Creates a linked list of all the TDs to be allocated. This function is called
*  by the startup code; you do not normally need to call it. You can call this
*  function if all of the DMA channels are inactive.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CyDmacConfigure(void) 
{
    uint8 dmaIndex;

    /* Set TD list variables. */
    CyDmaTdFreeIndex     = (uint8)(CY_DMA_NUMBEROF_TDS - 1u);
    CyDmaTdCurrentNumber = CY_DMA_NUMBEROF_TDS;

    /* Make TD free list. */
    for(dmaIndex = (uint8)(CY_DMA_NUMBEROF_TDS - 1u); dmaIndex != 0u; dmaIndex--)
    {
        CY_DMA_TDMEM_STRUCT_PTR[dmaIndex].TD0[0u] = (uint8)(dmaIndex - 1u);
    }

    /* Make last one point to zero. */
    CY_DMA_TDMEM_STRUCT_PTR[dmaIndex].TD0[0u] = 0u;
}


/*******************************************************************************
* Function Name: CyDmacError
********************************************************************************
*
* Summary:
*  Returns errors of the last failed DMA transaction.
*
* Parameters:
*  None
*
* Return:
*  Errors of the last failed DMA transaction.
*
*  DMAC_PERIPH_ERR:
*   Set to 1 when a peripheral responds to a bus transaction with an error
*   response.
*
*  DMAC_UNPOP_ACC:
*   Set to 1 when an access is attempted to an invalid address.
*
*  DMAC_BUS_TIMEOUT:
*   Set to 1 when a bus timeout occurs. Cleared by writing a 1. Timeout values
*   are determined by the BUS_TIMEOUT field in the PHUBCFG register.
*
* Theory:
*  Once an error occurs the error bits are sticky and are only cleared by 
*  writing 1 to the error register.
*
*******************************************************************************/
uint8 CyDmacError(void) 
{
    return((uint8)(((uint32) 0x0Fu) & *CY_DMA_ERR_PTR));
}


/*******************************************************************************
* Function Name: CyDmacClearError
********************************************************************************
*
* Summary:
*  Clears the error bits in the error register of the DMAC.
*
* Parameters:
* error:
*   Clears the error bits in the DMAC error register.
*
*  DMAC_PERIPH_ERR:
*   Set to 1 when a peripheral responds to a bus transaction with an error
*   response.
*
*  DMAC_UNPOP_ACC:
*   Set to 1 when an access is attempted to an invalid address.
*
*  DMAC_BUS_TIMEOUT:
*   Set to 1 when a bus timeout occurs. Cleared by writing 1. Timeout values
*   are determined by the BUS_TIMEOUT field in the PHUBCFG register.
*
* Return:
*  None
*
* Theory:
*  Once an error occurs the error bits are sticky and are only cleared by 
*  writing 1 to the error register.
*
*******************************************************************************/
void CyDmacClearError(uint8 error) 
{
    *CY_DMA_ERR_PTR = (((uint32)0x0Fu) & ((uint32)error));
}


/*******************************************************************************
* Function Name: CyDmacErrorAddress
********************************************************************************
*
* Summary:
*  When DMAC_BUS_TIMEOUT, DMAC_UNPOP_ACC, and DMAC_PERIPH_ERR occur the
*  address of the error is written to the error address register and can be read
*  with this function.
*
*  If there are multiple errors, only the address of the first is saved.
*
* Parameters:
*  None
*
* Return:
*  The address that caused the error.
*
*******************************************************************************/
uint32 CyDmacErrorAddress(void) 
{
    return(CY_GET_REG32(CY_DMA_ERR_ADR_PTR));
}


/*******************************************************************************
* Function Name: CyDmaChAlloc
********************************************************************************
*
* Summary:
*  Allocates a channel from the DMAC to be used in all functions that require a
*  channel handle.
*
* Parameters:
*  None
*
* Return:
*  The allocated channel number. Zero is a valid channel number.
*  DMA_INVALID_CHANNEL is returned if there are no channels available.
*
*******************************************************************************/
uint8 CyDmaChAlloc(void) 
{
    uint8 interruptState;
    uint8 dmaIndex;
    uint32 channel = 1u;


    /* Enter critical section! */
    interruptState = CyEnterCriticalSection();

    /* Look for free channel. */
    for(dmaIndex = 0u; dmaIndex < CY_DMA_NUMBEROF_CHANNELS; dmaIndex++)
    {
        if(0uL == (CyDmaChannels & channel))
        {
            /* Mark channel as used. */
            CyDmaChannels |= channel;
            break;
        }

        channel <<= 1u;
    }

    if(dmaIndex >= CY_DMA_NUMBEROF_CHANNELS)
    {
        dmaIndex = CY_DMA_INVALID_CHANNEL;
    }

    /* Exit critical section! */
    CyExitCriticalSection(interruptState);

    return(dmaIndex);
}


/*******************************************************************************
* Function Name: CyDmaChFree
********************************************************************************
*
* Summary:
*  Frees a channel allocated by DmaChAlloc().
*
* Parameters:
*  uint8 chHandle:
*   The handle previously returned by CyDmaChAlloc() or DMA_DmaInitalize().
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChFree(uint8 chHandle) 
{
    cystatus status = CYRET_BAD_PARAM;
    uint8 interruptState;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        /* Enter critical section */
        interruptState = CyEnterCriticalSection();

        /* Clear bit mask that keeps track of ownership. */
        CyDmaChannels &= ~(((uint32) 1u) << chHandle);

        /* Exit critical section */
        CyExitCriticalSection(interruptState);
        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChEnable
********************************************************************************
*
* Summary:
*  Enables the DMA channel. A software or hardware request still must happen
*  before the channel is executed.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitalize().
*
*  uint8 preserveTds:
*   Preserves the original TD state when the TD has completed. This parameter
*   applies to all TDs in the channel.
*
*   0 - When TD is completed, the DMAC leaves the TD configuration values in
*   their current state, and does not restore them to their original state.
*
*   1 - When TD is completed, the DMAC restores the original configuration
*   values of the TD.
*
*  When preserveTds is set, the TD slot that equals the channel number becomes
*  RESERVED and that becomes where the working registers exist. So, for example,
*  if you are using CH06 and preserveTds is set, you are not allowed to use TD
*  slot 6. That is reclaimed by the DMA engine for its private use.
*
*  Note Do not chain back to a completed TD if the preserveTds for the channel
*  is set to 0. When a TD has completed preserveTds for the channel set to 0,
*  the transfer count will be at 0. If a TD with a transfer count of 0 is
*  started, the TD will transfer an indefinite amount of data.
*
*  Take extra precautions when using the hardware request (DRQ) option when the
*  preserveTds is set to 0, as you might be requesting the wrong data.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChEnable(uint8 chHandle, uint8 preserveTds) 
{
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        if (0u != preserveTds)
        {
            /* Store intermediate TD states separately in CHn_SEP_TD0/1 to
            *  preserve original TD chain
            */
            CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0u] |= CY_DMA_CH_BASIC_CFG_WORK_SEP;
        }
        else
        {
            /* Store intermediate and final TD states on top of original TD chain */
            CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0u] &= (uint8)(~CY_DMA_CH_BASIC_CFG_WORK_SEP);
        }

        /* Enable channel */
        CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0u] |= CY_DMA_CH_BASIC_CFG_EN;

        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChDisable
********************************************************************************
*
* Summary:
*  Disables the DMA channel. Once this function is called, CyDmaChStatus() may
*  be called to determine when the channel is disabled and which TDs were being
*  executed.
*
*  If it is currently executing it will allow the current burst to finish
*  naturally.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitalize().
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChDisable(uint8 chHandle) 
{
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        /***********************************************************************
        * Should not change configuration information of a DMA channel when it
        * is active (or vulnerable to becoming active).
        ***********************************************************************/

        /* Disable channel */
        CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0] &= ((uint8) (~CY_DMA_CH_BASIC_CFG_EN));

        /* Store intermediate and final TD states on top of original TD chain */
        CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0] &= ((uint8) (~CY_DMA_CH_BASIC_CFG_WORK_SEP));
        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaClearPendingDrq
********************************************************************************
*
* Summary:
*  Clears pending the DMA data request.
*
* Parameters:
*  uint8 chHandle:
*   Handle to the dma channel.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaClearPendingDrq(uint8 chHandle) 
{
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        CY_DMA_CH_STRUCT_PTR[chHandle].action[0] |= CY_DMA_CPU_TERM_CHAIN;
        CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0] |= 0x01u;
        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChPriority
********************************************************************************
*
* Summary:
*  Sets the priority of a DMA channel. You can use this function when you want
*  to change the priority at run time. If the priority remains the same for a
*  DMA channel, then you can configure the priority in the .cydwr file.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitalize().
*
*  uint8 priority:
*   Priority to set the channel to, 0 - 7.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChPriority(uint8 chHandle, uint8 priority) 
{
    uint8 value;
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        value = CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0u] & ((uint8)(~(0x0Eu)));

        CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0u] = value | ((uint8) ((priority & 0x7u) << 0x01u));

        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChSetExtendedAddress
********************************************************************************
*
* Summary:
*  Sets the high 16 bits of the source and destination addresses for the DMA
*  channel (valid for all TDs in the chain).
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitalize().
*
*  uint16 source:
*   Upper 16 bit address of the DMA transfer source.
*
*  uint16 destination:
*   Upper 16 bit address of the DMA transfer destination.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChSetExtendedAddress(uint8 chHandle, uint16 source, uint16 destination) \
    
{
    cystatus status = CYRET_BAD_PARAM;
    reg16 *convert;

    #if(CY_PSOC5)

        /* 0x1FFF8000-0x1FFFFFFF needs to use alias at 0x20008000-0x2000FFFF */
        if(source == 0x1FFFu)
        {
            source = 0x2000u;
        }

        if(destination == 0x1FFFu)
        {
            destination = 0x2000u;
        }

    #endif  /* (CY_PSOC5) */


    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        /* Set source address */
        convert = (reg16 *) &CY_DMA_CFGMEM_STRUCT_PTR[chHandle].CFG1[0];
        CY_SET_REG16(convert, source);

        /* Set destination address */
        convert = (reg16 *) &CY_DMA_CFGMEM_STRUCT_PTR[chHandle].CFG1[2u];
        CY_SET_REG16(convert, destination);
        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChSetInitialTd
********************************************************************************
*
* Summary:
*  Sets the initial TD to be executed for the channel when the CyDmaChEnable()
*  function is called.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitialize().
*
*  uint8 startTd:
*   Set the TD index as the first TD associated with the channel. Zero is
*   a valid TD index.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChSetInitialTd(uint8 chHandle, uint8 startTd) 
{
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        CY_DMA_CH_STRUCT_PTR[chHandle].basic_status[1u] = startTd;
        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChSetRequest
********************************************************************************
*
* Summary:
*  Allows the caller to terminate a chain of TDs, terminate one TD, or create a
*  direct request to start the DMA channel.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitalize().
*
*  uint8 request:
*   One of the following constants. Each of the constants is a three-bit value.
*
*   CPU_REQ         - Create a direct request to start the DMA channel
*   CPU_TERM_TD     - Terminate one TD
*   CPU_TERM_CHAIN  - Terminate a chain of TDs
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChSetRequest(uint8 chHandle, uint8 request) 
{
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        CY_DMA_CH_STRUCT_PTR[chHandle].action[0u] |= (request & (CPU_REQ | CPU_TERM_TD | CPU_TERM_CHAIN));
        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChGetRequest
********************************************************************************
*
* Summary:
*  This function allows the caller of CyDmaChSetRequest() to determine if the
*  request was completed.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitalize().
*
* Return:
*  Returns a three-bit field, corresponding to the three bits of the request,
*  which describes the state of the previously posted request. If the value is
*  zero, the request was completed. CY_DMA_INVALID_CHANNEL if the handle is
*  invalid.
*
*******************************************************************************/
cystatus CyDmaChGetRequest(uint8 chHandle) 
{
    cystatus status = CY_DMA_INVALID_CHANNEL;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        status = (cystatus) ((uint32)CY_DMA_CH_STRUCT_PTR[chHandle].action[0u] &
                            (uint32)(CY_DMA_CPU_REQ | CY_DMA_CPU_TERM_TD | CY_DMA_CPU_TERM_CHAIN));
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChStatus
********************************************************************************
*
* Summary:
*  Determines the status of the DMA channel.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitalize().
*
*  uint8 * currentTd:
*   The address to store the index of the current TD. Can be NULL if the value
*   is not needed.
*
*  uint8 * state:
*   The address to store the state of the channel. Can be NULL if the value is
*   not needed.
*
*   STATUS_TD_ACTIVE
*    0: Channel is not currently being serviced by DMAC
*    1: Channel is currently being serviced by DMAC
*
*   STATUS_CHAIN_ACTIVE
*    0: TD chain is inactive; either no DMA requests have triggered a new chain
*       or the previous chain has completed.
*    1: TD chain has been triggered by a DMA request
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
* Theory:
*   The caller can check on the activity of the Current TD and the Chain.
*
*******************************************************************************/
cystatus CyDmaChStatus(uint8 chHandle, uint8 * currentTd, uint8 * state) 
{
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        if(NULL != currentTd)
        {
            *currentTd = CY_DMA_CH_STRUCT_PTR[chHandle].basic_status[1] & 0x7Fu;
        }

        if(NULL != state)
        {
            *state= CY_DMA_CH_STRUCT_PTR[chHandle].basic_status[0];
        }

        status = CYRET_SUCCESS;
    }

    return (status);
}


/*******************************************************************************
* Function Name: CyDmaChSetConfiguration
********************************************************************************
*
* Summary:
* Sets configuration information of the channel.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or DMA_DmaInitialize().
*
*  uint8 burstCount:
*   Specifies the size of bursts (1 to 127) the data transfer should be divided
*   into. If this value is zero then the whole transfer is done in one burst.
*
*  uint8 requestPerBurst:
*   The whole of the data can be split into multiple bursts, if this is
*   required to complete the transaction:
*    0: All subsequent bursts after the first burst will be automatically
*       requested and carried out
*    1: All subsequent bursts after the first burst must also be individually
*       requested.
*
*  uint8 tdDone0:
*   Selects one of the TERMOUT0 interrupt lines to signal completion. The line
*   connected to the nrq terminal will determine the TERMOUT0_SEL definition and
*   should be used as supplied by cyfitter.h
*
*  uint8 tdDone1:
*   Selects one of the TERMOUT1 interrupt lines to signal completion. The line
*   connected to the nrq terminal will determine the TERMOUT1_SEL definition and
*   should be used as supplied by cyfitter.h
*
*  uint8 tdStop:
*   Selects one of the TERMIN interrupt lines to signal to the DMAC that the TD
*   should terminate. The signal connected to the trq terminal will determine
*   which TERMIN (termination request) is used.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChSetConfiguration(uint8 chHandle, uint8 burstCount, uint8 requestPerBurst,
                                 uint8 tdDone0, uint8 tdDone1, uint8 tdStop) 
{
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        CY_DMA_CFGMEM_STRUCT_PTR[chHandle].CFG0[0] = (burstCount & 0x7Fu) | ((uint8)((requestPerBurst & 0x1u) << 7u));
        CY_DMA_CFGMEM_STRUCT_PTR[chHandle].CFG0[1] = ((uint8)((tdDone1 & 0xFu) << 4u)) | (tdDone0 & 0xFu);
        CY_DMA_CFGMEM_STRUCT_PTR[chHandle].CFG0[2] = 0x0Fu & tdStop;
        CY_DMA_CFGMEM_STRUCT_PTR[chHandle].CFG0[3] = 0u; /* burstcount_remain. */

        status = CYRET_SUCCESS;
    }

    return (status);
}


/*******************************************************************************
* Function Name: CyDmaTdAllocate
********************************************************************************
*
* Summary:
*  Allocates a TD for use with an allocated DMA channel.
*
* Parameters:
*  None
*
* Return:
*  Zero-based index of the TD to be used by the caller. Since there are 128 TDs
*  minus the reserved TDs (0 to 23), the value returned would range from 24 to
*  127 not 24 to 128. DMA_INVALID_TD is returned if there are no free TDs
*  available.
*
*******************************************************************************/
uint8 CyDmaTdAllocate(void) 
{
    uint8 interruptState;
    uint8 element = CY_DMA_INVALID_TD;

    /* Enter critical section! */
    interruptState = CyEnterCriticalSection();

    if(CyDmaTdCurrentNumber > NUMBEROF_CHANNELS)
    {
        /* Get pointer to Next available. */
        element = CyDmaTdFreeIndex;

        /* Decrement the count. */
        CyDmaTdCurrentNumber--;

        /* Update next available pointer. */
        CyDmaTdFreeIndex = CY_DMA_TDMEM_STRUCT_PTR[element].TD0[0];
    }

    /* Exit critical section! */
    CyExitCriticalSection(interruptState);

    return(element);
}


/*******************************************************************************
* Function Name: CyDmaTdFree
********************************************************************************
*
* Summary:
*  Returns a TD to the free list.
*
* Parameters:
*  uint8 tdHandle:
*   The TD handle returned by the CyDmaTdAllocate().
*
* Return:
*  None
*
*******************************************************************************/
void CyDmaTdFree(uint8 tdHandle) 
{
    if(tdHandle < CY_DMA_NUMBEROF_TDS)
    {
        /* Enter critical section! */
        uint8 interruptState = CyEnterCriticalSection();

        /* Get pointer to Next available. */
        CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD0[0u] = CyDmaTdFreeIndex;

        /* Set new Next Available. */
        CyDmaTdFreeIndex = tdHandle;

        /* Keep track of how many left. */
        CyDmaTdCurrentNumber++;

        /* Exit critical section! */
        CyExitCriticalSection(interruptState);
    }
}


/*******************************************************************************
* Function Name: CyDmaTdFreeCount
********************************************************************************
*
* Summary:
*  Returns the number of free TDs available to be allocated.
*
* Parameters:
*  None
*
* Return:
*  The number of free TDs.
*
*******************************************************************************/
uint8 CyDmaTdFreeCount(void) 
{
    return(CyDmaTdCurrentNumber - CY_DMA_NUMBEROF_CHANNELS);
}


/*******************************************************************************
* Function Name: CyDmaTdSetConfiguration
********************************************************************************
*
* Summary:
*  Configures the TD.
*
* Parameters:
*  uint8 tdHandle:
*   A handle previously returned by CyDmaTdAlloc().
*
*  uint16 transferCount:
*   The size of the data transfer (in bytes) for this TD. A size of zero will
*   cause the transfer to continue indefinitely. This parameter is limited to
*   4095 bytes; the TD is not initialized at all when a higher value is passed.
*
*  uint8 nextTd:
*   Zero based index of the next Transfer Descriptor in the TD chain. Zero is a
*   valid pointer to the next TD; DMA_END_CHAIN_TD is the end of the chain.
*   DMA_DISABLE_TD indicates an end to the chain and the DMA is disabled. No
*   further TDs are fetched. DMA_DISABLE_TD is only supported on PSoC3 and
*   PSoC 5LP silicons.
*
*  uint8 configuration:
*   Stores the Bit field of configuration bits.
*
*   CY_DMA_TD_SWAP_EN        - Perform endian swap
*
*   CY_DMA_TD_SWAP_SIZE4     - Swap size = 4 bytes
*
*   CY_DMA_TD_AUTO_EXEC_NEXT - The next TD in the chain will trigger
*                              automatically when the current TD completes.
*
*   CY_DMA_TD_TERMIN_EN      - Terminate this TD if a positive edge on the trq
*                              input line occurs. The positive edge must occur
*                              during a burst. That is the only time the DMAC
*                              will listen for it.
*
*   DMA__TD_TERMOUT_EN       - When this TD completes, the TERMOUT signal will
*                              generate a pulse. Note that this option is
*                              instance specific with the instance name followed
*                              by two underscores. In this example, the instance
*                              name is DMA.
*
*   CY_DMA_TD_INC_DST_ADR    - Increment DST_ADR according to the size of each
*                              data transaction in the burst.
*
*   CY_DMA_TD_INC_SRC_ADR    - Increment SRC_ADR according to the size of each
*                              data transaction in the burst.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if tdHandle or transferCount is invalid.
*
*******************************************************************************/
cystatus CyDmaTdSetConfiguration(uint8 tdHandle, uint16 transferCount, uint8 nextTd, uint8 configuration) \
    
{
    cystatus status = CYRET_BAD_PARAM;

    if((tdHandle < CY_DMA_NUMBEROF_TDS) && (0u == (0xF000u & transferCount)))
    {
        /* Set 12 bits transfer count. */
        reg16 *convert = (reg16 *) &CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD0[0u];
        CY_SET_REG16(convert, transferCount);

        /* Set Next TD pointer. */
        CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD0[2u] = nextTd;

        /* Configure the TD */
        CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD0[3u] = configuration;

        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaTdGetConfiguration
********************************************************************************
*
* Summary:
*  Retrieves the configuration of the TD. If a NULL pointer is passed as a
*  parameter, that parameter is skipped. You may request only the values you are
*  interested in.
*
* Parameters:
*  uint8 tdHandle:
*   A handle previously returned by CyDmaTdAlloc().
*
*  uint16 * transferCount:
*   The address to store the size of the data transfer (in bytes) for this TD.
*   A size of zero could indicate that the TD has completed its transfer, or
*   that the TD is doing an indefinite transfer.
*
*  uint8 * nextTd:
*   The address to store the index of the next TD in the TD chain.
*
*  uint8 * configuration:
*   The address to store the Bit field of configuration bits.
*   See CyDmaTdSetConfiguration() function description.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if tdHandle is invalid.
*
* Side Effects:
*  If TD has a transfer count of N and is executed, the transfer count becomes
*  0. If it is reexecuted, the Transfer count of zero will be interpreted as a
*  request for indefinite transfer. Be careful when requesting TD with a
*  transfer count of zero.
*
*******************************************************************************/
cystatus CyDmaTdGetConfiguration(uint8 tdHandle, uint16 * transferCount, uint8 * nextTd, uint8 * configuration) \
    
{
    cystatus status = CYRET_BAD_PARAM;

    if(tdHandle < CY_DMA_NUMBEROF_TDS)
    {
        /* If we have pointer */
        if(NULL != transferCount)
        {
            /* Get 12 bits of transfer count */
            reg16 *convert = (reg16 *) &CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD0[0];
            *transferCount = 0x0FFFu & CY_GET_REG16(convert);
        }

        /* If we have pointer */
        if(NULL != nextTd)
        {
            /* Get Next TD pointer */
            *nextTd = CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD0[2u];
        }

        /* If we have pointer */
        if(NULL != configuration)
        {
            /* Get configuration TD */
            *configuration = CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD0[3u];
        }

        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaTdSetAddress
********************************************************************************
*
* Summary:
*  Sets the lower 16 bits of the source and destination addresses for this TD
*  only.
*
* Parameters:
*  uint8 tdHandle:
*   A handle previously returned by CyDmaTdAlloc().
*
*  uint16 source:
*   The lower 16 address bits of the source of the data transfer.
*
*  uint16 destination:
*   The lower 16 address bits of the destination of the data transfer.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if tdHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaTdSetAddress(uint8 tdHandle, uint16 source, uint16 destination) 
{
    cystatus status = CYRET_BAD_PARAM;
    reg16 *convert;

    if(tdHandle < CY_DMA_NUMBEROF_TDS)
    {
        /* Set source address */
        convert = (reg16 *) &CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD1[0u];
        CY_SET_REG16(convert, source);

        /* Set destination address */
        convert = (reg16 *) &CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD1[2u];
        CY_SET_REG16(convert, destination);

        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaTdGetAddress
********************************************************************************
*
* Summary:
*  Retrieves the lower 16 bits of the source and/or destination addresses for
*  this TD only. If NULL is passed for a pointer parameter, that value is
*  skipped. You may request only the values of interest.
*
* Parameters:
*  uint8 tdHandle:
*   A handle previously returned by CyDmaTdAlloc().
*
*  uint16 * source:
*   The address to store the lower 16 address bits of the source of the data
*   transfer.
*
*  uint16 * destination:
*   The address to store the lower 16 address bits of the destination of the
*   data transfer.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if tdHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaTdGetAddress(uint8 tdHandle, uint16 * source, uint16 * destination) 
{
    cystatus status = CYRET_BAD_PARAM;
    reg16 *convert;

    if(tdHandle < CY_DMA_NUMBEROF_TDS)
    {
        /* If we have a pointer. */
        if(NULL != source)
        {
            /* Get source address */
            convert = (reg16 *) &CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD1[0u];
            *source = CY_GET_REG16(convert);
        }

        /* If we have a pointer. */
        if(NULL != destination)
        {
            /* Get Destination address. */
            convert = (reg16 *) &CY_DMA_TDMEM_STRUCT_PTR[tdHandle].TD1[2u];
            *destination = CY_GET_REG16(convert);
        }

        status = CYRET_SUCCESS;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyDmaChRoundRobin
********************************************************************************
*
* Summary:
*  Either enables or disables the Round-Robin scheduling enforcement algorithm.
*  Within a priority level a Round-Robin fairness algorithm is enforced.
*
* Parameters:
*  uint8 chHandle:
*   A handle previously returned by CyDmaChAlloc() or Dma_DmaInitialize().
*
*  uint8 enableRR:
*   0: Disable Round-Robin fairness algorithm
*   1: Enable Round-Robin fairness algorithm
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_BAD_PARAM if chHandle is invalid.
*
*******************************************************************************/
cystatus CyDmaChRoundRobin(uint8 chHandle, uint8 enableRR) 
{
    cystatus status = CYRET_BAD_PARAM;

    if(chHandle < CY_DMA_NUMBEROF_CHANNELS)
    {
        if (0u != enableRR)
        {
            CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0u] |= (uint8)CY_DMA_ROUND_ROBIN_ENABLE;
        }
        else
        {
            CY_DMA_CH_STRUCT_PTR[chHandle].basic_cfg[0u] &= (uint8)(~CY_DMA_ROUND_ROBIN_ENABLE);
        }

        status = CYRET_SUCCESS;
    }

    return(status);
}


/* [] END OF FILE */
