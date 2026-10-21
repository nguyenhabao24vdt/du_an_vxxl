  #ifndef LCD_H
  #define LCD_H
	#include <Stdint.h>
	#include "stm32f1xx_hal.h"
	
#define  MDDATA  0x01 
#define  MCMD     0x00
/*
RS&RW :PORT_R
D0-D7 :PORT_DATA
*/
typedef struct{
	GPIO_TypeDef *PORT_DATA;
  GPIO_TypeDef *PORT_R;
	uint16_t D0;
	uint16_t D1;
	uint16_t D2;
	uint16_t D3;
	uint16_t D4;
	uint16_t D5;
	uint16_t D6;
	uint16_t D7;
	uint16_t RS;
	uint16_t RW;
	uint16_t En;
	
}LCD;
void LCD_Init(LCD *mLCD);
void LCD_Write_byte(LCD *mLCD ,uint8_t data, uint8_t mode );
void LCD_send_cmd(uint8_t data);
void LCD_Send_data(uint8_t data);
void LCD_Print(char *str);
  #endif