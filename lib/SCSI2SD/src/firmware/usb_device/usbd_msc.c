/**
  ******************************************************************************
  * @file    usbd_msc.c
  * @author  MCD Application Team
  * @version V2.4.1
  * @date    19-June-2015
  * @brief   This file provides all the MSC core functions.
  *
  * @verbatim
  *      
  *          ===================================================================      
  *                                MSC Class  Description
  *          =================================================================== 
  *           This module manages the MSC class V1.0 following the "Universal 
  *           Serial Bus Mass Storage Class (MSC) Bulk-Only Transport (BOT) Version 1.0
  *           Sep. 31, 1999".
  *           This driver implements the following aspects of the specification:
  *             - Bulk-Only Transport protocol
  *             - Subclass : SCSI transparent command set (ref. SCSI Primary Commands - 3 (SPC-3))
  *      
  *  @endverbatim
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

#include "usbd_msc.h"


/**
* @brief  USBD_MSC_RegisterStorage
* @param  fops: storage callback
* @retval status
*/
uint8_t  USBD_MSC_RegisterStorage  (USBD_HandleTypeDef   *pdev, 
                                    USBD_StorageTypeDef *fops)
{
  if(fops != NULL)
  {
    pdev->pUserData= fops;
  }
  return 0;
}


