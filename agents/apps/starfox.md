# Retro Rail-Shooter Implementation Plan

Status: proposed implementation plan, 2026-09-12. Implementation has not started.

This plan targets an original low-poly rail shooter inspired by SNES-era Star Fox. `starfox` is a working project label only; release name, code, art, models, music, sound, characters, levels, and story must be original. Do not use Nintendo assets, trademarks, game data, or reverse-engineered source.

Proposed API names, resource budgets, and performance thresholds are recommendations, not new `[DECIDED]` architecture requirements. Measure physical Tab5 behavior before committing conditional platform work.

## Outcome and Scope

Deliver an independently compiled TabOS game with:

- A fixed forward-moving rail-shooter camera.
- Low-poly ships, enemies, obstacles, projectiles, and terrain or space structures.
- Flat-shaded opaque geometry, sprite effects, HUD, scoring, damage, checkpoints, and one complete replayable level.
- Keyboard-first controls, sound effects, music or looping ambience, persistent high scores and settings, pause/restart/quit flow, and clean return to shell.
- Same RV32 application artifact running on Tab5 and through macOS/Linux host interpreter.
- `320x180` RGB565 output scaled exactly 4x to `1280x720`.
- 30 FPS normal target and 20 FPS heavy-scene floor on physical Tab5.

Initial renderer supports near-plane clipping, back-face culling, opaque flat-shaded triangles, painter ordering and/or a 16-bit depth buffer, unlit sprites, and palette-controlled fog. Defer perspective-correct textures, dynamic lighting, shadows, skeletal animation, free-roaming worlds, network play, user-generated levels, and a general-purpose 3D engine.

## Feasibility and Current TabOS Evidence

ESP32-P4 has no conventional 3D GPU. Its PPA accelerates fill, scaling, rotation, mirroring, blending, and color conversion; it does not perform vertex transforms, triangle setup, depth testing, or perspective texture mapping. ESP32-P4 PIE supplies 128-bit SIMD operations useful for fixed-point transforms and raster spans, while its high-performance cores also include single-precision floating-point hardware. Keep both details behind TabOS portability rules rather than exposing ESP-IDF or native handles to applications.

Relevant references:

