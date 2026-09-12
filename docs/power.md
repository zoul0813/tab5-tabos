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
restores `ready`. Both state changes are published immediately after the operation.

## Power-management development

Automatic idle dimming is enabled; transparent suspend is not yet available. The
[power baseline](power-baseline.md) records initialized services, suspend blockers,
GPIO interrupt ownership, pinned-SDK restrictions, unverified wake paths, and the
repeatable measurement worksheet. Functional sleep/wake and instrumented power
measurements remain separate validation gates.

TabOS now contains an internal portable power-state manager and deterministic host
simulation used for development tests. After 60 seconds without physical keyboard or
pointer activity, display dims from default 75% active brightness to 20%. If active setting
is below 20%, dimming never raises it. Physical key presses/releases, active pointer events,
held keys, and active contacts restore or hold active brightness. Software key repeat,
cursor blink, background output, and service completions do not reset idle time.

Fullscreen graphics and open audio or camera streams inhibit dimming and suspend. Brightness
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
