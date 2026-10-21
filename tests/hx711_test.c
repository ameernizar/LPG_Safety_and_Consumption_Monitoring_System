#include "hx711.h"
#include "stm32f1xx_hal.h"
#include <assert.h>
#include <stdio.h>
static DWT_Type cycles;
CoreDebug_Type test_core;
static unsigned stopped, pulses, irq, high, ready, stuck_low, stop_on_high;
static unsigned configured;
static uint32_t bits, freq=72000000;
DWT_Type *TestDWT(void) { if (!stopped) cycles.CYCCNT+=72; return &cycles; }
uint32_t HAL_RCC_GetHCLKFreq(void) { return freq; }
uint32_t __get_PRIMASK(void) { return irq; }
void __disable_irq(void) { irq=1; }
void __set_PRIMASK(uint32_t m) { irq=m; }
void HAL_GPIO_Init(void *p, GPIO_InitTypeDef *g) {
    assert(p==GPIOB);
    if(g->Pin==GPIO_PIN_9) {assert(g->Mode==GPIO_MODE_OUTPUT_PP && !high); configured|=1;}
    else {assert(g->Pin==GPIO_PIN_8 && g->Mode==GPIO_MODE_INPUT && g->Pull==GPIO_PULLUP); configured|=2;}
}
void HAL_GPIO_WritePin(void *p,uint16_t pin,GPIO_PinState v) {
    assert(p==GPIOB && pin==GPIO_PIN_9);
    if(v) {assert(irq && !high); ++pulses; if(stop_on_high) stopped=1;}
    high=v;
}
GPIO_PinState HAL_GPIO_ReadPin(void *p,uint16_t pin) {
    assert(p==GPIOB && pin==GPIO_PIN_8);
    if(stuck_low) return GPIO_PIN_RESET;
    if(high) {assert(pulses>=1 && pulses<=24); return (bits>>(24-pulses))&1;}
    return !ready || pulses>=25 ? GPIO_PIN_SET:GPIO_PIN_RESET;
}
static void Frame(HX711 *h,uint32_t now,uint32_t word) {
    pulses=0; ready=1; bits=word; HX711_Service(h,now);
    assert(pulses==25 && !high && !irq);
}
static void Init(HX711 *h,uint32_t now) {
    stopped=stop_on_high=stuck_low=ready=pulses=irq=0; freq=72000000;
    HX711_Init(h,now); assert(configured==3 && h->state==HX711_WAITING);
}
int main(void) {
    HX711 h; Init(&h,0);
    HX711_Service(&h,999); assert(h.state==HX711_WAITING);
    HX711_Service(&h,1000); assert(h.error==HX711_NO_DATA && !h.valid && !pulses);
    Frame(&h,1100,42); assert(!h.valid);
    Frame(&h,1599,42); assert(!h.valid);
    Frame(&h,1600,0xFFFFFF); assert(h.raw==-1 && h.valid);
    Frame(&h,1700,0); assert(h.raw==0 && h.valid);
    Frame(&h,1800,0x800001); assert(h.raw==-8388607);
    Frame(&h,1900,0x7FFFFE); assert(h.raw==8388606);
    for(unsigned i=0;i<16;++i) Frame(&h,2000+i*100,123456);
    assert(h.stable && h.range==0 && h.count==16);
    Frame(&h,3600,124457); assert(!h.stable && h.range==1001);
    Frame(&h,3700,0x7FFFFF); assert(h.error==HX711_SATURATED && !h.valid);
    Frame(&h,3800,100); Frame(&h,4300,0x800000); assert(h.error==HX711_SATURATED);
    stuck_low=1; Frame(&h,4400,0); assert(h.error==HX711_PROTOCOL);
    Init(&h,UINT32_MAX-100); Frame(&h,UINT32_MAX-99,1); Frame(&h,401,2);
    assert(h.valid && h.raw==2);
    ready=0; pulses=0; HX711_Service(&h,1401); assert(h.error==HX711_NO_DATA && !h.valid);
    Init(&h,0); stopped=1; ready=1; HX711_Service(&h,1);
    assert(h.error==HX711_TIMING && !high && !pulses);
    Init(&h,0); ready=1; stop_on_high=1; HX711_Service(&h,1);
    assert(h.error==HX711_TIMING && !high && !irq && pulses==1);
    Init(&h,0); ready=1; irq=1; HX711_Service(&h,1); assert(irq==1 && !high && pulses==25);
    stopped=1; HX711_Init(&h,0); assert(h.error==HX711_TIMING);
    stopped=0; freq=36000000; HX711_Init(&h,0); assert(h.error==HX711_TIMING);
    printf("HX711 protocol, signed data, settling, timeout, recovery, stability, rollover and timing faults passed; context=%zu bytes\n",sizeof(h));
}
