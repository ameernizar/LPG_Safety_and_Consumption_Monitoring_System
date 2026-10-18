#ifndef DS3231_H
#define DS3231_H
#include "stm32f1xx_hal.h"

#define DS3231_ADDRESS (0x68U << 1)
#define DS3231_TIMEOUT_MS 10U
/* Setting capability is available, but only explicit RAM ARM/TRIGGER invokes it. */
#ifndef DS3231_ENABLE_SET
#define DS3231_ENABLE_SET 1
#endif

typedef struct {
    uint16_t year; /* Century bit interpreted as 2000..2199. */
    uint8_t month, date, day, hour, minute, second;
} DS3231_Time;
typedef enum {
    DS3231_WAIT, DS3231_OK, DS3231_NO_ACK, DS3231_IO_ERROR,
    DS3231_TIME_NOT_SET, DS3231_INVALID_DATA,
    DS3231_VERIFYING, DS3231_WRITE_ERROR, DS3231_VERIFY_ERROR, DS3231_NOT_RUNNING,
    DS3231_ALREADY_SET, DS3231_OSF_ERROR
} DS3231_Status;
typedef struct {
    DS3231_Time time;
    uint8_t osf, status_valid;
} DS3231_Reading;

DS3231_Status DS3231_Read(I2C_HandleTypeDef *bus, DS3231_Reading *reading);
/* Pure decoder, also used by host tests. Never publishes partial data. */
uint8_t DS3231_Decode(const uint8_t registers[7], DS3231_Time *time);
#if DS3231_ENABLE_SET
/* Explicit provisioning only: refuses to overwrite a clock whose OSF is clear.
 * Caller must supply trusted current time. Invoked only after RAM ARM/TRIGGER. */
DS3231_Status DS3231_SetUnsetClock(I2C_HandleTypeDef *bus, const DS3231_Time *time);
/* Call periodically after VERIFYING; OSF remains set until advancement verified. */
DS3231_Status DS3231_ServiceSet(I2C_HandleTypeDef *bus);
#endif
#endif
