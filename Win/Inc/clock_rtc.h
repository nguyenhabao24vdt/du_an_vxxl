#ifndef CLOCK_RTC_H_
#define CLOCK_RTC_H_
#include "stm32f1xx_hal.h"
#include <stdint.h>
typedef struct { uint16_t year; uint8_t month,day,hour,minute,second; } ClockDateTime;
HAL_StatusTypeDef ClockRTC_Init(const ClockDateTime *initial);
HAL_StatusTypeDef ClockRTC_Set(const ClockDateTime *value);
HAL_StatusTypeDef ClockRTC_Get(ClockDateTime *value);
#endif

