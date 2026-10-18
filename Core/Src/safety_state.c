#include "safety_state.h"

/* Synthetic logical inputs only. Never pass raw ADC/flame levels here.
   A future live adapter requires validated thresholds, polarity and hysteresis. */
typedef struct {
    uint8_t warning;
    uint8_t emergency;
    uint8_t fault;
} DemoInputs;

static DemoInputs DemoInputs_ForPhase(uint8_t phase)
{
    DemoInputs inputs = {0, 0, 0};
    inputs.warning = (phase == 1U);
    inputs.emergency = (phase == 2U);
    inputs.fault = (phase == 3U);
    return inputs;
}

static SafetyState EvaluateDemo(DemoInputs inputs)
{
    /* This precedence is for the demonstration, not a live safety policy. */
    if (inputs.fault) return SAFETY_SENSOR_FAULT;
    if (inputs.emergency) return SAFETY_EMERGENCY;
    if (inputs.warning) return SAFETY_WARNING;
    return SAFETY_SAFE;
}

void SafetyState_Init(SafetyStateMachine *machine, uint32_t now_ms)
{
    machine->mode = SAFETY_MODE_DEMO;
    machine->state = SAFETY_SAFE;
    machine->phase_started_ms = now_ms;
    machine->demo_phase = 0;
}

uint8_t SafetyState_Update(SafetyStateMachine *machine, uint32_t now_ms)
{
    SafetyState previous = machine->state;
    if (machine->mode != SAFETY_MODE_DEMO)
    {
        /* No live decision path exists yet; never silently indicate SAFE. */
        machine->state = SAFETY_SENSOR_FAULT;
        if (previous != machine->state) machine->phase_started_ms = now_ms;
        return previous != machine->state;
    }

    /* Wrap-safe tick subtraction; catch up without blocking after a delay. */
    uint32_t elapsed = now_ms - machine->phase_started_ms;
    uint32_t steps = elapsed / 5000U;
    if (steps != 0U)
    {
        machine->demo_phase = (uint8_t)((machine->demo_phase + steps) % 4U);
        machine->phase_started_ms += steps * 5000U;
    }
    machine->state = EvaluateDemo(DemoInputs_ForPhase(machine->demo_phase));
    return previous != machine->state;
}

uint8_t SafetyState_Leds(const SafetyStateMachine *machine, uint32_t now_ms)
{
    uint32_t elapsed = now_ms - machine->phase_started_ms;
    switch (machine->state)
    {
    case SAFETY_SAFE:
        return SAFETY_LED_GREEN;
    case SAFETY_WARNING:
        return ((elapsed / 500U) % 2U == 0U) ? SAFETY_LED_YELLOW : 0U;
    case SAFETY_EMERGENCY:
        return ((elapsed / 125U) % 2U == 0U) ? SAFETY_LED_RED : 0U;
    case SAFETY_SENSOR_FAULT:
        return ((elapsed / 250U) % 2U == 0U) ? SAFETY_LED_YELLOW : SAFETY_LED_RED;
    default:
        return 0U;
    }
}

const char *SafetyState_Label(SafetyState state)
{
    switch (state)
    {
    case SAFETY_SAFE: return "SAFE";
    case SAFETY_WARNING: return "WARNING";
    case SAFETY_EMERGENCY: return "EMERGENCY";
    /* Space instead of underscore: existing font covers ASCII 32 through 90. */
    case SAFETY_SENSOR_FAULT: return "SENSOR FAULT";
    default: return "INVALID";
    }
}
