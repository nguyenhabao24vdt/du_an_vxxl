/*
 * dht11.c
 *
 *  Created on: Oct 21, 2026
 *      Author: LENOVO
 */

#include "dht11.h"

#define DHT11_PORT GPIOA
#define DHT11_PIN  GPIO_PIN_2

static void DHT11_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while ((DWT->CYCCNT - start) < ticks);
}

static void DHT11_DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void DHT11_PinOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void DHT11_PinInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static uint8_t DHT11_WaitLevel(GPIO_PinState level,
                               uint32_t timeout_us)
{
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != level)
    {
        if (timeout_us == 0)
            return 0;

        DHT11_DelayUs(1);
        timeout_us--;
    }

    return 1;
}

uint8_t DHT11_Read(uint8_t *temperature, uint8_t *humidity)
{
    uint8_t data[5] = {0};

    if (temperature == NULL || humidity == NULL)
        return 0;

    DHT11_DWT_Init();

    DHT11_PinOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    DHT11_DelayUs(30);

    DHT11_PinInput();

    if (!DHT11_WaitLevel(GPIO_PIN_RESET, 100))
        return 0;

    if (!DHT11_WaitLevel(GPIO_PIN_SET, 100))
        return 0;

    if (!DHT11_WaitLevel(GPIO_PIN_RESET, 100))
        return 0;

    for (uint8_t i = 0; i < 40; i++)
    {
        if (!DHT11_WaitLevel(GPIO_PIN_SET, 100))
            return 0;

        DHT11_DelayUs(40);

        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)
            == GPIO_PIN_SET)
        {
            data[i / 8] |= (uint8_t)(1U << (7 - (i % 8)));
        }

        if (!DHT11_WaitLevel(GPIO_PIN_RESET, 100))
            return 0;
    }

    if ((uint8_t)(data[0] + data[1] + data[2] + data[3])
        != data[4])
    {
        return 0;
    }

    *humidity = data[0];
    *temperature = data[2];

    return 1;
}


