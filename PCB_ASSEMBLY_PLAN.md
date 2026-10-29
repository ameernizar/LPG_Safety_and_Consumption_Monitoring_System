# Permanent perfboard planning package

**MCU identity correction (29 October 2026):** owner-confirmed marking is
HK STM32F103C8T6; CubeIDE target STM32F103C8Tx, LQFP48, nominal 64 KB Flash and
20 KB SRAM. HK denotes compatible hardware, not verified genuine ST silicon.
Actual characteristics require verification; do not assume extra Flash.
This identity correction does not change the physical footprint or pin plan.

**Project:** LPG Safety and Consumption Monitoring System  
**Audit date:** 18 October 2026  
**Target:** DEVAAJ D-131 universal dot perfboard, approximately 100 × 150 mm; removable HW-267 board  
**Status:** PLANNING ONLY — NOT RELEASED FOR SOLDERING

The physical prototype is reported working. Female header sockets are not yet
available. Do not solder the MCU board directly to the perfboard. This document
specifies electrical intent and a conditional assembly sequence, not instructions
to begin physical assembly today. It is not a manufactured-PCB Gerber package.
Keep the working breadboard circuit unchanged during this planning stage.

This is an experimental student prototype, not a certified LPG safety system.
The warning states are simulated DEMO states, not validated leak assessments.

## 1. Evidence and scope

VERIFIED means firmware inspection plus user-reported working wiring, not a fresh
meter measurement by this audit. RESERVED means an existing project commitment.
PROPOSED means a future allocation with no implementation or physical verification.
AVAILABLE means unused by the inspected application, subject to board-level checks.

| Evidence | Finding |
| --- | --- |
| `STM32_HW_Test.ioc` | LQFP48 STM32F103C8Tx profile; PB6/PB7 I2C1, PA13/PA14 SWD, PC13 output, PD0/PD1 HSE |
| `Core/Src/main.c:134` — `OneWire_Init()` | PA0 output open-drain, no internal pull; released HIGH; DWT timing |
| `Core/Src/main.c:346` — LED service | PA3/PA4/PA5 active-high DEMO outputs |
| `Core/Src/main.c:443` — `MQ6_Init()` | PA2 analog, ADC1 channel 2, ADC clock PCLK2/6 = 12 MHz, 239.5-cycle sampling, calibration |
| `Core/Src/main.c:717` — `SystemClock_Config()` | HSE, PLL ×9, SYSCLK 72 MHz; APB1 /2, APB2 /1 |
| `Core/Src/main.c:755` — `MX_I2C1_Init()` | 100 kHz, 7-bit addressing |
| `Core/Src/main.c:795–827` — GPIO initialization | GPIO clocks; PC13 output; PA3–PA5 push-pull; PA1 input NOPULL |
| `Core/Src/stm32f1xx_hal_msp.c:76,98–109` | JTAG disabled but SWD retained; GPIOB/I2C1 clocks; PB6/PB7 AF open-drain |
| `Core/Inc/sh1106.h`, `Core/Inc/ds3231.h` | OLED 0x3C, RTC 0x68; HAL addresses shifted left one bit |
| `Core/Src/oled_ui.c`, `mq6_monitor.c`, `safety_state.c` | Display/processing only; no extra hardware pins |
| `Core/Src/stm32f1xx_it.c` | SysTick; no active SPI, timer, UART or DMA peripheral ISR |
| HAL configuration and application search | ADC/GPIO/I2C active; no SPI/TIM/UART application initialization |
| `README.md`, current user-confirmed hardware | Pull-ups, divider, LED resistors, external MQ-6 supply |

Line numbers are audit locators and may move. Older `.bak` files and
`PROJECT_CONTEXT.md` describe superseded bring-up states, not current wiring.
Vendor library pin definitions do not mean a peripheral is active. The external
SRAM GPIO code in `system_stm32f1xx.c` is conditional code for other profiles,
not an active GPIO assignment for this build.

**Metadata discrepancy:** `.ioc` omits PA0–PA5/ADC1 application setup and displays
`RCC.ADCFreqValue=36000000`. Actual code selects and checks 12000000 Hz. Do not
regenerate firmware from that file as part of assembly. No firmware or `.ioc`
changes are authorized by this planning document.

## 2. Master GPIO allocation

The actual chip is HK-marked STM32F103C8T6, not assumed genuine STM32. Names
below are MCU/board signal labels, **not socket pin numbers**. Do not infer header
orientation from this table. Identify each socket position from the actual board.

