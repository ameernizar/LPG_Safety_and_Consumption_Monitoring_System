#include "oled_ui.h"
#include <stdio.h>

/* 5x7 glyphs, 6-pixel advance. x=2, maximum 20 chars => at most x=120.
 * Header y=1; five body rows y=12,21,30,39,48; footer y=56. */
static void Text(uint8_t y, const char *text)
{
    SH1106_SetCursor(2, y);
    for (unsigned i = 0; i < 20U && text[i]; ++i) SH1106_WriteChar(text[i]);
}
static void Temp(const OLED_UI_Data *d, char text[32])
{
    if (d->temperature_status != 0) { snprintf(text,32,"TEMP: --.- C"); return; }
    int value = d->temperature_tenths;
    int magnitude = value < 0 ? -value : value;
    snprintf(text,32,"TEMP: %s%d.%d C",value < 0 ? "-":"",magnitude/10,magnitude%10);
}
static uint8_t ClockValid(const OLED_UI_Data *d)
{
    if (d->rtc_status != DS3231_OK || !d->rtc->status_valid || d->rtc->osf) return 0;
    const DS3231_Time *t = &d->rtc->time;
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (t->year < 2000 || t->year > 2199 || t->month < 1 || t->month > 12 ||
        t->hour > 23 || t->minute > 59 || t->second > 59 || t->day < 1 || t->day > 7) return 0;
    unsigned max = days[t->month-1] + (t->month == 2 && t->year%4 == 0 &&
                    (t->year%100 != 0 || t->year%400 == 0));
    return t->date >= 1 && t->date <= max;
}
static const char *RTCLabel(const OLED_UI_Data *d)
{
    if (ClockValid(d)) return "RTC: OK";
    if (d->rtc_status == DS3231_NO_ACK) return "RTC: NO ACK";
    if (d->rtc_status == DS3231_IO_ERROR) return "RTC: I2C ERROR";
    if (d->rtc_status == DS3231_WAIT) return "RTC: WAIT";
    return "RTC: TIME INVALID";
}
HAL_StatusTypeDef OLED_UI_Render(OLED_UI *ui, const OLED_UI_Data *d, uint32_t now)
{
    if (!ui || !d || !d->mq6 || !d->rtc) return HAL_ERROR;
    if (!ui->initialized) { ui->initialized=1; ui->page_tick=now; ui->page=0; }
    uint32_t steps = (uint32_t)(now-ui->page_tick)/5000U;
    ui->page = (uint8_t)((ui->page + steps)%4U);
    ui->page_tick += steps*5000U;
    static const char *titles[] = {"LPG MONITOR","MQ6 GAS MONITOR","SENSORS","SYSTEM"};
    char text[32];
    const MQ6_Monitor *m = d->mq6;
    SH1106_Clear(); /* RAM only; never send a blank frame. */
    Text(1,titles[ui->page]);
    SH1106_SetCursor(104,1);
    snprintf(text,sizeof(text),"%u/4",(unsigned)ui->page+1U);
    SH1106_WriteString(text);
    SH1106_InvertRect(0,0,128,9);
    switch (ui->page)
    {
    case 0:
        if (ClockValid(d))
        {
            const DS3231_Time *t=&d->rtc->time;
            snprintf(text,sizeof(text),"%02u-%02u-%04u",t->date,t->month,t->year); Text(12,text);
            snprintf(text,sizeof(text),"%02u:%02u:%02u IST",t->hour,t->minute,t->second); Text(21,text);
        }
        else { Text(12,"TIME INVALID"); Text(21,RTCLabel(d)); }
        Temp(d,text); Text(30,text);
        Text(39,d->mode == SAFETY_MODE_DEMO ? "MODE: DEMO":"MODE: LIVE DISABLED");
        Text(48,SafetyState_Label(d->state));
        Text(56,"SIMULATED STATE");
        break;
    case 1:
        if (m->raw_valid) snprintf(text,sizeof(text),"RAW ADC: %u",m->raw);
        else snprintf(text,sizeof(text),"RAW ADC: ----");
        Text(12,text);
        if (m->valid) snprintf(text,sizeof(text),"FILTER: %u",m->filtered);
        else snprintf(text,sizeof(text),"FILTER: ----");
        Text(21,text);
        if (m->baseline_valid) snprintf(text,sizeof(text),"BASE: %u",m->baseline);
        else snprintf(text,sizeof(text),"BASE: -- %u/%u",m->baseline_count,(unsigned)MQ6_BASELINE_SAMPLES);
        Text(30,text);
        if (m->valid && m->baseline_valid) snprintf(text,sizeof(text),"DELTA: %+d",m->deviation);
        else snprintf(text,sizeof(text),"DELTA: ----");
        Text(39,text);
        if (d->adc_status >= 2)
            Text(48,d->adc_status==2 ? "ERROR: ADC INIT":d->adc_status==3 ? "ERROR: ADC CAL":"ERROR: ADC READ");
        else Text(48,MQ6_MonitorLabel(m->status));
        Text(56,"EXPERIMENTAL ONLY");
        break;
    case 2:
        Temp(d,text); Text(12,text);
        if (d->temperature_status==0) Text(21,"DS18B20: OK");
        else if (d->temperature_status==7) Text(21,"DS18B20: WAIT");
        else {
            snprintf(text,sizeof(text),"DS ERROR E%u",d->temperature_status);
            if (d->temperature_status==4)
                snprintf(text,sizeof(text),"DS E4 CRC:%02X/%02X",d->crc_calculated,d->crc_received);
            if (d->temperature_status==2 || d->temperature_status==5)
                snprintf(text,sizeof(text),"DS E%u BUSY:%u DONE:%u",d->temperature_status,
                         d->conversion_busy,d->conversion_done);
            Text(21,text);
        }
        Text(30,d->flame_high ? "FLAME DO: HIGH":"FLAME DO: LOW");
        Text(39,"WEIGHT: PENDING");
        if (d->adc_status==1) {
            unsigned ao=(d->ao_mv+5)/10, pin=(d->pin_mv+5)/10;
            snprintf(text,sizeof(text),"AO:%u.%02u IN:%u.%02u V",ao/100,ao%100,pin/100,pin%100);
            Text(48,text);
        } else Text(48,"MQ6 VOLTS: INVALID");
        Text(56,"RAW DO / EST VOLTS");
        break;
    default:
        Text(12,RTCLabel(d));
        Text(21,"SD: NOT INSTALLED");
        Text(30,"LOAD CELL: PENDING");
        Text(39,d->mode==SAFETY_MODE_DEMO ? "MODE: DEMO":"MODE: LIVE DISABLED");
        if (d->rtc_set_state==1) Text(48,"SET MODE: ARMED");
        else if (d->rtc_set_state==2) Text(48,"RTC SET VERIFYING");
        else if (d->rtc_set_state==3) Text(48,"RTC SET OK");
        else if (d->rtc_set_state==4) {
            snprintf(text,sizeof(text),"RTC SET FAILED E%lu",(unsigned long)d->rtc_set_error); Text(48,text);
        } else Text(48,"MCU CLOCK: 72 MHZ");
        if (d->rtc_set_state==4) {
            const char *reason="NOT TRIGGERED";
            if (d->rtc_set_error==1) reason="RTC NO RESPONSE";
            if (d->rtc_set_error==2) reason="WRITE FAILED";
            if (d->rtc_set_error==3) reason="READBACK MISMATCH";
            if (d->rtc_set_error==4) reason="OSF CLEAR FAILED";
            if (d->rtc_set_error==5) reason="INVALID TARGET";
            if (d->rtc_set_status==DS3231_NOT_RUNNING) reason="CLOCK NOT RUNNING";
            if (d->rtc_set_status==DS3231_ALREADY_SET) reason="ALREADY SET";
            Text(56,reason);
        } else Text(56,"IST UTC+05:30");
        break;
    }
    /* Page indicator: active 3x3 square, inactive center dots.
     x=110..124, y=59..61; clear of footer text and framebuffer edges. */
    for (unsigned i=0;i<4;++i)
        for(unsigned x=0;x<3;++x)
            for(unsigned y=0;y<3;++y)
                if (i==ui->page || (x==1 && y==1)) SH1106_Pixel(110+i*4+x,59+y,1);
    return SH1106_UpdateScreen();
}
