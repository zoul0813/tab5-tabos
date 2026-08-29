# Battery-Powered Tab5 Boot and USB MSC

## Issue

TabOS may fail to boot when the Tab5 is powered only by its battery. Normal boot
currently calls `platform_usb_port_disable_host_power()` from `platform_init()`.
If that operation fails without USB-C power present, `platform_init()` returns
false and `app_main()` stops before normal runtime startup.

Relevant code:

- `platform/esp32p4/runtime.c`
- `platform/esp32p4/usb_storage.c`
- `targets/tab5/main/app_main.c`

The failure is likely caused by treating USB-A host-power shutdown as a fatal
requirement for every boot. USB-A power control is only required when entering
USB Mass Storage Class (MSC) device mode.

## Safety Background

In MSC mode, the Tab5 USB-A connector operates as a USB device connected to a
computer. The computer supplies USB VBUS. TabOS must first disable the Tab5 USB-A
host 5 V output so both sides do not source power onto the same USB connection.

This safety requirement applies when switching USB-A into MSC device mode. It
does not make USB-C charger power a prerequisite for ordinary battery boot or,
in principle, for battery-powered MSC operation.

## Hardware Evidence

M5Stack's Tab5 documentation states that USB-A 5 V output is controlled
independently from the other external 5 V output through `ext_USB`. The Tab5
power documentation also shows that USB-A output can be enabled or disabled
with the power-management API while the device supports battery power-on.

The documented hardware signal is `USB5V_EN`, connected to P3 of the second
PI4IOE5V6408 I/O expander at I2C address `0x44`. This is an independent USB-A
power-switch control, not the battery/system power rail. M5Stack's Tab5 example
source also exposes a `bsp_set_usb_5v_en(bool)` implementation that toggles this
signal.

Sources:

- M5Stack Tab5 hardware documentation: `https://docs.m5stack.com/en/core/Tab5`
- M5Stack Tab5 power documentation: `https://docs.m5stack.com/ja/arduino/m5tab5/power`
- M5Stack Tab5 example BSP: `https://github.com/m5stack/M5Tab5-UserDemo/blob/main/platforms/tab5/components/m5stack_tab5/m5stack_tab5.c`

Therefore, current evidence supports disabling USB-A VBUS during battery boot
without disabling the Tab5 itself. Remaining work is verifying that the pinned
`m5stack_tab5_noglib` BSP initializes and controls this expander successfully
when the Tab5 is powered only by its battery.

## Proposed Solution

Move the fatal USB-A host-power disable check out of the common normal-boot path
and keep it immediately before TinyUSB MSC device startup.

Expected behavior:

- Normal battery boot does not depend on USB-A power-control success.
- Normal USB-C boot does not depend on USB-A power-control success.
- MSC startup attempts to disable USB-A host power before enabling TinyUSB device
  mode.
- MSC startup aborts safely if USB-A host power cannot be disabled.
- MSC mode never starts while the Tab5 may still be sourcing USB-A 5 V.

Do not silently continue into MSC mode after a failed power-disable operation.

## Implementation Plan

1. Remove or stop treating `platform_usb_port_disable_host_power()` as fatal in
   `platform_init()`.
2. Call and validate it in `tab5_usb_storage_start()` immediately before
   TinyUSB device installation.
3. Keep normal USB-A host power disabled by default if the BSP supports a safe,
   nonfatal initialization path. Distinguish an unavailable control operation
   from a confirmed powered-off state where possible.
4. Preserve the existing MSC failure message and restart behavior.
5. Add host/unit coverage for the decision boundary if the platform abstraction
   can be faked: normal boot continues when USB-A disable fails, while MSC start
   fails closed.
6. Cross-build Tab5 Debug and Release firmware.

## Implementation Status

Implemented in current tree:

- Normal platform initialization attempts USB-A host-power shutdown but treats
  failure as a warning, so battery boot can continue.
- Platform initialization configures second PI4IOE5V6408 expander P7 as a
  push-pull output and drives `CHG_EN` high to enable IP2326 battery charging.
- MSC startup performs its own shutdown check immediately before TinyUSB setup
  and returns failure when shutdown cannot be confirmed.

Tab5 Debug and Release cross-builds complete. Still pending: host/unit seam
coverage and physical battery/USB-C/MSC validation below.

## Hardware Validation

Test all combinations:

- Battery only, normal boot.
- USB-C power, normal boot.
- Battery only, Delete-held MSC boot with USB-A connected to a computer.
- USB-C power, Delete-held MSC boot with USB-A connected to a computer.
- MSC startup with USB-A power-disable failure injected or observed.
- Safe eject and automatic restart back into normal boot.
- Repeated MSC sessions with a mounted microSD card.

Capture serial logs. Confirm normal battery boot reaches the boot report and that
MSC mode does not enable device mode unless USB-A host power is confirmed safe.
Check the computer sees the microSD disk, and verify filesystem integrity after
safe eject and restart.

## Hardware Caveat

The firmware must not assume that battery power prevents MSC operation. Official
M5Stack documentation and example code indicate that USB-A VBUS control is
available independently on battery power. If the pinned BSP call still fails,
compare its `BSP_FEATURE_USB` implementation with the documented `USB5V_EN`
expander path, then measure USB-A VBUS on hardware. Do not connect a host and
attempt MSC until VBUS-off is confirmed.
