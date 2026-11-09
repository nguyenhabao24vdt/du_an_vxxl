#ifndef GAS_APP_H
#define GAS_APP_H

#include "gas_alarm.h"

extern GasAlarm g_alarm;
extern GasOutputs g_outputs;

/* Bien xem trong Watch */
extern volatile uint16_t g_adc_raw;
extern volatile uint16_t g_adc_filtered;
extern volatile uint32_t g_adc_count;

extern bool g_manual_fan;
extern int8_t g_fan_direction;

void GasApp_Init(void);
void GasApp_Task(void);

#endif