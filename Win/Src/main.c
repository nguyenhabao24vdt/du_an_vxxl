/* STM32F103C8T6 - Stage 1 modular cooperative scheduler
 * HSE 8 MHz x 9 = SYSCLK 72 MHz; RTC HSE/128, PRL = 62499.
 * DHT11: PA2; LCD1602 I2C1: PB6 PB7 (address set in lcd1602_i2c.c);
 * display-switch button: PB12 (pull-up, active low).
 * Compatible with the previously supplied clock_rtc/dht11/lcd1602_i2c libs.
 */
#include "main.h"
#include "clock_rtc.h"
#include "dht11.h"
#include "lcd1602_i2c.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Set 1 for ONE programming run to set clock. Then set 0 and reflash. */
#define FORCE_SET_TIME 0

#define BUTTON_DEBOUNCE_MS   30U
#define DHT_PERIOD_MS     2000U
#define LCD_POLL_MS        100U

typedef enum {
    SCREEN_CLOCK = 0,
    SCREEN_SENSOR = 1
} ScreenMode;

typedef struct {
    ScreenMode screen;
    uint8_t temperature;
    uint8_t humidity;
    uint8_t sensor_valid;
    uint8_t display_dirty;
} AppState;

I2C_HandleTypeDef hi2c1;
static AppState app = {SCREEN_CLOCK, 0, 0, 0, 1};

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static HAL_StatusTypeDef RTC_HSE128_Init(void);
static void Button_Task(uint32_t now);
static void DHT_Task(uint32_t now);
static void Display_Task(uint32_t now);
void Error_Handler(void);

/* Called on every loop. Each task should return promptly. */
static void Button_Task(uint32_t now)
{
    static GPIO_PinState last_sample = GPIO_PIN_SET;
    static GPIO_PinState stable_state = GPIO_PIN_SET;
    static uint32_t last_change_ms = 0U;
    GPIO_PinState sample = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12);

    if (sample != last_sample) {
        last_sample = sample;
        last_change_ms = now;
    }

    if ((uint32_t)(now - last_change_ms) >= BUTTON_DEBOUNCE_MS &&
        sample != stable_state) {
        stable_state = sample;
        if (stable_state == GPIO_PIN_RESET) {
            app.screen = (app.screen == SCREEN_CLOCK) ? SCREEN_SENSOR : SCREEN_CLOCK;
            app.display_dirty = 1U;
        }
    }
}

/* One bounded DHT11 read about every 2 seconds. Current driver is blocking
 * (includes a 20 ms start pulse), so this is cooperative, not a hard RTOS. */
static void DHT_Task(uint32_t now)
{
    static uint32_t last_read_ms = 0U;
    uint8_t temp, hum;

    if ((uint32_t)(now - last_read_ms) < DHT_PERIOD_MS) {
        return;
    }
    last_read_ms = now;

    if (DHT11_Read(&temp, &hum) == HAL_OK) {
        if (!app.sensor_valid || temp != app.temperature || hum != app.humidity) {
            app.display_dirty = 1U;
        }
        app.temperature = temp;
        app.humidity = hum;
        app.sensor_valid = 1U;
    } else {
        if (app.sensor_valid) {
            app.display_dirty = 1U;
        }
        app.sensor_valid = 0U;
    }
}

/* Send ONLY lines that actually changed. This matters because the supplied
 * LCD driver uses blocking I2C and a per-character HAL_Delay(1). */
