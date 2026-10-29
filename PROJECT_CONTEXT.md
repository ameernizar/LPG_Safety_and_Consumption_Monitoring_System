> **Historical record — superseded MCU identification (29 October 2026).**
> The owner has physically confirmed **HK STM32F103C8T6**, using the
> **STM32F103C8Tx** compatibility target: nominal **64 KB Flash**, **20 KB SRAM**.
> Earlier K32/CB markings, CBTx configuration and 128 KB reports below are
> preserved as historical observations, not current hardware specifications.
> No genuine-ST or extra-Flash assumption is made. Test logs are unchanged.
> See [MCU_CONFIGURATION_UPDATE.md](MCU_CONFIGURATION_UPDATE.md).

# PROJECT_CONTEXT.md

## Project Title

**Intelligent LPG Safety and Consumption Monitoring System Using STM32**

This is a college Project-Based Learning embedded-system project. The goal is to build a standalone retrofit system for a domestic LPG cylinder that can monitor LPG leakage, cylinder weight, temperature, flame presence, and LPG consumption, while also providing warnings and basic automated low-voltage ventilation.

---

## 1. Important Working Rules

Follow these rules for all further development.

### Hardware identification
- Never assume the exact MCU, board, sensor, OLED, programmer, or module model from generic appearance alone.
- Use visible chip markings, PCB markings, labels, and actual tested behavior.
- If the exact device cannot be proven from markings, state the uncertainty explicitly.
- Do not silently treat a compatible clone as a genuine ST part.

### Pin assignment
Before assigning any new pin:
- Check current pin usage.
- Check STM32 alternate-function conflicts.
- Keep SWD pins reserved.
- Check future SPI, I2C, UART, ADC, timer, and interrupt requirements.
- Avoid pin conflicts with already connected modules.

### Electrical safety
Before connecting any new module:
- Verify supply voltage.
- Verify GPIO/ADC voltage compatibility.
- Check whether pull-ups, voltage dividers, level shifters, transistors, MOSFETs, or flyback diodes are required.
- Do not expose STM32 GPIO/ADC pins to unsafe voltages.
- Do not assume the STM32 board can power the entire finished system.

### Development process
Work one physical component at a time:
1. Identify hardware.
2. Verify electrical requirements.
3. Wire it.
4. Configure CubeMX.
5. Build.
6. Flash.
7. Run a minimal test.
8. Mark it VERIFIED only after observed physical behavior.
9. Then move to the next component.

### Terminology
Use these exact status meanings:
- **IDENTIFIED** = identified from markings/photos or strong evidence
- **WIRED** = physical connections completed
- **COMPILED** = project builds successfully
- **FLASHED** = firmware programmed successfully
- **TESTED** = physical behavior observed
- **VERIFIED** = behavior matches expected result

### LPG safety constraints
- MQ-6 is for leak detection only.
- Do not use MQ-6 analog value as a direct LPG quantity measurement.
- LPG quantity must come from the load cell + HX711.
- Do not assume one universal MQ-6 threshold.
- Do not perform dangerous LPG leakage experiments.
- Do not control AC mains directly.
- Use only a safe low-voltage DC fan/relay setup for the prototype.
- This is a student prototype, not a certified gas-safety device.

---

## 2. Confirmed Hardware Identity

### MCU / development board

Physical MCU chip marking observed:

```text
HK
K32F103CBT6
QL018494
```

Board PCB marking observed:

```text
HW-267
```

The board is a **Blue-Pill-style STM32F103-compatible development board**.

Important distinction:
- Physical MCU is **not an ST-branded STM32F103CBT6**.
- It is an **HK-marked K32F103CBT6 third-party STM32F103-compatible device**.
- Development is being done using the **STM32F103CBTx / STM32F103CBT6 reference profile**.
- Do not describe the physical MCU as a genuine ST part.
- Package visually matches LQFP48.

### Practical device identification through ST-LINK

STM32CubeProgrammer successfully connected and reported:

```text
Device ID: 0x410
Revision ID: Rev X
Family: STM32F101/F102/F103 Medium-density
NVM size: 128 KB
CPU: Cortex-M3
Target voltage: approximately 3.25–3.26 V
```

This strongly supports STM32F103 medium-density compatibility.

---

## 3. Programmer Identity

Programmer is a purple **ST-LINK/V2-style** USB programmer/debugger.

