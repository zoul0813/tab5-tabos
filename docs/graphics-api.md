# Application Graphics API

TabOS supports foreground fullscreen RGB565 graphics. Applications draw through
`<tabos/graphics.h>` and do not receive physical framebuffer or platform objects.

```c
tabos_graphics_t graphics = {0};
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

Applications request a smaller integer-scaled canvas by initializing `width` and
`height` before calling `tabos_graphics_open()`:

```c
tabos_graphics_t graphics = {.width = 320, .height = 240};
tabos_graphics_open(&graphics);
```

Both dimensions must be zero for native mode or both must be nonzero for logical-canvas
mode. TabOS chooses the largest uniform integer scale that fits the physical display,
keeps pixels square, and centers the result. Unused space becomes letterbox or pillarbox
borders. For 1280×720, examples include 320×180 at 4× fullscreen and 320×240 at 3×
with a centered 960×720 image and 160-pixel side borders. Allocation must also fit the
application's available heap.

Borders default to black. `tabos_graphics_set_letterbox_color()` changes their RGB565
color for the next and following presentations, so gameplay may animate them—for example,
flashing red when the player takes damage. Each letterboxed present clears the physical
output to the current border color before drawing the centered scaled canvas.

All normal drawing and blitting calls target the requested logical dimensions.
`tabos_graphics_pixels()` returns the application-owned logical RGB565 framebuffer for
direct software rendering. `tabos_graphics_present()` performs one nearest-neighbor
full-screen upscale, transparently using Tab5 acceleration when available. Native mode
continues to hide its physical framebuffer and returns null from
`tabos_graphics_pixels()`.

TabOS composites battery and WiFi status over both console and graphics output. Battery
status uses a battery icon and percentage; charging adds a bolt. WiFi bars appear while
connected and appear grey while connecting. Disconnected WiFi has no icon.

Graphics applications may hide either overlay for their current graphics session:

```c
tabos_graphics_set_overlays(&graphics, TABOS_GRAPHICS_OVERLAY_WIFI);
tabos_graphics_set_overlays(&graphics, TABOS_GRAPHICS_OVERLAY_NONE);
tabos_graphics_set_overlays(&graphics, TABOS_GRAPHICS_OVERLAY_ALL);
```

Closing or terminating an application restores both overlays.

`tabos_graphics_blit_ex()` adds source and destination rectangles, nearest-neighbor
scaling, quarter-turn rotation, mirroring, uniform opacity, an inclusive RGB565
color-key range, and an optional destination clip rectangle. Zero-initialized clip
fields disable clipping; an enabled empty clip draws nothing successfully. RGB565 has
no per-pixel alpha channel. Assets therefore use exact color-key transparency and may
blend the whole operation with `opacity`. Query portable behavior and active acceleration with
`tabos_graphics_capabilities()`.

`<tabos/sprite.h>` and `<tabos/tilemap.h>` provide portable sprites, explicit-time
animation, ordered metasprites, writable finite maps, object markers, and camera-based
layer drawing. They lower to the same blit API and work in native and logical-canvas
modes. See `docs/tile-assets.md` for API and authoring details.

Drawing calls enqueue commands. Their source buffers must remain valid and unchanged
until `tabos_graphics_present()` returns. Present is the completion fence and scanout
boundary. A full command queue drains old work until room exists. Closing or terminating
an application also fences pending work and restores the terminal.

The SDL host uses native surface fills and nearest-neighbor scaled blits for operations
matching its accelerated path, with the portable renderer preserving all other pixel
semantics. Its bounded RV32 execution slices are sized for framebuffer workloads while
remaining cooperative with SDL input and system services; each graphics presentation is
an explicit interpreter yield boundary. Tab5 uses ESP32-P4 PPA for
supported bulk fills, transforms, logical-canvas upscaling, and framebuffer rotation.
CPU-rendered RGB565 spans and terminal glyph spans
may transparently use ESP32-P4 PIE SIMD. Unsupported operations, small spans, unsuitable
alignment, and disabled or failed acceleration use pixel-identical scalar rendering.
PPA, PIE, SDL, and framebuffer pointers are never exposed to applications.

Tab5 may choose CPU rendering for operations unsupported by PPA. Applications see the
same queued API and output regardless of which backend executes an individual command.

Fullscreen Tab5 graphics render directly into the native portrait scanout framebuffer.
TabOS maps the public 1280×720 landscape coordinates into native memory per operation,
so application presentation does not rotate an entire frame. Opaque logical-canvas
presentation combines nearest-neighbor scaling with the required scanout rotation in one
PPA operation. Terminal rendering keeps its separate logical framebuffer and presentation
path.

Tab5 uses two native scanout framebuffers. Commands render into the back buffer, and
`tabos_graphics_present()` submits it at VSYNC before exchanging front and back buffers.
This caps visible presentation at the panel refresh rate and prevents partial frames and
terminal contents from appearing during graphics applications.

The macOS and Linux SDL hosts use renderer VSYNC when available for smooth presentation
at the desktop display cadence. If VSYNC is unavailable, they pace graphics `present()`
calls to 58 Hz by default. Change fallback `TABOS_HOST_REFRESH_RATE_HZ` through
`./tools/tabos config`. Headless tests remain unpaced.

Opening fullscreen graphics automatically suspends terminal rendering, presentation,
cursor blinking, and TTY scrollback shortcuts. Keyboard events—including Ctrl+Arrow—go
to the graphics application without requiring it to know about TTY modes. Terminal state
remains retained but cannot overwrite or present over fullscreen graphics. Closing the
graphics context—or exiting without closing it—redraws and restores the terminal once.

Run `graphics-demo` to exercise a 320×240 logical canvas automatically scaled 3× with
pillarboxing; use E/A/S/D to move, R to rotate, Up/Down to cycle the 16-color VGA
letterbox palette, and Q to exit. Run
`graphics-benchmark` to measure 120 queued frames and report active acceleration.
Run `tile-demo` to exercise binary assets, scrolling layers, transforms, animation,
metasprites, editable cells, flags, and object markers. Arrow keys scroll, E toggles a
wall cell, and Q exits.
