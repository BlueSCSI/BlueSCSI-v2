/**
  ******************************************************************************
  * @file           : usbh_conf.h
  * @version        : v1.0_Cube
  * @brief          : Header for usbh_conf file.
  ******************************************************************************
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  * 1. Redistributions of source code must retain the above copyright notice,
  * this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  * this list of conditions and the following disclaimer in the documentation
  * and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of its contributors
  * may be used to endorse or promote products derived from this software
  * without specific prior written permission.
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_CONF__H__
#define __USBH_CONF__H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f2xx.h"
#include "stm32f2xx_hal.h"

/**
	MiddleWare name : USB_HOST
	MiddleWare fileName : usbh_conf.h
	MiddleWare version : 
*/
/*----------   -----------*/
#define USBH_MAX_NUM_ENDPOINTS      2 
 
/*----------   -----------*/
#define USBH_MAX_NUM_INTERFACES      2 
 
/*----------   -----------*/
#define USBH_MAX_NUM_CONFIGURATION      1 
 
/*----------   -----------*/
#define USBH_KEEP_CFG_DESCRIPTOR      1 
 
/*----------   -----------*/
#define USBH_MAX_NUM_SUPPORTED_CLASS      1 
 
/*----------   -----------*/
#define USBH_MAX_SIZE_CONFIGURATION      256 
 
/*----------   -----------*/
#define USBH_MAX_DATA_BUFFER      512 
 
/*----------   -----------*/
#define USBH_DEBUG_LEVEL      0 
 
/*----------   -----------*/
#define USBH_USE_OS      0 
 
 
 

/****************************************/
/* #define for FS and HS identification */
#define HOST_HS 		0
#define HOST_FS 		1

/** @defgroup USBH_Exported_Macros
  * @{
  */ 
#if (USBH_USE_OS == 1)
  #include "cmsis_os.h"
  #define   USBH_PROCESS_PRIO          osPriorityNormal
  #define   USBH_PROCESS_STACK_SIZE    ((uint16_t)0)
#endif    

 /* Memory management macros */   
#define USBH_malloc               malloc
#define USBH_free                 free
#define USBH_memset               memset
#define USBH_memcpy               memcpy
    
 /* DEBUG macros */  

#if (USBH_DEBUG_LEVEL > 0)
#define  USBH_UsrLog(...)   printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBH_UsrLog(...)   
#endif 
                            
                            
#if (USBH_DEBUG_LEVEL > 1)

#define  USBH_ErrLog(...)   printf("ERROR: ") ;\
                            printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBH_ErrLog(...)   
#endif 
                            
                            
#if (USBH_DEBUG_LEVEL > 2)                         
#define  USBH_DbgLog(...)   printf("DEBUG : ") ;\
                            printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBH_DbgLog(...)                         
#endif
                            
/**
  * @}
  */ 
                                                                
    
    
/**
  * @}
  */ 

/** @defgroup USBH_CONF_Exported_Types
  * @{
  */ 
/**
  * @}
  */ 

/** @defgroup USBH_CONF_Exported_Macros
  * @{
  */ 
/**
  * @}
  */ 

/** @defgroup USBH_CONF_Exported_Variables
  * @{
  */ 
/**
  * @}
  */ 

/** @defgroup USBH_CONF_Exported_FunctionsPrototype
  * @{
  */ 
/**
  * @}
  */ 

#endif //__USBH_CONF__H__

/**
  * @}
  */ 

/**
  * @}
  */ 
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

