#include "lcd.h"

static void lcd_send_nibble(uint8_t data)
{
    uint8_t data_t[2];

    data &= 0xF0;

    data_t[0] = data | LCD_EN | LCD_BL;
    data_t[1] = data | LCD_BL;

    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADR, data_t, 2, 100);
}

void lcd_send_data(uint8_t data){
uint8_t data_h, data_l;
	uint8_t data_t[4];
	data_h = data & 0xF0;
	data_l = (data << 4) & 0xF0;
	data_t[0] =data_h | LCD_EN | LCD_BL  |LCD_RS;
	data_t[1] =data_h | LCD_BL| LCD_RS;
	data_t[2] =data_l | LCD_EN | LCD_BL  |LCD_RS;
	data_t[3] =data_l | LCD_BL| LCD_RS;
HAL_I2C_Master_Transmit(&hi2c1, LCD_ADR,data_t, 4, 100);// CON TRO I2C, DIA CHI , GIA TRI BIT, SO LUONG BIT , TIMEOUT
HAL_Delay(30);
}
void lcd_send_command(uint8_t cmd){ // COMMAND RS = 0 KH CAN OR 
	uint8_t data_h, data_l;
	uint8_t data_t[4];
	data_h=cmd & 0xF0;
	data_l=(cmd <<4)& 0xF0;// KHI AND VOI OXF0 GIU LAI 4 BIT CAO
	data_t[0] = data_h | LCD_EN | LCD_BL ;
	data_t[1] = data_h |  LCD_BL ;
	data_t[2] = data_l | LCD_EN | LCD_BL;
	data_t[3] = data_l |  LCD_BL;
HAL_I2C_Master_Transmit(&hi2c1, LCD_ADR,data_t, 4, 100);// CON TRO I2C, DIA CHI , GIA TRI BIT, SO LUONG BIT , TIMEOUT
HAL_Delay(30);
}
void lcd_init(){
	HAL_Delay(30);
	lcd_send_command(0x30);
	HAL_Delay(1);
	lcd_send_command(0x20);
  HAL_Delay(1);
	lcd_send_command(0x28);// 4  bit interface
	HAL_Delay(1);
	lcd_send_command(0x08);
	HAL_Delay(1);
	lcd_send_command(0x01);// clear
	HAL_Delay(5);
	lcd_send_command(0x06);
	lcd_send_command(0x0C);
}
void lcd_send_string(char *str){
	while(*str){
		lcd_send_data(*str++);
	}
}
void lcd_clear(void){
    lcd_send_command(0x01);// clear
    HAL_Delay(2);
}
void lcd_set_cusor( uint8_t cot , uint8_t hang){
switch (hang) {
        case 0:
            cot |= 0x80;
            break;
        case 1:
            cot	|= 0xC0;
            break;
    }
    lcd_send_command(cot);
}
