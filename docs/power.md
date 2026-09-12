# Reboot and Shutdown

The `reboot` command immediately performs an orderly restart:

```sh
reboot
```

The `shutdown` command immediately requests orderly power-off:

```sh
shutdown
```

Both actions stop applications, close open descriptors, unmount storage, and stop
system services before the platform action. They do not support delayed execution.

Applications use the Linux-style API from `<sys/reboot.h>`:

```c
reboot(RB_AUTOBOOT);
reboot(RB_POWER_OFF);
```

Successful calls do not return. Tab5 reboot uses the ESP32-P4 reset mechanism. Tab5
power-off uses the board power-control signal; if external power keeps the device
energized, TabOS remains halted with its display and services off. The host simulator
closes on power-off and performs a complete in-process runtime restart on reboot. These
calls currently require no privileges because TabOS has no user or permission model.

## Battery Charging

TabOS enables Tab5 battery charging during platform initialization. Charger
control uses second PI4IOE5V6408 I/O expander at I2C address `0x44`, pin P7
(`CHG_EN`). If serial output reports charger-control failure, connect USB-C and
inspect hardware/API initialization before relying on battery runtime.

The `battery` command reports the combined battery and external-power diagnostic:

```sh
battery
battery charge on
battery charge off
battery fast on
battery fast off
```

Tab5 reads battery-rail voltage and signed current from the INA226 at I2C address
`0x41`. Positive current means battery discharge; negative current means charging.
Power preserves the same sign. Source is reported only when current direction makes
it unambiguous; near-zero current reports an unknown source. Percentage is a bounded
voltage estimate across the Tab5 battery's documented 6.0 V to 8.23 V range, not a fuel
gauge measurement.

Applications use `<tabos/battery.h>`. `tabos_battery_status_t.valid` identifies which
telemetry, source, state, and charger-control fields are meaningful. Callers must not
interpret fields whose matching `TABOS_BATTERY_VALID_*` bit is clear. INA226 read or
charger-control failures move registry device `battery0` to `fault`; a successful retry
restores `ready`.

## Power-management development

Automatic idle dimming and screen-off are enabled; transparent suspend is not yet available. The
[power baseline](power-baseline.md) records initialized services, suspend blockers,
GPIO interrupt ownership, pinned-SDK restrictions, unverified wake paths, and the
repeatable measurement worksheet. Functional sleep/wake and instrumented power
measurements remain separate validation gates.

Display policy uses three deadlines from the same last physical activity:

- 60 seconds: dim to at most 20%.
- 180 seconds: backlight off, panel enabled; touch or keyboard restores active brightness.
- 300 seconds: backlight off and panel disabled; keyboard restores the display.
  Pointer activity no longer restores it or resets inactivity at this final stage.

The CPU, applications, networking, and timers continue running at every stage.
Dimming and screen-off are display power savings, separate from system sleep; actual
system sleep will also require the screen to be off. Fullscreen graphics, open audio/camera
streams, and held keys/contacts inhibit both dimming and screen-off. Releasing the final
inhibitor restarts all inactivity deadlines. Kernel panic restores the display and inhibits
idle blanking so failure output stays visible.

Tab5 independently controls backlight brightness and panel display enablement.
It retains panel/touch power rails, framebuffer allocations,
and continuous DMA/VSYNC scanout. This is not scanout quiescence or controller sleep.
Panel disable follows successful backlight shutdown; restoration enables the panel before
raising backlight brightness. SDL presents black using zero texture
brightness; framebuffer and screenshot pixels remain intact. Background rendering does
not turn the screen back on.

Failed display operations invalidate the corresponding brightness or panel status and retain the last
successful value for diagnostics. There is no periodic off retry; later activity or a
policy change can retry restoration. Physical screen-off/touch restoration and incremental
current savings still require validation on each supported display revision.

