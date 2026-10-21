#include "mq6_monitor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t qualify(MQ6_Monitor *m, uint32_t start, uint16_t value)
{
    MQ6_MonitorInit(m, start);
    MQ6_MonitorUpdate(m, start, 1, value, 0);
    assert(m->status == MQ6_MON_WARMUP && m->filtered == value);
    MQ6_MonitorUpdate(m, start+MQ6_SETTLING_MS-1, 1, value, 0);
    assert(m->window_count == 0 && !m->baseline_valid);
    uint32_t now = start+MQ6_SETTLING_MS;
    for (unsigned i=0;i<MQ6_STABILITY_SAMPLES;++i,now+=1000)
        MQ6_MonitorUpdate(m,now,1,value,0);
    assert(m->status == MQ6_MON_BASELINE && m->baseline_count == 0);
    return now;
}
static uint32_t baseline(MQ6_Monitor *m, uint32_t start, uint16_t value)
{
    uint32_t now=qualify(m,start,value);
    for(unsigned i=0;i<MQ6_BASELINE_SAMPLES;++i,now+=1000) {
        MQ6_MonitorUpdate(m,now,1,value,0);
        assert(m->baseline_valid == (i+1==MQ6_BASELINE_SAMPLES));
    }
    assert(m->baseline==value && m->deviation==0 && m->status==MQ6_MON_MONITORING);
    return now;
}
static void window_test(unsigned range, unsigned drift, unsigned pass)
{
    MQ6_Monitor m; MQ6_MonitorInit(&m,0);
    for(unsigned i=0;i<16;++i) {
        unsigned value=1000+(range ? (i%2)*range : (i>=8)*drift);
        MQ6_MonitorUpdate(&m,60000+i*1000,1,value,0);
    }
    assert((m.status==MQ6_MON_BASELINE)==pass);
}
int main(void)
{
    MQ6_Monitor m;
    uint32_t now=baseline(&m,0,454);
    MQ6_MonitorUpdate(&m,now,1,1254,0);
    assert(m.filtered==554 && m.deviation==100 && m.baseline==454);
    MQ6_MonitorUpdate(&m,now+1000,0,0,0);
    assert(!m.valid && !m.raw_valid && m.baseline_valid);
    MQ6_MonitorUpdate(&m,now+2000,1,380,0);
    assert(m.filtered==380 && m.deviation==-74 && m.baseline==454);
    MQ6_MonitorUpdate(&m,now+3000,1,0,0); assert(m.status==MQ6_MON_SAT_LOW);
    MQ6_MonitorUpdate(&m,now+4000,1,4095,0); assert(m.status==MQ6_MON_SAT_HIGH);
    MQ6_MonitorUpdate(&m,now+5000,1,2000,1); assert(m.status==MQ6_MON_SAT_LOW);
    MQ6_MonitorUpdate(&m,now+6000,1,2000,2); assert(m.status==MQ6_MON_SAT_HIGH);
    MQ6_MonitorUpdate(&m,now+7000,1,65535,0); assert(m.status==MQ6_MON_ACQ_ERROR);
    assert(m.baseline_valid && m.baseline==454);
    MQ6_MonitorRequestBaseline(&m,now+8000);
    assert(!m.baseline_valid && m.deviation==0 && m.baseline==0 && m.status==MQ6_MON_STABILIZING);
    assert(m.settling_started_ms==0);
    for(unsigned i=0;i<48;++i) MQ6_MonitorUpdate(&m,now+9000+i*1000,1,500,0);
    assert(m.baseline_valid && m.baseline==500);
    baseline(&m,UINT32_MAX-30000U,4094); /* Tick rollover and upper count bound. */
    baseline(&m,0,1);

    window_test(12,0,1); window_test(13,0,0);
    window_test(0,4,1); window_test(0,5,0);
    now=qualify(&m,0,1000);
    for(unsigned i=0;i<32;++i) MQ6_MonitorUpdate(&m,now+i*1000,1,1000+i/3,0);
    assert(!m.baseline_valid && m.status==MQ6_MON_STABILIZING && m.window_count==0);
    /* Rolling drift remains <=3, capture range <=11; whole-capture drift >4 rejects. */
    now=qualify(&m,0,1000);
    for(unsigned i=0;i<27;++i) MQ6_MonitorUpdate(&m,now+i*1000,1,1000+i/2,0);
    assert(!m.baseline_valid && m.status==MQ6_MON_STABILIZING && m.window_count==0);
    /* Rolling range <=8/drift <=4, but capture reaches range13 and is rejected. */
    now=qualify(&m,0,1000);
    for(unsigned i=0;i<32;++i) MQ6_MonitorUpdate(&m,now+i*1000,1,1000+(i>=16)*4,0);
    assert(m.baseline_valid && m.baseline==1002); /* Whole-capture drift equality accepted. */
    now=qualify(&m,0,1006);
    for(unsigned i=0;i<32;++i) MQ6_MonitorUpdate(&m,now+i*1000,1,1000+(i%2)*12,0);
    assert(m.baseline_valid && m.baseline==1006); /* Whole-capture range equality accepted. */
    now=qualify(&m,0,1000);
    for(unsigned i=0;i<32;++i) MQ6_MonitorUpdate(&m,now+i*1000,1,1000+i%2,0);
    assert(m.baseline_valid && m.baseline==1001); /* Rounded batch-average mean. */

    now=qualify(&m,0,1000);
    MQ6_MonitorUpdate(&m,now,1,1000,0); assert(m.baseline_count==1);
    MQ6_MonitorUpdate(&m,now+1000,0,0,0);
    assert(!m.baseline_valid && m.baseline_count==0 && m.window_count==0 && m.settled);
    assert(m.settling_started_ms==0);
    MQ6_MonitorUpdate(&m,now+2000,1,1000,0); assert(m.status==MQ6_MON_STABILIZING);
    now=qualify(&m,0,1000);
    MQ6_MonitorUpdate(&m,now+MQ6_MAX_SAMPLE_GAP_MS,1,1000,0);
    assert(m.status==MQ6_MON_STABILIZING && m.window_count==1 && m.settled);
    MQ6_Monitor snapshot=m;
    MQ6_MonitorUpdate(&m,now+MQ6_MAX_SAMPLE_GAP_MS,1,1001,0);
    assert(memcmp(&snapshot,&m,sizeof(m))==0);
    MQ6_MonitorInit(&m,0);
    MQ6_MonitorUpdate(&m,1000,0,0,0);
    MQ6_MonitorUpdate(&m,59999,1,1000,0); assert(m.status==MQ6_MON_WARMUP);
    MQ6_MonitorUpdate(&m,60000,1,1000,0); assert(m.status==MQ6_MON_STABILIZING);
    MQ6_MonitorInit(&m,0); MQ6_MonitorRequestBaseline(&m,1000);
    assert(m.status==MQ6_MON_WARMUP && m.settling_started_ms==0);
    for(unsigned i=0;i<16;++i) MQ6_MonitorUpdate(&m,60000+i,1,1000,0);
    assert(m.status==MQ6_MON_STABILIZING); /* Many fast calls do not establish 15s stability. */
    MQ6_MonitorInit(&m,0);
    for(unsigned i=0;i<10000;++i) MQ6_MonitorUpdate(&m,60000+i*1000,1,1000+(i%2)*20,0);
    assert(!m.baseline_valid && m.status==MQ6_MON_STABILIZING); /* No timeout acceptance. */
    MQ6_MonitorInit(&m,9999999); assert(!m.settled && !m.baseline_valid && m.status==MQ6_MON_WARMUP);
    puts("MQ6 stability/capture/error/manual-request/rollover tests passed");
    printf("sizeof(MQ6_Monitor)=%zu bytes\n",sizeof(m));
}
