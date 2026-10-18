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
int main(void)
{
    I2C_HandleTypeDef bus={0}; assert(SH1106_Init(&bus)==HAL_OK);
    MQ6_Monitor m={.raw=1234,.filtered=1200,.baseline=1150,.deviation=50,
                  .raw_valid=1,.valid=1,.baseline_valid=1,.status=MQ6_MON_MONITORING};
    DS3231_Reading rtc={.time={2026,10,18,7,9,43,12},.status_valid=1};
    OLED_UI_Data d={.temperature_tenths=265,.temperature_status=0,.flame_high=1,
        .mq6=&m,.adc_status=1,.pin_mv=1000,.ao_mv=2000,.rtc_status=DS3231_OK,
        .rtc=&rtc,.mode=SAFETY_MODE_DEMO,.state=SAFETY_SENSOR_FAULT};
    OLED_UI ui={0};
    unsigned before=transfers;
    for(unsigned page=0;page<4;++page) {
        assert(OLED_UI_Render(&ui,&d,page*5000)==HAL_OK); assert(ui.page==page); Save(page);
    }
    assert(transfers-before==4*32); /* One update per frame, not blank + redraw. */
    assert(OLED_UI_Render(&ui,&d,19999)==HAL_OK && ui.page==3);
    assert(OLED_UI_Render(&ui,&d,20000)==HAL_OK && ui.page==0);
    ui=(OLED_UI){0}; OLED_UI_Render(&ui,&d,UINT32_MAX-2000);
    OLED_UI_Render(&ui,&d,2999); assert(ui.page==1);
    for(unsigned error=1;error<=7;++error) {
        d.temperature_status=error; d.rtc_set_state=4; d.rtc_set_error=error;
        rtc.osf=1; m.valid=0; m.raw_valid=0; m.baseline_valid=0;
        for(unsigned page=0;page<4;++page) {
            ui=(OLED_UI){.initialized=1,.page=page};
            assert(OLED_UI_Render(&ui,&d,0)==HAL_OK);
        }
    }
    uint8_t copy[1024]; memcpy(copy,SH1106_Buffer,1024);
    SH1106_Pixel(65535,65535,1); SH1106_InvertRect(128,64,65535,65535);
    assert(memcmp(copy,SH1106_Buffer,1024)==0);
    SH1106_InvertRect(127,63,65535,65535);
    assert((copy[1023]^SH1106_Buffer[1023])==128);
    puts("OLED UI pages, rotation, rollover, errors and clipping passed");
}
