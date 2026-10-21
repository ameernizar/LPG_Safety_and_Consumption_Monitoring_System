#include "mq6_monitor.h"

static void ClearCapture(MQ6_Monitor *m)
{
    m->baseline_count = 0;
    m->baseline_sum = 0;
    m->capture_first_half_sum = 0;
    m->capture_min = 4095;
    m->capture_max = 0;
    m->capturing = 0;
}
static void ClearQualification(MQ6_Monitor *m)
{
    ClearCapture(m);
    m->window_count = 0;
    m->window_next = 0;
}
static void UpdateSettled(MQ6_Monitor *m, uint32_t now)
{
    if (!m->settled && (uint32_t)(now - m->settling_started_ms) >= MQ6_SETTLING_MS)
        m->settled = 1;
}
void MQ6_MonitorInit(MQ6_Monitor *m, uint32_t now_ms)
{
    *m = (MQ6_Monitor){0};
    m->settling_started_ms = now_ms;
    m->status = MQ6_MON_WARMUP;
}
void MQ6_MonitorRequestBaseline(MQ6_Monitor *m, uint32_t now_ms)
{
    ClearQualification(m);
    m->baseline_valid = 0;
    m->baseline = 0;
    m->deviation = 0;
    UpdateSettled(m, now_ms);
    m->status = m->settled ? MQ6_MON_STABILIZING : MQ6_MON_WARMUP;
}
/* Equal halves: compare integer sums, avoiding division/rounding at thresholds.
 * Each sum <= 4095*512, drift product <= 4095*512; all fit uint32_t. */
static uint8_t DriftOK(uint32_t first, uint32_t second, uint32_t half, uint32_t limit)
{
    uint32_t difference = first > second ? first-second : second-first;
    return difference <= half * limit;
}
static uint8_t WindowStable(const MQ6_Monitor *m, uint32_t now)
{
    if (m->window_count < MQ6_STABILITY_SAMPLES) return 0;
    /* When full, next insertion position is the oldest sample. */
    if ((uint32_t)(now - m->sample_ticks[m->window_next]) < MQ6_STABILITY_SPAN_MS) return 0;
    uint16_t low = 4095, high = 0;
    uint32_t first = 0, second = 0;
    for (unsigned i = 0; i < MQ6_STABILITY_SAMPLES; ++i)
    {
        uint16_t value = m->samples[(m->window_next + i) % MQ6_STABILITY_SAMPLES];
        if (value < low) low = value;
        if (value > high) high = value;
        if (i < MQ6_STABILITY_SAMPLES/2U) first += value;
        else second += value;
    }
    return (uint32_t)(high-low) <= MQ6_STABILITY_RANGE &&
           DriftOK(first, second, MQ6_STABILITY_SAMPLES/2U, MQ6_STABILITY_DRIFT);
}
void MQ6_MonitorUpdate(MQ6_Monitor *m, uint32_t now_ms,
                       uint8_t acquired, uint16_t raw, uint8_t batch_saturated)
{
    UpdateSettled(m, now_ms);
    if (!acquired || raw > 4095U || raw == 0U || raw == 4095U || batch_saturated)
    {
        m->raw_valid = acquired && raw <= 4095U;
        if (m->raw_valid) m->raw = raw;
        m->status = MQ6_MON_ACQ_ERROR;
        if (m->raw_valid && ((batch_saturated & 1U) || raw == 0U)) m->status = MQ6_MON_SAT_LOW;
        else if (m->raw_valid && ((batch_saturated & 2U) || raw == 4095U)) m->status = MQ6_MON_SAT_HIGH;
        m->valid = 0;
        m->filter_seeded = 0;
        m->deviation = 0;
        m->last_sample_valid = 0;
        ClearQualification(m); /* Keep original settling time and any accepted baseline. */
        return;
    }
    if (m->last_sample_valid)
    {
        uint32_t gap = now_ms - m->last_sample_ms;
        if (gap == 0U) return; /* Do not turn duplicate calls into evidence. */
        if (gap > MQ6_MAX_SAMPLE_GAP_MS)
        {
            ClearQualification(m);
            m->filter_seeded = 0;
        }
    }
    m->last_sample_valid = 1;
    m->last_sample_ms = now_ms;
    m->raw = raw;
    m->raw_valid = 1;
    int32_t target = (int32_t)raw * 256;
    if (!m->filter_seeded) m->filtered_q8 = target;
    else m->filtered_q8 += (target - m->filtered_q8) / (1L << MQ6_EMA_SHIFT);
    m->filter_seeded = 1;
    m->filtered = (uint16_t)((m->filtered_q8 + 128) / 256);
    m->valid = 1;
    if (!m->baseline_valid)
    {
        m->status = m->settled ? MQ6_MON_STABILIZING : MQ6_MON_WARMUP;
        if (!m->settled) return; /* No warm-up samples enter qualification. */
        m->samples[m->window_next] = raw;
        m->sample_ticks[m->window_next] = now_ms;
        m->window_next = (uint8_t)((m->window_next + 1U) % MQ6_STABILITY_SAMPLES);
        if (m->window_count < MQ6_STABILITY_SAMPLES) ++m->window_count;
        if (!WindowStable(m, now_ms))
        {
            ClearCapture(m); /* Rolling history can continue toward stability. */
            return;
        }
        m->status = MQ6_MON_BASELINE;
        if (!m->capturing)
        {
            m->capturing = 1;
            return; /* Qualification window is excluded from capture. */
        }
        if (raw < m->capture_min) m->capture_min = raw;
        if (raw > m->capture_max) m->capture_max = raw;
        m->baseline_sum += raw;
        if (m->baseline_count < MQ6_BASELINE_SAMPLES/2U) m->capture_first_half_sum += raw;
        ++m->baseline_count;
        if ((uint32_t)(m->capture_max - m->capture_min) > MQ6_CAPTURE_RANGE ||
            (m->baseline_count == MQ6_BASELINE_SAMPLES &&
             !DriftOK(m->capture_first_half_sum, m->baseline_sum - m->capture_first_half_sum,
                      MQ6_BASELINE_SAMPLES/2U, MQ6_CAPTURE_DRIFT)))
        {
            ClearQualification(m);
            m->status = MQ6_MON_STABILIZING;
            return;
        }
        if (m->baseline_count < MQ6_BASELINE_SAMPLES) return;
        m->baseline = (uint16_t)((m->baseline_sum + MQ6_BASELINE_SAMPLES/2U) / MQ6_BASELINE_SAMPLES);
        m->baseline_valid = 1;
    }
    m->deviation = (int16_t)((int32_t)m->filtered - m->baseline);
    m->status = MQ6_MON_MONITORING;
}
const char *MQ6_MonitorLabel(MQ6_MonitorStatus status)
{
    switch (status)
    {
    case MQ6_MON_WARMUP: return "WARMUP";
    case MQ6_MON_STABILIZING: return "STABILIZING";
    case MQ6_MON_BASELINE: return "BASELINE";
    case MQ6_MON_MONITORING: return "MONITORING";
    case MQ6_MON_SAT_LOW: return "SAT LOW";
    case MQ6_MON_SAT_HIGH: return "SAT HIGH";
    default: return "ACQ ERROR";
    }
}
