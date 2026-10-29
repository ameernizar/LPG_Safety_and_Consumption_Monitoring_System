#include "sensor_health.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(!SensorSampleFresh(0,0,0,2500));
    assert(SensorSampleFresh(1,0,2499,2500));
    assert(!SensorSampleFresh(1,0,2500,2500));
    assert(SensorSampleFresh(1,UINT32_MAX-10,10,2500));
    FlameFilter f={0};
    FlameFilter_Update(&f,1,0); assert(!FlameFilter_Valid(&f,0));
    FlameFilter_Update(&f,1,49); assert(!f.valid);
    FlameFilter_Update(&f,1,50); assert(f.valid && f.high);
    FlameFilter_Update(&f,0,51); assert(!f.valid);
    FlameFilter_Update(&f,1,55); assert(!f.valid);
    FlameFilter_Update(&f,1,104); assert(!f.valid);
    FlameFilter_Update(&f,1,105); assert(f.valid && f.high);
    FlameFilter_Update(&f,0,106); FlameFilter_Update(&f,0,156);
    assert(f.valid && !f.high);
    assert(!FlameFilter_Valid(&f,256));
    FlameFilter_Update(&f,0,256); assert(!f.valid); /* gap does not count as stable */
    FlameFilter_Update(&f,1,256); assert(f.candidate==0); /* duplicate ignored */
    FlameFilter_Update(&f,0,306); assert(f.valid && !f.high);
    f=(FlameFilter){0}; FlameFilter_Update(&f,1,UINT32_MAX-20);
    FlameFilter_Update(&f,1,29); assert(f.valid && f.high);
    for(unsigned i=0;i<100;i++) {
        FlameFilter_Update(&f,i%2,30+i); assert(!f.valid);
    }
    puts("Temperature freshness and flame qualification/bounce/gap/rollover tests passed");
}
