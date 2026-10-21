# LPG Safety and Consumption Monitoring System

An STM32-compatible embedded student prototype for displaying temperature,
raw flame-sensor output, experimental MQ-6 measurements, and local time.
A four-state **DEMO** exercises warning LEDs independently of the sensors.
Cylinder weight and consumption measurement are planned, not implemented.

**This is an experimental prototype, not a certified LPG detector or safety
controller.** MQ-6 readings are not calibrated gas concentrations, and DEMO
states do not represent actual safety assessments.

## Current features

- DS18B20 temperature acquisition with scratchpad CRC validation and diagnostic errors.
- Flame-sensor raw digital HIGH/LOW display; detection polarity is not assumed.
- MQ-6 ADC acquisition, temporal filtering, experimental ambient baseline and deviation.
- Four-page monochrome OLED interface, rotating approximately every five seconds.
- DS3231 date/time readout in Indian Standard Time (IST, UTC+05:30).
- Explicit RTC ARM/TRIGGER setting interface with readback and advancing-seconds checks.
- Non-blocking DEMO state/LED scheduling and a PC13 heartbeat.

OLED, temperature, flame DO, MQ-6 input, warning LEDs and running RTC time have
been physically tested during development. New firmware revisions still require
board testing. MicroSD, HX711/load cell, buzzer and exhaust fan are not integrated.

## Hardware and pin map

The board is an **HW-267 Blue-Pill-style board with an HK K32F103CBT6-compatible
MCU**. The CubeMX compatibility profile is **STM32F103CBTx**; this does not imply
the installed device is a genuine ST part. The configured system clock is 72 MHz.

| Component | Connection | Notes |
| --- | --- | --- |
| SH1106-compatible 128×64 OLED | PB6 SCL, PB7 SDA | I2C1, 7-bit address `0x3C`, 3.3 V |
| DS3231 RTC | PB6 SCL, PB7 SDA | Same I2C1 bus, address `0x68`, 3.3 V |
| DS18B20 | PA0 | 3.3 V; external 4.7 kΩ data pull-up |
| Flame sensor DO | PA1 | Digital input; raw state only |
| MQ-6 AO | PA2 / ADC1_IN2 | Through the divider described below |
| Green LED | PA3 | 220 Ω series resistor, cathode to GND |
| Yellow LED | PA4 | 220 Ω series resistor, cathode to GND |
| Red LED | PA5 | 220 Ω series resistor, cathode to GND |
| Onboard LED | PC13 | Active-low heartbeat |
| Future HX711 | PB8/PB9 | Reserved; not implemented |
| SWD | PA13/PA14 | Reserved for ST-LINK |

I2C1 runs at **100 kHz**, using alternate-function open-drain pins and external
4.7 kΩ pull-ups to 3.3 V. HAL receives each 7-bit device address shifted left by one.

The MQ-6 module uses a separate regulated 5 V supply with common ground:

```text
MQ-6 AO ---- 10 kΩ ----+---- PA2
                      |
                    10 kΩ
                      |
                     GND
```

The divider reduces a nominal 0–5 V AO signal to approximately 0–2.5 V.
MQ-6 DO is disconnected. Do not connect 5 V AO directly to PA2.
The RTC currently has no verified backup battery; continuity is not guaranteed
when its supply is removed. SQW and 32K are unused.

## OLED pages

| Page | Contents |
| --- | --- |
| HOME | LPG MONITOR, IST date/time, temperature, DEMO mode and simulated state |
| MQ-6 GAS MONITOR | Raw and filtered ADC, experimental baseline, delta and status |
| SENSORS | Temperature diagnostics, raw flame DO, weight pending, estimated voltages |
| SYSTEM | RTC status, MicroSD not installed, load cell pending, DEMO/system information |

The display uses the existing 5×7 uppercase font, inverted headings, page numbers
and four small footer markers. The active marker is a filled square; inactive
markers are dots. The full-width bottom separator has been removed. Rendering
uses one 1024-byte framebuffer and sends one composed frame per update.

Page scheduling uses `HAL_GetTick()` without a page-delay loop. The sensor/display
cycle is approximately 1 Hz, so a page transition appears on the next refresh.
OLED transfers remain synchronous. Timing-critical 1-Wire operations use the
existing DWT cycle-counter delays and are separate from display rendering.

## MQ-6 processing and DEMO behavior

ADC1 uses a 12 MHz ADC clock, 239.5-cycle sampling time and one-time ADC
calibration. Each raw update averages 16 conversions. The monitor then applies
an EMA with default weight 1/8. Configuration lives in `Core/Inc/mq6_monitor.h`:

- Software settling interval: 60 seconds.
- Stability qualification: 16 batches spanning at least 15 seconds, with configurable range/drift limits.
- Baseline collection: 32 valid updates after qualification, with range/drift checks across the whole capture.
- Baseline remains fixed until reset; delta is filtered ADC minus baseline.
- Invalid acquisition or ADC-rail samples clear qualification without restarting the minimum warm-up timer.
- Manual baseline recapture is an API only; no physical or automatic trigger is assigned.

