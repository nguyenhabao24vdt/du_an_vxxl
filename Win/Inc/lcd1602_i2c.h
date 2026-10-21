/*
 * lcd1602_i2c.h
 *
 *  Created on: Oct 21, 2026
 *      Author: LENOVO
 */

#ifndef INC_LCD1602_I2C_H_
#define INC_LCD1602_I2C_H_

#include "stm32f1xx_hal.h"

void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Print(char *str);

#endif /* INC_LCD1602_I2C_H_ */
