# Tab5 board-routing evidence

## Scope

This is the Phase 0 routing audit for `feature/power`, captured September 7, 2026.
Boot identifies P4 v1.3 and ST7121/ST712x firmware 1(1.80.1.16). After flashing the GPIO
ownership fix and rebuilding matching apps, the operator confirmed keyboard/touch
operation with `hello` and `touchtest`. No sleep, alarm, motion or power-button wake
experiment was performed. No board was opened or electrically probed.

## Published net trace

Visual review covered pages 1, 2, 4 and 5 of the
[current linked schematic](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/Tab5_Schematics_PDF.pdf).
Pin numbers below are schematic connector/package pins, not GPIO numbers.

| Route | Schematic evidence |
|---|---|
| Keyboard | J9 pins 7/8/10 → GPIO0/1/50; GPIO50 terminates at U1 pin 93. |
| Touch | J3 pin 8 → `TP_INT_GPIO23` → U1 pin 25; R92 pulls up to `SOC_3.3V`. J3 pins 9/10 carry GPIO31/32 I2C. |
| BMI270 | U19 INT1 pin 4 drives Q8 gate; Q8 pulls `E_TRG` low. INT2 has no connection shown. |
| RTC | U30 nIRQ pin 6 drives Q11; Q11/R125/C157/R127 condition Q12 gate; Q12 pulls `E_TRG` low. nRSTO/FOUT have no connection shown. |
| Controller interrupt | `E_TRG` → R78 → `nINT_STAT_TRIG` → U28 PMS150G PA6 pin 3. |
| Power button | S1 grounds `SW_PWR` → U28 PA4 pin 1. |
| Controller outputs | U28 PA3 pin 6 → `MPWR_EN`; PA5 pin 4 → `BOOT_GPIO35` / U1 pin 66. |
| Software power-off | U7 expander P4 → `PWROFF_PLUSE` → D12/Q3 conditioning network. |

These are source-level connections; controller firmware timing and retained-state
behavior are not established by the schematic.

## Cross-check against running software

The [vendor keyboard example](https://docs.m5stack.com/en/arduino/projects/tab5/tab5_keyboard)
uses ExtPort1 SDA0/SCL1 and IRQ50 in Normal mode. `platform/esp32p4/keyboard.c` uses
those pins, an active-low level check and the shared GPIO ISR service. The resolved
BSP defines `BSP_LCD_TOUCH_INT` as GPIO23; `pointer.c` attaches its handler there.
Operator-confirmed input operation supports these runtime paths on this physical unit.
It does not establish electrical pulse timing, exhaustive gesture coverage or recovery
on other touch-controller revisions.

The source in `platform/esp32p4/power.c` drives second-expander P4 for shutdown, matching
the power-off net. The [vendor power-button documentation](https://docs.m5stack.com/en/core/Tab5)
describes power-on and double-press shutdown. Neither proves continuation of a suspended
P4 process. No direct RTC/IMU/button P4 runtime IRQ is shown. Do not repurpose GPIO35:
it is explicitly a boot strap, and the PMS150G's custom firmware protocol is unverified.

## Revision limits and suspend policy

The [older core schematic](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/C145_Schematic_Core.pdf)
has sheet dates `3/12/2025` and blank revision fields. It powers BMI270 from `VDD_STBY`.
The current linked export has PDF creation metadata `2025-06-09`, blank sheet identity
fields and a changed IMU supply/interface: U39, `VDD_IMU` and Q13 I2C isolation. PDF
metadata is not a physical board revision. ST7121 identification cannot determine which
IMU/power circuit is populated. Both exports show the same relevant interrupt endpoints.

PCB revision is recorded as `unknown`. The user accepted this limitation for Phase 0
closure and deferred exact physical matching and circuit-specific wake features.
Matching vendor revision information or a separate physical inspection would still be
needed to establish undocumented population differences before relying on those circuits.

Keyboard and touch remain unarmed light-sleep candidates. RTC, BMI270 and power-button
transparent wake remain unavailable. The trace supports that conservative classification;
it does not enable sleep, demonstrate power savings or prove hardware wake retains RAM.
Before enabling any unsupported source, establish the actual populated route, retained
supplies, controller behavior, wake attribution and successful resume without reset.

## Deferred circuit-specific features

| Feature | Evidence required before enabling transparent wake |
|---|---|
| RX8130 RTC alarm wake | Alarm conditioning, actual controller route and behavior, and resume without power cycling. |
| BMI270 motion wake | Actual IMU supply/isolation circuit, retained sensor configuration, interrupt conditioning and controller behavior. |
| Physical power-button wake | PMS150G button handling and a proven retained P4 resume path rather than power-on, power-off or boot control. |

Circuit-specific means behavior depends on unverified board wiring, populated supply
circuitry or power-controller firmware. Related revision-dependent IMU supply/isolation
optimizations remain unverified too. Ordinary RTC timekeeping, reboot/shutdown and
current keyboard/touch operation are not deferred by this decision.

Portable power policy and idle dimming can proceed. Keyboard GPIO50 and touch GPIO23
remain later light-sleep candidates, with driver-specific preparation/restoration and
physical validation required for each supported controller. Phase 0 completion does
not enable these wake sources or certify all variants for suspend.

## Reproducibility

| Download | SHA-256 |
|---|---|
| `Tab5_Schematics_PDF.pdf` | `13cf3fd9954d39aa198e57b1bb94ec56af7d49107c80a245acc5bd90ac8ea329` |
| `C145_Schematic_Core.pdf` | `b8d242cd23d07642fc424834ca3d6fc570db7458c28ebd4a1bfb5cf2a0c1133a` |

Downloads and rendered inspection pages are retained locally under
`.local/power-phase0/routing/`. The [baseline](power-baseline.md) remains authoritative
for the participant inventory, SDK constraints and measurement procedure.
