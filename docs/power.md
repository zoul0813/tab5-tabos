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

Transparent suspend and automatic idle dimming are not yet available. The
[power baseline](power-baseline.md) records initialized services, suspend blockers,
GPIO interrupt ownership, pinned-SDK restrictions, unverified wake paths, and the
repeatable measurement worksheet. Functional sleep/wake and instrumented power
measurements remain separate validation gates.

TabOS now contains an internal portable power-state manager and deterministic host
simulation used for development tests. This does not add an application API or enable
automatic dimming or Tab5 light sleep. Those behaviors remain disabled until later
power phases add service participation, safety gates, and hardware validation.

Debug firmware also reports cumulative `Platform activity:` counters beside the
60-second runtime wake report. They expose codec, headphone-monitor, display-refresh
and accelerator work that can continue while the runtime is blocked. See the baseline
for units, wrap handling and measurement limits.
