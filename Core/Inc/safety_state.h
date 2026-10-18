#ifndef SAFETY_STATE_H
#define SAFETY_STATE_H

#include <stdint.h>

/* Engineering demonstration only. No calibrated live assessment is provided. */
typedef enum {
    SAFETY_SAFE,
    SAFETY_WARNING,
    SAFETY_EMERGENCY,
    SAFETY_SENSOR_FAULT
} SafetyState;

typedef enum {
    SAFETY_MODE_DEMO,
    SAFETY_MODE_LIVE_DISABLED
} SafetyMode;

enum {
    SAFETY_LED_GREEN = 1U,
    SAFETY_LED_YELLOW = 2U,
    SAFETY_LED_RED = 4U
};

typedef struct {
    SafetyMode mode;
    SafetyState state;
    uint32_t phase_started_ms;
    uint8_t demo_phase;
} SafetyStateMachine;

/* Initializes in DEMO mode. Supply HAL_GetTick() from the application. */
void SafetyState_Init(SafetyStateMachine *machine, uint32_t now_ms);
/* Returns nonzero on a state transition; a future event logger can use this. */
uint8_t SafetyState_Update(SafetyStateMachine *machine, uint32_t now_ms);
/* Pure output calculation; no GPIO, delays, interrupts or sensor accesses. */
uint8_t SafetyState_Leds(const SafetyStateMachine *machine, uint32_t now_ms);
const char *SafetyState_Label(SafetyState state);

#endif
