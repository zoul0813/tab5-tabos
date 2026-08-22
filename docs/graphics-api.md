# Application Graphics API

TabOS ABI v1 supports foreground fullscreen RGB565 graphics. Applications draw through
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
are available. `tabos_graphics_blit()` copies an RGB565 bitmap. Closing or
terminating a graphics application restores the terminal. Windowing, compositing,
transparency, touch, and direct framebuffer access are not part of this API.

Opening fullscreen graphics automatically hides and pauses the terminal cursor. Closing
the graphics context—or exiting without closing it—restores the terminal and cursor.

Run `graphics-demo`; use W/A/S/D to move and Q to exit.
