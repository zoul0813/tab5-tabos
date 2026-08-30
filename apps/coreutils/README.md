# TabOS Core Utilities

Core utilities are small, independently loaded TabOS programs. Each utility has its own
source directory and produces its own ELF binary.

Build every utility:

```sh
make -C apps/coreutils
```

Build one utility:

```sh
make -C apps/coreutils ls
make -C apps/coreutils mkdir
make -C apps/coreutils date
make -C apps/coreutils reboot
make -C apps/coreutils shutdown
make -C apps/coreutils battery
```

Use `build-ls`, `build-mkdir`, or `build-date` to compile without installing. Runnable
binaries install to `.local/rootfs/T/bin/` without filename extensions.

`reboot` immediately performs an orderly system restart. `shutdown` immediately
unmounts storage and requests system power-off. Neither command schedules a delayed
action.

`battery` reports Tab5 battery telemetry. `battery charge on|off` controls normal
charging. `battery fast on|off` manually controls fast charging; fast charging is
disabled by default.
