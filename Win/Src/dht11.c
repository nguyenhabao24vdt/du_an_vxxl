#include "dht11.h"

#define DHT_PORT GPIOA
#define DHT_PIN  GPIO_PIN_2

static uint32_t cycles_per_us;

static void pin_mode(uint32_t output)
{
    GPIO_InitTypeDef p = {0};
    p.Pin = DHT_PIN;
    p.Speed = GPIO_SPEED_FREQ_LOW;
    if (output) {
        p.Mode = GPIO_MODE_OUTPUT_OD;
    } else {
        p.Mode = GPIO_MODE_INPUT;
        p.Pull = GPIO_PULLUP;
    }
    HAL_GPIO_Init(DHT_PORT, &p);
}

static void wait_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * cycles_per_us;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {}
}

/* Wait until DATA reaches expected level, limited by microsecond timeout. */
static uint8_t wait_level(GPIO_PinState level, uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = timeout_us * cycles_per_us;
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) != level) {
        if ((uint32_t)(DWT->CYCCNT - start) >= cycles) return 0;
    }
    return 1;
}

void DHT11_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    cycles_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
    if (cycles_per_us == 0) cycles_per_us = 1;
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    pin_mode(0);
}

HAL_StatusTypeDef DHT11_Read(uint8_t *temperature, uint8_t *humidity)
{
    uint8_t data[5] = {0};
    if (!temperature || !humidity) return HAL_ERROR;
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) return HAL_ERROR;

    /* MCU start: LOW >= 18ms, then release DATA. */
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    pin_mode(1);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET); /* open drain releases */
    wait_us(30);
    pin_mode(0);

    if (!wait_level(GPIO_PIN_RESET, 110)) return HAL_TIMEOUT;
    if (!wait_level(GPIO_PIN_SET,   110)) return HAL_TIMEOUT;
    if (!wait_level(GPIO_PIN_RESET, 110)) return HAL_TIMEOUT;

    for (uint8_t i = 0; i < 40; i++) {
        if (!wait_level(GPIO_PIN_SET, 90)) return HAL_TIMEOUT;  /* end ~50us LOW */
        wait_us(40);  /* sample in HIGH pulse: ~28us means 0, ~70us means 1 */
        data[i / 8] <<= 1;
        if (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET)
            data[i / 8] |= 1;
        if (!wait_level(GPIO_PIN_RESET, 100)) return HAL_TIMEOUT;
    }

    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
        return HAL_ERROR;
    *humidity = data[0];
    *temperature = data[2];
    return HAL_OK;
}
