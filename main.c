
#include "stm32f1xx_hal.h"
#include "dht11.h"
#include "lcd1602_i2c.h"
#include <stdio.h>
#include <stdint.h>

/* I2C handle dung chung voi thu vien LCD */
I2C_HandleTypeDef hi2c1;

/* Khai bao ham */
void SystemClock_Config(void);
static void GPIO_Init_Custom(void);
static void I2C1_Init_Custom(void);
void Error_Handler(void);

int main(void)
{
    HAL_Init();

    SystemClock_Config();
    GPIO_Init_Custom();
    I2C1_Init_Custom();

    HAL_Delay(100);

    LCD_Init();
    LCD_Clear();

    uint8_t temperature = 0;
    uint8_t humidity = 0;
    char line[17];

    LCD_SetCursor(0, 0);
    LCD_Print("DHT11 + LCD");
    LCD_SetCursor(1, 0);
    LCD_Print("Initializing");
    HAL_Delay(1500);
    LCD_Clear();

    while (1)
    {
        if (DHT11_Read(&temperature, &humidity))
        {
            LCD_SetCursor(0, 0);
            snprintf(line, sizeof(line),
                     "Temp: %2u C    ", temperature);
            LCD_Print(line);

            LCD_SetCursor(1, 0);
            snprintf(line, sizeof(line),
                     "Humi: %2u %%    ", humidity);
            LCD_Print(line);
        }
        else
        {
            LCD_SetCursor(0, 0);
            LCD_Print("DHT11 READ ERROR");

            LCD_SetCursor(1, 0);
            LCD_Print("Check sensor   ");
        }

        HAL_Delay(2000);
    }
}

/* Clock: HSE 8 MHz, PLL x9 => 72 MHz */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState = RCC_HSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* DHT11 DATA: PA2 */
static void GPIO_Init_Custom(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
}

/* LCD I2C: PB6=SCL, PB7=SDA */
static void I2C1_Init_Custom(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
