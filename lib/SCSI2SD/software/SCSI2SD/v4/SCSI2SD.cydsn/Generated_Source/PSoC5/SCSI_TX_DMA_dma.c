/***************************************************************************
* File Name: SCSI_TX_DMA_dma.c  
* Version 1.70
*
*  Description:
*   Provides an API for the DMAC component. The API includes functions
*   for the DMA controller, DMA channels and Transfer Descriptors.
*
*
*   Note:
*     This module requires the developer to finish or fill in the auto
*     generated funcions and setup the dma channel and TD's.
*
********************************************************************************
* Copyright 2008-2010, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
********************************************************************************/
#include <CYLIB.H>
#include <CYDMAC.H>
#include <SCSI_TX_DMA_dma.H>



/****************************************************************************
* 
* The following defines are available in Cyfitter.h
* 
* 
* 
* SCSI_TX_DMA__DRQ_CTL_REG
* 
* 
* SCSI_TX_DMA__DRQ_NUMBER
* 
* Number of TD's used by this channel.
* SCSI_TX_DMA__NUMBEROF_TDS
* 
* Priority of this channel.
* SCSI_TX_DMA__PRIORITY
* 
* True if SCSI_TX_DMA_TERMIN_SEL is used.
* SCSI_TX_DMA__TERMIN_EN
* 
* TERMIN interrupt line to signal terminate.
* SCSI_TX_DMA__TERMIN_SEL
* 
* 
* True if SCSI_TX_DMA_TERMOUT0_SEL is used.
* SCSI_TX_DMA__TERMOUT0_EN
* 
* 
* TERMOUT0 interrupt line to signal completion.
* SCSI_TX_DMA__TERMOUT0_SEL
* 
* 
* True if SCSI_TX_DMA_TERMOUT1_SEL is used.
* SCSI_TX_DMA__TERMOUT1_EN
* 
* 
* TERMOUT1 interrupt line to signal completion.
* SCSI_TX_DMA__TERMOUT1_SEL
* 
****************************************************************************/


/* Zero based index of SCSI_TX_DMA dma channel */
uint8 SCSI_TX_DMA_DmaHandle = DMA_INVALID_CHANNEL;

/*********************************************************************
* Function Name: uint8 SCSI_TX_DMA_DmaInitalize
**********************************************************************
* Summary:
*   Allocates and initialises a channel of the DMAC to be used by the
*   caller.
*
* Parameters:
*   BurstCount.
*       
*       
*   ReqestPerBurst.
*       
*       
*   UpperSrcAddress.
*       
*       
*   UpperDestAddress.
*       
*
* Return:
*   The channel that can be used by the caller for DMA activity.
*   DMA_INVALID_CHANNEL (0xFF) if there are no channels left. 
*
*
*******************************************************************/
uint8 SCSI_TX_DMA_DmaInitialize(uint8 BurstCount, uint8 ReqestPerBurst, uint16 UpperSrcAddress, uint16 UpperDestAddress) 
{

    /* Allocate a DMA channel. */
    SCSI_TX_DMA_DmaHandle = (uint8)SCSI_TX_DMA__DRQ_NUMBER;

    /* Configure the channel. */
    (void)CyDmaChSetConfiguration(SCSI_TX_DMA_DmaHandle,
                                  BurstCount,
                                  ReqestPerBurst,
                                  (uint8)SCSI_TX_DMA__TERMOUT0_SEL,
                                  (uint8)SCSI_TX_DMA__TERMOUT1_SEL,
                                  (uint8)SCSI_TX_DMA__TERMIN_SEL);

    /* Set the extended address for the transfers */
    (void)CyDmaChSetExtendedAddress(SCSI_TX_DMA_DmaHandle, UpperSrcAddress, UpperDestAddress);

    /* Set the priority for this channel */
    (void)CyDmaChPriority(SCSI_TX_DMA_DmaHandle, (uint8)SCSI_TX_DMA__PRIORITY);
    
    return SCSI_TX_DMA_DmaHandle;
}

/*********************************************************************
* Function Name: void SCSI_TX_DMA_DmaRelease
**********************************************************************
* Summary:
*   Frees the channel associated with SCSI_TX_DMA.
*
*
* Parameters:
*   void.
*
*
*
* Return:
*   void.
*
*******************************************************************/
void SCSI_TX_DMA_DmaRelease(void) 
{
    /* Disable the channel */
    (void)CyDmaChDisable(SCSI_TX_DMA_DmaHandle);
}

