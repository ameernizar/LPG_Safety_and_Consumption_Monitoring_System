# Sensor reliability audit and targeted corrections

Current bench: OLED, DS18B20, MQ-6, flame DO, HX711/load cell and three LEDs.
RTC, MicroSD, buzzer and the mechanical weighing platform are postponed. RTC
code is retained unchanged; a missing RTC can still show NO ACK. This revision
adds no real alarm behavior, weight calibration, gas concentration or actuators.

## Existing protections retained

| Area / implementation | Protections already present |
| --- | --- |
| `main.c`: `OneWire_Presence`, read/write slots | Stuck-LOW rejection, HIGH/LOW/HIGH presence checks, bounded DWT waits, open-drain release and saved interrupt-mask restoration |
| `DS18B20_ReadTemperature`, `DS18B20_Decode` | Serviced 800 ms conversion wait, completion check, two presence checks, nine-byte scratchpad, CRC-8 (0x8C), configuration validation, signed resolution-aware decoding, -55..125 C range and rejection of 85 C without BUSY evidence |
| `MQ6_Init`, `MQ6_Sample` | ADC setup/calibration status, 16 conversions, finite 10 ms conversion polls, start/poll/stop error checks, rejection of partial averages and rail tracking |
| `mq6_monitor.c` | EMA, separate raw/filter/baseline validity, minimum settling, rolling AND full-capture stability checks, signed delta, fixed baseline, API-only recapture, qualification reset on invalid data/gaps |
| `hx711.c` | Signed 24-bit counts, channel A/gain 128, startup discard/settling, readiness timeout, saturation/handshake/timing faults, window reset and recovery; no kilograms |
| `oled_ui.c` | Five pages, diagnostic codes, invalid-value placeholders, bounded framebuffer drawing and finite transport timeouts |
| `safety_state.c` | DEMO-only LED states, independent of sensors |

DS errors already cannot become numeric valid temperatures: each main-loop attempt
has a fresh local result, and the UI gates it on DS_OK. DS diagnostics remain
E1 initial presence, E2 conversion, E3 second presence, E4 CRC, E5 invalid value,
E6 internal/timing, and pending. No duplicate acquisition/CRC driver was added.

No IWDG/WWDG is configured. Fatal initialization/OLED failures still use the
existing Error_Handler (interrupts disabled, no automatic reset). That behavior
is explicitly preserved, not presented as automatic fault recovery.

## Confirmed gaps and corrections

1. **HX711 timeout depended on DOUT being HIGH.** After a service interruption,
   DOUT could already be LOW with a buffered conversion; the driver accepted it
   immediately and retained old stability history. Timeout now runs before the
   DOUT test. After >=1000 ms without a completed frame, previous validity/window
   are cleared and the returning frame is discarded through existing gain/settling
   recovery. `HX711_Fresh()` also hides a cached reading at rendering time if its
   age reaches 1000 ms, even before service updates the stored state.
2. **Temperature had no age metadata.** Only a successful acquisition updates
   `temperature_sample_tick`. The UI considers success fresh for <2500 ms; older
   success shows TEMP: STALE / DS18B20: STALE, without the numeric value. Failures
   retain their specific E-code and dashed temperature, not a stale success.
3. **Flame DO was one instantaneous sample per display refresh.** A small pure
   `FlameFilter` now qualifies the same observed level for 50 ms during existing
   background service. A transition or >=100 ms sampling gap invalidates the
   candidate until requalified. Duplicate timestamps cannot qualify it; tick wrap
   is covered. UI shows WAIT while qualifying, then HIGH/LOW. No detection polarity
   is assigned, and stable DO does not establish module connection or health.
4. **MQ-6 voltage estimates ignored the monitor's saturation flag.** The SENSORS
   page now requires both successful acquisition and monitor validity before
   showing voltages. The MQ page explicitly says ADC OK/SAT/ERR/WAIT independently
   of BASE WAIT n/32 or the accepted BASE value. Saturated raw counts are retained
   for diagnosis, while filter/delta are hidden when invalid. Existing baseline
   arithmetic/qualification and signed deviations are unchanged.

Policy values are in `sensor_health.h` and `hx711.h`. These are freshness/debounce
limits, not calibrated sensor accuracy or gas thresholds. Flame qualification
means consistent observed samples, not proof the wire never changed between them.
Sensor disconnection cannot always be inferred from GPIO/ADC values. Likewise,
freshness denotes a recently accepted host acquisition, not independent proof
of the physical sensor's conversion history.

No new delay is added. The necessary externally-powered DS18B20 conversion wait
still services flame filtering, HX711 and DEMO/heartbeat every background iteration.
1-Wire slots and HX711 pulse timing remain unchanged. Existing bounded ADC/I2C
transactions remain synchronous. This is not a fully asynchronous acquisition
rewrite. Age labels can only update while the application is running/rendering;
a frozen CPU or failed OLED can leave the last pixels visible. No watchdog is
claimed to solve that case.

## Changes and validation

Production files:
- Added `Core/Inc/sensor_health.h` (pure freshness/debounce helpers).
- Updated `Core/Inc/hx711.h`, `Core/Src/hx711.c` (age check and timeout placement).
- Updated `Core/Inc/oled_ui.h`, `Core/Src/oled_ui.c` (snapshot metadata/diagnostics).
- Updated only USER CODE in `Core/Src/main.c` (flame sampling and success timestamp).

