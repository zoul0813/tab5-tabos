# Project Configuration

TabOS-owned options are configured through an interactive project command:

```sh
./tools/tabos config
```

Configuration is stored in `.local/tabos.config`, which is ignored by Git. Every
host build, Tab5 build, and Tab5 flash passes saved values explicitly to CMake.
Settings therefore survive build-directory cleanup and do not rely on prior CMake
cache contents.

Current settings cover:

- host startup application: `none`, `console-test`, `filesystem-test`, or `elf-hello`
- Tab5 startup application: `none`, `console-test`, `filesystem-test`, or `elf-hello`
- raw bitmap font file
- font glyph width, height, and glyph count (1 through 256)
- terminal font cell width and height
- terminal scale
- terminal scrollback capacity
- cursor blink interval
- controlled host filesystem root

The host executes `elf-hello` through bounded RV32 interpretation using same application artifact as Tab5.
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
