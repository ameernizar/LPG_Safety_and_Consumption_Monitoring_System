# Phase 9 state-machine test

Run from the project root with a host C compiler:

```sh
cc -std=c11 -Wall -Wextra -Werror -ICore/Inc Core/Src/safety_state.c tests/safety_state_test.c -o /tmp/safety_state_test
/tmp/safety_state_test
```

The module has no HAL dependencies. Tests cover three 20-second cycles, each
millisecond of LED timing, tick rollover, delayed servicing, transition reports,
OLED label coverage, and the disabled live-assessment path.

DEMO supplies synthetic logical inputs only. It never uses live temperature,
flame polarity, MQ-6 counts, or acquisition faults to choose a state. Live mode
is deliberately unimplemented and reports SENSOR_FAULT if selected. This is
an engineering demonstration, not a certified LPG safety system.

Hardware test: flash the Debug ELF and observe green for 5 seconds, blinking
yellow for 5 seconds, fast-blinking red for 5 seconds, then alternating yellow/red
for 5 seconds. The cycle repeats. Check that sensor readings and the PC13
heartbeat continue. The OLED state label updates with the approximately 1 Hz
sensor frame; GPIO servicing pauses briefly during 1-Wire slots and OLED I2C
transfers, so this cooperative demonstration is not a hard real-time alarm.

Only PA3/PA4/PA5 are demo outputs. PA6, PB8/PB9, buzzer, fan and relay are unused.
Future live assessment needs a separate validated input adapter and policy,
including calibrated thresholds, polarity, hysteresis and fault precedence.
SafetyState_Update's transition result provides a hook for future event logging.