Printed 2x5 header mapping observed:

```text
1  RST
2  SWDIO
3  GND
4  GND
5  SWIM
6  SWCLK
7  3.3V
8  3.3V
9  5.0V
10 5.0V
```

Board SWD header labels observed:

```text
3.3
DIO
CLK
GND
```

### Verified SWD wiring

```text
ST-LINK 3.3V   -> Board 3.3
ST-LINK SWDIO  -> Board DIO
ST-LINK SWCLK  -> Board CLK
ST-LINK GND    -> Board GND
```

Do not use:
- 5V
- SWIM

RST was not required for the working setup.

### Linux detection

Linux command showed:

```text
Bus 002 Device 004: ID 0483:3748 STMicroelectronics ST-LINK/V2
```

So Linux detects the programmer correctly.

---

## 4. Development Environment

Main development environment:
- Linux Mint
- STM32CubeMX
- STM32CubeIDE
- STM32CubeProgrammer
- VS Code + Codex

Important workflow detail:

CubeIDE direct debug/run failed with:

```text
Error in final launch sequence:
Failed to start GDB server
Failed to start GDB server
Error in initializing ST-LINK device.
Reason: (18) Could not verify ST device! Abort connection.
```

Likely cause:
- CubeIDE/ST-LINK GDB server is rejecting the clone/non-ST MCU.

This does **not** block development.

Working workflow is:

```text
CubeMX
  -> generate project
CubeIDE
  -> build
STM32CubeProgrammer
  -> flash .elf manually through ST-LINK
```

This workflow is proven to work.

---

## 5. Current Project

Project name:

```text
STM32_HW_Test
```

Development target selected in CubeMX:

```text
STM32F103CBTx
```

Expected reference characteristics:
- LQFP48
- 128 KB Flash
- 20 KB RAM
- Cortex-M3

---

## 6. System Clock Configuration

External board crystal physically observed:
- 8.000 MHz HSE crystal
- 32.768 kHz low-speed crystal also visible on the board

CubeMX clock configuration currently used:

```text
HSE = 8 MHz
PLL source = HSE
HSE divider = /1
PLL multiplier = x9
PLLCLK = 72 MHz
System Clock source = PLLCLK
SYSCLK = 72 MHz
AHB prescaler = /1
HCLK = 72 MHz
APB1 prescaler = /2
PCLK1 = 36 MHz
APB2 prescaler = /1
PCLK2 = 72 MHz
```

USB and ADC clock controls were greyed out because those peripherals were not enabled. That is acceptable.

Clock configuration was validated by rebuilding and running the PC13 blink firmware successfully after switching to the 72 MHz PLL configuration.

### Clock status
- HSE: **VERIFIED**
- PLL x9: **VERIFIED**
- SYSCLK 72 MHz: **VERIFIED**

---

## 7. PC13 LED Test

PC13 was configured as:

```text
GPIO_Output
```

Minimal test:

```c
while (1)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(500);
}
```

The board LED is active-low on this Blue-Pill-style board, but toggling still produces a visible blink.

Final status:
- PC13 pin configured: **VERIFIED**
- Firmware build: **COMPILED**
- Firmware manually programmed with CubeProgrammer: **FLASHED**
- LED blink: **TESTED + VERIFIED**

---

## 8. OLED Hardware

OLED photos showed a 4-pin 1.3-inch I2C OLED module.

Visible pin labels:

```text
GND
VCC
SCL
SDA
```

PCB also contains address-selection markings:

```text
0x78 / 0x7A
```

These correspond to common 8-bit I2C representations of:
- 7-bit 0x3C
- 7-bit 0x3D

The exact controller marking is not visible in the photos.

Likely controller family:
- SH1106-compatible 1.3-inch 128x64 OLED

However, this is **not yet fully verified** from a chip marking.
Do not claim the controller is definitely SH1106 until the module is proven operational.

---

## 9. OLED Wiring

Current intended and manually confirmed wiring:

```text
OLED GND -> STM32 GND
OLED VCC -> STM32 3.3V
OLED SCL -> PB6
OLED SDA -> PB7
```

I2C bus:
- I2C1
- PB6 = I2C1_SCL
- PB7 = I2C1_SDA
- Standard mode
- 100 kHz
- 7-bit addressing

User manually confirmed the physical wiring and does not want repeated photo-by-photo wire verification unless needed.

---