| Pin | Current use/status | Permanent-board treatment |
| --- | --- | --- |
| PA0 | VERIFIED DS18B20, GPIO open-drain | DS data net; preserve external 4.7 kΩ |
| PA1 | VERIFIED flame DO, input NOPULL | Raw digital signal; no polarity assumption |
| PA2 | VERIFIED MQ-6 ADC1_IN2, analog | Divider midpoint only |
| PA3 | VERIFIED green LED, push-pull | Individual 220 Ω resistor |
| PA4 | VERIFIED yellow LED, push-pull | Individual 220 Ω resistor |
| PA5 | VERIFIED red LED, push-pull | Individual 220 Ω resistor |
| PA6 | Unused; PROPOSED buzzer control | Future driver input only; do not wire a buzzer directly |
| PA7 | AVAILABLE | Spare, no permanent connection proposed |
| PA8 | Unused; PROPOSED fan control | Future driver input only |
| PA9 | AVAILABLE | Preserve as possible future serial TX |
| PA10 | AVAILABLE | Preserve as possible future serial RX |
| PA11 | No firmware use; board USB reservation | Leave untouched; USB-connected hardware may exist |
| PA12 | No firmware use; board USB reservation | Leave untouched; inspect onboard pull-up/network |
| PA13 | VERIFIED SWDIO | Programming access, never reassign |
| PA14 | VERIFIED SWCLK | Programming access, never reassign |
| PA15 | AVAILABLE after JTAG disable | Spare; consider reset/debug behavior before any use |
| PB0 | Unused; PROPOSED UP button | Future active-low input, external pull-up proposed |
| PB1 | Unused; PROPOSED DOWN button | Same |
| PB2 | BOOT1-related board connection | Exclude from general allocation; preserve boot straps |
| PB3 | AVAILABLE after JTAG disable | Spare; keep free if trace output is needed |
| PB4 | AVAILABLE after JTAG disable | Spare; reset debug behavior must be considered |
| PB5 | AVAILABLE | Spare |
| PB6 | VERIFIED I2C1 SCL | OLED + RTC + one shared pull-up network |
| PB7 | VERIFIED I2C1 SDA | OLED + RTC + one shared pull-up network |
| PB8 | RESERVED HX711 pair | PROPOSED DOUT input; direction not previously fixed |
| PB9 | RESERVED HX711 pair | PROPOSED PD_SCK output; direction not previously fixed |
| PB10 | Unused; PROPOSED SELECT button | Future active-low input |
| PB11 | Unused; PROPOSED BACK button | Future active-low input |
| PB12 | Unused; PROPOSED MicroSD CS | GPIO chip select, default HIGH in future design |
| PB13 | Unused; PROPOSED SPI2 SCK | Short route to future SD connector |
| PB14 | Unused; PROPOSED SPI2 MISO | SD DO to MCU |
| PB15 | Unused; PROPOSED SPI2 MOSI | MCU to SD DI |
| PC13 | VERIFIED onboard active-low LED | No external warning LED or load attached |
| PC14 | Not used in firmware; conditional board oscillator pin | Hold unused until onboard 32 kHz crystal checked |
| PC15 | Not used in firmware; conditional board oscillator pin | Same |
| PD0/OSC_IN | VERIFIED HSE board crystal connection | Do not route to accessories |
| PD1/OSC_OUT | VERIFIED HSE board crystal connection | Do not route to accessories |

Also preserve **NRST**, **BOOT0**, all board ground/3.3 V/USB supply connections
and existing boot jumpers. MCU VBAT is not the DS3231 module battery input.
PC0–PC12 and other pins absent from this LQFP48 profile are not spare header pins.

### Peripheral conflicts and reservations