# Phase 10 MQ-6 baseline qualification (stability revision)

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -ICore/Inc Core/Src/mq6_monitor.c tests/mq6_monitor_test.c -o /tmp/mq6_monitor_test
/tmp/mq6_monitor_test
```

RAW remains the average of 16 ADC conversions acquired about once per second.
The Q8 EMA retains alpha 1/8. ADC initialization, pin assignments and timing-critical
DS18B20 code are unchanged. DEMO remains independent of all gas-monitor results.

Configuration in Core/Inc/mq6_monitor.h:

| Parameter | Default | Purpose |
| --- | --- | --- |
| MQ6_SETTLING_MS | 60000 ms | Minimum elapsed period from initialization/known power restart |
| MQ6_STABILITY_SAMPLES | 16 | Even-sized rolling window of raw batches |
| MQ6_STABILITY_SPAN_MS | 15000 ms | Minimum oldest-to-newest window duration |
| MQ6_MAX_SAMPLE_GAP_MS | 2500 ms | Maximum permitted gap during qualification/capture |
| MQ6_STABILITY_RANGE | 12 counts | Maximum rolling max-minus-min |
| MQ6_STABILITY_DRIFT | 4 counts | Maximum absolute difference of rolling half-window means |
| MQ6_BASELINE_SAMPLES | 32 | Even-sized baseline capture following qualification |
| MQ6_CAPTURE_RANGE | 12 counts | Maximum max-minus-min over ALL captured batches |
| MQ6_CAPTURE_DRIFT | 4 counts | Maximum first-half vs second-half mean difference over ALL capture batches |
| MQ6_EMA_SHIFT | 3 | EMA alpha 1/8 |

These are experimental noise/drift criteria, not calibrated gas thresholds. A
short stable interval does not establish heater readiness, manufacturer
preconditioning, clean air, reliable detection or gas concentration. Use actual
module guidance and measured ambient behavior to tune them. Constant but incorrect
voltages can pass stability checks. ADC saturation checks cannot detect all faults.

State sequence: WARMUP -> STABILIZING -> BASELINE -> MONITORING. Warm-up samples
are excluded from qualification. After the minimum timer, collect 16 recent valid
batches spanning at least 15 seconds; both range and drift must pass. The NEXT
32 batches form the baseline; the qualification window itself is not averaged
into it. Rolling stability is checked throughout capture. Capture-wide range
is checked as samples arrive; capture-wide drift is checked before acceptance.
If either fails, discard the candidate and requalify. No timeout can accept an
unstable baseline. With 1 Hz sampling and default parameters, acceptance occurs
about 107–109 seconds from initialization; timing/invalid data can extend this
indefinitely. Baseline is the rounded mean of the 32 raw batch averages (512
underlying conversions), then stays fixed. Signed DELTA is FILTER minus BASE.

Half-mean differences are compared using integer sums: abs(sum_first-sum_second)
<= half_count * limit. There is no rounded-mean threshold ambiguity. Samples
are 0..4095, capture length is even and <=1024, window length even and <=64;
32-bit sums safely bound the arithmetic. Duplicate timestamps are ignored;
uint32_t subtraction handles tick wrap. Call at least once per full tick wrap.

An invalid reading/rail observation shows its existing ACQ ERROR/SAT status,
hides FILTER/DELTA and clears qualification/capture. On the next valid sample,
resume STABILIZING if the original minimum warm-up time has elapsed; otherwise
finish the original WARMUP. The warm-up start time is not restarted by such
errors. Excessive sample gaps also discard qualification but retain this timer;
the returning valid sample can seed a fresh window. EMA reseeds after errors or
long gaps. After acceptance, faults preserve the fixed baseline, with invalid
delta hidden. There is no automatic recalibration.

Manual API (not connected to any button, RAM trigger or main-loop caller):

```c
MQ6_MonitorRequestBaseline(&monitor, HAL_GetTick());
```

This explicitly invalidates the old baseline/delta, clears qualification and
starts a new stability/capture workflow while retaining live EMA. It preserves
the original minimum-warm-up deadline: an already warmed monitor goes directly
to STABILIZING; an unfinished warm-up completes its remaining time. Use
MQ6_MonitorInit() instead on genuine initialization or a KNOWN heater power
interruption to restart the full minimum period. Firmware cannot infer heater
power loss from ordinary ADC samples. Baselines remain RAM-only and reset on
MCU restart. Repeated manual requests cancel partial collection, never accept it.

The existing MQ6 OLED page gains STABILIZING without changing its layout, font,
page scheduling or other pages. BASE progress remains n/32; BASE/DELTA are dashes
until accepted. Footer remains EXPERIMENTAL ONLY. No ppm or automatic alarm is
introduced; negative delta (including 380-454=-74) remains valid arithmetic.

Tests cover warm-up boundaries, duration qualification, duplicate timestamps,
rolling/capture range and drift equality/rejection, slow drift that passes the
rolling window but fails full capture, rounded baseline mean, invalid samples,
rails, gaps, persistent instability, manual recapture, fixed baseline recovery,
negative delta, count limits and tick rollover. No device flashing is performed.

Hardware validation is pending explicit user approval. Once approved, observe
WARMUP/STABILIZING/BASELINE/MONITORING and verify that unstable signals do not
produce a baseline; check raw vs filtered readings, delta, RTC/sensor pages and
DEMO LEDs. Do not intentionally release LPG for this software check. The API
has no physical trigger in this revision.

# Phase 11 RTC explicit setting and diagnostics

Reference: https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf
RTC stores IST wall-clock values (UTC+05:30), not UTC. No hardware timezone
field exists. OLED uses DD-MM-YYYY, HH:MM IST and a seconds indicator. Future
CSV format is YYYY-MM-DD HH:MM, documented as IST; no logger is added.

## Diagnosis of the earlier firmware

The previous default ds3231.h had DS3231_ENABLE_SET=0. Its guarded boot setter
was absent from the normal ELF, so normal firmware could only display OSF=1.
The separate timestamp-specific ELF did contain the setter. The current OLED
output does not prove that image ever ran, or identify its failure stage.
The old setting path also depended on timed RESET/startup and an OSF-clear
one-time guard. No evidence establishes a hardware RTC fault.

## Current behavior

One firmware supports NORMAL and explicitly armed SET modes. Boot zeros the
RAM request; there is no timestamp or automatic setter in startup. RTC is read
about once per second at 0x68<<1, OLED remains 0x3C<<1. All sensor and DEMO
acquisition continues. No pin, bus speed, clock, .ioc or driver unrelated to RTC
is changed. The default build includes setting capability but no automatic
request. Keep DS3231_ENABLE_SET=1 for this interface.

Target is supplied in RAM, validated and snapshotted at ARM. Seconds start at
00; weekday is calculated Monday=1..Sunday=7. Both the host helper and firmware
validate calendar fields and leap years. The driver writes 00h..06h in BCD,
24-hour mode, verifies exact readback, then checks elapsed seconds using a later
read. Only then does it clear OSF (0Fh bit7), preserve EN32kHz, and write ones to
A1F/A2F to avoid clearing alarm flags. Control register 0Eh is untouched. A final
read must confirm OSF clear and valid fields. All I2C calls have 10 ms timeouts.
Read buffers provide a coherent date/time snapshot. Century convention 2000..2199.

## Arm and trigger with existing ST-LINK / CubeProgrammer

1. Flash Debug/STM32_HW_Test.elf and run normally. Do not use any previous
   RTC_SET_* images: those are obsolete and contain expired targets.
2. Choose a FRESH future IST minute, allowing time to prepare and connect.
   Run `python3 tools/rtc_request.py 'YYYY-MM-DD HH:MM'` with actual values.
   The helper validates the date and prints RAM addresses derived from this
   exact ELF, creating target.bin, arm.bin and trigger.bin in /tmp/rtc-request.
   It does not obtain a timestamp or interact with hardware.
3. Connect CubeProgrammer via ST-LINK in Hot Plug mode to the running MCU.
   Use its RAM memory-write facility (not flash erase/download/run): write
   target.bin at the printed target address, then arm.bin at the printed
   command address. Alternatively enter five 32-bit integer calendar fields
   followed by the ARM word 0x41524D31 at offset20. Target data must finish
   writing BEFORE the separate ARM command. Do not reset the MCU.
4. Observe SET MODE: ARMED on the RTC page (which appears every 15 seconds).
   ARM within 60 seconds before the chosen minute. If the target changes,
   reload target then ARM again before any trigger.
5. At the target minute, write trigger.bin (word 0x53455431) at the command
   address. If this access halts the CPU, resume without reset; processing
   occurs after resumption. No debugger breakpoint or new GPIO is required.
   Verify that your programmer permits RAM writes without resetting; otherwise
   stop and report that limitation rather than switching to timed RESET.
6. Observe RTC SET VERIFYING, then RTC SET OK and NORMAL readout with OSF=0
   and advancing seconds. The result is retained on subsequent RTC pages.
   Confirm the wall-clock value against the trusted IST reference.

The trigger is consumed by the approximately 1 Hz acquisition loop, so it can
be delayed roughly one second plus SWD/host latency. Startup and flash latency
are excluded. This is an approximate clock-setting test, not precision time
synchronization. Choose a fresh target if late; never silently reuse an expired
one. Verification uses MCU elapsed ticks, not an assumption that startup is
instantaneous. The seconds-check service must run 1100..4000 ms after writing;
a stalled debugger during this period causes verification failure.

There is at most one write attempt per boot. After an attempt (successful or
failed), commands are ignored until reset; reset itself never writes the RTC.
An OSF-clear, initialized RTC is refused rather than overwritten. After losing
RTC power, OSF may become 1: NORMAL displays TIME INVALID, requiring a fresh
explicit ARM/TRIGGER. There is no installed/assumed backup battery and no
persistent calibration/time-setting request.

## Results

- E1 RTC not responding / initial I2C read or probe failed.
- E2 time-register I2C write failed.
- E3 readback mismatch/read failure, seconds not advancing, late verification,
  or already-initialized clock refused; the OLED gives the latter two specific
  reasons where applicable.
- E4 status read/OSF clear/write/final confirmation failed.
- E5 invalid target timestamp; no RTC write.
- E6 ARM expired without trigger, trigger without ARM, or unknown command.

An ordinary unarmed boot says NORMAL: NOT ARMED, not a failure. E6 is shown when
an explicit setting request did not trigger correctly. Armed/failed modes do
not affect the DEMO safety state or sensor readings. SET OK records completed
write/readback/advancement/OSF verification, not certified clock accuracy.

## Tests

```sh
cc -std=c11 -Wall -Wextra -Werror -Itests/rtc_stub -ICore/Inc Core/Src/ds3231.c tests/ds3231_test.c -o /tmp/rtc_test
/tmp/rtc_test
```

Host tests cover BCD, 12/24-hour conversion, invalid calendar fields, leap years,
century handling, no ACK, read errors, no writes from the reading path, setting,
stationary seconds, readback mismatch, failed time write, failed OSF clear and
refusal to overwrite an OSF-clear RTC. The stub is excluded from firmware.
Physical ARM/TRIGGER timing and RTC operation still require board verification.

Run `python3 tests/rtc_command_test.py` to test the actual main.c command
helpers with a host HAL stub: unarmed boot, trigger without ARM, invalid target,
leap-day weekday calculation, ARM expiry, target snapshot, successful trigger,
advancement, and ignored repeat commands after a completed attempt.

# Four-page OLED UI (supersedes earlier page-layout descriptions)

Core/Inc/oled_ui.h and Core/Src/oled_ui.c receive a snapshot of existing sensor
results; they do not acquire sensors or set the RTC. Main retains acquisition,
heartbeat, DEMO LED servicing and I2C error handling. RTC_RESET_PROVISION stays
zero: UI startup does not set/overwrite the running clock. No .ioc changes.

Pages rotate HOME -> MQ6 GAS MONITOR -> SENSORS -> SYSTEM every 5000 ms using
wrap-safe HAL_GetTick subtraction; display changes occur on the next existing
approximately 1 Hz sensor frame (not a hard real-time 5-second deadline).
A skipped interval advances without loops or delays. The existing synchronous
SH1106 transfer remains; UI adds no sleeps or 1-Wire activity.

All graphics are 1-bit. Existing 5x7 uppercase ASCII glyphs occupy 6x8 cells.
Text is bounded to 20 characters at x=2. Inverted heading is y=0..8, five content
rows begin at y=12,21,30,39,48; separator y=55 and footer y=56. Footer labels
leave room for four page markers at x=110..124; header also shows page/4.
SH1106_Pixel and InvertRect clip coordinates before framebuffer access. The
existing 1024-byte framebuffer is cleared in RAM, composed, then sent once;
no blank frame is sent and no second framebuffer is allocated.

HOME: LPG MONITOR, DD-MM-YYYY, HH:MM:SS IST, temperature, DEMO and simulated
state. RTC rendering requires successful driver status, OSF clear, and valid
Gregorian calendar fields. Otherwise TIME INVALID/status is shown.
MQ6: raw, EMA filter, experimental baseline/progress, signed delta and monitor
status. Missing/invalid values are dashes; ADC init/cal/read errors remain
separate. No ppm, concentration or leak claims.
SENSORS: temperature + DS error/CRC/busy diagnostic, raw HIGH/LOW flame DO,
WEIGHT: PENDING, existing approximate AO/input voltages. Flame polarity is not
interpreted as a detected event.
SYSTEM: RTC status, SD: NOT INSTALLED (MicroSD), LOAD CELL: PENDING, DEMO mode,
72 MHz clock or RTC setting result, and IST label or setting failure reason.

Host framebuffer/rotation test:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/ui_stub -ICore/Inc tests/oled_ui_test.c Core/Src/oled_ui.c Core/Src/mq6_monitor.c Core/Src/safety_state.c -o /tmp/oled_ui_test
ASAN_OPTIONS=detect_leaks=0 /tmp/oled_ui_test
```

Leak checking is disabled because LeakSanitizer cannot run under this sandbox's
ptrace environment; address and undefined-behavior checks remain enabled. Tests
cover four pages, one transfer per frame, timing boundaries, tick rollover,
invalid states, and extreme clipping coordinates. PBM previews are written to
/tmp/oled-page-0.pbm through -3.pbm, using synthetic test data only. The firmware
renders exclusively the existing live snapshot.

Physical check: flash Debug/STM32_HW_Test.elf, keeping RTC power connected.
Observe a full 20-second cycle. Confirm readable headings/footers, no overlaps,
no blank-frame flicker, live temperature, changing RTC seconds and correct IST
date. Confirm MQ6 values, raw flame DO, PC13 and all four DEMO LED patterns.
Check that pending hardware remains labeled pending/not installed. Read back
RTC after reset to confirm no time overwrite. Rendering is not physically
verified by the host preview or build.
