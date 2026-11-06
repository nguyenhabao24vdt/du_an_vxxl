#ifndef LCD1602_I2C_H
#define LCD1602_I2C_H
#include "stm32f1xx_hal.h"
#include <stdint.h>
/* PCF8574: P0=RS P1=RW P2=EN P3=BL P4..P7=LCD D4..D7 */
HAL_StatusTypeDef LCD1602_Init(I2C_HandleTypeDef *i2c);
void LCD1602_WriteLine(uint8_t row, const char *text);
#endif
