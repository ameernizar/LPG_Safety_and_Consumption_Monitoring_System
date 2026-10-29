# MCU configuration correction — 29 October 2026

## Confirmed marking and scope

The owner physically rechecked the board and reports **HK STM32F103C8T6**.
Use **STM32F103C8Tx**, nominal **64 KB Flash**, **20 KB SRAM**, LQFP48 and the
existing 72 MHz clock. HK-marked compatible hardware is not assumed to be genuine
STMicroelectronics silicon. No capacity beyond the nominal 64 KB is assumed.

| Setting | Before | After |
| --- | --- | --- |
| CubeIDE Debug/Release target | STM32F103CBTx | STM32F103C8Tx |
| CubeMX CPN | STM32F103CBT6TR | STM32F103C8T6TR |
| Active linker | STM32F103CBTX_FLASH.ld | STM32F103C8TX_FLASH.ld |
| Flash | 128 KB | 64 KB |
| SRAM / clock / CPU | 20 KB / 72 MHz / Cortex-M3 | Unchanged |

## Exact configuration changes

- `.cproject`: corrected MCU and default-settings metadata, plus linker paths,
  in both Debug and Release.
- `STM32_HW_Test.ioc`: changed only `Mcu.CPN`, `Mcu.UserName`, `PCC.PartNumber`
  and `ProjectManager.DeviceId`. All peripheral and clock properties are unchanged.
  `Mcu.Name` and `PCC.MCU` remain `STM32F103C(8-B)Tx`: these are shared family
  identifiers, not a selection of the CB capacity.
- Renamed `STM32F103CBTX_FLASH.ld` to `STM32F103C8TX_FLASH.ld`, corrected the
  header and Flash length. Other sections, stack/heap settings and entry point
  remain unchanged.
- Updated current MCU descriptions in README and PCB_ASSEMBLY_PLAN.md.
- Added a historical-status notice to PROJECT_CONTEXT.md; all previous test
  results, old identification claims and the 128 KB programmer report remain
  verbatim below that notice.
- Added this report. CubeIDE regenerated ignored Debug/Release makefiles and
  artifacts; both configurations use the corrected linker.

```ld
MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
}
```

`_estack` remains `0x20005000`. Minimum heap is `0x200` (512 bytes); minimum
stack reservation is `0x400` (1024 bytes). No application Flash-storage region
was added or moved.

## Files deliberately preserved

SHA-256 checks confirmed all **92 files under Core and Drivers are byte-identical**
to their pre-change versions. This includes GPIO/interrupt/clock setup, every
sensor driver, OLED pages, MQ-6 baseline logic and DEMO LED behavior.

`STM32F103xB` remains the correct CMSIS device macro. The bundled
`Drivers/CMSIS/Device/ST/STM32F1xx/Include/stm32f1xx.h` explicitly lists both
STM32F103C8 and STM32F103CB under this macro and selects `stm32f103xb.h`.
The existing `Core/Startup/startup_stm32f103cbtx.s` contains the shared F103xB
Cortex-M3 vector table; its historical filename does not set Flash capacity.
No startup rename or macro replacement is needed. Vendor definitions for other
Flash densities and generic 128 KB MPU regions are unrelated and remain intact.
The PCB document's ST datasheet link also covers the C8/CB family.

`.project`, `.mxproject`, HAL configuration and ST-LINK launch settings remain
unchanged. The launch still selects SWD, connect-under-reset and
`Debug/STM32_HW_Test.elf`; it has no hardcoded 128 KB download region. Physical
ST-LINK compatibility has not been retested. Its disabled logging path predates
the folder rename; its disabled SWV/RTOS settings are not active configuration.

## Existing CubeMX regeneration risks

Do not blindly regenerate application source from this .ioc:

- It lists only I2C1, NVIC, RCC and SYS, and only the original I2C/SWD/HSE/PC13
  pins. ADC1/PA2, PA0 1-Wire, PA1 flame input, PA3–PA5 LEDs and PB8/PB9 HX711
  are implemented separately in USER CODE or application modules.
- `RCC.ADCFreqValue=36000000` is stale; working `MQ6_Init()` explicitly sets
  PCLK2/6 and checks the resulting **12 MHz** ADC clock.
- `RCC.USBFreq_Value=72000000` is cached metadata; no USB peripheral is enabled.
  It must not be taken as a valid future USB clock configuration.
- `ProjectManager.KeepUserCode=true` helps preserve marked sections but does not
  make the incomplete metadata a complete hardware configuration.

This correction changes MCU identity only; repairing those pre-existing metadata
gaps would be a separate reviewed change. No CubeMX source regeneration occurred.

## Build and verification results

CubeIDE 2.2.0 headless managed builds ran in a separate temporary workspace:

```sh
/opt/st/stm32cubeide_2.2.0/headless-build.sh \
  -data /tmp/lpg-c8-workspace \
  -import /home/ameer/Documents/S5/MC/project/LPG_Safety_and_Consumption_Monitoring_System \
  -cleanBuild STM32_HW_Test/Debug -cleanBuild STM32_HW_Test/Release
```

Debug was cleaned and rebuilt; Release was generated and built fresh. Both
completed with **0 compiler/linker errors and 0 warnings**, using Cortex-M3,
Thumb, soft-float, `USE_HAL_DRIVER` and `STM32F103xB`. Debug uses `-O0 -g3`;
Release uses `-Os` without DEBUG. This builds firmware only; it never launches
ST-LINK or CubeProgrammer. Eclipse's background indexer reported unresolved
entries; those are IDE index diagnostics, not compiler/linker failures.

| Configuration | Flash text + data | Flash usage | RAM data + BSS | Heap/stack reservation | Total link-reserved RAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| Debug | 32,472 bytes | 49.55% of 65,536 | 2,016 bytes | 1,536 bytes | 3,552 bytes / 17.34% |
| Release | 20,580 bytes | 31.40% of 65,536 | 1,992 bytes | 1,536 bytes | 3,528 bytes / 17.23% |

GNU `size` includes the NOLOAD `._user_heap_stack` reservation in its BSS total.
It is separated above to avoid calling that reservation application globals.
These figures do not establish worst-case runtime stack/heap usage.

ELF load segments and map files were checked: Debug's Flash end is `0x08007ED8`,
Release's is `0x08005064` (exclusive), both below `0x08010000`. Link-reserved RAM
ends at `0x20000DE0` and `0x20000DC8`, respectively, below `0x20005000`.

All available host suites passed: MQ-6 stability/baseline, DEMO state/LED timing,
DS3231 driver, RTC ARM/TRIGGER helpers, HX711 protocol/timing-fault simulation,
and OLED five-page rendering/clipping. MQ-6 and HX711 used UBSan; OLED used
ASan/UBSan with leak checks disabled for the sandbox. XML parses, source hashes,
IOC-property comparison and linker-path checks passed.

Outputs: `Debug/STM32_HW_Test.elf` and `Release/STM32_HW_Test.elf` in the project
folder. Build log for this run: `/tmp/lpg-c8-build.log` (temporary, not committed).

## Remaining physical verification

This change preserves source behavior and every pin assignment, but no hardware
run was performed. HK device identity, actual memory behavior and peripheral
compatibility are still hardware properties. Optimized Release 1-Wire/HX711 timing
also needs physical validation before assuming equivalence on the board.
Use the exact flashed ELF when generating RTC mailbox addresses; rebuilding can
change symbol addresses. No RTC timestamp was changed or provisioning enabled.

**No flashing, commit or push was performed.**
