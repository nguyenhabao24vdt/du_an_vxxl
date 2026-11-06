#include "lcd1602_i2c.h"
#include <string.h>

#define LCD_ADDR (0x26U << 1)  /* 7-bit I2C address = 0x26; change if needed */
#define LCD_BL   0x08U
#define LCD_EN   0x04U
#define LCD_RS   0x01U

static I2C_HandleTypeDef *lcd_i2c;
static void write_raw(uint8_t x)
{
    HAL_I2C_Master_Transmit(lcd_i2c, LCD_ADDR, &x, 1, 30);
}
static void send_nibble(uint8_t high_nibble, uint8_t rs)
{
    uint8_t v = (high_nibble & 0xF0U) | LCD_BL | (rs ? LCD_RS : 0);
    write_raw(v | LCD_EN);
    write_raw(v);
}
static void send_byte(uint8_t val, uint8_t rs)
{
    send_nibble(val & 0xF0U, rs);
    send_nibble((uint8_t)(val << 4), rs);
    HAL_Delay(1);
}
HAL_StatusTypeDef LCD1602_Init(I2C_HandleTypeDef *i2c)
{
    if (i2c == NULL) return HAL_ERROR;
    lcd_i2c = i2c;
    if (HAL_I2C_IsDeviceReady(lcd_i2c, LCD_ADDR, 3, 100) != HAL_OK)
        return HAL_ERROR;
    HAL_Delay(50);
    send_nibble(0x30, 0); HAL_Delay(5);
    send_nibble(0x30, 0); HAL_Delay(5);
    send_nibble(0x30, 0); HAL_Delay(2);
    send_nibble(0x20, 0); HAL_Delay(2);
    send_byte(0x28, 0); /* 4-bit, 2 lines */
    send_byte(0x08, 0); /* display off */
    send_byte(0x01, 0); HAL_Delay(2); /* clear once */
    send_byte(0x06, 0); /* entry */
    send_byte(0x0C, 0); /* display on, cursor off */
    return HAL_OK;
}
void LCD1602_WriteLine(uint8_t row, const char *text)
{
    if (!lcd_i2c || !text || row > 1) return;
    send_byte((uint8_t)(0x80U + (row ? 0x40U : 0x00U)), 0);
    uint8_t j = 0;
    for (; j < 16 && text[j] != '\0'; ++j) send_byte((uint8_t)text[j], 1);
    for (; j < 16; ++j) send_byte(' ', 1);
}