Tests/docs:
- Added `tests/sensor_health_test.c` and `tests/ds18b20_diagnostic_test.py`.
- Extended `tests/hx711_test.c` and `tests/oled_ui_test.c`.
- Added this report and a README link/current-status note.

All **eight test suites pass**: six existing suites plus sensor-health policy and
a DS18B20 harness extracting the actual decode/acquisition functions from main.c.
The latter checks error propagation, CRC/configuration/range rejection, negative
values, 85 C handling and background servicing of the conversion wait. It stubs
the bus and therefore does not verify real 1-Wire electrical timing.
HX711 regression covers ready-LOW after a gap and recovery; UI tests compare actual
font pixels for fresh/stale/error values, ADC/baseline separation, negative delta,
saturation and HX711 timeout. Freshness/flame tests include boundaries, bounce,
missing observations, duplicates and uint32_t tick rollover.

New standalone tests:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -ICore/Inc \
  tests/sensor_health_test.c -o /tmp/sensor_health_test
/tmp/sensor_health_test
python3 tests/ds18b20_diagnostic_test.py
```

The existing suite commands remain in `tests/README.md` and `HX711_DIAGNOSTIC.md`.
OLED tests used ASan/UBSan (sandbox leak checks disabled); state tests used UBSan
where applicable. Full clean Debug build for STM32F103C8Tx passed with **0 errors,
0 warnings**. Release was not rebuilt for this revision.

| Resource | This Debug build | Change |
| --- | ---: | ---: |
| Flash (text + data) | 33,232 / 65,536 bytes (50.71%) | +760 bytes |
| Application data + BSS | 2,032 bytes | +16 bytes |
| Minimum heap/stack reservation | 1,536 bytes | Unchanged |
| Total link-reserved RAM | 3,568 / 20,480 bytes (17.42%) | +16 bytes |

Runtime stack/heap high-water use has not been measured. ELF is
`Debug/STM32_HW_Test.elf`; the build log is `/tmp/lpg-reliability-build.log`.
Source hashes and function comparisons confirm no changes this turn to MCU/IOC,
linker, clock/GPIO initialization, I2C, ADC setup/acquisition, RTC code, MQ-6
processing, DEMO outputs, DS18B20 timing/acquisition, startup or vendor sources.
Previous uncommitted MCU-correction changes are preserved. No CubeMX regeneration,
flashing, commit or push occurred.

## Breadboard checks after explicit approval to flash

Power off before changing wires. Keep the verified pin assignments and common
ground. Do not release LPG, use an open flame, apply 5 V to MCU inputs, short an
output, or place a cylinder/heavy load on the unfinished weighing assembly.

1. **General/OLED/LEDs:** after an approved flash, observe a complete five-page
   rotation (~25 s), PC13 heartbeat and all four DEMO patterns. Recheck the reported
   yellow-LED issue. An absent RTC may show NO ACK; no RTC provisioning is needed
   for these tests. Confirm there are no extra pages or new output behaviors.
2. **DS18B20:** observe temperature and DS18B20: FRESH. Warm the probe gently by
   hand and verify response. Power off, disconnect the probe, then restart: expect
   dashed temperature and a presence error, never zero presented as valid. Reconnect
   with power off and verify recovery. CRC and stale-age paths have host coverage;
   do not manufacture unsafe electrical faults to force them. Normal sampling
   should not display STALE.
3. **MQ-6:** observe ADC OK during WARMUP/STABILIZING, while BASE WAIT and dashed
   delta remain separate. Baseline may take longer than its minimum interval or
   remain unqualified; never force acceptance. Once qualified, check signed delta.
   For a low-rail diagnostic, power off and disconnect AO from the divider's upper
   resistor, keeping the lower 10 kΩ resistor from PA2 to GND intact. After restart,
   expect near-zero raw ADC / SAT LOW and invalid filter/delta/voltage. Reconnect
   with power off. A floating ADC is not a reliable disconnection test; do not
   inject 3.3 V/5 V to force the upper rail. Do not test with released gas.
4. **Flame DO:** observe the module's raw output under ordinary ambient conditions.
   If the module's sensitivity adjustment changes DO, confirm HIGH/LOW updates
   without interpreting either as a flame event. Brief changes may be suppressed
   or show WAIT; the ~1 Hz OLED refresh may not display every 50 ms transition.
   Unplugged DO may float and cannot be reliably reported as disconnected. No
   automatic alarm should result from either level.
5. **HX711:** verify READY, signed RAW and eventual STABLE while undisturbed.
   Apply/release only gentle force suitable for the unsupported test setup; verify
   responsiveness without kg/tare. Power off, disconnect the HX711 module, restart:
   expect NO DATA / CHECK DT after about one second, then on its scheduled page.
   Reconnect with power off and verify startup requalification and raw recovery.
   Module 3.3 V compatibility and provisional bridge-wire colors still need
   electrical confirmation. Software cannot distinguish every wiring fault.

If any sensor shows persistent errors, record the exact page/status/raw values and
whether other pages and LEDs continue updating. Hardware accuracy, signal timing,
noise rejection and physical recovery remain unverified until these tests pass.
