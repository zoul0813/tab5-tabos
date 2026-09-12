# Project Configuration

Display power settings are runtime configuration, separate from build options below.
Edit `T:/etc/power.conf` to set idle dim/backlight-off/panel-off seconds and normal/dim
brightness, then reboot. See [display configuration](power.md#display-configuration)
for the INI format, defaults, limits, and recovery behavior.

TabOS works without a saved project configuration. Built-in defaults start
`T:/bin/shell` on both host and Tab5 targets. To override TabOS-owned options, use:

```sh
./tools/tabos config
```

Configuration is stored in `.local/tabos.config`, which is ignored by Git. Every
host build, Tab5 build, and Tab5 flash passes saved values explicitly to CMake.
Settings therefore survive build-directory cleanup and do not rely on prior CMake
cache contents.

Current settings cover:

- host startup application, defaulting to `shell`: `none`, `console-test`, `filesystem-test`, `elf-hello`, or `shell`
- Tab5 startup application, defaulting to `shell`: `none`, `console-test`, `filesystem-test`, `elf-hello`, or `shell`
- raw bitmap font file
- font glyph width, height, and glyph count (1 through 256)
- terminal font cell width and height
- terminal scale
- terminal scrollback capacity
- cursor blink interval
- held-key repeat delay and interval
- Tab5 PIE SIMD acceleration and optional serial boot diagnostics
- controlled host drive-container directory
- maximum files open across TabOS (default 32, range 1 through 255)
- maximum directories open across TabOS (default 8, range 1 through 255)
- filesystem-backed ELF startup path, defaulting to `T:/bin/hello`
- filesystem-backed PID 0 shell path, defaulting to `T:/bin/shell`

The host executes `elf-hello` from configured TabOS drive through bounded RV32
interpretation using same application artifact as Tab5.
Font paths may be relative to the repository root or absolute. The configurator
checks that the file size exactly matches the configured packed bitmap dimensions.
Fonts with fewer than 256 glyphs are valid; character values outside their range
use glyph 0.

The host filesystem root defaults to `.local/rootfs`. Relative paths are resolved
from the repository root. Host applications cannot escape this directory through
`..` components or symbolic links.

ESP-IDF `menuconfig` remains the advanced interface for ESP32-P4-specific settings.
It is independent of TabOS project configuration:

```sh
eval "$(./tools/tabos activate-idf)"
idf.py -C targets/tab5 -B build/tab5-debug menuconfig
```

Tab5 uses a custom 2 MiB single-factory-app partition on the device's 16 MiB
flash. TabOS does not currently reserve OTA application partitions; future
internal `A:` filesystem partitioning remains separate work.

## Provisional GUI Surface Budgets

`TABOS_GUI_SURFACE_BYTES` defaults to 12582912 (12 MiB aggregate), and
`TABOS_GUI_PROCESS_SURFACE_BYTES` to 6291456 (6 MiB per owner). They count both
retained and temporary staging images. Set them through the existing project
configuration workflow; wrappers pass them to macOS and Tab5 builds. Values must
be positive integers no greater than 33554432, with per-owner no greater than total.

These are bounded prototype defaults, pending physical Tab5 peak and timing
validation. They do not include client canvases, app heaps, compositor/scanout,
executables or OS memory, and do not reserve RAM for fullscreen games.
