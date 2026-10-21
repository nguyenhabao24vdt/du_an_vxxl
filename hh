MX_I2C1_Init();

lcd_init();

lcd_send_string("HELLO STM32");
lcd_send_command(0xC0);
lcd_send_string("LCD TEST");

while (1)
{
}
