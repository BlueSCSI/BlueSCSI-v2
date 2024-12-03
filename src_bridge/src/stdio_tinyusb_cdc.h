
#pragma once
#include "FreeRTOS.h"


#ifdef __cplusplus
extern "C" {
#endif

#define CDC_STACK_SIZE      configMINIMAL_STACK_SIZE

void stdio_tinyusb_cdc_init(void);
bool stdio_tinyusb_cdc_start_task(uint8_t priority);
void stdio_tinyusb_cdc_task(void* pvParameters);
void stdio_tinyusb_cdc_deinit();
char stdio_tinyusb_cdc_readchar();

#ifdef __cplusplus
}
#endif

