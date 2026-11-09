#include "gas_alarm.h"
#include "gas_config.h"

static bool IsAlarm(GasState state)
{
    return state == GAS_LEVEL1 || state == GAS_LEVEL2;
}

static void ChangeState(
    GasAlarm *g,
    GasState next,
    uint32_t now)
{
    GasState old = g->state;

    if (old == next)
    {
        return;
    }

    g->state = next;
    g->state_ms = now;

    if (IsAlarm(next))
    {
        g->post_active = false;
    }

    /* Len muc 2: dong van va huy tat coi tam thoi */
    if (next == GAS_LEVEL2)
    {
        g->valve_closed = true;
        g->muted = false;
    }

    if (next == GAS_NORMAL)
    {
        g->muted = false;

        /* Sau bao dong, quat tiep tuc chay them */
        if (IsAlarm(old))
        {
            g->post_active = true;
            g->post_since_ms = now;
        }
    }
}

void Gas_Init(GasAlarm *g, uint32_t now)
{
    *g = (GasAlarm){0};

    g->state = GAS_WARMUP;
    g->valve_closed = true;

    g->boot_ms = now;
    g->state_ms = now;
    g->last_sample_ms = now;
}

void Gas_Fault(GasAlarm *g, uint32_t now)
{
    if (g->state != GAS_FAULT)
    {
        g->state_ms = now;
        g->muted = false;
    }

    g->state = GAS_FAULT;
    g->valve_closed = true;
    g->safe_ready = false;
    g->safe_tracking = false;
    g->post_active = false;
}

void Gas_Tick(GasAlarm *g, uint32_t now)
{
    if (g->muted &&
        (uint32_t)(now - g->mute_since_ms) >= GAS_MUTE_MS)
    {
        g->muted = false;
    }
    /*
     * Phat hien chuong trinh khong nhan mau ADC.
     * Khong tu phat hien duoc dut day cam bien.
     */
    if ((uint32_t)(now - g->last_sample_ms) > GAS_STALE_MS)
    {
        Gas_Fault(g, now);
    }
}

void Gas_OnSample(
    GasAlarm *g,
    uint16_t adc,
    uint32_t now)
{
    /* Loi duoc giu den khi khoi dong lai */
    if (g->state == GAS_FAULT)
    {
        return;
    }

    if (adc > 4095U)
    {
        Gas_Fault(g, now);
        return;
    }

    g->adc = adc;
    g->last_sample_ms = now;

    /* Trong warm-up: chi cap nhat ADC, chua bao khi */
    if (g->state == GAS_WARMUP &&
        (uint32_t)(now - g->boot_ms) < GAS_WARMUP_MS)
    {
        g->safe_tracking = false;
        g->safe_ready = false;
        return;
    }

    /* Xac nhan ADC thap lien tuc */
    if (adc <= GAS_SAFE_ADC)
    {
        if (!g->safe_tracking)
        {
            g->safe_tracking = true;
            g->safe_since_ms = now;
        }

        g->safe_ready =
            (uint32_t)(now - g->safe_since_ms) >= GAS_SAFE_MS;
    }
    else
    {
        g->safe_tracking = false;
        g->safe_ready = false;
    }

    /* Muc 2 uu tien cao nhat */
    if (adc >= GAS_LEVEL2_ON_ADC)
    {
        ChangeState(g, GAS_LEVEL2, now);
        return;
    }

    switch (g->state)
    {
        case GAS_WARMUP:
            if (adc >= GAS_LEVEL1_ON_ADC)
            {
                ChangeState(g, GAS_LEVEL1, now);
            }
            else if (g->safe_ready)
            {
                ChangeState(g, GAS_NORMAL, now);
            }
            break;

        case GAS_NORMAL:
            if (adc >= GAS_LEVEL1_ON_ADC)
            {
                ChangeState(g, GAS_LEVEL1, now);
            }
            break;

        case GAS_LEVEL1:
            if (g->safe_ready)
            {
                ChangeState(g, GAS_NORMAL, now);
            }
            break;

        case GAS_LEVEL2:
            if (g->safe_ready)
            {
                ChangeState(g, GAS_NORMAL, now);
            }
            else if (adc <= GAS_LEVEL2_OFF_ADC)
            {
                ChangeState(g, GAS_LEVEL1, now);
            }
            break;

        case GAS_FAULT:
        default:
            break;
    }
}

void Gas_Mute(GasAlarm *g, uint32_t now)
{
    if (IsAlarm(g->state) || g->state == GAS_FAULT)
    {
        g->muted = true;
        g->mute_since_ms = now;
    }
}

bool Gas_ResetValve(GasAlarm *g, uint32_t now)
{
    if (g->state == GAS_NORMAL &&
        g->safe_ready &&
        (uint32_t)(now - g->last_sample_ms) <= GAS_STALE_MS)
    {
        g->valve_closed = false;
        g->muted = false;
        return true;
    }

    return false;
}

GasOutputs Gas_GetOutputs(
    const GasAlarm *g,
    uint32_t now)
{
    GasOutputs out = {0};
    uint32_t elapsed = now - g->state_ms;

    out.valve_closed = g->valve_closed;

    switch (g->state)
    {
        case GAS_WARMUP:
            out.led_normal = ((elapsed / 500U) % 2U) == 0U;
            break;

        case GAS_NORMAL:
            out.led_normal = true;
            out.fan_percent = g->post_active ? 100U : 0U;
            break;

        case GAS_LEVEL1:
            out.led_level1 = true;
            out.buzzer = (elapsed % 1000U) < 200U;
            out.fan_percent = FAN_LEVEL1_PERCENT;
            break;

        case GAS_LEVEL2:
            out.led_level2 = ((elapsed / 200U) % 2U) == 0U;
            out.buzzer = true;
            out.fan_percent = FAN_LEVEL2_PERCENT;
            break;

        case GAS_FAULT:
        default:
            out.led_level1 = ((elapsed / 250U) % 2U) == 0U;
            out.led_level2 = out.led_level1;
            out.buzzer = out.led_level1;
            out.fan_percent = 100U;
            out.valve_closed = true;
            break;
    }

    out.led_fan = out.fan_percent > 0U;

    if (g->muted)
    {
        out.buzzer = false;
    }

    return out;
}
