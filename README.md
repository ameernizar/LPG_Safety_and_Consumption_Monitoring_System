# LPG Safety and Consumption Monitoring System

An experimental embedded student project built around an **HW-267 Blue-Pill-style
board with an HK K32F103CBT6-compatible MCU**. It displays live temperature, raw
flame-sensor output, experimental MQ-6 gas-sensor measurements and IST date/time.
A separate DEMO state machine exercises warning LEDs. An HX711 interface now
provides raw load-cell diagnostics in preparation for future weight measurement.

**This is not a certified LPG detector or safety controller.** The firmware does
not calculate gas concentration, detect a validated LPG leak, or measure cylinder
contents. SAFE/WARNING/EMERGENCY labels are simulated DEMO states, independent of
live sensor readings.

## Current development status

| Feature | Current status |
| --- | --- |
| SH1106-compatible OLED, DS18B20, flame DO, MQ-6 ADC | Physically tested during development |
| DS3231 RTC | Running IST time previously confirmed on hardware |
| MQ-6 processing | EMA filtering, stability-qualified ambient baseline and signed deviation implemented |
| Warning LEDs | Four-state DEMO implemented and previously tested; a later yellow-LED issue was reported, with no confirmed resolution recorded |
| HX711 + 80 kg full-bridge load cell | Raw diagnostic implemented, built and host-tested; physical communication/response confirmation pending |
| Weight calibration and weighing platform | Not implemented; platform not yet constructed |
| MicroSD, buzzer, fan and navigation buttons | Not integrated |

The latest recorded HX711 firmware build completed with **zero errors and
warnings**. Host tests passed for HX711, OLED, MQ-6, RTC and DEMO behavior. This
revision has not been flashed by the development assistant; hardware testing
requires explicit approval. A successful build does not establish sensor accuracy.

## Hardware and pin assignments

The CubeMX compatibility profile is **STM32F103CBTx**, with a verified **72 MHz**
system clock. This profile does not imply that the installed MCU is a genuine ST
part. Programming uses ST-LINK and STM32CubeProgrammer.

| Component | MCU connection | Configuration / wiring |
| --- | --- | --- |
| 128×64 SH1106-compatible OLED | PB6 SCL, PB7 SDA | I2C1, address `0x3C`, 3.3 V |
| DS3231 RTC | PB6 SCL, PB7 SDA | Shared I2C1, address `0x68`, 3.3 V |
| DS18B20 temperature probe | PA0 | 1-Wire, 3.3 V, external 4.7 kΩ data pull-up |
| Flame sensor DO | PA1 | Digital input, raw HIGH/LOW; detection polarity not assumed |
| MQ-6 AO | PA2 / ADC1_IN2 | Analog input through two 10 kΩ divider resistors |
| Green / yellow / red LEDs | PA3 / PA4 / PA5 | Each through 220 Ω to anode; cathode to GND |
| HX711 DOUT / DT | PB8 | GPIO input with internal pull-up |
| HX711 PD_SCK / SCK | PB9 | Push-pull output, idle LOW |
| Onboard status LED | PC13 | Active-low heartbeat |
| ST-LINK SWD | PA13 / PA14 | Reserved for debugging/programming |

I2C1 remains at **100 kHz**, with alternate-function open-drain PB6/PB7 and
external 4.7 kΩ pull-ups to 3.3 V. Addresses above are 7-bit; HAL calls use each
address shifted left by one. HX711 uses separate GPIO and does not remap I2C1.

### Power and analog connections

The latest reported prototype uses **one 5 V adapter for the STM32 and MQ-6**,
with common ground. Previously, the STM32 used Micro-USB and MQ-6 used a separate
5 V adapter. The actual single-adapter input path, regulator capacity and rail
voltages have not been independently verified in this development session.
Keep MQ-6 heater current off the STM32 3.3 V regulator and review USB/ST-LINK
backfeeding before connecting an additional power source.

```text
MQ-6 AO ---- 10 kΩ ----+---- PA2
                      |
                    10 kΩ
                      |
                     GND
```

This divider reduces nominal 0–5 V AO to approximately 0–2.5 V. MQ-6 DO is unused.
Displayed voltage estimates assume a nominal 3.3 V ADC reference and equal divider
resistors; they are not precision measurements.