## 10. I2C CubeMX / Generated Code Verification

Codex inspected the generated project and reported:

| Check | Result |
|---|---|
| `MX_I2C1_Init()` exists | PASS |
| `HAL_I2C_MspInit()` exists | PASS |
| `hi2c1` exists and is initialized | PASS |
| PB6 = SCL and PB7 = SDA | PASS |
| `GPIO_MODE_AF_OD` is used | PASS |
| GPIOB clock enabled | PASS |
| I2C1 peripheral clock enabled | PASS |
| I2C speed = 100 kHz | PASS |
| 7-bit addressing | PASS |
| `MX_I2C1_Init()` is called before I2C use | PASS |

Relevant locations reported by Codex:
- `Core/Src/main.c:217` -> `MX_I2C1_Init()`
- `Core/Src/stm32f1xx_hal_msp.c:89` -> `HAL_I2C_MspInit()`
- `main.c:44` -> global `hi2c1`
- `main.c:227-236` -> I2C initialization
- MSP PB6/PB7 setup around `:99-106`
- `GPIO_MODE_AF_OD` around `:104`
- GPIOB clock around `:98`
- I2C1 peripheral clock around `:109`

Therefore current evidence says the CubeMX-generated I2C firmware configuration is correct.

---

## 11. I2C / OLED Diagnostic History

### First scanner test
A temporary I2C scan tested all addresses and used the PC13 LED to signal if address 0x3C or 0x3D was found.
The first reported test appeared successful.

Later, while trying the OLED driver, the display remained blank.

### SH1106 driver attempt

Files created/planned:

```text
Core/Inc/sh1106.h
Core/Inc/font5x7.h
Core/Src/sh1106.c
```

The driver assumed:

```c
#define SH1106_ADDRESS (0x3C << 1)
```

Test text intended:

```text
LPG SYSTEM
OLED OK
```

The OLED did not display the text.

### Address-specific diagnostic
The OLED initialization was commented out and direct `HAL_I2C_IsDeviceReady()` checks were performed for:

```c
0x3C << 1
0x3D << 1
```

A more obvious LED-pattern test was then used:
- 1 long blink repeatedly -> 0x3C detected
- 2 blinks repeatedly -> 0x3D detected
- 5 fast blinks repeatedly -> no I2C device detected

Actual result:

```text
5 fast blinks repeatedly
```

Therefore, at the current point in time:

**No I2C ACK is being received from the OLED at 0x3C or 0x3D.**

This means the OLED driver itself should not be debugged yet.

The problem is currently considered lower-level:
- electrical I2C bus issue
- module power issue
- pull-up issue
- wiring/contact problem
- module fault
- or some other bus-level condition

The generated STM32 I2C configuration has already been verified as correct by Codex.

---

## 12. Current Code Caveats Found by Codex

Codex reported three caveats:

### A. OLED initialization currently commented out
This is intentional while performing I2C bus diagnostics.
Do not uncomment the OLED initialization until the I2C device responds again.

### B. Duplicate USER CODE markers
Codex reported duplicate markers in `main.c` around:

```text
main.c:94 / 106 -> USER CODE BEGIN 2
main.c:123 / 125 -> USER CODE END 2
```

This is dangerous for future CubeMX regeneration.

The next code-maintenance action is to clean this so there is exactly one:

```c
/* USER CODE BEGIN 2 */
...
/* USER CODE END 2 */
```

while preserving the diagnostic code.

### C. `.ioc` metadata inconsistency
Codex reported:

```text
STM32_HW_Test.ioc:100
```

`ProjectManager.functionlistsort` lists only system-clock and GPIO initialization and omits I2C1.

Important:
- Actual source code currently calls I2C1 correctly.
- This metadata is suspicious/inconsistent.
- Do not casually edit the `.ioc` file by hand.
- Resolve only if necessary, preferably through CubeMX regeneration once USER CODE sections are cleaned.

---

## 13. Immediate Next Action

Do **not** modify the OLED driver yet.

First clean the duplicate USER CODE markers using this constraint:

```text
There must be exactly one matching:

/* USER CODE BEGIN 2 */
...
/* USER CODE END 2 */

Preserve the current I2C diagnostic code.
Do not alter I2C configuration.
Do not uncomment OLED initialization.
Do not change PB6, PB7, PA13, PA14, PC13, clocks, or SWD.
Do not edit the .ioc file yet.
```

