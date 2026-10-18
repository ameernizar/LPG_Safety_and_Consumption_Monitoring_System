#include "mq6_monitor.h"

void MQ6_MonitorInit(MQ6_Monitor *m, uint32_t now_ms)
{
    *m = (MQ6_Monitor){0};
    m->settling_started_ms = now_ms;
    m->status = MQ6_MON_WARMUP;
}

void MQ6_MonitorUpdate(MQ6_Monitor *m, uint32_t now_ms,
                       uint8_t acquired, uint16_t raw, uint8_t batch_saturated)
{
    m->raw_valid = acquired && raw <= 4095U;
    m->valid = 0;
    if (m->raw_valid) m->raw = raw;
    if (!m->raw_valid) m->status = MQ6_MON_ACQ_ERROR;
    else if ((batch_saturated & 1U) || raw == 0U) m->status = MQ6_MON_SAT_LOW;
    else if ((batch_saturated & 2U) || raw == 4095U) m->status = MQ6_MON_SAT_HIGH;
    else
    {
        /* Q8 retains sub-count EMA precision; all values remain 0..4095*256.
         * Signed division avoids implementation-defined negative shifts. */
        int32_t target = (int32_t)raw * 256;
        if (!m->filter_seeded) m->filtered_q8 = target;
        else m->filtered_q8 += (target - m->filtered_q8) / (1L << MQ6_EMA_SHIFT);
        m->filter_seeded = 1;
        m->filtered = (uint16_t)((m->filtered_q8 + 128) / 256);
        m->valid = 1;
        if (!m->settled && (uint32_t)(now_ms - m->settling_started_ms) >= MQ6_SETTLING_MS)
            m->settled = 1;
        if (!m->baseline_valid && m->settled)
        {
            /* Max sum is 4095*1024; baseline excludes settling-period samples. */
            m->baseline_sum += raw;
            if (++m->baseline_count == MQ6_BASELINE_SAMPLES)
            {
                m->baseline = (uint16_t)((m->baseline_sum + MQ6_BASELINE_SAMPLES / 2U)
                                         / MQ6_BASELINE_SAMPLES);
                m->baseline_valid = 1;
            }
        }
        if (m->baseline_valid)
        {
            m->deviation = (int16_t)((int32_t)m->filtered - m->baseline);
            m->status = MQ6_MON_MONITORING;
        }
        else m->status = m->settled ? MQ6_MON_BASELINE : MQ6_MON_WARMUP;
        return;
    }
    /* Never use invalid data in the EMA/baseline or expose stale deviations.
     * Before baseline completion require a fresh uninterrupted settling period.
     * A completed baseline stays fixed until reset; recovery re-seeds the EMA. */
    m->filter_seeded = 0;
    m->deviation = 0;
    if (!m->baseline_valid)
    {
        m->settling_started_ms = now_ms;
        m->settled = 0;
        m->baseline_count = 0;
        m->baseline_sum = 0;
    }
}

const char *MQ6_MonitorLabel(MQ6_MonitorStatus status)
{
    switch (status)
    {
    case MQ6_MON_WARMUP: return "WARMUP";
    case MQ6_MON_BASELINE: return "BASELINE";
    case MQ6_MON_MONITORING: return "MONITORING";
    case MQ6_MON_SAT_LOW: return "SAT LOW";
    case MQ6_MON_SAT_HIGH: return "SAT HIGH";
    default: return "ACQ ERROR";
    }
}