HX711 VCC is connected to 3.3 V, pending verification of the actual module's
compatibility. Load-cell wiring is provisional: **red E+, black E−, green A+,
white A−**; wire colors alone do not verify the connections. The RTC has no verified
backup battery, so retaining time after power removal is not guaranteed.

## OLED interface

Five pages rotate every **5 seconds** using `HAL_GetTick()`; a full cycle takes
approximately 25 seconds. Page changes appear on the next display refresh.

| Page | Displayed information |
| --- | --- |
| HOME | Project title, IST date/time, temperature and simulated DEMO state |
| MQ6 GAS MONITOR | Raw ADC, filtered ADC, baseline, signed delta and processing status |
| SENSORS | Temperature/communication errors, raw flame DO, weight pending and estimated MQ-6 voltages |
| SYSTEM | RTC status, MicroSD/load-cell placeholders, DEMO mode and system/RTC-setting information |
| HX711 RAW | READY/WAITING/ERROR, latest signed raw count, sample range and stability |

The UI uses a 5×7 font, inverted headings, page numbers and five compact footer
markers. One 1024-byte framebuffer is composed before transfer; no blank frame
is sent between updates. Weight/load-cell PENDING labels on the older pages mean
that usable weight measurement is still pending; raw diagnostics are on page 5.

## Sensor processing and scheduling

**Temperature:** PA0 uses open-drain 1-Wire signaling and the existing DWT
microsecond timing. The firmware waits for conversion, reads all nine scratchpad
bytes and validates Dallas CRC-8 before displaying signed temperature. Failures
produce diagnostic codes instead of fabricated readings.

**MQ-6:** ADC1 runs at 12 MHz with 239.5-cycle sampling and initialization-time
ADC calibration. Each approximately one-second update averages 16 conversions,
then applies an EMA with weight 1/8. Defaults in `Core/Inc/mq6_monitor.h` are:

- `WARMUP`: minimum 60-second software settling interval.
- `STABILIZING`: 16 recent batches spanning at least 15 seconds; range ≤12 ADC
  counts and first-half/second-half mean drift ≤4 counts.
- `BASELINE`: collect 32 batches, checking the rolling window and the entire
  capture's range/drift before accepting their rounded mean.
- `MONITORING`: retain a fixed baseline; signed delta = filtered ADC − baseline.

These are experimental, configurable stability criteria, not gas alarm thresholds
or evidence of completed manufacturer preconditioning. Instability never qualifies
through timeout alone. Invalid samples or gaps over 2.5 seconds reset qualification
without restarting the minimum warm-up period. An accepted baseline survives
transient acquisition errors; invalid live values/delta are hidden. Baselines are
RAM-only and reset on restart. `MQ6_MonitorRequestBaseline()` allows explicit
recapture through an API only; no button or automatic trigger is assigned.

**HX711:** the driver reads signed 24-bit data and sends the 25th clock to select
channel A/gain 128. It discards the first frame and drains startup frames for
500 ms. Background servicing returns immediately when data is not ready, and a
one-second no-data timeout reports an error. It also reports saturation, a failed
DOUT handshake and timing faults. Raw values are not smoothed or converted to kg.
STABLE means that 16 recent counts have a range ≤1000 counts, an experimental
criterion defined in `Core/Inc/hx711.h`.

HX711 service runs during temperature-conversion and main-loop background waits,
outside timing-critical 1-Wire slots. Both interfaces share DWT without resetting
its counter. LED/page scheduling is non-blocking; existing OLED transfers and
some sensor transactions remain synchronous and bounded. This is not a guarantee
of capturing every HX711 conversion. See [HX711_DIAGNOSTIC.md](HX711_DIAGNOSTIC.md)
for timing, error handling, tests and pending physical checks.

## DEMO warning behavior

| Simulated state — 5 seconds each | LED output |
| --- | --- |
| SAFE | Green steady |
| WARNING | Yellow blinking |
| EMERGENCY | Red blinking quickly |
| SENSOR_FAULT | Yellow/red alternating |

The cycle repeats independently of all sensor readings. PC13 provides a firmware
heartbeat. No buzzer, fan, relay, calibrated leak threshold or automatic gas alarm
is implemented.

## RTC time and setting

