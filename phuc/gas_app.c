#include "main.h"
#include "gas_config.h"
#include "gas_app.h"

/* Hai handle do CubeMX tao trong main.c */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim4;

GasAlarm g_alarm;
GasOutputs g_outputs;

volatile uint16_t g_adc_raw = 0U;
volatile uint16_t g_adc_filtered = 0U;
volatile uint32_t g_adc_count = 0U;

/* Du lieu tu ngat ADC */
static volatile uint16_t adc_mailbox = 0U;
static volatile uint8_t adc_ready = 0U;
static volatile uint8_t adc_error = 0U;

static bool adc_busy;
static uint32_t sample_ms;

/* Bo loc trung binh truot */
static uint16_t filter_buffer[GAS_FILTER_COUNT];
static uint32_t filter_sum;
static uint32_t filter_index;
static uint32_t filter_used;

/* Du lieu nut tu ngat */
static volatile uint8_t button_edge_pending;
static volatile uint8_t button_edge_pressed;
static volatile uint32_t button_edge_ms;

/* Trang thai chong doi nut */
static bool button_candidate;
static bool button_stable;
static bool button_long_sent;
static uint32_t button_change_ms;
static uint32_t button_press_ms;

/* Nut hut mui PA2 */
static volatile uint8_t odor_edge_pending;
static bool odor_candidate;
static bool odor_stable;
static bool odor_armed;
static bool odor_press_pending;
static uint32_t odor_change_ms;

/* Bien xem trong Watch */
bool g_manual_fan = false;

/* 0: dung, 1: hut ra, -1: day vao */
int8_t g_fan_direction = 0;

/* Dieu khien doi chieu */
static bool fan_reverse_level;
static uint32_t fan_stop_ms;
static bool post_started;

static uint16_t Filter_Push(uint16_t value)
{
    if (filter_used < GAS_FILTER_COUNT)
    {
        filter_buffer[filter_index] = value;
        filter_sum += value;
        filter_used++;
    }
    else
    {
        filter_sum -= filter_buffer[filter_index];
        filter_buffer[filter_index] = value;
        filter_sum += value;
    }

    filter_index++;

    if (filter_index >= GAS_FILTER_COUNT)
    {
        filter_index = 0U;
    }

    return (uint16_t)(filter_sum / filter_used);
}

static void WriteOutput(uint16_t pin, bool on)
{
    HAL_GPIO_WritePin(
        GPIOB,
        pin,
        on ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
}

static void Fan_Brake(void)
{
    uint32_t period;

    period = __HAL_TIM_GET_AUTORELOAD(&htim4) + 1U;

    /*
     * Dua PB6 ve cung muc voi PB5:
     * PB5 = 0 -> PB6 = 0
     * PB5 = 1 -> PB6 = 1
     * Hai dau vao cung muc: phanh motor.
     */
    __HAL_TIM_SET_COMPARE(
        &htim4,
        TIM_CHANNEL_1,
        fan_reverse_level ? period : 0U
    );
}

static void Fan_Task(uint32_t now)
{
    int8_t requested = 0;
    uint32_t duty = 0U;
    uint32_t period;

    period = __HAL_TIM_GET_AUTORELOAD(&htim4) + 1U;

    /* Khi bao dong / loi / warm-up, huy hut mui bang tay */
    if (g_alarm.state != GAS_NORMAL)
    {
        g_manual_fan = false;
    }

    if (!g_alarm.post_active)
    {
        post_started = false;
    }

    /* Dem 40 giay tu luc motor bat dau chay chieu day vao */
    if (g_alarm.post_active && post_started &&
        (uint32_t)(now - g_alarm.post_since_ms)
            >= GAS_FAN_POST_MS)
    {
        g_alarm.post_active = false;
        post_started = false;
    }

    /* Chon yeu cau chay quat */
    if (g_alarm.state == GAS_NORMAL)
    {
        if (g_manual_fan)
        {
            requested = 1;
            duty = 100U;
        }
        else if (g_alarm.post_active)
        {
            requested = -1;
            duty = 100U;
        }
    }
    else if (g_outputs.fan_percent > 0U)
    {
        /*
         * Bao dong va GAS_FAULT:
         * giu huong hut ra, giu toc do tu logic gas.
         */
        requested = 1;
        duty = g_outputs.fan_percent;
    }

    if (duty > 100U)
    {
        duty = 100U;
    }

    /*
     * Can dung hoac can doi chieu:
     * phanh truoc, ghi lai thoi diem dung.
     */
    if (requested == 0 ||
        (g_fan_direction != 0 &&
         requested != g_fan_direction))
    {
        Fan_Brake();

        if (g_fan_direction != 0)
        {
            fan_stop_ms = now;
        }

        g_fan_direction = 0;
        g_outputs.fan_percent = 0U;
        return;
    }

    /* Cho motor dung truoc khi cap dien lai */
    if (g_fan_direction == 0 &&
        (uint32_t)(now - fan_stop_ms) < FAN_REVERSE_WAIT_MS)
    {
        Fan_Brake();
        g_outputs.fan_percent = 0U;
        return;
    }

    fan_reverse_level = requested < 0;

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_5,
        fan_reverse_level ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    if (fan_reverse_level)
    {
        /* PB5 = 1: thoi gian PB6 o muc 0 tao chieu nguoc */
        __HAL_TIM_SET_COMPARE(
            &htim4,
            TIM_CHANNEL_1,
            (period * (100U - duty)) / 100U
        );
    }
    else
    {
        /* PB5 = 0: PWM thong thuong */
        __HAL_TIM_SET_COMPARE(
            &htim4,
            TIM_CHANNEL_1,
            (period * duty) / 100U
        );
    }

    g_fan_direction = requested;
    g_outputs.fan_percent = (uint8_t)duty;

    if (requested < 0 &&
        g_alarm.post_active &&
        !post_started)
    {
        post_started = true;
        g_alarm.post_since_ms = now;
    }
}


static void ApplyOutputs(void)
{
    WriteOutput(GPIO_PIN_0,  g_outputs.buzzer);
    WriteOutput(GPIO_PIN_12, g_outputs.led_normal);
    WriteOutput(GPIO_PIN_13, g_outputs.led_level1);
    WriteOutput(GPIO_PIN_14, g_outputs.led_level2);
    WriteOutput(GPIO_PIN_15, g_outputs.valve_closed);

    Fan_Task(HAL_GetTick());

    /*
     * Giu bien led_fan de xem trang thai trong Watch.
     * Khong xuat bien nay ra PB5 nua.
     */
    g_outputs.led_fan = g_outputs.fan_percent > 0U;
}

/* Ngat ADC: chi luu ket qua */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc_mailbox = (uint16_t)HAL_ADC_GetValue(hadc);
        adc_ready = 1U;
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc_error = 1U;
    }
}