After that, perform an electrical I2C check with a multimeter while the board and OLED are powered and connected.

Measure relative to GND:

```text
OLED VCC
PB6 / SCL
PB7 / SDA
```

Expected idle values:

```text
OLED VCC ≈ 3.3 V
SCL ≈ 3.3 V
SDA ≈ 3.3 V
```

Interpretation:

### If all are approximately 3.3 V
Then:
- power is present
- pull-ups probably exist
- next investigate module/address/controller/contact/fault

### If SCL or SDA is near 0 V
Possible causes:
- line held low
- short
- incorrect wiring/contact
- module fault
- peripheral/bus issue

### If SCL/SDA are floating or not pulled high
Likely missing/weak I2C pull-ups.

Potential corrective hardware if needed:

```text
3.3V -- 4.7kΩ -- SCL / PB6
3.3V -- 4.7kΩ -- SDA / PB7
```

Do not add these resistors until the voltages are measured first.

---

## 14. Current Master Pin Map

| MCU Pin | Board Label | Function | Module | Status |
|---|---|---|---|---|
| PA13 | A13 | SWDIO | ST-LINK | RESERVED / VERIFIED |
| PA14 | A14 | SWCLK | ST-LINK | RESERVED / VERIFIED |
| PC13 | C13 | GPIO output | onboard LED | TESTED / VERIFIED |
| PB6 | B6 | I2C1_SCL | OLED | RESERVED / CONFIGURED |
| PB7 | B7 | I2C1_SDA | OLED | RESERVED / CONFIGURED |

PB6/PB7 may later also be shared with an RTC such as DS3231 because I2C supports multiple addressed devices.

---

## 15. Current Power Map

| Device | Supply | Notes |
|---|---|---|
| MCU board | 3.3 V domain | Powered through current development setup |
| ST-LINK interface | 3.3 V logic | SWD verified |
| OLED | 3.3 V | Current test supply |
| Future MQ-6 | TBD after exact module verification | Heater/current/analog output need special care |
| Future HX711 | TBD after exact module verification | Usually low-voltage digital interface |
| Future DS18B20 | TBD after exact part/module verification | Pull-up may be required |
| Future relay/fan | Separate low-voltage power expected | Do not drive fan directly from GPIO |

Do not assume the final project can be powered entirely from the MCU board.

---

## 16. Current Implementation Status

| Stage | Status |
|---|---|
| MCU identification | IDENTIFIED |
| Board identification | IDENTIFIED |
| Programmer identification | IDENTIFIED |
| Linux ST-LINK detection | VERIFIED |
| SWD connection | VERIFIED |
| CubeProgrammer target connection | VERIFIED |
| Device ID / flash size detection | VERIFIED |
| PC13 LED output | VERIFIED |
| External HSE clock | VERIFIED |
| PLL 72 MHz system clock | VERIFIED |
| I2C1 CubeMX configuration | VERIFIED in source |
| PB6/PB7 I2C configuration | VERIFIED in source |
| OLED physical wiring | WIRED / user-confirmed |
| OLED I2C ACK | CURRENTLY FAILING |
| OLED driver | NOT VERIFIED |
| OLED text display | NOT WORKING YET |

---

## 17. Planned Development Stages

### Completed
1. MCU identification
2. Programmer/SWD
3. LED blink
4. External HSE + 72 MHz clock validation

### Current
5. OLED / I2C bus bring-up

### After OLED is verified
6. DS18B20 temperature sensor
7. HX711 + load cell
   - raw reading
   - tare
   - calibration
   - filtering
8. MQ-6 gas sensor
   - raw ADC first
   - no false ppm claims
9. Flame sensor
10. Warning LEDs + buzzer
11. Relay + low-voltage DC exhaust fan
12. Integrate sensors
13. SAFE / WARNING / EMERGENCY state machine
14. DS3231 RTC
15. MicroSD + FATFS
16. Timestamped CSV logging
17. LPG consumption analysis
18. Remaining-days / refill estimate
19. Final OLED user interface
20. Full-system integration and testing

Do not skip the one-component-at-a-time validation process.

---

## 18. Future Functional Requirements

The finished project should support:
- LPG leakage detection
- cylinder weight measurement
- remaining LPG estimation
- temperature monitoring
- flame detection
- local OLED display
- buzzer warning
- LED warning
- automatic low-voltage DC exhaust fan activation
- timestamped data logging
- consumption analysis
- estimated remaining days
- refill estimate

