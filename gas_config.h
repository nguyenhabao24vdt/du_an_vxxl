#ifndef GAS_CONFIG_H
#define GAS_CONFIG_H

/* Nguong ADC sau bo chia ap: CHI DUNG THU NGHIEM */
#define GAS_SAFE_ADC          400U
#define GAS_LEVEL1_ON_ADC     600U
#define GAS_LEVEL2_OFF_ADC    900U
#define GAS_LEVEL2_ON_ADC     1100U

/* Thoi gian, don vi ms */
#define GAS_WARMUP_MS       60000U
#define GAS_SAMPLE_MS           20U
#define GAS_STALE_MS           200U
#define GAS_SAFE_MS           5000U
/* Thoi gian day khi vao sau bao dong */
#define GAS_FAN_POST_MS      10000U

/* Khoang dung truoc khi doi chieu */
#define FAN_REVERSE_WAIT_MS    500U
#define GAS_MUTE_MS          30000U

#define BUTTON_DEBOUNCE_MS       30U
#define BUTTON_LONG_MS         3000U

/* So mau trung binh truot */
#define GAS_FILTER_COUNT         8U

/* Quat hai day: ban dau de 100% o ca hai muc */
#define FAN_LEVEL1_PERCENT     60U
#define FAN_LEVEL2_PERCENT     100U

#if !((GAS_SAFE_ADC < GAS_LEVEL1_ON_ADC) && \
      (GAS_LEVEL1_ON_ADC < GAS_LEVEL2_OFF_ADC) && \
      (GAS_LEVEL2_OFF_ADC < GAS_LEVEL2_ON_ADC))
#error Invalid ADC threshold ordering
#endif

#if (GAS_FILTER_COUNT < 1U)
#error GAS_FILTER_COUNT must be positive
#endif

#endif