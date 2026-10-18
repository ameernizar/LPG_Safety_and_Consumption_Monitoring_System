#include "ds3231.h"

static uint8_t ValidTime(const DS3231_Time *t)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (t->year < 2000 || t->year > 2199 || t->month < 1 || t->month > 12 ||
        t->day < 1 || t->day > 7 || t->hour > 23 || t->minute > 59 || t->second > 59)
        return 0;
    uint8_t limit = days[t->month - 1];
    if (t->month == 2 && t->year % 4 == 0 &&
        (t->year % 100 != 0 || t->year % 400 == 0)) ++limit;
    return t->date >= 1 && t->date <= limit;
}
static uint8_t Bcd(uint8_t b, uint8_t *value)
{
    if ((b & 15U) > 9 || (b >> 4) > 9) return 0;
    *value = (b >> 4) * 10U + (b & 15U);
    return 1;
}
uint8_t DS3231_Decode(const uint8_t r[7], DS3231_Time *time)
{
    DS3231_Time t = {0};
    uint8_t year;
    if (!r || !time || (r[0] & 0x80) || (r[1] & 0x80) || (r[2] & 0x80) ||
        (r[3] & 0xF8) || (r[4] & 0xC0) || (r[5] & 0x60)) return 0;
    if (!Bcd(r[0], &t.second) || !Bcd(r[1], &t.minute) ||
        !Bcd(r[4], &t.date) || !Bcd(r[5] & 0x1F, &t.month) || !Bcd(r[6], &year)) return 0;
    t.year = 2000U + year + ((r[5] & 0x80) ? 100U : 0U);
    t.day = r[3]; /* 1..7 numbering is user-defined; do not invent weekday names. */
    if (r[2] & 0x40)
    {
        if (!Bcd(r[2] & 0x1F, &t.hour) || t.hour < 1 || t.hour > 12) return 0;
        t.hour = t.hour % 12U + ((r[2] & 0x20) ? 12U : 0U);
    }
    else if (!Bcd(r[2] & 0x3F, &t.hour)) return 0;
    if (!ValidTime(&t)) return 0;
    *time = t;
    return 1;
}
DS3231_Status DS3231_Read(I2C_HandleTypeDef *bus, DS3231_Reading *reading)
{
    uint8_t r[7], status;
    if (!reading) return DS3231_IO_ERROR;
    *reading = (DS3231_Reading){0}; /* Never retain a stale valid timestamp. */
    if (!bus) return DS3231_IO_ERROR;
    HAL_StatusTypeDef probe = HAL_I2C_IsDeviceReady(bus, DS3231_ADDRESS, 1, DS3231_TIMEOUT_MS);
    if (probe != HAL_OK)
    {
        /* F1 IsDeviceReady clears AF without necessarily retaining ErrorCode. */
        uint32_t error = HAL_I2C_GetError(bus);
        return (probe == HAL_ERROR && (error == 0U || error == HAL_I2C_ERROR_AF))
               ? DS3231_NO_ACK : DS3231_IO_ERROR;
    }
    /* One burst uses the RTC's buffered snapshot, avoiding mixed rollover fields. */
    if (HAL_I2C_Mem_Read(bus, DS3231_ADDRESS, 0, I2C_MEMADD_SIZE_8BIT,
                         r, sizeof(r), DS3231_TIMEOUT_MS) != HAL_OK ||
        HAL_I2C_Mem_Read(bus, DS3231_ADDRESS, 0x0F, I2C_MEMADD_SIZE_8BIT,
                         &status, 1, DS3231_TIMEOUT_MS) != HAL_OK) return DS3231_IO_ERROR;
    reading->status_valid = 1;
    reading->osf = (status & 0x80U) != 0;
    if (reading->osf) return DS3231_TIME_NOT_SET;
    if (!DS3231_Decode(r, &reading->time)) return DS3231_INVALID_DATA;
    return DS3231_OK; /* OSF clear + valid fields does not certify wall-clock accuracy. */
}
#if DS3231_ENABLE_SET
static uint8_t set_pending;
static uint32_t set_tick;
static DS3231_Time set_time;
static I2C_HandleTypeDef *set_bus;
static uint64_t Seconds(const DS3231_Time *t)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t total = 0;
    for (unsigned y = 2000; y < t->year; ++y)
        total += 365U + (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    for (unsigned m = 1; m < t->month; ++m)
        total += days[m-1] + (m == 2 && t->year % 4 == 0 &&
                 (t->year % 100 != 0 || t->year % 400 == 0));
    return ((uint64_t)(total + t->date - 1U) * 24U + t->hour) * 3600U
           + t->minute * 60U + t->second;
}
static uint8_t Encode(uint8_t value) { return (value / 10U) * 16U + value % 10U; }
DS3231_Status DS3231_SetUnsetClock(I2C_HandleTypeDef *bus, const DS3231_Time *t)
{
    uint8_t verify[7];
    DS3231_Reading prior;
    if (!bus || !t || !ValidTime(t)) return DS3231_INVALID_DATA;
    if (set_pending) return DS3231_VERIFYING;
    DS3231_Status previous = DS3231_Read(bus, &prior);
    if (previous == DS3231_OK) return DS3231_ALREADY_SET;
    if (previous != DS3231_TIME_NOT_SET) return previous;
    uint8_t r[7] = {Encode(t->second), Encode(t->minute), Encode(t->hour), t->day,
                   Encode(t->date), Encode(t->month) | (t->year >= 2100 ? 0x80 : 0),
                   Encode((uint8_t)(t->year % 100))};
    if (HAL_I2C_Mem_Write(bus, DS3231_ADDRESS, 0, I2C_MEMADD_SIZE_8BIT,
                          r, 7, DS3231_TIMEOUT_MS) != HAL_OK) return DS3231_WRITE_ERROR;
    set_tick = HAL_GetTick();
    if (
        HAL_I2C_Mem_Read(bus, DS3231_ADDRESS, 0, I2C_MEMADD_SIZE_8BIT,
                         verify, 7, DS3231_TIMEOUT_MS) != HAL_OK) return DS3231_VERIFY_ERROR;
    /* Conservative: a rollover during verification requires an explicit retry. */
    for (unsigned i = 0; i < 7; ++i) if (r[i] != verify[i]) return DS3231_VERIFY_ERROR;
    set_time = *t;
    set_bus = bus;
    set_pending = 1;
    return DS3231_VERIFYING;
}
DS3231_Status DS3231_ServiceSet(I2C_HandleTypeDef *bus)
{
    uint8_t status, r[7];
    DS3231_Time current;
    DS3231_Reading prior;
    if (!set_pending || bus != set_bus) return DS3231_VERIFY_ERROR;
    uint32_t elapsed = HAL_GetTick() - set_tick;
    if (elapsed < 1100U) return DS3231_VERIFYING;
    set_pending = 0; /* One attempt, never an automatic rewrite/retry. */
    if (elapsed > 4000U) return DS3231_VERIFY_ERROR;
    if (HAL_I2C_Mem_Read(bus, DS3231_ADDRESS, 0, I2C_MEMADD_SIZE_8BIT,
                         r, 7, DS3231_TIMEOUT_MS) != HAL_OK) return DS3231_VERIFY_ERROR;
    if (!DS3231_Decode(r, &current)) return DS3231_VERIFY_ERROR;
    uint64_t start = Seconds(&set_time), end = Seconds(&current);
    if (end == start) return DS3231_NOT_RUNNING;
    if (end < start || end - start < elapsed / 1000U ||
        end - start > elapsed / 1000U + 1U) return DS3231_VERIFY_ERROR;
    if (HAL_I2C_Mem_Read(bus, DS3231_ADDRESS, 0x0F, I2C_MEMADD_SIZE_8BIT,
                         &status, 1, DS3231_TIMEOUT_MS) != HAL_OK) return DS3231_OSF_ERROR;
    /* Preserve EN32kHz; write ones to alarm flags so neither is cleared. */
    status = (status & 0x08U) | 0x03U;
    if (HAL_I2C_Mem_Write(bus, DS3231_ADDRESS, 0x0F, I2C_MEMADD_SIZE_8BIT,
                          &status, 1, DS3231_TIMEOUT_MS) != HAL_OK) return DS3231_OSF_ERROR;
    DS3231_Status result = DS3231_Read(bus, &prior);
    return result == DS3231_OK ? DS3231_OK : DS3231_OSF_ERROR;
}
#endif
