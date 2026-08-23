# Display System

TabOS provides one shared boot-console display path across Tab5, macOS, and Linux. This is a kernel boot facility and terminal-rendering foundation, not yet a public application graphics API.

## Logical Framebuffer

Portable code renders into a 1280×720 RGB565 framebuffer. Each pixel is a 16-bit value and the framebuffer reports its width, height, and row stride through the internal platform interface.

Portable graphics code supplies an embedded 8×12 CP437 bitmap font, scaled text rendering, and an RGB565 terminal framebuffer with cursor movement, line wrapping, color selection, clearing, and scrolling.

Tab5 terminal raster spans may use private ESP32-P4 PIE SIMD fill/copy kernels. PPA
continues to handle display rotation and supported bulk graphics operations, including
logical-canvas scaling directly into native scanout orientation. Hosts use SDL surface
operations for matching fills and opaque nearest-neighbor scaling, with scalar fallback.
Applications and terminal callers do not select accelerators.

Font rendering and glyph data are separate. `graphics/font.c` contains drawing and byte-to-glyph indexing. `graphics/blueterm.f12` is the default raw 3072-byte font asset: 256 glyphs in CP437 byte order, with twelve one-byte rows per 8-pixel glyph. Build-generated assembly uses `.incbin` to place the raw file in firmware or host executable; no generated C bitmap table exists.

Compile-time font settings live in `config/Font.cmake`. They select asset path, glyph width, glyph height, available glyph count, and terminal cell dimensions. Fonts may contain 1 through 256 fixed-width glyphs. Requested byte values beyond configured glyph count render glyph zero. Rows are packed most-significant bit first and use `ceil(width / 8)` bytes, so widths above eight pixels are supported. Unused low bits in final row byte should be zero. Build configuration rejects missing assets, invalid dimensions, counts above 256, cells smaller than glyphs, and files whose exact size does not match `glyph_count × glyph_height × ceil(glyph_width / 8)`.

Default renderer can address all 256 CP437 glyphs. Console text still assigns control behavior to backspace, tab, newline, and carriage return bytes, and public strings terminate at byte zero. Displaying those control-range glyphs through terminal output will require a future explicit glyph-write or Unicode-to-CP437 API.

All targets use identical terminal scaling. Change `TABOS_TERMINAL_SCALE` in `config/Display.cmake` and rebuild to adjust compiled default. Default is 2. The 8×12 glyph becomes 16×24 pixels inside a 16×30-pixel terminal cell, producing exactly 80 columns and 24 rows on the 1280×720 display. Glyphs have no added horizontal spacing; six vertical pixels remain between rows. `TABOS_TERMINAL_SCROLLBACK_LINES` controls additional retained history rows and defaults to 256. Keeping settings shared makes macOS/Linux output faithful size, wrapping, and history preview of Tab5 output.

SDL host graphics applications use renderer VSYNC when available. Timer pacing uses
`TABOS_HOST_REFRESH_RATE_HZ`, default 58 Hz, only as fallback. Terminal updates remain
event-driven, and headless tests are not artificially delayed. Configure the fallback
value with `./tools/tabos config`.

Software can change scale from 1 through 8 at runtime using `tabos_terminal_set_scale()` from `<tabos/terminal.h>` and inspect it with `tabos_terminal_get_scale()`. A value set before runtime startup becomes initial boot scale. A value set after startup rebuilds geometry, reflows retained hard and soft-wrapped cell history, preserves colors and cursor, and redraws current viewport. Runtime value is retained for life of running system but is not yet persisted across restarts; persistent preference storage will depend on future filesystem/configuration service.

At startup, the kernel creates one structured boot report and writes it to both the platform log and the terminal framebuffer. The report identifies the TabOS version, target, detected display, logical framebuffer, processor, memory, storage state, and kernel runtime state. Tab5 also reports internal heap availability, PSRAM availability, and physical flash capacity. This prevents serial and on-screen diagnostics from evolving as separate lists.

Memory values are captured after display initialization, so Tab5 free-PSRAM reporting includes the cost of active display buffers. Flash capacity describes the physical flash chip; it is not filesystem free space. Until a filesystem is mounted, storage is explicitly reported as not mounted rather than showing an invented free-space value. Host builds report total host RAM when SDL can determine it, but label free memory as unknown because it is managed by the host operating system.

## Host Presentation

macOS and Linux upload the logical framebuffer to an SDL3 RGB565 streaming texture. SDL preserves the 16:9 aspect ratio and letterboxes the image when the window has a different shape.

The host window remains resizable and remembers its last valid screen position. Headless smoke tests create the framebuffer and texture without showing a window.

## Tab5 Presentation

The Tab5 target uses the official `espressif/m5stack_tab5_noglib` BSP component, pinned by the ESP-IDF dependency lock. Display buffers are allocated in external PSRAM. PIE SIMD acceleration is enabled by default and can be disabled through `./tools/tabos config`; optional boot diagnostics compare scalar and PIE internal-RAM and PSRAM throughput over serial.

At boot, the platform detects and reports the installed display controller over serial in both debug and release builds. Current detection supports ILI9881C, ST7123, and ST7121 Tab5 variants. The detected controller name is also available through the platform interface for a future on-screen boot driver list.

TabOS keeps its shared logical orientation at 1280×720. Before presenting, the Tab5 backend rotates that frame counter-clockwise into the panel's native 720×1280 layout, sends it to the LCD, and then enables the backlight. The pure rotation operation is covered by a host unit test.

The BSP integration deliberately excludes LVGL. UI composition remains owned by TabOS and will be designed separately.

## Validation

Build and test the host path:

```sh
./tools/tabos macos debug build
./tools/tabos macos debug test
./tools/tabos macos debug run
```

Use `linux` in place of `macos` on Ubuntu.

Build the firmware with the ESP-IDF v5.4.4 environment activated:

```sh
./tools/tabos tab5 debug build
```

Flashing remains an explicit hardware action:

```sh
./tools/tabos tab5 debug flash
```

Monitor Tab5 serial output using matching build configuration. Debug is default, and
`ESPPORT` may select device when automatic detection is ambiguous. Exit with `Ctrl+]`:

```sh
./tools/tabos tab5 monitor
ESPPORT=/dev/cu.usbmodem211201 ./tools/tabos tab5 release monitor
```

For container-built firmware, use local `esptool` through the standalone flash helper. It defaults to the debug image:

```sh
./tools/flash.sh
./tools/flash.sh release
```

Set `ESPPORT` when more than one serial device is connected.

After flashing, verify that the boot report is readable, starts at the physical top-left in landscape orientation, and names the detected display controller. Compare it with the corresponding report on the serial port. Automated builds validate compilation, font pixels, terminal state, and pixel transforms, but only this hardware check can validate the physical panel path.

## Current Limits

Static content is not continuously rerendered: Tab5 LCD hardware refreshes from its scanout framebuffer while platform run loop sleeps. Terminal tracks dirty cells, so text and cursor changes avoid rerendering unchanged cells; viewport shifts, clear, and scale changes redraw full view. Current platform presentation contract still submits framebuffer after changes. Display has no compositor, widgets, touch input, or general application graphics API. Shell will be built as application using console and input services, not embedded in kernel.