Reference-profile alternatives: PA0–PA3 share TIM2 channels and ADC inputs;
PA4/PA5 are SPI1 NSS/SCK and ADC inputs. PA6/PA7 offer TIM3 CH1/CH2,
SPI1 MISO/MOSI and ADC6/7. PB0/PB1 offer TIM3 CH3/CH4 and ADC8/9.
PA8 offers TIM1 CH1/MCO. PA9/PA10 offer USART1 TX/RX. PB10/PB11 share
I2C2/USART3 and remapped TIM2. PB12–PB15 provide SPI2 NSS/SCK/MISO/MOSI;
PB13–PB15 also overlap TIM1 complementary outputs. PB6–PB9 share TIM4;
remapped I2C1 uses PB8/PB9. PA15/PB3/PB4/PB5 overlap remapped SPI1 or timers.
PA11/PA12 overlap USB/CAN. These are reference-chip capabilities, not verified
HK peripheral behavior. [ST pin-function reference, Table 5](https://www.st.com/resource/en/datasheet/stm32f103cb.pdf).

Design consequences:

- Do not move I2C1 onto the HX711 pair. Keep PB6/PB7 unchanged.
- SPI1 default wiring conflicts with existing warning LEDs. Reserve SPI2 instead.
- Buttons on PB10/PB11 sacrifice that I2C2/USART3 option; confirm this tradeoff.
- Separate proposed buzzer/fan timer peripherals avoid forcing one shared PWM
  period. No PWM/timer configuration exists yet; hardware timer clocks/settings
  require future validation. No remap is proposed now.
- DWT provides current DS18B20 timing; it does not consume a general-purpose timer.
- Current HAL timebase is SysTick. Keep all new interrupts away from tight 1-Wire
  timing until reviewed. Buttons can initially be polled; no EXTI commitment.
- Leaving pins unused in software does not guarantee an unloaded physical board pin.

## 3. Power architecture — verified baseline and proposed alternative

### A. VERIFIED current breadboard arrangement — leave unchanged

```text
Independent USB source -> Blue Pill Micro-USB -> onboard regulator -> 3V3_LOGIC
                                                               -> present sensors
External regulated 5 V, 1 A adapter -> 5V_MQ -> MQ-6 module/heater only
USB/board GND ------------------ GND_COMMON ---------------- adapter negative
```

**In this verified TWO-SUPPLY arrangement, never join 5V_MQ to Blue Pill 5 V,
USB VBUS, 3V3_LOGIC, or ST-LINK power.** This remains the current wiring; the
single-source alternative below is not permission to bridge these existing rails.
The common ground is deliberate; the two independent positive 5 V rails are not
joined. MQ-6 heater supply/return runs as its own pair from the adapter connector,
not through the Blue Pill regulator, header ground segment, or thin analog return.
Use a ground junction near the power entry: high-current return goes directly
there; the quiet logic/analog reference branches from it. It is one electrical
GND net, not disconnected ground islands.

Present 3V3_LOGIC comes from the USB-powered board's regulator and serves OLED,
RTC, DS18B20 and flame module. Confirm its actual voltage and regulator capacity
before adding HX711 or SD. No regulator current budget can be inferred from the
compatible MCU marking. Reserve an optional future 3.3 V supply area, but do not
parallel another regulator onto 3V3_LOGIC without a separate power design.

When programming, retain the proven ST-LINK wiring. Verify whether the programmer's
3.3 V pin is a sense input or supply output before connecting it. Do not introduce
a second board supply through ST-LINK. Provide GND/SWDIO/SWCLK/NRST access.

No independent load supply is assigned for the future fan/buzzer yet. Their
supply pins remain NC. Current ratings, driver devices, protection and wiring
must be chosen after the loads are identified. Do not assume the MQ adapter has
spare capacity. Reserve a separate load-return route and connector area.

### B. PROPOSED single-adapter permanent supply — NOT VERIFIED

One regulated 5 V adapter would be the **sole normal operating supply**. Its
protected, switched output would split into two parallel branches. This is a new
power topology, not an instruction to connect the existing two positive sources.
The current MQ adapter is rated 5 V, 1 A; reusing it is only a candidate until the
complete current budget and under-load voltage are checked.

```text
REGULATED 5 V ADAPTER (+)
        |
   keyed input / polarity label
        |
   F1 input fuse or resettable protection (selection pending)
        |
   reverse-polarity protection (low-drop device, selection pending)
        |
   SW1 rated DC master power switch
        |
        +---- 5V_SYSTEM ----+---- 5V_MQ_BRANCH ---> MQ-6 VCC
                           |
                           +---- 5V_LOGIC_IN ---> VERIFIED U1 5 V input
                                                 onboard regulator
                                                       |
                                                   3V3_LOGIC
                                       MCU + existing low-power modules

ADAPTER (-) ---- GND_STAR ---- dedicated MQ-6 heater/module return
                    +-------- Blue Pill / logic / analog reference return
```

Both branches share one protected source; neither routes heater current through
the Blue Pill regulator or its header-ground path. Branch labels distinguish
routing/current flow, not galvanic isolation. GND_STAR is a distribution junction,
not a separate electrical ground. PA2's divider ground remains referenced to MCU
ground, with the heater current kept out of that quiet return segment.

**No connection to the U1 5 V header is authorized yet.** The HW-267 path must be
confirmed on the ACTUAL board, not inferred from a generic Blue Pill schematic:

- Identify the regulator marking, pinout, supported input voltage, dropout,
  thermal limits and onboard decoupling requirements.
- With all sources disconnected, trace the labeled 5 V pin to regulator input
  and USB VBUS; identify any diode, fuse or direct copper link and its direction.
- Confirm 3.3 V output and ground pin identities; inspect polarity and continuity.
  A diode-mode/continuity observation alone does not establish reverse-current
  blocking or a safe maximum load.
- After unpowered checks, measure the regulator input/output in a controlled test
  under representative load. Verify that the selected input does not bypass a
  required protection device. Record the board-specific connection drawing.

The proposed low-voltage input protection consists of a keyed/labeled connector,
F1 and reverse-polarity protection before SW1/distribution. Choose the fuse/PTC
from measured operating/inrush current, adapter fault behavior and wire/connector
ratings, not simply its 1 A label. Choose SW1 for DC voltage/current and inrush.
Allow service access to the fuse/switch in the bottom power band. Optional eFuse,
overvoltage clamp or transient suppression requires component selection and
coordination with the source/fuse; no unqualified TVS part is specified.

Protection voltage drop matters: a simple series diode may reduce the heater
voltage too much. Select a low-loss protection topology and measure the actual
voltage at both branch endpoints. Do not raise adapter voltage to compensate
without checking every connected device. Input/local capacitors must meet the
identified regulator's requirements and not introduce excessive inrush.

#### Current budget and voltage acceptance — incomplete, no 1 A approval yet

| Load/path | Budget evidence required |
| --- | --- |
| MQ-6 heater | Reference MQ-6 device data lists up to 950 mW: about 190 mA at nominal 5 V; actual sensor identity/startup current must be checked |
| MQ breakout extras | Comparator, indicator LEDs and sensing circuit current: measure actual board; not included automatically in heater figure |
| Blue Pill | MCU at 72 MHz, board LEDs and regulator quiescent current: measure |
| OLED + RTC + DS18B20 + flame module | Measure combined worst operating load, including display content and conversion activity |
| Three warning LEDs | Account for resistor/forward-voltage current and possible initialization/test patterns, not only average DEMO current |
| Future HX711 / MicroSD | Not approved for this budget; add excitation demand and SD transient peaks when selected |
| Future buzzer / fan | Excluded from current proposal; require a revised supply/driver budget including starting/stall demand |

The heater figure is a manufacturer reference, **not a measurement or identity
confirmation for the Flying Fish module**. [Winsen MQ-6 specifications](https://www.winsen-sensor.com/product/mq-6.html).

Calculate `I_ADAPTER = I_MQ_MODULE + I_BLUEPILL_5V_BRANCH + I_PROTECTION`.
Measure the Blue Pill branch with its downstream 3.3 V loads attached; do not
count those loads twice. For an identified linear regulator, output current is
approximately input load current, and thermal dissipation is approximately
`(V_IN - 3.3 V) × I_3V3 + quiescent losses`. At nominal 5 V this is about
`1.7 × I_3V3` watts; acceptable dissipation still depends on the actual regulator
and board cooling. A 1 A adapter does not make the onboard regulator a 1 A supply.

Provisional design target: allow at least 25% headroom over measured worst-case
continuous demand (thus at most about 0.8 A continuous for a qualified 1 A source),
AND separately verify startup/transient capability. This is a planning margin,
not proof of suitability. Existing information is insufficient to total the
budget or approve the 1 A adapter.

Measure adapter output, post-protection rail, MQ VCC, regulator input and 3V3_LOGIC
under simultaneous worst-case operation, not only unloaded. Include cable and
switch losses, rail dips on startup, regulator temperature and evidence of MCU
resets. Compare readings against actual component limits. A multimeter establishes
DC level/polarity but can miss short dips/noise; use additional waveform/current
measurement if instability or transient-load uncertainty remains.

#### USB / ST-LINK arrangements

| Situation | Permitted planning arrangement after verification |
| --- | --- |
| Normal standalone operation | Single adapter on; Blue Pill Micro-USB disconnected; no programmer supply tied to rails |
| SWD programming with adapter | Adapter powers target; ST-LINK GND/SWDIO/SWCLK and optional NRST; genuine target-voltage reference connected only after pin function confirmed |
| USB-powered maintenance instead | Physically isolate adapter branch from U1 first; disconnect MQ power/signal as needed to prevent unpowered-input feed; never rely on switch OFF alone as source isolation |
| Simultaneous USB and adapter desired later | Requires a verified source selector/power mux or suitable reverse-blocking arrangement; no such circuit is currently approved |

A Blue Pill 5 V header may be directly linked to USB VBUS. Do not insert a live
USB cable while the adapter powers that rail until isolation is proven. An OFF
master switch can still leave downstream USB-to-MQ backfeed paths, so it is not
a source selector. A charge-only USB cable still carries VBUS and is not an
isolation solution. No USB data function is needed for existing SWD programming.

ST-LINK model/pinout must be checked: genuine target-reference pins and clone
3.3 V POWER OUTPUT pins are not interchangeable. Do not parallel programmer 3.3 V
or 5 V outputs with the target rails. Use the specific programmer manual and
verify its electrical function. [ST-LINK/V2 manual](https://www.st.com/resource/en/user_manual/um1075-stlink-v2-in-circuit-debuggerprogrammer-for-stm8-and-stm32-stmicroelectronics.pdf).
Avoid driving SWD into an unpowered target; establish the agreed power sequence.

#### Netlist delta — PROPOSED ONLY

- New `5V_ADAPTER_IN`: adapter positive to input protection.
- New `5V_SYSTEM`: protected SW1 output, branches to `5V_MQ_BRANCH` and `5V_LOGIC_IN`.
- `5V_MQ_BRANCH`: MQ module VCC, with dedicated wiring; not through U1.
- `5V_LOGIC_IN`: qualified Blue Pill 5 V input, only after the above audit.
- `GND_COMMON`: all existing grounds retained; dedicated MQ return to GND_STAR.
- `3V3_LOGIC`, `MQ_AO_RAW`, `MQ_ADC`, and both existing 10 kΩ divider resistors:
  unchanged. No I2C/DS pull-up is moved to 5 V.
- USB VBUS and programmer outputs are excluded as normal power sources.

One adapter reduces independent-supply misuse but does not guarantee safe power
sequencing: the MCU regulator can start later or a logic branch can disconnect
while MQ remains energized. Review PA2 backfeed during startup/shutdown and branch
faults before approving the permanent input circuit. Keep the divider; it is not
by itself complete unpowered-input protection.

**Transition hold:** keep the two-supply breadboard exactly as it is. No bridging,
rewiring, soldering or first power-up of the proposed topology is authorized.
A multimeter remains unavailable, so the required voltage, polarity and rail
continuity checks cannot yet be completed. Record results and resolve the power
path/current budget before selecting a final schematic or physical routing.

### Existing passive components

| Reference for this plan | Value | Nodes | Status |
| --- | --- | --- | --- |
| R_SCL | 4.7 kΩ | 3V3_LOGIC ↔ I2C_SCL | VERIFIED external pull-up |
| R_SDA | 4.7 kΩ | 3V3_LOGIC ↔ I2C_SDA | VERIFIED external pull-up |
| R_DS | 4.7 kΩ | 3V3_LOGIC ↔ DS_DATA | VERIFIED |
| R_MQ_TOP | 10 kΩ | MQ_AO_RAW ↔ MQ_ADC | VERIFIED |
| R_MQ_BOTTOM | 10 kΩ | MQ_ADC ↔ GND_COMMON | VERIFIED |
| R_GREEN | 220 Ω | LED_G_GPIO ↔ LED_G_ANODE | VERIFIED |
| R_YELLOW | 220 Ω | LED_Y_GPIO ↔ LED_Y_ANODE | VERIFIED |
| R_RED | 220 Ω | LED_R_GPIO ↔ LED_R_ANODE | VERIFIED |

OLED/RTC breakouts may already contain pull-ups. Inventory their values and supply
connections: parallel resistors reduce effective resistance. Do not add a second
external pair per module. Keep this entire shared bus pulled to 3.3 V, never
5V_MQ. The RTC module's own level/pull-up/battery circuits must be checked rather
than inferred from the chip. [DS3231 device reference](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf).

The nominal divider gain is 1/2; 5 V AO gives about 2.5 V at PA2. Actual voltage
requires measurement. ADC scaling uses nominal 3.3 V, not a measured reference.
Avoid MQ power ON with the MCU unpowered: AO can feed an unpowered input through
the divider. Independent-supply sequencing/backfeed protection is an unresolved
permanent-design item. During supervised testing keep logic powered before the
MQ signal is energized and remove MQ power before logic power.

## 4. Verified TWO-SUPPLY netlist — electrical node membership

This remains the current breadboard netlist. Section 3B gives a separate PROPOSED
single-source delta; do not merge the two architectures physically.

U1 denotes the complete removable Blue Pill module, not a bare MCU. J names below
are proposed carrier connector identifiers; signal assignments reproduce verified
wiring. Numbers describe the NEW connector convention, not existing module order.

| Net | All required endpoints |
| --- | --- |
| GND_COMMON | U1 GND; OLED GND; RTC GND; DS black/GND; flame GND; MQ GND; external adapter negative; R_MQ_BOTTOM low end; all three LED cathodes; ST-LINK GND when attached |
| 3V3_LOGIC | U1 3.3 V output; OLED VCC; RTC VCC; DS red/VCC; flame VCC; R_SCL/R_SDA/R_DS supply ends |
| USB_5V_LOCAL | USB input and U1 onboard power path only; no carrier connection to 5V_MQ |
| 5V_MQ | External adapter positive; MQ module VCC only |
| I2C_SCL | U1 PB6; OLED SCL; RTC SCL; R_SCL signal end |
| I2C_SDA | U1 PB7; OLED SDA; RTC SDA; R_SDA signal end |
| DS_DATA | U1 PA0; DS yellow/data; R_DS signal end |
| FLAME_DO | U1 PA1; flame DO |
| MQ_AO_RAW | MQ module AO; R_MQ_TOP high end |
| MQ_ADC | U1 PA2; R_MQ_TOP low end; R_MQ_BOTTOM high end; optional test point |
| LED_G_GPIO | U1 PA3; R_GREEN input end |
| LED_G_ANODE | R_GREEN output end; green LED anode |
| LED_Y_GPIO | U1 PA4; R_YELLOW input end |
| LED_Y_ANODE | R_YELLOW output end; yellow LED anode |
| LED_R_GPIO | U1 PA5; R_RED input end |
| LED_R_ANODE | R_RED output end; red LED anode |
| SWDIO | U1 PA13; ST-LINK SWDIO only |
| SWCLK | U1 PA14; ST-LINK SWCLK only |
| RESET | Existing U1 NRST/reset circuit; optional accessible ST-LINK NRST |

**No-connect:** MQ DO; RTC SQW and 32K; all future supply/driver/load nets until
approved. Leave the RTC battery holder empty for this stage. Board crystals,
regulator, PC13 LED and boot resistors remain on U1, not duplicated on the carrier.

### Carrier connector plan

| ID | Proposed contact order (label pin 1 explicitly) | Wiring class |
| --- | --- | --- |
| J_OLED, J_RTC (each 4) | 1 GND, 2 3V3, 3 SCL, 4 SDA | Verified nets; harness order must match actual module labels |
| J_DS (3) | 1 GND, 2 3V3, 3 DATA | Verified nets; do not rely on color alone during transfer |
| J_FLAME (3) | 1 GND, 2 3V3, 3 DO | Verified nets |
| J_MQ_POWER (2) | 1 GND, 2 5V_MQ | Verified separate adapter feed |
| J_MQ_MODULE (3) | 1 GND, 2 5V_MQ, 3 AO_RAW | Verified nets; DO excluded |
| J_LED (6), if panel mounted | G drive, G ground, Y drive, Y ground, R drive, R ground | Each drive downstream of its own 220 Ω resistor |
| J_SWD | Labeled GND, SWDIO, SWCLK, NRST; optional verified reference | Reproduce actual ST-LINK connector order, not guessed numbering |

Label supply voltage on every connector. Use keyed/clearly differentiated
connectors where feasible so the 5 V MQ harness cannot be mistaken for a 3.3 V
sensor harness. Do not mechanically force existing module connectors to match
this proposed order; make a documented adapter harness instead.

## 5. Proposed future netlist and space reservations — NOT populated yet

| Connector/area | Proposed signals | Unresolved conditions |
| --- | --- | --- |
| J_HX (4) | GND, logic-compatible supply, PB8=DOUT, PB9=PD_SCK | Confirm board voltage/logic, exact pin order, excitation/regulator arrangement |
| J_LOAD (4 + optional shield) | HX E+, E−, A+, A−; shield separate if used | Full-bridge leads identified by datasheet/measurement, never color assumptions; no direct MCU connection |
| J_SD (6 + optional CD) | GND, qualified supply, PB13=SCK, PB14=MISO/DO, PB15=MOSI/DI, PB12=CS | Select 3.3 V compatible socket/module, supply capacity, level shifting, card-detect pin if needed |
| J_BUZZ_CTRL | PA6 control, GND reference, future supply NC | Driver circuit and buzzer type/current not selected |
| J_FAN_CTRL | PA8 control, GND reference, future supply NC | Fan type/current, PWM compatibility and stall current unknown |
| J_BUTTONS (5) | GND, PB0 UP, PB1 DOWN, PB10 SELECT, PB11 BACK | Final key naming/connector orientation pending |

Proposed buttons each connect one input to GND when pressed; reserve four 10 kΩ
pull-ups to 3V3_LOGIC and software debounce. No buttons are configured by current
firmware. Do not assume a four-pin tactile switch's internal pairs without checking.

Future drivers need default-OFF bias independent of firmware during reset. Reserve
space for the switching device, control resistor, off-bias resistor, supply/load
connector and load-appropriate suppression/decoupling. A generic low-side stage
is only a placement concept: no transistor, gate/base resistor, diode or fan
supply value is finalized. Never power a load directly from PA6/PA8. Magnetic
loads need suppression selected for the actual load; not every fan is a simple
coil or supports supply PWM. No mains switching is planned.

Reserve local bypass footprints (for example 100 nF ceramic near logic connectors)
and bulk-capacitor space, but determine population from module decoupling and
supply tests. No large capacitor is specified blindly for a regulator of unknown
stability. SD transient current and load-cell excitation must enter the budget.

## 6. DEVAAJ D-131 physical information and provisional layout

### Supplied physical observations

These observations were supplied by the user from ruler photographs; this update
has not independently measured the board or inspected those photographs.

| Item | Current information | Confidence/release condition |
| --- | --- | --- |
| Perfboard | DEVAAJ D-131 universal dot perfboard | User-reported model |
| Overall size | Approximately 100 mm × 150 mm | Planning envelope, not an exact fabrication dimension |
| Hole pitch | Apparently 2.54 mm | Confirm using a real header test fit |
| Grid | Approximately 39–40 columns and 58–60 rows | Exact usable counts remain unverified |
| Copper | Individual solder pads plus elongated edge pads | Edge-pad interconnections must be checked; not assumed to be power rails |
| MCU board | HW-267 Blue-Pill-style board with HK-marked STM32F103C8T6 | Existing working module; STM32F103C8Tx compatibility profile |
| MCU board dimensions | Approximately 53 mm long × 23 mm wide | User-supplied ruler/photo estimate; verify clearance during test fit |
| Header count | 20 pins on each side, 40 total | User-confirmed count |
| Header pitch | Nominal 2.54 mm | Confirm socket/perfboard fit |
| Header-row spacing | Approximately 17.78 mm centre-to-centre (7 × 2.54 mm intervals) | PROVISIONAL; physical verification required |
| Female sockets | Two removable 1×20 female socket headers, nominal 2.54 mm pitch; not yet purchased | No soldering or direct-solder substitute; verify height and fit |
| Digital multimeter | Not available | Continuity/polarity/voltage verification cannot yet be completed |

The apparent 60-row upper estimate must be recounted: 60 hole centers at 2.54 mm
pitch span 59 × 2.54 = 149.86 mm before edge margins. It must not be treated as
60 usable rows within a nominal 150 mm board. Likewise, nominal board dimensions
are not interchangeable with the hole-center rectangle.

### Orientation and access convention

Use a **portrait component-side/top view: approximately 100 mm wide and 150 mm
high**, corresponding to the reported shorter column count and longer row count.
The 100 mm upper edge is the display/user-interface edge; the lower 100 mm edge
is the power/load connector edge. Copper is viewed from the reverse side, so an
underside drawing will need mirroring. No silk/logo orientation, mounting-hole
position or exact photograph handedness is inferred from the text description.
Mark a temporary top/left reference during the later dry fit before routing.

Use **two removable 1×20 female socket headers at nominal 2.54 mm pitch**, matching
the user-confirmed 20 pins per side. The module envelope is approximately
**53 mm long × 23 mm wide**. The provisional row-center separation is
**17.78 mm**, equivalent to **seven 2.54 mm hole intervals**; this is not a
verified socket footprint. Seven intervals means six intervening hole centers,
not seven empty holes between the socket rows.

Each 20-contact row spans nominally 19 × 2.54 = **48.26 mm** between its first
and last contact centers. This differs from the board length and socket-body
length. These relative dimensions do not assign any exact perfboard coordinates.
Verify both rows simultaneously with the actual board and purchased sockets in
an unpowered test fit before fixing socket placement. Do not force or bend the
headers to accommodate a mismatch. Keep the underside of U1 clear of raised
wiring, and leave removal clearance along both rows.

The existing 40 × 75 mm MCU/service-bay reservation remains provisional around
the 23 × 53 mm board envelope. Socket plastic, USB plug/cable, SWD connector,
RESET access and BOOT-jumper access require additional clearance. Check all of
these with U1 inserted; the board outline alone does not establish a usable fit.

### Provisional modular zoning (not a hole-coordinate drawing)

The following envelopes divide the nominal 100 × 150 mm board for paper planning.
They are adjustable space reservations, not confirmed module footprints. Allow
edge/mounting clearances within them; do not fill the entire nominal rectangle.

| Region | Approximate planning envelope | Contents/access |
| --- | --- | --- |
| Upper-left user interface | 55 mm wide × 45 mm high | OLED/RTC connectors, three warning LEDs or panel connector, four future buttons in a 2×2 group; actual button spacing pending |
| Upper-right access corridor | 40 mm wide × 35 mm high | Clear USB cable approach from top edge; accessible SWD harness route; no tall modules blocking either |
| Center-right MCU/service bay | 40 mm wide × 75 mm high | Removable HW-267, socket rows, RESET and BOOT jumper finger/tool access; orientation finalized by dry fit |
| Middle-left quiet bay | 40 mm wide × 35 mm high | Future HX711 module and bridge connector/strain relief; full load cell stays external |
| Middle signal/connector corridor | Flexible gap between MCU and quiet bay | DS18B20 and flame connectors; MQ divider immediately adjacent to PA2 side, with a quiet ground reference |
| Lower-middle expansion bay | About 45 mm wide × 25 mm high | Future MicroSD SPI2 module/connector with card access toward left edge; shift toward PB12–PB15 after fit |
| Bottom power/driver band | About 100 mm wide × 30 mm high | Separate MQ supply entry/return, ground junction, future buzzer and fan driver islands plus load connectors |

These envelopes use most of the board: connector bodies, mounting holes or larger
modules may require moving OLED/RTC, buttons or drivers to a panel/daughterboard.
Do not claim a guaranteed fit from this sketch. Preserve a routing gap between
zones instead of squeezing parts beneath the MCU. The lower driver band remains
unpopulated until the load and supply choices are known.

```text
             100 mm (approx.) — COMPONENT SIDE
       USER / DISPLAY EDGE                  USB entry
 +--------------------------------------------------+
 | OLED + RTC plugs   LED G/Y/R | clear USB corridor|
 | UP DOWN SELECT BACK (2x2)    | SWD cable access  |
 |------------------------------|                   |
 | HX711 / bridge plug          | removable HW-267  |
 | quiet analog area            | socket + service  |
 |                              | RESET / BOOT free |
 | DS + flame harness corridor  |                   | 150 mm
 | divider near actual PA2 side |                   | approx.
 | SD bay -> card access left   | short SPI2 routing|
 |                              |                   |
 | future buzzer / fan drivers  | MQ power entry    |
 | load connectors & returns    | ground junction   |
 +--------------------------------------------------+
               POWER / LOAD EDGE
 MQ-6 heater module remains external, thermally separated.
 Dashed/solid sketch divisions are zones, NOT copper tracks.
```

**Socket orientation preference:** put the USB end toward the upper edge and keep
a clear plug approach. If the actual module pin-side layout makes this conflict
with a short SPI2/divider route, rotate/rearrange the bays during an unpowered dry
fit. Do not move electrical pin assignments to force the drawing to fit. Keep the
reset button and BOOT jumpers reachable while U1 is inserted; locate SWD access
based on its actual end/side connector, not a generic board photograph.

OLED and RTC are reserved as connector-accessible modules; their physical mounting
may be on a panel or external standoffs. Keep the RTC away from the regulator and
MQ heater. The DS18B20 probe and flame sensor remain external sensing heads; their
connector space does not imply burying the sensors next to heat sources. Reserve
bridge cable strain relief at the quiet edge, and button/LED harness strain relief
at the user edge.

Keep HX711 differential bridge wiring away from SD clocks, heater return and
future switched-load paths. Keep SPI2 traces short with nearby ground returns;
no high SPI clock is committed. The divider belongs near the actual PA2 socket
contact, even if its zone must move. Route the MQ heater power/return as a dedicated
pair directly to its connector and power junction. Keep the existing two positive 5 V
sources separate under Section 3A. Reserve input protection/switch space for the
Section 3B proposal without implementing its common positive-source split yet.

**Elongated edge pads:** reserve them as unassigned copper until their electrical
connectivity and clearances are measured. Do not treat them as common ground or
supply buses based on appearance. They may be useful later for connector anchoring
or short local distribution, but only after the net assignment is documented.

No exact solder-hole coordinates, numbered header-to-hole map, or underside wire
routing is released. The next mechanical deliverable requires exact usable row/
column counts, measured header row spacing, actual sockets, module footprints,
mounting clearances and an unpowered fit check. Keep the breadboard intact.

## 7. Conditional assembly sequence — planning only

**Do not start today.** Soldering is not authorized until the socket test fit and
required pre-assembly continuity, polarity and supply-voltage verification can be
performed and has passed; first power-up
requires the applicable Section 8 checks to pass. This is the intended work order
after sockets, mechanical
measurements, equipment and unresolved electrical decisions are available:

1. Freeze the connector drawing and power/ground plan; verify the socket footprint
   by an unpowered dry fit. Record top view, underside mirror and pin-1 marks.
2. Plan mounting/strain relief and low-profile passive placement before tall parts.
3. Schedule socket/connector installation, then passive components and the quiet
   signal wiring; maintain access for inspection underneath.
4. Schedule the separate supply/ground paths and test points; check each net before
   installing the next module. No MCU/module is fitted during continuity work.
5. Complete inspection and the verification gates below before powering any module.
6. Bring up the existing logic and modules in stages, then transfer the known-good
   firmware behavior. Leave all proposed future areas unpopulated.

Detailed soldering technique, hole-by-hole routing and powered assembly steps
are intentionally deferred. A universal perfboard has no assumed electrical
connections between adjacent holes; every intended connection must be recorded.

## 8. Verification gates before release to powered operation

A multimeter/continuity and voltage measurement capability is REQUIRED. The user
confirms a digital multimeter is not yet available. Do not substitute GPIO blink
tests for supply checks. The permanent assembly is NOT cleared for power.

### Unpowered gate (all supplies and ST-LINK disconnected)

- Verify actual socket labels/orientation against U1, including power and ground.
- Check every netlist connection end-to-end and check neighboring pads for shorts.
- Confirm resistor values, LED polarity, connector polarity and capacitor polarity.
- Check there is no low-resistance short between 3V3_LOGIC/GND, 5V_MQ/GND,
  or, for the current two-supply topology, the independent positive supplies.
  In a future approved single-source design the two branch positives intentionally
  meet at 5V_SYSTEM; USB/programmer source isolation must still be verified.
  Semiconductor/capacitor paths can affect readings;
  a continuity beep alone is not a complete interpretation.
- Confirm MQ_AO_RAW is not directly tied to PA2; check both divider resistors and
  the midpoint. Confirm signal/power harnesses cannot be reversed accidentally.
- Confirm reserved pins, SWD, boot straps and crystal nodes remain untouched.

### Supply gate (before power is applied to the assembled electronics)

- Measure each external source separately, unloaded, for voltage and polarity.
- Confirm they are suitable regulated low-voltage supplies with a compatible common
  ground arrangement. Do not assume two unknown adapters can safely be commoned.
- Only after unpowered checks pass, arrange a supervised, current-limited staged
  rail test with modules disconnected where practical. First energizing rails is
  itself a controlled test, not permission to power the entire assembly blindly.
- Measure 3V3_LOGIC on the actual board, 5V_MQ, and check for unintended backfeed
  with each supply individually present. Investigate unexpected powered rails.
- With intended loads added incrementally, verify rail stability and regulator
  temperature. Do not commit SD/HX711 expansion until current budget is established.

### Signal/functional gate

- Verify I2C idle HIGH levels near 3.3 V and module pull-up rails; never 5 V on this bus.
- Verify DS data pull-up, flame DO logic voltage, and PA2 voltage within 0..3.3 V;
  expected nominal MQ range after the divider is about 0..2.5 V, not a guarantee.
- Confirm OLED/RTC ACK and valid readout, advancing RTC seconds, DS temperature,
  raw flame DO, MQ baseline/filter updates, PC13 heartbeat and all DEMO LEDs.
- Verify USB/ST-LINK operation and no unexpected current/heat. Do not release LPG
  to test this transfer. Document readings and discrepancies before normal use.

## 9. Decisions required before final hole layout

| Open item | Why it blocks finalization |
| --- | --- |
| D-131 exact usable grid/pitch, edge-pad connectivity and mounting holes | Approximate 100 × 150 mm envelope is supplied; exact coordinates/fit remain blocked |
| HW-267 socket row spacing, orientation and physical fit | Count is confirmed as 20 per side; 17.78 mm centre spacing and 53 × 23 mm envelope remain approximate until test fit |
| Female socket availability/height | Not yet purchased; no direct-solder substitute authorized |
| Multimeter availability | Confirmed unavailable; required continuity/polarity/voltage gates are blocked |
| Single-adapter proposal: actual HW-267 5 V path, USB link and regulator | Must be traced and measured before any 5 V header connection is authorized |
| Adapter/protection/switch and total current budget | Existing MQ adapter is 5 V, 1 A; combined-load suitability and under-load voltages remain unverified |
| USB/ST-LINK source isolation | Programmer identity, reference vs supply pins and maintenance procedure require confirmation |
| Independent power sequencing/backfeed handling | MQ signal may remain energized while MCU is off |
| Breakout pin order and onboard I2C pull-ups | Avoid 5 V pull-ups, reversed harnesses and excessive parallel pull-ups |
| HX711 board + full-bridge load-cell specifications | Logic supply, bridge excitation, wire identification and mechanics |
| MicroSD socket/module type | Regulator/level shifter requirements, transient power, footprint |
| Buzzer and fan voltage/current/type | Driver/protection/supply cannot yet be finalized |
| Approval of PA6/PA8 driver and PB0/PB1/PB10/PB11 button proposals | New reservations are not current verified wiring |
| Enclosure, cable lengths, heat spacing, connector keying | Affects routing, thermal errors, noise and service access |

**Release decision: electrical planning draft complete; permanent physical layout
NOT FINAL.** No firmware, GPIO initialization, clock, driver or `.ioc` was changed.
This document alone does not authorize soldering or first power-up.
