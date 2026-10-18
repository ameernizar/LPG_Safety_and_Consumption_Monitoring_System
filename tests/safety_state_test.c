#include "safety_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    SafetyStateMachine machine;
    for (unsigned wrap = 0; wrap < 2; wrap++)
    {
        uint32_t base = wrap ? UINT32_MAX - 7000U : 0U;
        SafetyState_Init(&machine, base);
        assert(machine.mode == SAFETY_MODE_DEMO);
        for (uint32_t ms = 0; ms < 60000U; ms++)
        {
            SafetyState previous = machine.state;
            uint8_t changed = SafetyState_Update(&machine, base + ms);
            unsigned phase = (ms / 5000U) % 4U;
            uint32_t elapsed = ms % 5000U;
            uint8_t expected;
            assert((unsigned)machine.state == phase);
            assert(changed == (machine.state != previous));
            switch (phase)
            {
            case 0: expected = SAFETY_LED_GREEN; break;
            case 1: expected = (elapsed / 500U) % 2U ? 0 : SAFETY_LED_YELLOW; break;
            case 2: expected = (elapsed / 125U) % 2U ? 0 : SAFETY_LED_RED; break;
            default: expected = (elapsed / 250U) % 2U ? SAFETY_LED_RED : SAFETY_LED_YELLOW; break;
            }
            uint8_t leds = SafetyState_Leds(&machine, base + ms);
            assert(leds == expected);
            assert((leds & (leds - 1U)) == 0U); /* Zero or one LED. */
        }
    }
    SafetyState_Init(&machine, 100U);
    assert(SafetyState_Update(&machine, 16100U));
    assert(machine.state == SAFETY_SENSOR_FAULT && machine.phase_started_ms == 15100U);
    SafetyState_Update(&machine, 20100U);
    assert(machine.state == SAFETY_SAFE);
    machine.mode = SAFETY_MODE_LIVE_DISABLED;
    assert(SafetyState_Update(&machine, 20200U));
    assert(machine.state == SAFETY_SENSOR_FAULT);
    assert(!SafetyState_Update(&machine, 20300U));
    assert(!strcmp(SafetyState_Label(SAFETY_SENSOR_FAULT), "SENSOR FAULT"));
    for (unsigned state = 0; state < 4; state++)
    {
        const char *label = SafetyState_Label((SafetyState)state);
        assert(strlen(label) + 6 <= 21); /* DEMO: prefix and 128px font width. */
        for (; *label; label++) assert(*label >= 32 && *label <= 90);
    }
    puts("PASS: demo default, all transitions/blinks, alternating fault LEDs, tick rollover, delayed updates, transition signal, disabled live mode, and OLED labels.");
    return 0;
}
