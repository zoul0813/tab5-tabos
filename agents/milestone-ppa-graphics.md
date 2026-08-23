# ESP32-P4 PPA Graphics Acceleration Milestone

## Summary

Add a portable queued RGB565 graphics command surface backed by ESP32-P4 PPA
hardware. Keep PPA hidden behind the TabOS platform boundary. Host targets use a
matching software renderer. PIE SIMD remains a future optimization.

Drawing operations enqueue work. `tabos_graphics_present()` is the completion
fence: it waits for all commands, rotates the framebuffer, and submits scanout.
When the bounded queue fills, TabOS drains older work until space is available.

## Public API and Architecture

- Add portable capability bits for transforms, blending, color-keying, queued
  commands, and active hardware acceleration.
- Add an extended RGB565 bitmap operation with source and destination rectangles,
  scaling, 0/90/180/270-degree rotation, horizontal/vertical mirroring, fixed
  opacity, and an optional color-key range.
- Keep existing clear, fill, and blit calls source-compatible.
- Require source buffers to remain valid and unchanged until `present()` returns.
- Preserve command order and make present, close, and process cleanup fence pending work.
- Keep command storage fixed-capacity with no per-command heap allocation.
- Never expose ESP-IDF, PPA handles, PIE instructions, or native pixel types in
  the public SDK.

## Tab5 Backend

- Add `esp_driver_ppa` as a private platform dependency.
- Use 64-byte-aligned PSRAM buffers and correct cache synchronization.
- Register SRM, blend, and fill clients during display initialization.
- Accelerate presentation rotation, fills, scaling, rotation, mirroring,
  fixed-alpha blending, and RGB565 color-key composition where supported.
- Render fullscreen graphics applications directly into the native portrait scanout
  buffer with logical landscape coordinate mapping, avoiding a full-frame rotation
  on every application frame. Keep terminal presentation on its existing path.
- Use two native DPI framebuffers and switch them at VSYNC so applications never draw
  into the buffer currently being scanned by the panel.
- Use nonblocking PPA transactions and completion synchronization while preserving
  command order.
- Reuse an aligned scratch surface for transform-plus-blend work.
- Log one warning and fall back to software when initialization or an operation fails.
- Advertise hardware acceleration only while the PPA path remains usable.

## Host, Examples, and Documentation

- Implement pixel-identical software semantics for every operation.
- Expand `graphics-demo` to exercise transforms, opacity, and color-keyed sprites.
- Add a benchmark application reporting frame timing and active capabilities.
- Document API behavior, source-buffer lifetime, and present fencing in `docs/`.

## Validation

- Test command ordering, saturation/draining, present fencing, cleanup, and capabilities.
- Pixel-test transforms, clipping, opacity, color keys, and combined operations.
- Force acceleration failure and verify software fallback output.
- Build and test macOS, Linux, Tab5, and independently compiled applications.
- Validate orientation and terminal restoration on supported Tab5 display revisions.
- Benchmark CPU and PPA presentation paths on physical hardware.

## Fixed Decisions

- Fullscreen RGB565 remains the public graphics format.
- Commands are asynchronous; `present()` is the fence and scanout boundary.
- Queue pressure blocks and drains work instead of returning `EAGAIN`.
- Software fallback is mandatory.
- PPA is private; PIE and direct application SIMD are out of scope.
- Per-pixel alpha and ARGB8888 are out of scope.
