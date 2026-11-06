#include "clock_rtc.h"

/* Hardware RTC CNT stores seconds since 2000-01-01 00:00:00. */
#define CLOCK_MAGIC 0xA55AU
static uint8_t leap(uint16_t y) {
    return (uint8_t)((y % 4U == 0U) && ((y % 100U != 0U) || (y % 400U == 0U)));
}
static uint8_t days_in_month(uint16_t y, uint8_t m) {
    static const uint8_t tab[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1U || m > 12U) return 0;
    return (uint8_t)(tab[m-1U] + ((m==2U) && leap(y)));
}
static uint8_t valid(const ClockDateTime *d) {
    return d && d->year >= 2000U && d->year <= 2099U &&
        d->month >= 1U && d->month <= 12U &&
        d->day >= 1U && d->day <= days_in_month(d->year, d->month) &&
        d->hour < 24U && d->minute < 60U && d->second < 60U;
}
static uint32_t seconds_from_datetime(const ClockDateTime *d) {
    uint32_t days = 0U;
    for (uint16_t y=2000U; y<d->year; ++y) days += leap(y) ? 366U : 365U;
    for (uint8_t m=1U; m<d->month; ++m) days += days_in_month(d->year,m);
    days += (uint32_t)d->day - 1U;
    return days*86400UL + (uint32_t)d->hour*3600UL + (uint32_t)d->minute*60UL + d->second;
}
static void datetime_from_seconds(uint32_t sec, ClockDateTime *d) {
    uint32_t days=sec/86400UL, rem=sec%86400UL;
    d->hour=(uint8_t)(rem/3600UL);
    d->minute=(uint8_t)((rem%3600UL)/60UL);
    d->second=(uint8_t)(rem%60UL);
    d->year=2000U;
    while (d->year < 2100U) {
        uint32_t n=leap(d->year)?366U:365U;
        if(days<n) break;
        days-=n;
        ++d->year;
    }
    d->month=1U;
    while(d->month <= 12U) {
        uint32_t n=days_in_month(d->year,d->month);
        if(days<n || n==0U) break;
        days-=n;
        ++d->month;
    }
    d->day=(uint8_t)(days+1U);
}
static uint32_t rtc_count(void) {
    uint32_t h1,h2,l;
    do { h1=RTC->CNTH; l=RTC->CNTL; h2=RTC->CNTH; } while(h1!=h2);
    return (h1<<16) | l;
}
static HAL_StatusTypeDef rtc_wait_writable(void) {
    uint32_t start=HAL_GetTick();
    while((RTC->CRL & RTC_CRL_RTOFF)==0U) {
        if((uint32_t)(HAL_GetTick()-start)>1000U) return HAL_TIMEOUT;
    }
    return HAL_OK;
}
HAL_StatusTypeDef ClockRTC_Set(const ClockDateTime *d) {
    if(!valid(d)) return HAL_ERROR;
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess(); // MUST stay 1 for HSE/128 RTC
    if(rtc_wait_writable()!=HAL_OK) return HAL_TIMEOUT;
    SET_BIT(RTC->CRL, RTC_CRL_CNF);
    uint32_t s=seconds_from_datetime(d);
    RTC->CNTH=(uint16_t)(s>>16);
    RTC->CNTL=(uint16_t)s;
    CLEAR_BIT(RTC->CRL, RTC_CRL_CNF);
    if(rtc_wait_writable()!=HAL_OK) return HAL_TIMEOUT;
    BKP->DR1=CLOCK_MAGIC;
    return HAL_OK;
}
HAL_StatusTypeDef ClockRTC_Init(const ClockDateTime *first) {
    if(BKP->DR1 == CLOCK_MAGIC) return HAL_OK;
    return ClockRTC_Set(first);
}
HAL_StatusTypeDef ClockRTC_Get(ClockDateTime *d) {
    if(!d || BKP->DR1 != CLOCK_MAGIC) return HAL_ERROR;
    datetime_from_seconds(rtc_count(),d);
    return valid(d) ? HAL_OK : HAL_ERROR;
}
