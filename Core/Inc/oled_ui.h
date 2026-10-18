#ifndef OLED_UI_H
#define OLED_UI_H
#include "sh1106.h"
#include "ds3231.h"
#include "mq6_monitor.h"
#include "safety_state.h"

typedef struct {
    int16_t temperature_tenths;
    uint8_t temperature_status; /* Existing DS codes: 0 OK, 1..6 error, 7 pending. */
    uint8_t flame_high, crc_calculated, crc_received, conversion_busy, conversion_done;
    const MQ6_Monitor *mq6;
    uint8_t adc_status; /* Existing acquisition codes: pending, OK, init/cal/read error. */
    uint32_t pin_mv, ao_mv;
    DS3231_Status rtc_status, rtc_set_status;
    const DS3231_Reading *rtc;
    uint32_t rtc_set_state, rtc_set_error;
    SafetyMode mode;
    SafetyState state;
} OLED_UI_Data;
typedef struct {
    uint32_t page_tick;
    uint8_t page, initialized;
} OLED_UI;
/* Render-only: no sensor acquisition, RTC writes, or delays. One frame transfer. */
HAL_StatusTypeDef OLED_UI_Render(OLED_UI *ui, const OLED_UI_Data *data, uint32_t now);
#endif