The DS3231 stores **local IST wall-clock time (UTC+05:30), not UTC**, in 24-hour
format. OLED dates use `DD-MM-YYYY`; invalid fields or the oscillator-stop flag
produce TIME INVALID. Normal startup does not overwrite the clock:
`RTC_RESET_PROVISION` must remain `0`.

A fresh, explicit target can be prepared with:

```sh
python3 tools/rtc_request.py 'YYYY-MM-DD HH:MM'
```

Replace the placeholder with a future IST timestamp. The helper generates RAM
payloads; it does not access hardware. Use the exact flashed ELF because mailbox
addresses can change between builds. Follow the [RTC ARM/TRIGGER procedure](tests/README.md#arm-and-trigger-with-existing-st-link--cubeprogrammer):
ARM expires after 60 seconds, and setting requires an explicit trigger. Readback,
advancing seconds and OSF handling are checked. Never reuse old timestamp-specific
firmware. Without backup power, time may need explicit setting again after power
loss. Future CSV timestamps are planned as `YYYY-MM-DD HH:MM` IST; no logger exists.

## Build and validation

1. Import this directory into STM32CubeIDE as an existing project. Its internal
   project name remains **STM32_HW_Test** despite the renamed folder.
2. Build the Debug configuration, including every application module in
   `Core/Src`, especially `hx711.c`, and the ADC HAL sources.
3. The ELF output is **`Debug/STM32_HW_Test.elf`**. Flash through ST-LINK and
   STM32CubeProgrammer only after explicit approval for hardware testing.

`Debug/` is Git-ignored. A fresh checkout needs CubeIDE-generated managed build
files before `make -C Debug -j2 all` works, with the GNU Arm tools on `PATH`.
Do not blindly regenerate application code from the `.ioc`: several working
peripherals are initialized inside USER CODE, and its metadata is incomplete.
Preserve USER CODE and review changes. Generating build files is distinct from
regenerating CubeMX application code.

The last recorded HX711 Debug build used **32,472 bytes Flash** (`text + data`)
and **3,552 bytes static RAM** (`data + bss`, excluding runtime stack/heap).
Commands and coverage are documented in [tests/README.md](tests/README.md) and
[HX711_DIAGNOSTIC.md](HX711_DIAGNOSTIC.md). Tests cover MQ-6 qualification, DEMO
transitions, RTC validation/setting, five-page rendering/bounds and HX711 protocol,
signed values, startup, timeout and recovery. Host tests do not verify wiring or
physical waveform timing.

After an approved flash, observe all five pages, live sensors, advancing RTC
seconds and each LED pattern—including the previously reported yellow-LED issue.
For HX711, confirm raw counts respond to gentle force and settle when undisturbed;
no calibration should be attempted before the mechanical platform is ready.

## Project structure and next steps

| Path | Purpose |
| --- | --- |
| `Core/Src/main.c` | Acquisition, GPIO/ADC initialization and foreground scheduling |
| `Core/Src/sh1106.c`, `Core/Inc/font5x7.h` | OLED transport, framebuffer and font |
| `Core/Src/oled_ui.c` | Five-page presentation layer |
| `Core/Src/mq6_monitor.c` | Experimental filtering and baseline qualification |
| `Core/Src/hx711.c` | Raw load-cell ADC communication and diagnostics |
| `Core/Src/ds3231.c` | RTC decoding, validation and controlled setting |
| `Core/Src/safety_state.c` | Independent DEMO state machine |
| `Drivers/`, `Core/Startup/` | HAL/CMSIS dependencies and MCU startup |
| `tests/`, `tools/` | Host tests and RTC request utility |

[PCB_ASSEMBLY_PLAN.md](PCB_ASSEMBLY_PLAN.md) contains provisional DEVAAJ D-131
100×150 mm perfboard planning with removable Blue Pill sockets. It predates HX711
integration and still describes older PB8/PB9 reservations and power arrangements;
reconcile those sections before using it as an assembly netlist. Exact fit and
continuity/voltage verification remain prerequisites for permanent assembly.
[PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) is historical bring-up context and includes
superseded troubleshooting information.

Next stages are physical HX711 validation, construction of the weighing platform,
then a separately approved calibration workflow. Storage, consumption estimates,
buttons and actuator drivers remain future work; none is implied by the current
raw diagnostics.
