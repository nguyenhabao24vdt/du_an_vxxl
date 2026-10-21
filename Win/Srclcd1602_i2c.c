/*
 * lcd1602_i2c.c
 *
 *  Created on: Oct 21, 2026
 *      Author: LENOVO
 */

#include "lcd1602_i2c.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

#define LCD_ADDR    (0x26 << 1)
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

static void LCD_Write4Bits(uint8_t data)
{
    uint8_t value = data | LCD_BACKLIGHT;

    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR,
                            &value, 1, 100);

    value |= LCD_ENABLE;

    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR,
                            &value, 1, 100);

    HAL_Delay(1);

    value &= ~LCD_ENABLE;

    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR,
                            &value, 1, 100);
}

static void LCD_Send(uint8_t data, uint8_t mode)
{
    uint8_t high = (data & 0xF0) | mode;
    uint8_t low  = ((data << 4) & 0xF0) | mode;

    LCD_Write4Bits(high);
    LCD_Write4Bits(low);
}

static void LCD_Command(uint8_t cmd)
{
    LCD_Send(cmd, 0);

    if (cmd == 0x01 || cmd == 0x02)
        HAL_Delay(2);
}

static void LCD_Data(uint8_t data)
{
    LCD_Send(data, LCD_RS);
}

void LCD_Init(void)
{
    HAL_Delay(50);

    LCD_Write4Bits(0x30);
    HAL_Delay(5);

    LCD_Write4Bits(0x30);
    HAL_Delay(1);

    LCD_Write4Bits(0x30);
    HAL_Delay(1);

    LCD_Write4Bits(0x20);
    HAL_Delay(1);

    LCD_Command(0x28);
    LCD_Command(0x0C);
    LCD_Command(0x06);
    LCD_Command(0x01);
}

void LCD_Clear(void)
{
    LCD_Command(0x01);
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    if (row == 0)
        address = 0x00 + col;
    else
        address = 0x40 + col;

    LCD_Command(0x80 | address);
}

void LCD_Print(char *str)
{
    while (*str)
    {
        LCD_Data((uint8_t)*str++);
    }
}


