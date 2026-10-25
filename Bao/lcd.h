
#ifndef LCD_H
#define LCD_H
#include "stm32f1xx_hal.h"
extern I2C_HandleTypeDef hi2c1;
#define LCD_ADR (0x27 <<1) // tai i2c chi chon 7  bit  bit con lai se la rs chon data hoac cmd
#define LCD_BL  0x08
#define LCD_EN  0x04
#define LCD_RS  0x01
void lcd_send_data(uint8_t data);
void lcd_send_command(uint8_t cmd);
void lcd_init(void);
void lcd_send_string(char *str);
void lcd_clear(void);
void lcd_set_cusor(uint8_t cot, uint8_t hang);
#endif