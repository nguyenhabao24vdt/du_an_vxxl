#ifndef DHT11_H
#define DHT11_H
#include "stm32f1xx_hal.h"
#include <stdint.h>

/* DATA: PA2. Uses CPU DWT CYCCNT (no TIM peripheral). */
void DHT11_Init(void);
HAL_StatusTypeDef DHT11_Read(uint8_t *temperature, uint8_t *humidity);
#endif
