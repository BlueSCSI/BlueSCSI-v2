/**
 ******************************************************************************
  * @file    bsp_driver_sd.c (based on stm324x9i_eval_sd.c)
  * @brief   This file includes a generic uSD card driver.
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
#define BUS_4BITS 1
/* USER CODE BEGIN 0 */
/* Includes ------------------------------------------------------------------*/
#include "bsp_driver_sd.h"
#include "sd.h"

/* Extern variables ---------------------------------------------------------*/ 
  
extern SD_HandleTypeDef hsd;

/**
  * @brief  Initializes the SD card device.
  * @param  None
  * @retval SD status
  */
uint8_t BSP_SD_Init(void)
{
  uint8_t SD_state = MSD_OK;
  /* Check if the SD card is plugged in the slot */
  if (BSP_SD_IsDetected() != SD_PRESENT)
  {
    return MSD_ERROR;
  }
  SD_state = HAL_SD_Init(&hsd);
#ifdef BUS_4BITS
  if (SD_state == HAL_OK)
  {
    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK)
    {
      SD_state = MSD_ERROR;
    }
    else
    {
      SD_state = MSD_OK;

// Clock bypass mode is broken on STM32F205
// This just corrupts data for now.
//#ifdef STM32F4xx
#if 0
      uint8_t SD_hs[64]  = {0};
      //uint32_t SD_scr[2] = {0, 0};
      //uint32_t SD_SPEC   = 0 ;
      uint32_t count = 0;
      uint32_t *tempbuff = (uint32_t *)SD_hs;

      // Prepare to read 64 bytes training data
      SDIO_DataInitTypeDef config;
      config.DataTimeOut   = SDMMC_DATATIMEOUT;
      config.DataLength    = 64;
      config.DataBlockSize = SDIO_DATABLOCK_SIZE_64B;
      config.TransferDir   = SDIO_TRANSFER_DIR_TO_SDIO;
      config.TransferMode  = SDIO_TRANSFER_MODE_BLOCK;
      config.DPSM          = SDIO_DPSM_ENABLE;
      (void)SDIO_ConfigData(hsd.Instance, &config);

      // High speed switch.
      // SDR25 (25MB/s) mode 0x80FFFF01
      // Which is the max without going to 1.8v
      uint32_t errorstate = SDMMC_CmdSwitch(hsd.Instance, 0x80FFFF01);

      // Low-level init for the bypass. Changes registers only
      hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_ENABLE;
      SDIO_Init(hsd.Instance, hsd.Init); 

      // Now we read some training data

      if (errorstate == HAL_SD_ERROR_NONE)
      {
        while(!__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DATAEND/* | SDIO_FLAG_STBITERR*/))
        {
          if (__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_RXFIFOHF))
          {
            for (count = 0; count < 8; count++)
            {
              *(tempbuff + count) = SDIO_ReadFIFO(hsd.Instance);
            }

            tempbuff += 8;
          }
        }

        if (__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_DTIMEOUT))
        {
          __HAL_SD_CLEAR_FLAG(&hsd, SDIO_FLAG_DTIMEOUT);
          SD_state = MSD_ERROR;
        }
        else if (__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_DCRCFAIL))
        {
          __HAL_SD_CLEAR_FLAG(&hsd, SDIO_FLAG_DCRCFAIL);
          SD_state = MSD_ERROR;
        }
        else if (__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_RXOVERR))
        {
          __HAL_SD_CLEAR_FLAG(&hsd, SDIO_FLAG_RXOVERR);
          SD_state = MSD_ERROR;
        }
        /*else if (__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_STBITERR))
        {
          __HAL_SD_CLEAR_FLAG(&hsd, SDIO_FLAG_STBITERR);
          SD_state = MSD_ERROR;
        }*/
        else
        {
          count = SD_DATATIMEOUT;

          while ((__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_RXDAVL)) && (count > 0))
          {
            *tempbuff = SDIO_ReadFIFO(hsd.Instance);
            tempbuff++;
            count--;
          }

          /* Clear all the static flags */
          __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);
        }
      }
#endif
    }
  }