The previous panel-display-off implementation was tested on the current ST7121 board:
the operator reported 0.08–0.09 A active, 0.04 A after dimming, and 0.01 A after screen-off.
Keyboard restored the screen without a blue flash. Touch did not restore from screen-off,
but `touchtest` worked after keyboard restoration. This suggests the panel command may
suppress touch reporting; the cause is not yet proven. The backlight-only trial isolates
that command. Repeat the same readings and touch/keyboard checks before comparing savings;
the previous 0.01 A reading does not describe this trial. Meter/setup limitations still apply.

For the backlight-only trial at 180 seconds, the operator reports a predominantly
0.02 A reading, fluctuating between 0.01 A and 0.03 A. This is a typical displayed
value and observed range, not a sampled mean. The operator confirms that tapping the
screen restores it from backlight-only off. The typical reading is 0.01 A above the earlier panel-off
reading, but meter resolution and uncontrolled variation limit the comparison.

TabOS now contains an internal portable power-state manager and deterministic host
simulation used for development tests. After 60 seconds without physical keyboard or
pointer activity, display dims from default 75% active brightness to 20%. If active setting
is below 20%, dimming never raises it. Physical key presses/releases, active pointer events,
held keys, and active contacts restore or hold active brightness, except pointer activity
after the final panel-off stage. Software key repeat,
cursor blink, background output, and service completions do not reset idle time.

Fullscreen graphics and open audio or camera streams inhibit dimming, screen-off, and suspend. Brightness
restores when inhibitor begins; final inhibitor release starts fresh 60-second interval.
Framebuffer pixels, terminal contents, display ownership, and input ordering remain intact.
Host SDL applies dimming only while presenting texture, so framebuffer and screenshots retain
original pixel values. Brightness failures remain recorded internally with desired and last
known effective values. No public power configuration API or Tab5 light sleep exists yet.

Physical Tab5 validation confirms dimming after 60 seconds, restoration from touch and
keyboard input, no dimming during fullscreen `gdemo`, and no dimming beneath a held contact.
Captured USB-C setup reads 0.07–0.09 A active and 0.04 A dimmed, a coarse 43–56% reduction.
Meter precision and two-second stability interval limit accuracy; full setup appears below.

Debug firmware also reports cumulative `Platform activity:` counters beside the
60-second runtime wake report. They expose codec, headphone-monitor, display-refresh
and accelerator work that can continue while the runtime is blocked. See the baseline
for units, wrap handling and measurement limits.

Audio hardware is now demand-driven. With no open audio streams, codec transfers and
headphone-jack polling are stopped and speaker routing is disabled. Opening the first stream
starts codecs at its requested sample rate before returning; closing the last stream stops
them again. Speaker playback samples headphone state before enabling speaker, then retains
50 ms insertion detection while that route remains active. A failed hardware start returns
an I/O error and can be retried by a later open.

This removes known idle audio work but does not establish whole-system current savings.
Compare quiet-shell `Platform activity:` deltas before and after, then record current with
same supply, charger, peripherals, brightness, and measurement interval. Display scanout
still runs continuously: pinned ESP-IDF has no public retained-framebuffer DPI pause API,
so TabOS does not mislabel panel display-off as scanout quiescence.

Physical Phase 3 check played `audiotest tone speaker` correctly. Codec logs show ES8388
playback and ES7210 capture opening at 44.1 kHz. Following diagnostic report contains 199
audio chunks and 41 headphone reads, matching roughly the two-second tone at 10 ms and
50 ms cadences rather than continuous idle operation. Display dimmed about 60 seconds after
stream close.

Power was measured at the Tab5 USB-C input with a generic inline USB meter reading 5.12 V.
No battery was connected and charging was disabled. Keyboard and SD were attached, USB-A
was connected to an unpowered host, and Wi-Fi was connected. After readings remained stable
for two seconds, idle shell at 75% brightness varied from 0.07 A to 0.09 A; 20% dimmed idle
read 0.04 A. This is reproducible coarse whole-system evidence. Meter resolution and short
sampling do not support precise energy or isolated Phase 3 savings claims. Available
equipment cannot intercept the battery-only path, so battery-powered current cannot be
reported; power validation is limited to USB-C input measurements.
