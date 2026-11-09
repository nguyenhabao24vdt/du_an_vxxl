#ifndef GAS_ALARM_H
#define GAS_ALARM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    GAS_WARMUP = 0,
    GAS_NORMAL,
    GAS_LEVEL1,
    GAS_LEVEL2,
    GAS_FAULT
} GasState;

typedef struct
{
    bool buzzer;

    bool led_normal;
    bool led_level1;
    bool led_level2;
    bool led_fan;

    bool valve_closed;
    uint8_t fan_percent;
} GasOutputs;

typedef struct
{
    GasState state;
    uint16_t adc;

    bool valve_closed;
    bool muted;
    bool safe_tracking;
    bool safe_ready;
    bool post_active;

    uint32_t boot_ms;
    uint32_t state_ms;
    uint32_t last_sample_ms;
    uint32_t safe_since_ms;
    uint32_t mute_since_ms;
    uint32_t post_since_ms;
} GasAlarm;

void Gas_Init(GasAlarm *g, uint32_t now);

void Gas_OnSample(
    GasAlarm *g,
    uint16_t adc,
    uint32_t now
);

void Gas_Tick(GasAlarm *g, uint32_t now);
void Gas_Fault(GasAlarm *g, uint32_t now);
void Gas_Mute(GasAlarm *g, uint32_t now);

bool Gas_ResetValve(GasAlarm *g, uint32_t now);

GasOutputs Gas_GetOutputs(
    const GasAlarm *g,
    uint32_t now
);

#endif