#endif
  return SD_state;
}

/**
  * @brief  Configures Interrupt mode for SD detection pin.
  * @param  None
  * @retval Returns 0 in success otherwise 1. 
  */
uint8_t BSP_SD_ITConfig(void)
{  
  /* TBI: add user code here depending on the hardware configuration used */
  
  return 0;
}

/** @brief  SD detect IT treatment
  * @param  None
  * @retval None
  */
void BSP_SD_DetectIT(void)
{
  /* TBI: add user code here depending on the hardware configuration used */
}

/** @brief  SD detect IT detection callback
  * @param  None
  * @retval None
  */
__weak void BSP_SD_DetectCallback(void)
{
  /* NOTE: This function Should not be modified, when the callback is needed,
  the SD_DetectCallback could be implemented in the user file
  */ 
  
}

/**
  * @brief  Reads block(s) from a specified address in an SD card, in polling mode. 
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  BlockAddr: Address from where data is to be read  
  * @param  NumOfBlocks: Number of SD blocks to read 
  * @retval SD status
  */
/*
uint8_t BSP_SD_ReadBlocks(uint8_t *pData, uint64_t BlockAddr, uint32_t NumOfBlocks)
{
  if(HAL_SD_ReadBlocks_DMA(&hsd, pData, BlockAddr, NumOfBlocks) != HAL_OK)  
  {
    return MSD_ERROR;
  }
  return MSD_OK;
}*/

/**
  * @brief  Writes block(s) to a specified address in an SD card, in polling mode. 
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  BlockAddr: Address from where data is to be written  
  * @param  NumOfBlocks: Number of SD blocks to write
  * @retval SD status
  */
/*uint8_t BSP_SD_WriteBlocks(uint8_t *pData, uint64_t BlockAddr, uint32_t NumOfBlocks)
{
  if(HAL_SD_WriteBlocks_DMA(&hsd, pData, BlockAddr, NumOfBlocks) != HAL_OK)  
  {
    return MSD_ERROR;
  }
  return MSD_OK;
}*/

/**
  * @brief  Reads block(s) from a specified address in an SD card, in DMA mode. 
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  BlockAddr: Address from where data is to be read  
  * @param  NumOfBlocks: Number of SD blocks to read 
  * @retval SD status
  */
uint8_t BSP_SD_ReadBlocks_DMA(uint8_t *pData, uint64_t BlockAddr, uint32_t NumOfBlocks)
{
  uint8_t SD_state = MSD_OK;
  
  /* Read block(s) in DMA transfer mode */
  if(HAL_SD_ReadBlocks_DMA(&hsd, pData, BlockAddr, NumOfBlocks) != HAL_OK)  
  {
    SD_state = MSD_ERROR;
  }
  
  /* Wait until transfer is complete */
  if(SD_state == MSD_OK)
  {
    while (HAL_SD_GetState(&hsd) == HAL_SD_STATE_BUSY) {}

    if(HAL_SD_GetState(&hsd) == HAL_SD_STATE_ERROR)
    {
      SD_state = MSD_ERROR;
    }
    else
    {
      SD_state = MSD_OK;
    }

    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd);
    while (cardState == HAL_SD_CARD_PROGRAMMING || cardState == HAL_SD_CARD_SENDING) 
    {
        cardState = HAL_SD_GetCardState(&hsd);
    }
  }
  
  return SD_state; 
}

/**
  * @brief  Writes block(s) to a specified address in an SD card, in DMA mode.  
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  BlockAddr: Address from where data is to be written  
  * @param  NumOfBlocks: Number of SD blocks to write 
  * @retval SD status
  */