/* Ngat nut PA1: luu muc chan va thoi diem */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_1)
    {
        /* Giu nguyen xu ly nut van / mute */
        button_edge_pressed =
            (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)
             == GPIO_PIN_RESET) ? 1U : 0U;

        button_edge_ms = HAL_GetTick();
        button_edge_pending = 1U;
    }
    else if (GPIO_Pin == GPIO_PIN_2)
    {
        /* Nut hut mui: chi danh dau su kien */
        odor_edge_pending = 1U;
    }
}

static void Button_InitLocal(uint32_t now)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    button_edge_pending = 0U;

    button_stable =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET;

    button_candidate = button_stable;
    button_change_ms = now;
    button_press_ms = now;

    /* Neu giu nut luc khoi dong: doi tha ra truoc */
    button_long_sent = button_stable;

    __set_PRIMASK(primask);
}

static void Button_Task(void)
{
    uint8_t pending;
    uint8_t pressed;
    uint32_t edge_ms;
    uint32_t now;
    uint32_t primask;
    bool physical_pressed;

    primask = __get_PRIMASK();
    __disable_irq();

    pending = button_edge_pending;
    pressed = button_edge_pressed;
    edge_ms = button_edge_ms;
    button_edge_pending = 0U;
    now = HAL_GetTick();

    __set_PRIMASK(primask);

    if (pending != 0U)
    {
        button_candidate = pressed != 0U;
        button_change_ms = edge_ms;
    }

    /* Doc lai chan de xac nhan sau khi doi */
    physical_pressed =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET;

    if (physical_pressed != button_candidate)
    {
        button_candidate = physical_pressed;
        button_change_ms = now;
    }

    if (button_candidate != button_stable &&
        (uint32_t)(now - button_change_ms) >= BUTTON_DEBOUNCE_MS)
    {
        button_stable = button_candidate;

        if (button_stable)
        {
            button_press_ms = now;
            button_long_sent = false;
        }
        else if (!button_long_sent)
        {
            /* Nhan ngan: mute khi tha nut */
            Gas_Mute(&g_alarm, now);
        }
    }

    if (button_stable &&
        button_candidate &&
        !button_long_sent &&
        (uint32_t)(now - button_press_ms) >= BUTTON_LONG_MS)
    {
        button_long_sent = true;
        (void)Gas_ResetValve(&g_alarm, now);
    }
}

static void OdorButton_Init(uint32_t now)
{
    odor_edge_pending = 0U;

    odor_stable =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET;

    odor_candidate = odor_stable;
    odor_change_ms = now;

    /* Neu giu nut khi khoi dong: phai tha ra truoc */
    odor_armed = !odor_stable;
    odor_press_pending = false;

    g_manual_fan = false;
}

