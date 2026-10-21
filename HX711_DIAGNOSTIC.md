# HX711 raw communication diagnostic

This revision adds raw counts only: no tare, weight, calibration factor or LPG
quantity. Existing warning LEDs remain simulated DEMO outputs.

## Connections and configuration

PB8 is DOUT input with an internal pull-up so an unplugged data wire normally
reads not-ready. PB9 is push-pull PD_SCK, preloaded LOW before GPIO configuration.
PB6/PB7 stay I2C1; no remapping, timer, ADC or clock changes are made. Initialization
is called from main.c USER CODE after the existing 1-Wire timing initialization.
The .ioc is unchanged; these pins are owned by HX711_Init, not generated GPIO.

The user reports one 5 V adapter supplying the Blue Pill and MQ-6. This firmware
does not verify that power path. HX711 module VCC and digital logic compatibility
at 3.3 V remain to be checked for the actual module. Do not infer module behavior
from the IC's supply range alone. Check common ground and avoid applying 5 V logic
to these MCU connections. Load-cell colors (red E+, black E-, green A+, white A-)
remain provisional; use the actual load-cell documentation/electrical checks.

## Acquisition

HX711_Service is called by the existing background/heartbeat service during the
800 ms temperature conversion wait and main-loop idle. It is never called inside
1-Wire bit or reset slots. Calls are limited to once per millisecond; a not-ready
input returns immediately. A ready input causes one bounded 25-pulse transfer:
24 MSB-first data bits and a final pulse selecting channel A, gain 128.

DWT at the existing 72 MHz HCLK provides at least 1 us high/low phases. The shared
counter is enabled but never reset. Each HIGH phase masks interrupts and restores
the previous mask after driving LOW, preventing a normal ISR from accidentally
powering down the HX711. Delay loops have finite guards; a stopped counter latches
TIMING ERROR and leaves SCK LOW. Avoid debugger halts during a HIGH phase. Actual
waveform timing on this compatible MCU still requires hardware confirmation.

Reference: AVIA HX711 datasheet (manufacturer document mirrored by TI):
https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/73/HX711.pdf

The first frame is discarded because the previous channel/gain may be unknown.
Frames are drained/discarded for another 500 ms for settling (datasheet 400 ms at
10 SPS). Readout supports either module RATE setting; no RATE pin is controlled.
OLED transfers can delay servicing, so every conversion is not guaranteed to be
captured, especially at 80 SPS. This is a foreground diagnostic, not a logger.

Signed conversion subtracts 2^24 when bit 23 is set. Both zero and -1 are legitimate
raw values. Exact endpoint codes -8388608 and +8388607 are reported as saturation.
DOUT must return HIGH after pulse 25; failure reports DT STUCK LOW. One second
without a completed frame reports NO DATA / CHECK DT. Missing modules, wiring
errors and power faults cannot be uniquely distinguished by this interface.
There is no device identification or CRC, and a faulty/unconnected bridge can
still produce plausible counts. READY proves neither bridge integrity nor weight
accuracy. Recoverable errors clear the sample window and requalify gain/settling;
a timing fault needs reinitialization/reset after its cause is resolved.

## Fifth OLED page

The original four pages retain their contents. HX711 RAW is page 5/5, visible for
5 seconds once per approximately 25-second rotation. Rendering retains the 5x7
font, inverted heading and existing framebuffer; no second framebuffer is added.
Five compact page markers occupy x=110..123, y=59..61.

Example layout (illustrative only; firmware uses actual readings):

    HX711 RAW        5/5
    HX711: READY
    RAW: -123456
    RANGE: 215
    STABILITY: STABLE
    CHANNEL A GAIN 128
    RAW COUNTS ONLY

WAITING hides values during initialization. ERROR hides invalid/stale values and
displays the specific reason. READY remains visible between valid frames until
the readiness timeout. RANGE is max-minus-min across up to 16 recent raw samples;
STABLE requires a full 16-sample window with range <=1000 counts, configured in
hx711.h. This experimental threshold is not a mechanical accuracy specification
or a calibration. No additional smoothing is applied to RAW.

## Build and tests

Full Debug rebuild: zero compiler/linker warnings or errors.
Flash (text + data): 32472 bytes, +1812 versus the previous ELF.
Static RAM (data + bss): 3552 bytes, +96. This excludes runtime stack/heap use.
HX711 context is 104 bytes; linker section padding affects the net RAM increase.

New host test:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined \
  -Itests/hx711_stub -ICore/Inc Core/Src/hx711.c tests/hx711_test.c \
  -o /tmp/hx711_test
/tmp/hx711_test
```

Tests exercise 25 pulses, signed extremes/zero/-1, startup/discarding, timeout,
recovery, saturation, stuck LOW, stability, tick wrap, frozen counter and IRQ-mask
preservation. They simulate GPIO/DWT and do not prove electrical pulse widths.
The existing OLED framebuffer/rotation test now covers five pages and HX error
states (ASan/UBSan). MQ-6 stability, DEMO state, RTC driver and ARM/TRIGGER tests
also pass. PBM test previews contain synthetic data only.

## Physical verification — only after approval to flash

No hardware action has been performed. Before connecting/powering, verify module
3.3 V compatibility, supply polarity, shared ground and provisional bridge wiring.
After approval, flash Debug/STM32_HW_Test.elf and observe a full page rotation.
Confirm READY, raw counts, and eventual stability while the cell is undisturbed.
Gently apply and release force without overloading or stressing unsupported
mounting points: raw counts should change and return toward their prior level.
A reversed direction can reflect A+/A- polarity; do not invent calibration from it.
Without a weighing platform, this checks response only, not repeatable weighing.

Power off before disconnecting the HX711, then restart and confirm ERROR / NO DATA
while the other sensors, RTC, four original pages and DEMO LEDs continue working.
Reconnect with power off and retest. Verify negative counts display correctly if
observed. Do not expose sensors to LPG for this diagnostic.

ELF:
/home/ameer/Documents/S5/MC/project/LPG_Safety_and_Consumption_Monitoring_System/Debug/STM32_HW_Test.elf
