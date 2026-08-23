# Application Graphics API

TabOS supports foreground fullscreen RGB565 graphics. Applications draw through
`<tabos/graphics.h>` and do not receive physical framebuffer or platform objects.

```c
tabos_graphics_t graphics;
tabos_graphics_open(&graphics);
tabos_graphics_clear(&graphics, TABOS_RGB565(0, 0, 0));
tabos_graphics_fill_rect(&graphics, 20, 20, 100, 60, TABOS_RGB565(255, 0, 0));
tabos_graphics_line(&graphics, 0, 0, 127, 127, TABOS_RGB565(255, 255, 255));
tabos_graphics_rect(&graphics, 10, 10, 120, 80, TABOS_RGB565(0, 128, 255));
tabos_graphics_present(&graphics);
tabos_graphics_close(&graphics);
```

Coordinates are clipped. Pixel, line, outline rectangle, and filled rectangle helpers
are available. `tabos_graphics_blit()` copies an RGB565 bitmap.

`tabos_graphics_blit_ex()` adds source and destination rectangles, nearest-neighbor
scaling, quarter-turn rotation, mirroring, fixed opacity, and an inclusive RGB565
color-key range. Query portable behavior and active acceleration with
`tabos_graphics_capabilities()`.

Drawing calls enqueue commands. Their source buffers must remain valid and unchanged
until `tabos_graphics_present()` returns. Present is the completion fence and scanout
boundary. A full command queue drains old work until room exists. Closing or terminating
an application also fences pending work and restores the terminal.

The host uses the portable software renderer. Tab5 uses ESP32-P4 PPA for supported
fills, opaque transforms, and framebuffer rotation. Unsupported operations and hardware
failures transparently use identical software rendering. PPA and framebuffer pointers
are never exposed to applications.

Tab5 may choose CPU rendering for operations unsupported by PPA. Applications see the
same queued API and output regardless of which backend executes an individual command.

Fullscreen Tab5 graphics render directly into the native portrait scanout framebuffer.
TabOS maps the public 1280×720 landscape coordinates into native memory per operation,
so application presentation does not rotate an entire frame. Terminal rendering keeps
its separate logical framebuffer and presentation path.

Tab5 uses two native scanout framebuffers. Commands render into the back buffer, and
`tabos_graphics_present()` submits it at VSYNC before exchanging front and back buffers.
This caps visible presentation at the panel refresh rate and prevents partial frames and
terminal contents from appearing during graphics applications.

The macOS and Linux SDL hosts pace graphics `present()` calls to 58 Hz by default so
animation timing resembles the ST7121 Tab5 panel. Change `TABOS_HOST_REFRESH_RATE_HZ`
through `./tools/tabos config` when another emulated refresh rate is needed. Headless
tests remain unpaced.

Opening fullscreen graphics automatically hides and pauses the terminal cursor. Closing
the graphics context—or exiting without closing it—restores the terminal and cursor.

Run `graphics-demo`; use W/A/S/D to move, R to rotate, and Q to exit. Run
`graphics-benchmark` to measure 120 queued frames and report active acceleration.