uint8_t BSP_SD_WriteBlocks_DMA(uint8_t *pData, uint64_t BlockAddr, uint32_t NumOfBlocks)
{
  uint8_t SD_state = MSD_OK;
  
  /* Write block(s) in DMA transfer mode */
  if(HAL_SD_WriteBlocks_DMA(&hsd, BlockAddr, NumOfBlocks) != HAL_OK)  
  {
    SD_state = MSD_ERROR;
  }
  
  /* Wait until transfer is complete */
  if(SD_state == MSD_OK)
  {
    for (int i = 0; i < NumOfBlocks; ++i)
    {
        while (HAL_SD_GetState(&hsd) == HAL_SD_STATE_BUSY) {}

        // Wait while the SD card has no buffer space
        while (sdIsBusy()) {}

        HAL_SD_WriteBlocks_Data(&hsd, pData + (i * 512));
    }

    while (HAL_SD_GetState(&hsd) == HAL_SD_STATE_BUSY) {} // Wait for DMA to complete

    if (NumOfBlocks > 1)
    {
        SDMMC_CmdStopTransfer(hsd.Instance);
    }

    if(HAL_SD_GetState(&hsd) == HAL_SD_STATE_ERROR)
    {
      SD_state = MSD_ERROR;
    }
    else
    {
      SD_state = MSD_OK;
    }

    // Wait while the SD card is in the PROGRAMMING state.
    while (sdIsBusy()) {}

    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd);
    while (cardState == HAL_SD_CARD_PROGRAMMING || cardState == HAL_SD_CARD_RECEIVING) 
    {
        // Wait while the SD card is writing buffer to flash
        // The card may remain in the RECEIVING state (even though it's programming) if
        // it has buffer space to receive more data available.

        cardState = HAL_SD_GetCardState(&hsd);
    }
  }
  
  return SD_state; 
}

/**
  * @brief  Erases the specified memory area of the given SD card. 
  * @param  StartAddr: Start byte address
  * @param  EndAddr: End byte address
  * @retval SD status
  */
/*
uint8_t BSP_SD_Erase(uint64_t StartAddr, uint64_t EndAddr)
{
  if(HAL_SD_Erase(&hsd, StartAddr, EndAddr) != SD_OK)  
  {
    return MSD_ERROR;
  }

  return MSD_OK;
}
*/

/**
  * @brief  Handles SD card interrupt request.
  * @param  None
  * @retval None
  */
/*void BSP_SD_IRQHandler(void)
{
  HAL_SD_IRQHandler(&hsd);
}*/

/**
  * @brief  Handles SD DMA Tx transfer interrupt request.
  * @param  None
  * @retval None
  */
/*void BSP_SD_DMA_Tx_IRQHandler(void)
{
  HAL_DMA_IRQHandler(hsd.hdmatx); 
}*/

/**
  * @brief  Handles SD DMA Rx transfer interrupt request.
  * @param  None
  * @retval None
  */
/*
void BSP_SD_DMA_Rx_IRQHandler(void)
{
  HAL_DMA_IRQHandler(hsd.hdmarx);
}*/

/**
  * @brief  Gets the current SD card data status.
  * @param  None
  * @retval Data transfer state.
  *          This value can be one of the following values:
  *            @arg  SD_TRANSFER_OK: No data transfer is acting
  *            @arg  SD_TRANSFER_BUSY: Data transfer is acting
  *            @arg  SD_TRANSFER_ERROR: Data transfer error 
  */
HAL_SD_CardStateTypeDef BSP_SD_GetStatus(void)
{
  return(HAL_SD_GetState(&hsd));
}

/**
  * @brief  Get SD information about specific SD card.
  * @param  CardInfo: Pointer to HAL_SD_CardInfoTypeDef structure
  * @retval None 
  */
void BSP_SD_GetCardInfo(HAL_SD_CardInfoTypeDef* CardInfo)
{
  /* Get SD card Information */
  HAL_SD_GetCardInfo(&hsd, CardInfo);
}
/* USER CODE END 0 */

/**
 * @brief  Detects if SD card is correctly plugged in the memory slot or not.
 * @param  None
 * @retval Returns if SD is detected or not
 */
uint8_t BSP_SD_IsDetected(void)
{
  __IO uint8_t status = SD_PRESENT;

  /* USER CODE BEGIN 1 */
  /* user code can be inserted here */
  /* USER CODE END 1 */    
  
  return status;
}

/* USER CODE BEGIN AdditionalCode */
/* user code can be inserted here */
/* USER CODE END AdditionalCode */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
