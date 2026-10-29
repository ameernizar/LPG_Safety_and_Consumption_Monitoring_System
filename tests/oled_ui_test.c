#include <assert.h>
#include <stdio.h>
#include "../Core/Src/sh1106.c"
#include "oled_ui.h"
static unsigned transfers;
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *b, uint16_t address, uint8_t *p, uint16_t n, uint32_t t)
{ (void)b; (void)p; assert(address==0x78 && (n==2 || n==129) && t==100); ++transfers; return HAL_OK; }
void HAL_Delay(uint32_t t) { (void)t; }
static void Save(unsigned page)
{
    char path[64]; snprintf(path,sizeof(path),"/tmp/oled-page-%u.pbm",page);
    FILE *f=fopen(path,"w"); assert(f); fprintf(f,"P1\n128 64\n");
    for(unsigned y=0;y<64;++y) {
        for(unsigned x=0;x<128;++x) fprintf(f,"%u ",(SH1106_Buffer[x+(y/8)*128]>>(y%8))&1);
        fprintf(f,"\n");
    }
    fclose(f);
}
/* Compare the rendered row with actual font pixels for the expected message. */
static void ExpectText(unsigned y, const char *expected)
{
    uint8_t actual[1024]; memcpy(actual,SH1106_Buffer,sizeof(actual));
    SH1106_Clear(); SH1106_SetCursor(2,y); SH1106_WriteString(expected);
    for(unsigned row=y;row<y+7;row++) for(unsigned x=2;x<122;x++)
        assert(((actual[x+(row/8)*128] ^ SH1106_Buffer[x+(row/8)*128]) & (1U<<(row%8)))==0);
    memcpy(SH1106_Buffer,actual,sizeof(actual));
}
int main(void)
{
    I2C_HandleTypeDef bus={0}; assert(SH1106_Init(&bus)==HAL_OK);
    MQ6_Monitor m={.raw=1234,.filtered=1200,.baseline=1150,.deviation=50,
                  .raw_valid=1,.valid=1,.baseline_valid=1,.status=MQ6_MON_MONITORING};
    DS3231_Reading rtc={.time={2026,10,18,7,9,43,12},.status_valid=1};
    HX711 h={.state=HX711_READY,.valid=1,.raw=-8388607,.range=16777214,.count=16};
    OLED_UI_Data d={.temperature_tenths=265,.temperature_status=0,.flame_high=1,.flame_valid=1,
        .hx711=&h,.mq6=&m,.adc_status=1,.pin_mv=1000,.ao_mv=2000,.rtc_status=DS3231_OK,
        .rtc=&rtc,.mode=SAFETY_MODE_DEMO,.state=SAFETY_SENSOR_FAULT};
    OLED_UI ui={0};
    unsigned before=transfers;
    for(unsigned page=0;page<5;++page) {
        d.temperature_sample_tick=page*5000; h.last_tick=page*5000;
        assert(OLED_UI_Render(&ui,&d,page*5000)==HAL_OK); assert(ui.page==page); Save(page);
    }
    assert(transfers-before==5*32); /* One update per frame, not blank + redraw. */
    assert(OLED_UI_Render(&ui,&d,24999)==HAL_OK && ui.page==4);
    assert(OLED_UI_Render(&ui,&d,25000)==HAL_OK && ui.page==0);
    ui=(OLED_UI){0}; OLED_UI_Render(&ui,&d,UINT32_MAX-2000);
    OLED_UI_Render(&ui,&d,2999); assert(ui.page==1);
    for(unsigned error=1;error<=7;++error) {
        h.state=HX711_ERROR; h.valid=0; h.error=(HX711_Error)(error%5);
        d.temperature_status=error; d.rtc_set_state=4; d.rtc_set_error=error;
        rtc.osf=1; m.valid=0; m.raw_valid=0; m.baseline_valid=0;
        for(unsigned page=0;page<5;++page) {
            ui=(OLED_UI){.initialized=1,.page=page};
            assert(OLED_UI_Render(&ui,&d,0)==HAL_OK);
        }
    }
    m.status=MQ6_MON_STABILIZING; d.adc_status=1;
    ui=(OLED_UI){.initialized=1,.page=1};
    assert(OLED_UI_Render(&ui,&d,0)==HAL_OK && ui.page==1);
    assert(strcmp(MQ6_MonitorLabel(m.status),"STABILIZING")==0);
    /* Errors never display numeric temperatures; stale success is explicit. */
    d.temperature_status=0; d.temperature_sample_tick=0; d.temperature_tenths=-74;
    d.flame_valid=0; ui=(OLED_UI){.initialized=1,.page=2};
    OLED_UI_Render(&ui,&d,2499); ExpectText(12,"TEMP: -7.4 C"); ExpectText(21,"DS18B20: FRESH");
    ExpectText(30,"FLAME DO: WAIT");
    OLED_UI_Render(&ui,&d,2500); ExpectText(12,"TEMP: STALE"); ExpectText(21,"DS18B20: STALE");
    for(unsigned error=1;error<=7;error++) {
        d.temperature_status=error; OLED_UI_Render(&ui,&d,2500); ExpectText(12,"TEMP: --.- C");
    }
    d.temperature_status=0; d.temperature_sample_tick=UINT32_MAX-10;
    ui.page_tick=0; OLED_UI_Render(&ui,&d,10); ExpectText(21,"DS18B20: FRESH");
    /* ADC validity and baseline availability are independent. */
    m.raw=380; m.raw_valid=m.valid=1; m.baseline_valid=0; m.baseline_count=0;
    m.filtered=380; m.status=MQ6_MON_WARMUP; d.adc_status=1;
    ui=(OLED_UI){.initialized=1,.page=1}; OLED_UI_Render(&ui,&d,0);
    ExpectText(12,"ADC OK: 380"); ExpectText(30,"BASE WAIT: 0/32"); ExpectText(39,"DELTA: ----");
    m.baseline_valid=1; m.baseline=454; m.deviation=-74;
    OLED_UI_Render(&ui,&d,0); ExpectText(30,"BASE: 454"); ExpectText(39,"DELTA: -74");
    m.valid=0; m.raw=0; m.status=MQ6_MON_SAT_LOW;
    OLED_UI_Render(&ui,&d,0); ExpectText(12,"ADC SAT: 0"); ExpectText(39,"DELTA: ----");
    ui.page=2; OLED_UI_Render(&ui,&d,0); ExpectText(48,"MQ6 VOLTS: INVALID");
    ui.page=1; d.adc_status=4; OLED_UI_Render(&ui,&d,0); ExpectText(12,"ADC ERR: ----");
    /* Cached HX flags cannot display old counts after a scheduling gap. */
    h=(HX711){.valid=1,.state=HX711_READY,.raw=-1,.last_tick=0,.count=16};
    ui=(OLED_UI){.initialized=1,.page=4}; OLED_UI_Render(&ui,&d,999);
    ExpectText(12,"HX711: READY"); ExpectText(21,"RAW: -1");
    OLED_UI_Render(&ui,&d,1000); ExpectText(12,"HX711: STALE"); ExpectText(21,"RAW: --------");
    h.state=HX711_ERROR; h.valid=0; h.error=HX711_NO_DATA;
    OLED_UI_Render(&ui,&d,1000); ExpectText(12,"HX711: ERROR"); ExpectText(48,"NO DATA / CHECK DT");
    uint8_t copy[1024]; memcpy(copy,SH1106_Buffer,1024);
    SH1106_Pixel(65535,65535,1); SH1106_InvertRect(128,64,65535,65535);
    assert(memcmp(copy,SH1106_Buffer,1024)==0);
    SH1106_InvertRect(127,63,65535,65535);
    assert((copy[1023]^SH1106_Buffer[1023])==128);
    puts("OLED UI pages, rotation, rollover, errors and clipping passed");
}
