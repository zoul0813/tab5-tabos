# Display System

TabOS currently provides one shared diagnostic display path across Tab5, macOS, and Linux. This is the first hardware-backed vertical slice, not a public application graphics API.

## Logical Framebuffer

Portable code renders into a 1280×720 RGB565 framebuffer. Each pixel is a 16-bit value and the framebuffer reports its width, height, and row stride through the internal platform interface.

The startup diagnostic frame contains color bars, a grayscale ramp, a white border, and distinct corner markers. All targets render this exact portable frame, making orientation, color, clipping, and scaling faults visible.

## Host Presentation

macOS and Linux upload the logical framebuffer to an SDL3 RGB565 streaming texture. SDL preserves the 16:9 aspect ratio and letterboxes the image when the window has a different shape.

The host window remains resizable and remembers its last valid screen position. Headless smoke tests create the framebuffer and texture without showing a window.

## Tab5 Presentation

The Tab5 target uses the official `espressif/m5stack_tab5_noglib` BSP component, pinned by the ESP-IDF dependency lock. Display buffers are allocated in external PSRAM.

At boot, the platform detects and reports the installed display controller over serial in both debug and release builds. Current detection supports ILI9881C, ST7123, and ST7121 Tab5 variants. The detected controller name is also available through the platform interface for a future on-screen boot driver list.

TabOS keeps its shared logical orientation at 1280×720. Before presenting, the Tab5 backend rotates that frame counter-clockwise into the panel's native 720×1280 layout, sends it to the LCD, and then enables the backlight. The pure rotation operation is covered by a host unit test.

The BSP integration deliberately excludes LVGL. UI composition remains owned by TabOS and will be designed separately.

## Validation

Build and test the host path:

```sh
./tools/tabos build macos debug
./tools/tabos test macos debug
./tools/tabos run macos debug
```

Use `linux` in place of `macos` on Ubuntu.

Build the firmware with the ESP-IDF v5.4.4 environment activated:

```sh
./tools/tabos build tab5 debug
```

Flashing remains an explicit hardware action:

```sh
./tools/tabos flash tab5 debug
```

For container-built firmware, use local `esptool` through the standalone flash helper. It defaults to the debug image:

```sh
./tools/flash.sh
./tools/flash.sh release
```

Set `ESPPORT` when more than one serial device is connected.

After flashing, verify that the full white border and all four corner markers are visible, the color bars are correct, and the frame is in landscape orientation. Automated builds validate compilation and pixel transforms, but only this hardware check can validate the physical panel path.

## Current Limits

The display currently presents one startup frame. It has no redraw loop, damage tracking, compositor, fonts, widgets, touch input, or public application-facing API. Those features should build on this framebuffer boundary without leaking SDL3 or ESP-IDF types into portable code.
