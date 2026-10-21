#ifndef HX711_H
#define HX711_H
#include <stdint.h>
/* Experimental raw-count stability, not a weight or calibration threshold. */
#define HX711_WINDOW 16U
#define HX711_STABLE_RANGE 1000U
#define HX711_SETTLE_MS 500U
#define HX711_TIMEOUT_MS 1000U
typedef enum { HX711_WAITING, HX711_READY, HX711_ERROR } HX711_State;
typedef enum { HX711_NO_ERROR, HX711_NO_DATA, HX711_PROTOCOL,
               HX711_SATURATED, HX711_TIMING } HX711_Error;
typedef struct {
    int32_t raw, samples[HX711_WINDOW];
    uint32_t range, start_tick, last_tick, poll_tick;
    uint32_t cycles_per_us;
    HX711_State state;
    HX711_Error error;
    uint8_t valid, count, index, synced, stable;
} HX711;
/* PB8 input with pull-up; PB9 push-pull clock, idle LOW. No I2C remap. */
void HX711_Init(HX711 *h, uint32_t now);
/* Call from foreground/background waits, never inside a 1-Wire time slot.
 * Returns immediately if not ready. One bounded frame maximum per call. */
void HX711_Service(HX711 *h, uint32_t now);
#endif