Potential final states:

```text
SAFE
WARNING
EMERGENCY
```

These should eventually be implemented as an explicit state machine.

---

## 19. Important Sensor Notes for Later

### MQ-6
- Used for LPG leakage detection.
- Do not interpret raw ADC directly as exact ppm without proper calibration.
- Do not use MQ-6 to estimate cylinder quantity.
- Sensor module output voltage must be verified before connecting to STM32 ADC.
- MQ heater power requirements must be checked carefully.
- Do not power the MQ heater blindly from a weak 3.3 V rail.

### HX711 + load cell
This is the main source for:
- cylinder weight
- LPG remaining mass
- usage trend
- estimated remaining days

Need:
- tare
- known-weight calibration
- filtering
- stable mechanical mounting

### Flame sensor
Exact module must be identified before wiring.
Verify digital/analog output voltage.

### DS18B20
Likely 1-Wire.
Check whether using bare sensor or module.
A pull-up resistor may be needed.

### Relay + fan
GPIO must not drive the fan directly.
Use suitable transistor/MOSFET/relay module.
Use flyback protection where required.
Keep fan low-voltage DC only.

### MicroSD
Likely SPI.
Must reserve suitable SPI pins without conflicting with other modules.

### DS3231
Can share I2C1 with OLED if bus is healthy and addresses do not conflict.

---

## 20. Guidance for Codex

When continuing this project:
- Inspect the actual project files before changing code.
- Prefer minimal, reversible changes.
- Never rewrite unrelated generated CubeMX code.
- Preserve CubeMX USER CODE sections.
- Do not move SWD away from PA13/PA14.
- Do not break the verified 72 MHz clock configuration.
- Do not silently reassign PB6/PB7 while OLED/I2C is under test.
- Do not assume the physical chip is genuine STM32.
- Do not treat a successful compile as hardware verification.
- After every code change, state:
  - files modified
  - exact reason
  - expected hardware behavior
  - how to test it
- If a CubeMX regeneration is needed, warn before doing anything that might overwrite manual code.
- Before adding any new hardware driver, check the current master pin map for conflicts.

---

## 21. Exact Current Problem Statement

The immediate problem is:

> The 1.3-inch 4-pin I2C OLED is wired to 3.3 V, GND, PB6/SCL, and PB7/SDA. CubeMX-generated I2C1 code has been verified as correct: PB6/PB7 alternate-function open-drain, GPIOB clock enabled, I2C1 clock enabled, 100 kHz, 7-bit addressing, and MX_I2C1_Init() runs before the test. However, HAL_I2C_IsDeviceReady() currently receives no ACK at either 0x3C or 0x3D, producing the firmware's five-fast-blink error pattern. Therefore, do not debug the SH1106 rendering code yet. First clean duplicate USER CODE markers, then verify the electrical I2C bus with a multimeter by measuring OLED VCC, SCL, and SDA relative to GND.

---

## 22. Recommended Immediate Codex Task

Codex should first:
1. Inspect `Core/Src/main.c`.
2. Fix only the duplicate `USER CODE BEGIN 2 / END 2` markers.
3. Preserve the current I2C diagnostic.
4. Do not uncomment the SH1106 driver.
5. Do not alter I2C configuration.
6. Do not edit the `.ioc` manually.
7. Show the resulting clean USER CODE section.
8. Then wait for the multimeter readings before making further electrical/software changes.

---

## 23. Flashing Procedure That Works

After any successful build:
1. Open STM32CubeProgrammer.
2. Select ST-LINK.
3. Connect.
4. Open the compiled ELF:
   ```text
   STM32_HW_Test/Debug/STM32_HW_Test.elf
   ```
5. Click Download.
6. Wait for successful programming/verification.
7. Disconnect if desired.
8. Press RESET on the board.
9. Observe physical behavior.

Do not rely on CubeIDE direct GDB debug for now because it previously failed with clone verification error.

---

## 24. Final Reminder

The current project state is healthy overall.

The MCU, ST-LINK, SWD, flash, PC13 GPIO, external HSE, 72 MHz system clock, and generated I2C1 configuration are all proven working.

The only active blocker is the **OLED I2C bus/device response**.

Resolve that electrical/bus-level issue first before continuing with the OLED display driver or any new sensor.
