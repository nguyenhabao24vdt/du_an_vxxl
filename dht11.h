/*
 * dht11.h
 *
 *  Created on: Oct 21, 2026
 *      Author: LENOVO
 */

#ifndef INC_DHT11_H_
#define INC_DHT11_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

uint8_t DHT11_Read(uint8_t *temperature, uint8_t *humidity);

#endif /* INC_DHT11_H_ */