static void OdorButton_Task(void)
{
    uint32_t now;
    uint32_t primask;
    uint8_t edge;
    bool pressed;

    /* Lay co ngat, tranh xung dot voi callback */
    primask = __get_PRIMASK();
    __disable_irq();

    edge = odor_edge_pending;
    odor_edge_pending = 0U;

    __set_PRIMASK(primask);

    now = HAL_GetTick();

    if (edge != 0U)
    {
        odor_press_pending = true;
    }

    pressed =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET;

    if (pressed != odor_candidate)
    {
        odor_candidate = pressed;
        odor_change_ms = now;
    }

    if (odor_candidate != odor_stable &&
        (uint32_t)(now - odor_change_ms) >= BUTTON_DEBOUNCE_MS)
    {
        odor_stable = odor_candidate;

        if (!odor_stable)
        {
            /* Da tha nut: cho phep lan nhan tiep theo */
            odor_armed = true;
            odor_press_pending = false;
        }
        else
        {
            if (odor_armed && odor_press_pending)
            {
                /* Chi dieu khien tay khi khong bao dong */
                if (g_alarm.state == GAS_NORMAL)
                {
                    g_manual_fan = !g_manual_fan;

                    /* Nhan nut thi ket thuc che do day vao */
                    g_alarm.post_active = false;
                    post_started = false;
                }
            }

            odor_armed = false;
            odor_press_pending = false;
        }
    }
}

void GasApp_Init(void)
{
    uint32_t now = HAL_GetTick();

    adc_busy = false;
    adc_ready = 0U;
    adc_error = 0U;

    filter_sum = 0U;
    filter_index = 0U;
    filter_used = 0U;

    g_adc_raw = 0U;
    g_adc_filtered = 0U;
    g_adc_count = 0U;

        Gas_Init(&g_alarm, now);
    Button_InitLocal(now);
    OdorButton_Init(now);

    g_fan_direction = 0;
    fan_reverse_level = false;
    post_started = false;

    /* Cho phep lan chay dau ma khong phai cho them */
    fan_stop_ms = now - FAN_REVERSE_WAIT_MS;

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);

    /*
     * Tat preload CCR1 de gia tri duty duoc ap dung ngay,
     * tranh duty cu con giu den chu ky sau khi doi PB5.
     */
    htim4.Instance->CCMR1 &= ~TIM_CCMR1_OC1PE;

    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0U);

    if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1) != HAL_OK)
    {
        Gas_Fault(&g_alarm, HAL_GetTick());
    }

    /* Hieu chuan ADC truoc lan do dau */
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        Gas_Fault(&g_alarm, HAL_GetTick());
    }

    now = HAL_GetTick();
    g_alarm.last_sample_ms = now;

    /* Cho phep do ngay o lan chay Task dau */
    sample_ms = now - GAS_SAMPLE_MS;

    g_outputs = Gas_GetOutputs(&g_alarm, now);
    ApplyOutputs();
}

void GasApp_Task(void)
{
    uint32_t now;
    uint32_t primask;
    uint16_t sample;
    uint8_t ready;
    uint8_t error;

    primask = __get_PRIMASK();
    __disable_irq();

    sample = adc_mailbox;
    ready = adc_ready;
    error = adc_error;

    adc_ready = 0U;
    adc_error = 0U;

    __set_PRIMASK(primask);

    now = HAL_GetTick();

    /* Kiem tra mau qua han truoc khi cap nhat mau moi */
    Gas_Tick(&g_alarm, now);

    if (error != 0U)
    {
        (void)HAL_ADC_Stop_IT(&hadc1);
        adc_busy = false;
        Gas_Fault(&g_alarm, now);
    }
    else if (ready != 0U)
    {
        if (HAL_ADC_Stop_IT(&hadc1) != HAL_OK)
        {
            Gas_Fault(&g_alarm, now);
        }

        adc_busy = false;

        g_adc_raw = sample;
        g_adc_filtered = Filter_Push(sample);
        g_adc_count++;

        Gas_OnSample(&g_alarm, g_adc_filtered, now);
    }

    /* Bat dau mot phep do don, khong cho doi ket qua */
    if (g_alarm.state != GAS_FAULT &&
        !adc_busy &&
        (uint32_t)(now - sample_ms) >= GAS_SAMPLE_MS)
    {
        sample_ms = now;
        adc_busy = true;

        if (HAL_ADC_Start_IT(&hadc1) != HAL_OK)
        {
            adc_busy = false;
            Gas_Fault(&g_alarm, now);
        }
    }

    Button_Task();
    OdorButton_Task();

    now = HAL_GetTick();
    g_outputs = Gas_GetOutputs(&g_alarm, now);
    ApplyOutputs();
}