static void Display_Task(uint32_t now)
{
    static uint32_t last_poll_ms = 0U;
    static char previous_line1[17] = "";
    static char previous_line2[17] = "";
    char line1[17] = {0};
    char line2[17] = {0};

    if (!app.display_dirty &&
        (uint32_t)(now - last_poll_ms) < LCD_POLL_MS) {
        return;
    }
    last_poll_ms = now;

    if (app.screen == SCREEN_CLOCK) {
        ClockDateTime t;
        if (ClockRTC_Get(&t) == HAL_OK) {
            snprintf(line1, sizeof(line1), "TIME: %02u:%02u:%02u",
                     (unsigned)t.hour, (unsigned)t.minute, (unsigned)t.second);
            snprintf(line2, sizeof(line2), "DATE: %02u/%02u/%04u",
                     (unsigned)t.day, (unsigned)t.month, (unsigned)t.year);
        } else {
            snprintf(line1, sizeof(line1), "RTC ERROR");
            snprintf(line2, sizeof(line2), "CHECK RTC");
        }
    } else if (app.sensor_valid) {
        snprintf(line1, sizeof(line1), "TEMP: %u C", (unsigned)app.temperature);
        snprintf(line2, sizeof(line2), "HUM : %u %%", (unsigned)app.humidity);
    } else {
        snprintf(line1, sizeof(line1), "DHT11 NO DATA");
        snprintf(line2, sizeof(line2), "CHECK PA2");
    }

    if (strcmp(line1, previous_line1) != 0) {
        LCD1602_WriteLine(0, line1);
        strcpy(previous_line1, line1);
    }
    if (strcmp(line2, previous_line2) != 0) {
        LCD1602_WriteLine(1, line2);
        strcpy(previous_line2, line2);
    }
    app.display_dirty = 0U;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    if (RTC_HSE128_Init() != HAL_OK) Error_Handler();
    DHT11_Init();

    /* Set initial time only when the RTC backup marker is missing. */
    const ClockDateTime initial = {2026, 11, 6, 17, 30, 0};
    if (ClockRTC_Init(&initial) != HAL_OK) Error_Handler();

#if FORCE_SET_TIME
    /* CAUTION: at every reset, this forces RTC back to this exact time. */
    if (ClockRTC_Set(&initial) != HAL_OK) Error_Handler();
#endif

    if (LCD1602_Init(&hi2c1) != HAL_OK) Error_Handler();
    app.display_dirty = 1U;

    while (1) {
        uint32_t now = HAL_GetTick();

        /* Add future fast safety-related status checks BEFORE slow tasks.
         * Never block here waiting for servo movement or RFID timeouts. */
        // Fire_Task(now);
        // Rain_Task(now);
        // Door_Task(now);
        // RFID_Task(now);

        Button_Task(now);
        DHT_Task(now);
        Display_Task(now);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static HAL_StatusTypeDef RTC_HSE128_Init(void)
{
    uint32_t start;
    uint32_t prl;

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* RTC source can only be changed after resetting Backup Domain.
     * Existing time is preserved on resets when source is already HSE/128. */
    if ((RCC->BDCR & RCC_BDCR_RTCSEL) != RCC_RTCCLKSOURCE_HSE_DIV128) {
        __HAL_RCC_BACKUPRESET_FORCE();
        __HAL_RCC_BACKUPRESET_RELEASE();
        HAL_PWR_EnableBkUpAccess();
    }

    __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_HSE_DIV128);
    __HAL_RCC_RTC_ENABLE();
    HAL_PWR_EnableBkUpAccess(); /* Stay enabled for HSE/128 on STM32F1. */

    CLEAR_BIT(RTC->CRL, RTC_CRL_RSF);
    start = HAL_GetTick();
    while ((RTC->CRL & RTC_CRL_RSF) == 0U) {
        if ((uint32_t)(HAL_GetTick() - start) > 2000U) return HAL_TIMEOUT;
    }

    start = HAL_GetTick();
    while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
        if ((uint32_t)(HAL_GetTick() - start) > 2000U) return HAL_TIMEOUT;
    }

    prl = ((uint32_t)(RTC->PRLH & 0x0FU) << 16) | RTC->PRLL;
    if (prl != 62499U) {
        SET_BIT(RTC->CRL, RTC_CRL_CNF);
        RTC->PRLH = 0U;
        RTC->PRLL = 62499U;
        CLEAR_BIT(RTC->CRL, RTC_CRL_CNF);

        start = HAL_GetTick();
        while ((RTC->CRL & RTC_CRL_RTOFF) == 0U) {
            if ((uint32_t)(HAL_GetTick() - start) > 2000U) return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef p = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    p.Pin = GPIO_PIN_12;
    p.Mode = GPIO_MODE_INPUT;
    p.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &p);

    p.Pin = GPIO_PIN_2;
    p.Mode = GPIO_MODE_INPUT;
    p.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &p);
}

static void MX_I2C1_Init(void)
{
    GPIO_InitTypeDef p = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    p.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    p.Mode = GPIO_MODE_AF_OD;
    p.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &p);

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000U;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0U;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0U;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
