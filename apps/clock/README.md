# Clock

Clock is a standalone graphical UTC wall clock for TabOS. It uses a 640x360 logical
canvas, a large seven-segment time display, and a compact date in the upper-right
corner. TabOS scales the canvas to the physical display.

Run it from the shell:

```sh
clock
```

Press Q or Escape to return to the shell. Clock reads the standard `time()` API and
always displays UTC because TabOS does not currently implement timezone rules.

Build and install it into the host root filesystem with:

```sh
make -C apps/clock
```

The display is rendered from programmatic seven-segment shapes and an original small
bitmap alphabet. It contains no third-party graphical assets.
