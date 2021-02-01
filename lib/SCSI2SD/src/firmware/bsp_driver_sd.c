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
  if (SD_state == MSD_OK)
  {
    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK)
    {
      SD_state = MSD_ERROR;
    }
    else
    {
      SD_state = MSD_OK;
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
  * @param  ReadAddr: Address from where data is to be read  
  * @param  BlockSize: SD card data block size, that should be 512
  * @param  NumOfBlocks: Number of SD blocks to read 
  * @retval SD status
  */
uint8_t BSP_SD_ReadBlocks(uint8_t *pData, uint64_t ReadAddr, uint32_t NumOfBlocks)
{
  if(HAL_SD_ReadBlocks_DMA(&hsd, pData, ReadAddr, NumOfBlocks) != HAL_OK)  
  {
    return MSD_ERROR;
  }
  return MSD_OK;
}

/**
  * @brief  Writes block(s) to a specified address in an SD card, in polling mode. 
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  WriteAddr: Address from where data is to be written  
  * @param  NumOfBlocks: Number of SD blocks to write
  * @retval SD status
  */
uint8_t BSP_SD_WriteBlocks(uint8_t *pData, uint64_t WriteAddr, uint32_t NumOfBlocks)
{
  if(HAL_SD_WriteBlocks_DMA(&hsd, pData, WriteAddr, NumOfBlocks) != HAL_OK)  
  {
    return MSD_ERROR;
  }
  return MSD_OK;
}

/**
  * @brief  Reads block(s) from a specified address in an SD card, in DMA mode. 
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  ReadAddr: Address from where data is to be read  
  * @param  BlockSize: SD card data block size, that should be 512
  * @param  NumOfBlocks: Number of SD blocks to read 
  * @retval SD status
  */
uint8_t BSP_SD_ReadBlocks_DMA(uint8_t *pData, uint64_t ReadAddr, uint32_t NumOfBlocks)
{
  uint8_t SD_state = MSD_OK;
  
  /* Read block(s) in DMA transfer mode */
  if(HAL_SD_ReadBlocks_DMA(&hsd, pData, ReadAddr, NumOfBlocks) != HAL_OK)  
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
  }
  
  return SD_state; 
}

/**
  * @brief  Writes block(s) to a specified address in an SD card, in DMA mode.  
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  WriteAddr: Address from where data is to be written  
  * @param  NumOfBlocks: Number of SD blocks to write 
  * @retval SD status
  */
uint8_t BSP_SD_WriteBlocks_DMA(uint8_t *pData, uint64_t WriteAddr, uint32_t NumOfBlocks)
{
  uint8_t SD_state = MSD_OK;
  
  /* Write block(s) in DMA transfer mode */
  if(HAL_SD_WriteBlocks_DMA(&hsd, pData, WriteAddr, NumOfBlocks) != HAL_OK)  
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
