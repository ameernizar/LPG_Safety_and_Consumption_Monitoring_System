#include "mq6_monitor.h"
#include <assert.h>
#include <stdio.h>

static void baseline(MQ6_Monitor *m, uint32_t start, uint16_t raw)
{
    MQ6_MonitorInit(m, start);
    MQ6_MonitorUpdate(m, start, 1, raw, 0);
    assert(m->valid && m->filtered == raw && !m->baseline_valid);
    MQ6_MonitorUpdate(m, start + MQ6_SETTLING_MS - 1U, 1, raw, 0);
    assert(m->baseline_count == 0);
    for (uint32_t i = 0; i < MQ6_BASELINE_SAMPLES; ++i)
        MQ6_MonitorUpdate(m, start + MQ6_SETTLING_MS + i * 1000U, 1, raw, 0);
    assert(m->status == MQ6_MON_MONITORING && m->baseline == raw);
    assert(m->baseline_valid && m->deviation == 0);
}
int main(void)
{
    MQ6_Monitor m;
    baseline(&m, 0, 1000);
    MQ6_MonitorUpdate(&m, 100000, 1, 1800, 0);
    assert(m.filtered == 1100 && m.deviation == 100);
    for (unsigned i = 0; i < 300; ++i)
        MQ6_MonitorUpdate(&m, 101000 + i * 1000, 1, 100, 0);
    assert(m.filtered == 100 && m.deviation == -900 && m.baseline == 1000);
    MQ6_MonitorUpdate(&m, 500000, 0, 1234, 0);
    assert(m.status == MQ6_MON_ACQ_ERROR && !m.valid && !m.raw_valid);
    assert(m.baseline_valid && m.baseline == 1000);
    MQ6_MonitorUpdate(&m, 501000, 1, 1200, 0);
    assert(m.filtered == 1200 && m.deviation == 200);
    MQ6_MonitorUpdate(&m, 502000, 1, 0, 0);
    assert(m.status == MQ6_MON_SAT_LOW && !m.valid && m.raw_valid);
    MQ6_MonitorUpdate(&m, 503000, 1, 4095, 0);
    assert(m.status == MQ6_MON_SAT_HIGH && !m.valid);
    MQ6_MonitorUpdate(&m, 504000, 1, 2000, 2);
    assert(m.status == MQ6_MON_SAT_HIGH && !m.valid);
    MQ6_MonitorUpdate(&m, 505000, 1, 2000, 1);
    assert(m.status == MQ6_MON_SAT_LOW && !m.valid);
    MQ6_MonitorUpdate(&m, 506000, 1, 65535, 0);
    assert(m.status == MQ6_MON_ACQ_ERROR && !m.raw_valid);
    baseline(&m, UINT32_MAX - 30000U, 4094); /* Tick wrap and high-count sum. */
    MQ6_MonitorInit(&m, 0);
    MQ6_MonitorUpdate(&m, MQ6_SETTLING_MS, 1, 1000, 0);
    assert(m.baseline_count == 1);
    MQ6_MonitorUpdate(&m, MQ6_SETTLING_MS + 1, 0, 0, 0);
    assert(!m.baseline_valid && m.baseline_count == 0 && !m.settled);
    MQ6_MonitorUpdate(&m, MQ6_SETTLING_MS + 2, 1, 2000, 0);
    assert(m.status == MQ6_MON_WARMUP && m.filtered == 2000);
    puts("MQ6 monitor tests passed");
}