This software baseline does not replace manufacturer conditioning/preheating or
gas calibration. Rail checks cannot detect every disconnection or sensor fault.
No ppm, gas percentage, calibrated leak threshold or cylinder quantity is computed.

The DEMO cycles independently through four five-second phases:

| Simulated state | LED indication |
| --- | --- |
| SAFE | Green steady |
| WARNING | Yellow blinking |
| EMERGENCY | Red blinking quickly |
| SENSOR_FAULT | Yellow/red alternating |

Live sensor readings do not drive these states. No buzzer, fan or relay is driven.

## Build and flash

Tools used during development: STM32CubeIDE, its GNU Arm toolchain, ST-LINK and
STM32CubeProgrammer. Host tests additionally use a C compiler and Python 3.

1. Import this directory into STM32CubeIDE as an existing project.
2. The internal project name remains **STM32_HW_Test** even though the folder is
   named `LPG_Safety_and_Consumption_Monitoring_System`.
3. Build the Debug configuration. Let CubeIDE regenerate managed build files for
   the current location; old generated files can retain paths from before renaming.
4. Confirm the build includes all application modules in `Core/Src` and the ADC
   HAL sources. The output is normally `Debug/STM32_HW_Test.elf`.
5. In STM32CubeProgrammer, connect through ST-LINK, download that ELF and verify
   programming. Reset/run the board and inspect the OLED and LEDs.

`Debug/` is ignored by Git. A fresh checkout must generate its build files in
CubeIDE before a command-line `make -C Debug -j2 all` can work. For that command,
`arm-none-eabi-gcc` and the other toolchain binaries must be on `PATH`.

**Do not blindly regenerate application code from the `.ioc`.** Some working
peripherals are initialized in USER CODE, and the `.ioc` metadata does not fully
represent all current hardware assignments. Preserve USER CODE sections and
review generated changes. Building managed makefiles is distinct from CubeMX
application-code regeneration.

## RTC operation and setting

RTC values represent **local IST wall-clock time, not UTC**. OLED dates use
`DD-MM-YYYY`, with 24-hour time. Future CSV timestamps are specified as
`YYYY-MM-DD HH:MM` in IST; no CSV/MicroSD logger exists yet.

Normal startup does not overwrite the clock. Leave `RTC_RESET_PROVISION` at `0`.
Do not flash old timestamp-specific `RTC_SET_*` images or re-enable their expired
targets. OSF or invalid fields produce a time-invalid indication.

The preferred setting method accepts an explicit fresh timestamp through RAM:

```sh
python3 tools/rtc_request.py 'YYYY-MM-DD HH:MM'
```

Replace the placeholder with a real future IST minute. The helper validates it,
finds the mailbox address in the current ELF, and generates separate target,
ARM and TRIGGER payloads. It does not write hardware or obtain a timestamp.
Use the exact ELF currently flashed: mailbox addresses can change after builds.

Follow [the RTC procedure](tests/README.md#arm-and-trigger-with-existing-st-link--cubeprogrammer)
for RAM writes through ST-LINK. ARM expires after 60 seconds. Trigger processing
has approximately one sensor-cycle latency plus host/SWD delay; it is not precise
time synchronization. Firmware verifies register readback and advancing seconds
before clearing OSF. Commands are consumed once, and reset clears the request.
A clock whose OSF is clear is protected from overwrite by this procedure.

After loss of RTC power without a backup battery, supply a new explicit setting
request if needed. Never assume an old provisioning timestamp is still appropriate.

## Source layout

```text
Core/Inc/                 Application headers and font
Core/Src/main.c          Sensor acquisition, GPIO/ADC setup and application loop
Core/Src/sh1106.c        OLED framebuffer and I2C transport
Core/Src/oled_ui.c       Four-page presentation layer
Core/Src/mq6_monitor.c   MQ-6 filtering and experimental baseline
Core/Src/safety_state.c  Independent DEMO state machine
Core/Src/ds3231.c        RTC decoding, validation and controlled setting
Core/Startup/           MCU startup code
Drivers/                STM32 HAL and CMSIS dependencies
STM32_HW_Test.ioc       CubeMX configuration (see regeneration note)
STM32F103CBTX_FLASH.ld  Linker script
tests/                  Host tests and hardware test instructions
tools/                  RTC request payload utility
```

Tests are in `tests/`; utilities are in `tools/`. See
[tests/README.md](tests/README.md) for commands and coverage of DEMO timing,
MQ-6 processing, RTC validation/setting, ARM/TRIGGER and UI framebuffer bounds.
Host tests do not establish physical sensor accuracy or hardware reliability.

`PROJECT_CONTEXT.md` is historical bring-up documentation and contains superseded
OLED/I2C troubleshooting state. Use current source and recent verified hardware
results when continuing development.

## Physical checks after flashing

- Watch all four OLED pages over a full rotation; check spacing and page markers.
- Confirm temperature, raw flame DO and MQ-6 readings continue updating.
- Confirm RTC seconds advance and the displayed date/time is IST.
- Observe PC13 heartbeat and all four independent DEMO LED patterns.
- Confirm absent hardware stays labeled PENDING or NOT INSTALLED.

Planned work includes HX711 weight/consumption measurement, storage and other
outputs only after their hardware is connected and individually validated.