- [ESP32-P4 PPA documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/ppa.html)
- [ESP32-P4 platform overview](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html)
- [Espressif PIE SIMD reference](https://github.com/espressif/esp-dl/blob/master/tools/agents/skills/esp32p4-pie-simd/SKILL.md)
- [TabOS graphics API](../../docs/graphics-api.md)
- [TabOS ELF contract](../../docs/elf-loader.md)
- [PPA milestone](../milestone-ppa-graphics.md)
- [PIE milestone](../milestone-pie-simd.md)
- [Starfall milestone](../milestone-game-starfall.md)

Current TabOS already provides most MVP services:

| Surface | Current behavior and consequence |
| --- | --- |
| Graphics | SDK-owned RGB565 logical canvas, direct pixel pointer, clipped 2D operations, queued blits, explicit present, 4x integer scale, PPA presentation, native double buffering, and VSYNC. Sufficient for software 3D MVP. |
| Input | Raw key-down/up events and keyboard wait source. Game can maintain held controls; verify overflow/focus-reset behavior before final acceptance. |
| Audio | Nonblocking signed-16-bit playback streams, status, volume, flush, and wait-source integration. Sufficient for bounded game mixing. |
| Time/waits | Monotonic milliseconds and finite multi-source waits. Sufficient for 30 Hz fixed-step scheduling; standard monotonic `clock_gettime()` can provide finer profiling if current libc path preserves useful resolution. |
| Storage | Normal file access supports external models, level data, PCM/music, settings, and scores without inflating ELF image. |
| Process lifecycle | Foreground ownership, resource cleanup, and shell restoration exist. Graphics/audio/input teardown still needs game-specific stress validation. |
| Application ISA | Shared SDK currently compiles RV32I although Tab5 and host interpreter support integer multiply/divide capability. This is biggest likely CPU-side limitation for fixed-point 3D. |

A `320x180` RGB565 color buffer consumes 115,200 bytes. A matching 16-bit depth buffer consumes another 115,200 bytes. Two color buffers plus one depth buffer consume 345,600 bytes before renderer state and assets, comfortably below a provisional multi-megabyte heap request, subject to physical PSRAM measurement alongside TabOS display buffers and services.

## Effort Estimate

One engineer, assuming existing TabOS services remain stable:

| Deliverable | Estimated effort |
| --- | ---: |
| Renderer and rotating-model technical demo | 3–6 weeks |
| Playable vertical slice with one encounter | 3–5 months total |
| Polished short game with one complete level | 8–14 months total |
| Content/polish approaching commercial Star Fox scale | 18–30+ person-months |

Art production, level design, audio, balancing, accessibility, and physical-device tuning dominate after renderer proof. A reusable kernel 3D service would add roughly 2–4 months and is not justified before application-local profiling.

## API and Platform Decisions

### Required for MVP

No new public graphics API is required. Render directly into `tabos_graphics_pixels()` and call `tabos_graphics_present()` once per visible frame. Keep models, camera, clipping, projection, depth, and triangle rasterization in application code.

Before optimizing game code, evaluate upgrading shared application compilation from RV32I to non-compressed RV32IM while keeping `ilp32`. Hardware multiply/divide should materially improve fixed-point matrices, projection, clipping, edge stepping, and collision calculations. Host `mini-rv32ima` already claims M support, but loader, relocations, libc/libgcc output, host interpreter correctness, and every bundled application must be validated before changing shared defaults.

Do not expose PPA clients, PIE registers/instructions, FreeRTOS tasks, framebuffer addresses, cache primitives, or ESP-IDF types through public SDK.

### Conditional After Measurement

Add nothing solely because it appears useful. Use frame-stage measurements from physical Tab5 to select one response:

1. If transform/raster work dominates, first optimize application fixed-point algorithms and memory access.
2. If `present()`/VSYNC stalls prevent overlap with next frame, design a portable double logical-canvas acquire/submit API with explicit completion fences.
3. If scalar depth/span operations remain dominant after RV32M and application optimization, prototype private PIE kernels behind a batched portable graphics operation. Require pixel-identical host fallback.
4. If one application task cannot meet budget but work is tile-parallel, compare a graphics-specific batched renderer against a general application job API. Prefer narrower graphics service unless another application proves generic jobs are needed.
5. Consider RV32F only after choosing how same executable runs on host. Options are adding F support to host interpreter or defining portable multi-ISA application packaging. Do not silently make Tab5-only binaries.

Partial-present/damage APIs do not help a full-frame 3D scene. JPEG, H.264, ISP, camera, and MIPI APIs provide no meaningful triangle-rendering acceleration. Existing PPA remains valuable for final canvas upscale, large clears, HUD/sprite composition, fades, and framebuffer handling.

## Phase 0: Performance and ISA Proof

Build a deterministic standalone benchmark before game systems.

- [ ] Define representative scene: camera, 32–64 objects, clipped geometry, sprites, and HUD load.
- [ ] Implement reference fixed-point matrix/vector operations and screen projection using current RV32I build.
- [ ] Implement clipped flat-triangle reference rasterizer with deterministic framebuffer hash.
- [ ] Measure transforms, clipping, rasterization, depth clear, sprite/HUD work, present, VSYNC wait, and total frame separately.
- [ ] Record internal RAM versus PSRAM placement, cache alignment, heap use, stack high-water mark, and thermal/power behavior.
- [ ] Run existing graphics benchmark before and after test scene to establish platform baseline.
- [ ] Prototype RV32IM application build without changing global default.
- [ ] Verify RV32IM artifact on host interpreter and physical Tab5, including multiply, divide, remainder, libc/libgcc helpers, faults, nested execution, and teardown.
- [ ] Compare RV32I and RV32IM stage timings and hashes.
- [ ] Decide whether RV32IM becomes shared default, explicit application requirement metadata, or remains app-local build configuration.
- [ ] Update ELF/build documentation and maintained tests if application ISA contract changes.

Exit gate: deterministic scene runs correctly on host and Tab5, resource use is recorded, and measured bottleneck supports renderer plan. Do not commit conditional graphics APIs during this phase.

## Phase 1: Application Skeleton and Assets

Suggested layout:

```text
apps/starfox/
  Makefile
  LICENSE
  README.md
  include/starfox/
  src/main.c
  src/game.c
  src/input.c
  src/audio.c
  src/storage.c
  src/assets.c
  src/level.c
  src/math_fixed.c
  src/renderer.c
  src/raster.c
  src/sprites.c
  src/hud.c
  data/
docs/starfox.md
```

- [ ] Choose original release name and keep compatibility with extensionless `T:/bin/<name>` discovery.
- [ ] Create independently compiled C17 RV32 application through shared SDK Make rules.
- [ ] Request provisional 4 MiB heap and 64 KiB stack; refine from physical high-water measurements.
- [ ] Keep executable image under current 1 MiB loaded-image and 2 MiB file limits.
- [ ] Put large models, level data, sprite sheets, audio, and music under `T:/data/<name>/`.
- [ ] Define versioned, endian-stable asset formats with explicit counts, offsets, bounds, and maximum allocation sizes.
- [ ] Add offline asset converter for meshes, fixed-point coordinates, face normals/colors, sprites, and level placements.
- [ ] Validate every asset before allocation or pointer arithmetic; fail cleanly to shell on corruption or unsupported version.
- [ ] Record source, licence, conversion settings, and hashes for every third-party tool or asset.
- [ ] Build deterministic minimal fixtures for tests; do not require full game assets in unit tests.

## Phase 2: Fixed-Point 3D Renderer

Use application-local renderer so design can evolve without freezing public OS API.

- [ ] Define fixed-point formats and overflow bounds for world positions, matrices, camera space, reciprocal/projection, depth, and screen edges.
- [ ] Use 64-bit intermediates where required; saturate or reject out-of-range values rather than invoking signed overflow.
- [ ] Implement object-to-world, world-to-view, and perspective projection transforms.
- [ ] Implement near-plane clipping before projection and viewport clipping during rasterization.
- [ ] Implement consistent winding and back-face culling.
- [ ] Implement top-left fill convention to avoid cracks and double-drawn shared edges.
- [ ] Implement flat-shaded opaque triangles first.
- [ ] Compare painter sorting against 16-bit depth buffering on representative scenes; retain simplest approach meeting correctness and performance.
- [ ] If using depth, define near/far mapping, clear value, comparison direction, precision, and tie behavior.
- [ ] Group mesh data for sequential reads and cache-friendly transformed-vertex reuse.
- [ ] Add coarse object/frustum rejection before vertex work.
- [ ] Bound visible objects, source vertices, clipped vertices, faces, and temporary command storage.
- [ ] Avoid allocation in frame loop; preallocate all renderer scratch memory.
- [ ] Add fog/palette bands to reduce distant geometry and hide far clipping.
- [ ] Add billboards for explosions, exhaust, shots, stars, and distant objects through existing RGB565 blits.
- [ ] Add HUD after 3D pass and before present.
- [ ] Preserve application buffers until present fence returns.
- [ ] Keep pixel-perfect scalar reference path for optimized-kernel comparison.

Renderer exit gate: reference and optimized paths produce expected hashes for canonical scenes; near-plane crossings, viewport edges, degenerate faces, extreme coordinates, and full occlusion are safe.

## Phase 3: Game Loop, Input, and Camera

Use fixed simulation step independent of display refresh. Initial simulation rate is 30 Hz; presentation may repeat or skip frames without changing gameplay time.

- [ ] Implement absolute monotonic deadline schedule with bounded catch-up.
- [ ] Drain input in bounded batches and track held movement/fire state from raw events.
- [ ] Ignore repeat events as state transitions.
- [ ] Verify behavior after input queue overflow, host focus loss, keyboard reset/disconnect, process focus transfer, and pause/resume.
- [ ] Clear gameplay latches whenever reliable release state is unavailable.
- [ ] Map keyboard controls suitable for Tab5 keyboard and document alternatives for host keyboards.
- [ ] Implement player movement inside constrained rail corridor, banking, firing, damage, lives, score, and invulnerability timing.
- [ ] Implement authored camera path with deterministic interpolation and look targets.
- [ ] Implement enemy/object spawn stream keyed to level distance or simulation time.
- [ ] Implement bounded projectile, collision, enemy, particle, and pickup pools.
- [ ] Use coarse spatial tests before precise collision tests.
- [ ] Define pause, restart, checkpoint, game-over, completion, and quit state transitions.
- [ ] Prevent paused duration from creating catch-up storm on resume.
- [ ] Present once per selected rendered frame and continue input/audio service when visual frame is skipped.

## Phase 4: Level and Content Pipeline

- [ ] Define one complete level structure: introduction, three encounter groups, obstacle section, mid-level escalation, boss or finale, and completion sequence.
- [ ] Create original low-poly player, enemy, obstacle, projectile, and environment models.
- [ ] Create sprite effects and HUD glyphs with explicit redistribution rights.
- [ ] Define palette, fog colors, visibility distance, and per-section visual identity.
- [ ] Add deterministic spawn scripting without embedding executable scripts in untrusted data.
- [ ] Add difficulty parameters for enemy speed, health, fire cadence, formations, and scoring.
- [ ] Add checkpoints that restore authored state without serializing raw pointers or renderer internals.
- [ ] Add attract/title, controls, pause, settings, high-score, game-over, and completion screens.
- [ ] Verify level can finish without allocation growth or entity-pool exhaustion.
- [ ] Add debug counters for visible objects, submitted/clipped/drawn triangles, shaded pixels, rejected objects, particles, and frame-stage timings.

## Phase 5: Audio and Persistence

- [ ] Choose one advertised TabOS playback rate and open one bounded stereo stream.
- [ ] Implement small application mixer for sound effects and music/ambience; avoid one stream per effect unless measured service behavior favors it.
- [ ] Handle partial writes, `EAGAIN`, writable waits, complete PCM-frame alignment, underruns, volume, pause flush, and clean close.
- [ ] Keep audio queue short enough for responsive effects while tolerating renderer jitter.
- [ ] Degrade to explicit silent mode when audio is unavailable or busy.
- [ ] Create original sound effects and music/ambience with recorded licences and conversion settings.
- [ ] Define versioned settings/high-score file containing no pointers, padding, or host-native structs.
- [ ] Save settings, volume, controls, unlocked progress, and high scores through recoverable temporary-file/rename flow.
- [ ] Treat persistence failure as nonfatal, visible, and retryable; preserve previous valid file where possible.
- [ ] Test corrupt, truncated, oversized, missing, read-only, and interrupted-save cases.

## Phase 6: Conditional Platform Optimization

Enter only if Phase 0/renderer measurements miss target after ordinary optimization.

### Async Logical Canvases

- [ ] Specify acquire/submit/wait lifecycle, buffer ownership, maximum in-flight frames, cancellation, teardown, and error recovery.
- [ ] Preserve current blocking `tabos_graphics_present()` behavior and ABI source compatibility.
- [ ] Implement matching host and Tab5 behavior with explicit completion fence.
- [ ] Ensure PPA never reads canvas while application renders into it.
- [ ] Verify nested process changes, close, faults, forced teardown, overlays, and terminal restoration.
- [ ] Keep extension only if physical frame latency or throughput materially improves.

### Batched Raster Acceleration

- [ ] Profile exact hot loops before designing public command shape.
- [ ] Prefer large batches over per-triangle ABI calls.
- [ ] Keep renderer semantics portable and deterministic, with scalar host/reference fallback.
- [ ] Add private PIE only for kernels showing repeatable physical gains.
- [ ] Validate PIE state across task switches and application teardown.
- [ ] Require byte-identical output or explicitly document allowed rounding differences with golden tests.
- [ ] Remove experiment if ABI complexity exceeds measured benefit.

### Second-Core Work

- [ ] Prove scene is CPU-bound and tile/geometry work parallelizes enough to exceed synchronization/cache cost.
- [ ] Compare graphics-specific worker service with generic application job API.
- [ ] Specify copied/borrowed buffer ownership, cancellation, process teardown, task priority, affinity policy, and host parity before implementation.
- [ ] Confirm input, audio, filesystem, camera, network, and runtime services remain responsive under load.

## Validation and Acceptance

| Layer | Required tests and evidence |
| --- | --- |
| Math | Fixed-point boundary values, signed overflow avoidance, matrix composition, projection, reciprocal accuracy, and deterministic host/Tab5 results. |
| Geometry | Near-plane splits, off-screen clipping, winding, back-face culling, degenerate triangles, shared-edge convention, depth ties, and maximum vertex expansion. |
| Raster | Golden framebuffer hashes, sentinel guards, odd widths/strides, partial visibility, painter/depth comparison, sprite transparency, and HUD ordering. |
| Assets | Bad magic/version/counts/offsets, arithmetic overflow, truncation, oversized allocations, unsupported features, and clean partial-load teardown. |
| Timing | Fixed-step totals, early/spurious wakes, bounded catch-up, alternate host refresh rates, pause/resume, frame skipping, and long-run drift. |
| Input | Held movement/fire, simultaneous keys, repeats, overflow, focus loss, disconnect/reset, pause transitions, and physical keyboard rollover limits. |
| Audio | Partial writes, backpressure, underruns, queue cap, pause flush, unavailable device, rate conflict, and teardown during wait. |
| Lifecycle | Launch/quit/restart loops, initialization failures at every stage, forced exit during rendering/audio wait, no leaks, restored overlays/TTY/terminal, and usable parent shell. |
| Performance | RV32I/RV32IM comparison, stage timing percentiles, triangles and shaded pixels per frame, present/VSYNC duration, memory bandwidth, heap/stack high-water, and service latency. |
| Build | Offline deterministic assets, dependency rebuilds, metadata, ELF/image limits, host Debug/Release, Tab5 cross-build, and extensionless installation. |

Physical Tab5 acceptance:

- [ ] Normal representative scenes meet 30 FPS target, including presentation.
- [ ] Deliberate heavy scene remains at or above 20 FPS without runaway catch-up or audio queue growth.
- [ ] Input-to-visible-response latency remains acceptable during heavy rendering.
- [ ] Audio remains continuous within recorded underrun threshold.
- [ ] No watchdog, memory corruption, allocation growth, service starvation, or framebuffer tearing during sustained 60-minute run.
- [ ] Repeated launch/play/quit cycles return to usable shell with stable free memory.
- [ ] All supported ILI9881C, ST7121, and ST7123 display paths preserve orientation, scaling, VSYNC, and terminal restoration.
- [ ] Release build records free PSRAM, application heap peak, stack high-water, frame-stage p50/p95/p99, presented FPS, and worst observed frame.

## Delivery Checklist

- [ ] Complete Phase 0 deterministic renderer and physical performance spike.
- [ ] Decide and validate RV32IM application-build policy.
- [ ] Create application skeleton, asset formats, converters, and fixtures.
- [ ] Complete fixed-point transforms, clipping, culling, rasterization, depth/painter path, sprites, and HUD.
- [ ] Complete fixed-step loop, raw controls, rail camera, entity pools, collisions, scoring, and state transitions.
- [ ] Produce original player/enemy/environment art, effects, UI, audio, and one complete level.
- [ ] Complete bounded audio integration and recoverable persistence.
- [ ] Pass unit, component, sanitizer, actual RV32 host, and supported-target builds.
- [ ] Implement conditional async/raster/worker platform changes only when recorded measurements justify them.
- [ ] Complete physical Tab5 performance, lifecycle, audio, input, and multi-display acceptance.
- [ ] Write contributor/user documentation under `docs/`; update application/build listings and affected SDK documentation.
- [ ] Update `agents/roadmap.md` as work starts and validation completes.
- [ ] Record accepted API or ISA decisions in `agents/architecture.md`, `agents/TABOS_CONTEXT.md`, and `agents/testing.md`.

Completion means one polished, original, replayable level at accepted device performance with robust input/audio/storage behavior and safe shell restoration. Rotating geometry or a benchmark alone remains intermediate proof, not completed game.
