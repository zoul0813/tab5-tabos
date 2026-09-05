# Sprite and Tile Graphics Milestone

> Status: core implementation landed on `feature/tiling`; detailed review and acceptance remain open.
> Keep this checklist synchronized with the milestone summary in [roadmap.md](roadmap.md).

## Tracking Rules

- Checked implementation items record work already reported complete in the roadmap.
- Unchecked review items may already be implemented; they still need a focused review or the full stated validation before sign-off.
- Check each review or acceptance item only after its complete requirement is verified. Record supporting test results, target, and commit when marking acceptance complete.
- Build validation does not imply physical hardware acceptance.

## Recorded Implementation Status

- [x] Add clip rectangles across SDK, private ABI, scalar, SDL, and Tab5 paths.
- [x] Add portable sprite, animation, metasprite, tilemap, object, property, and flag APIs.
- [x] Add versioned `.tsp` and `.tmap` loaders.
- [x] Add PNG/GIF/Tiled conversion and generated C output.
- [x] Add explicit runtime-asset installation and MSC copying.
- [x] Add binary-backed `tile-demo` and initial host tests.
- [x] Make imported TMJ/TSJ tilesets authoritative for images, regions, metadata, and tile animations.

Implementation evidence: `ec22fed` and `17d7d40`. Existing tests provide partial coverage;
the detailed review and acceptance items below are not yet fully signed off.

## Summary

Add a portable, game-focused SDK layer over the existing RGB565 graphics API.

Model the API after:

- SDL/raylib source regions, destination geometry, flips, and rotation for core drawing
  ([SDL3](https://wiki.libsdl.org/SDL3/SDL_RenderTextureRotated),
  [raylib](https://www.raylib.com/cheatsheet/cheatsheet.html)).
- PICO-8/TIC-80 sprite IDs, editable map cells, tile flags, and concise helpers for game ergonomics
  ([PICO-8](https://www.lexaloffle.com/dl/docs/pico-8_manual.html),
  [TIC-80](https://tic80.com/learn)).
- Ordered tile/object layers and camera-based rendering from common map engines
  ([libGDX](https://libgdx.com/wiki/graphics/2d/tile-maps)).
- Finite orthogonal Tiled maps as authoring input, including standard GID transformations
  ([Tiled](https://doc.mapeditor.org/en/stable/reference/global-tile-ids/)).

Keep sprites, animation, metasprites, and tilemaps in the SDK. They lower to
`tabos_graphics_blit_ex()`; do not add OS-owned texture handles or a retained tile engine.
Extend blit options only for per-operation clipping needed by scrolling viewports.

## Public API Review

Add `<tabos/sprite.h>`:

- [ ] Review: `tabos_sprite_set_t` contains one or more RGB565 images, sprite regions, pivots,
  user flags, animation clips, and metasprites.
- [ ] Review: Sprite IDs remain zero-based. `TABOS_SPRITE_NONE` represents an invalid or missing sprite.
- [ ] Review: `tabos_sprite_draw()` draws a natural-size sprite at its pivot.
- [ ] Review: `tabos_sprite_draw_ex()` adds destination size, quarter-turn rotation, mirroring,
  opacity, and an optional clip rectangle.
- [ ] Review: Transform the sprite pivot with the image, keeping the requested world position fixed.
- [ ] Review: `tabos_sprite_animation_frame()` selects a frame from explicit elapsed milliseconds.
  Each clip stores a repeat count: zero repeats forever, while a positive value plays
  that many cycles and then holds its final frame.
- [ ] Review: `tabos_metasprite_draw()` draws ordered component sprites with signed offsets,
  per-part transforms, and opacity. Support overall horizontal and vertical mirroring,
  but not arbitrary-angle rotation.
- [ ] Review: `tabos_sprite_flags()` returns generated 32-bit application-defined flags. The library
  assigns no collision meaning to them.
- [ ] Review: Inherit exact RGB565 color-key transparency from the source image. Do not add tinting,
  palettes, per-pixel alpha, physics, or entities.

Add `<tabos/tilemap.h>`:

- [ ] Review: `tabos_tile_t` is 32-bit. Zero means empty; the low 28 bits contain a one-based
  sprite ID; the high bits preserve Tiled horizontal, vertical, and diagonal transforms.
- [ ] Review: Provide encode/decode macros so applications normally use zero-based sprite IDs.
- [ ] Review: `tabos_tilemap_t` retains ordered tile and object layers, dimensions, a uniform tile
  size, and writable cell arrays.
- [ ] Review: `tabos_tilemap_get()` and `tabos_tilemap_set()` use layer, column, and row coordinates
  and return `ERANGE` outside the map.
- [ ] Review: `tabos_tilemap_draw_layer()` draws only visible cells from one tile layer. Options
  contain the pixel camera position, destination viewport, and animation time.
- [ ] Review: Coordinates use a top-left origin, positive X right, and positive Y down. The camera
  supports negative and sub-tile pixel offsets; outside-map cells remain empty.
- [ ] Review: Clip viewport edges exactly. Keep interior tiles on the normal accelerated path.
- [ ] Review: Applications draw layers individually, allowing sprites between background and
  foreground layers.
- [ ] Review: Object layers expose point, rectangle, and tile markers with integer coordinates,
  object ID, name, type, and integer properties. Object layers never render automatically.
- [ ] Review: `tabos_tilemap_object_property()` performs named integer-property lookup.
- [ ] Review: Tile flags support application collision queries; do not add a collision solver.

Extend `tabos_graphics_blit_options_t` with an optional clip rectangle:

- [ ] Review: Zero-initialized options preserve current behavior.
- [ ] Review: Intersect the clip rectangle with framebuffer bounds.
- [ ] Review: An empty clip succeeds without drawing.
- [ ] Review: Update the private ABI layout, RV32 marshaling, scalar renderer, SDL host, and Tab5 paths.
- [ ] Review: PPA may render unclipped interior tiles; unsupported clipped boundary operations use
  the pixel-identical fallback.

- [ ] Review: All APIs follow the existing `0`/`-1` plus `errno` convention. Draw calls retain the
  existing queued-source lifetime: assets must remain unchanged until
  `tabos_graphics_present()` completes.

## Asset Pipeline and Storage Review

Add `./tools/tabos assets build <manifest>`:

- [ ] Review: Accept a versioned JSON manifest, PNG or GIF sprite images, and finite orthogonal
  Tiled JSON maps.
- [ ] Review: Treat a single-frame GIF as one image. Fully composite animated GIF frames according
  to GIF transparency and disposal rules, import frame delays, and generate one named
  animation clip. Preserve the GIF repeat count, using zero for indefinite repetition
  and one when no loop extension exists. Clamp an animated frame delay below 10 ms to
  10 ms unless the manifest supplies an explicit duration override.
- [ ] Review: Support multiple tile and object layers, multiple tilesets, tile flips, Tiled tile
  animations, and integer tile properties.
- [ ] Review: Convert multiple source images into one logical sprite set without requiring physical
  atlas repacking.
- [ ] Review: Imported TMJ/TSJ files remain authoritative for tileset images, regions, tile metadata, and tile animations. Manifests orchestrate outputs and non-Tiled metadata without duplicating imported tiles or images.
- [ ] Review: Let the manifest define arbitrary sprite regions, pivots, named animations,
  metasprites, and application flags.
- [ ] Review: Generate sanitized C constants for named sprites, clips, metasprites, maps, layers,
  objects, and flags. Reject identifier collisions.
- [ ] Review: Reject infinite or isometric maps, ellipse/text/polygon/polyline/template objects,
  arbitrary object rotation, non-integral object geometry, unsupported properties, and
  malformed GIDs with clear diagnostics.
- [ ] Review: Convert opaque PNG and composited GIF pixels exactly like `TABOS_RGB565`. Require
  alpha values of 0 or 255; reject partial alpha.
- [ ] Review: Select a deterministic unused RGB565 color key automatically when transparency exists.
  Allow an explicit manifest key only when no opaque pixel converts to the same value.
- [ ] Review: Produce deterministic generated `.c`/`.h` descriptors and versioned binary assets from
  the same normalized data.

Binary formats:

- [ ] Review: `.tsp`: little-endian sprite-set file with `TSP1` magic, images, regions, flags,
  animations, frames, metasprites, and parts.
- [ ] Review: `.tmap`: little-endian map file with `TMP1` magic, dimensions, ordered layer metadata,
  tile cells, simple objects, properties, and a string table.
- [ ] Review: Store offsets and counts, never pointers. Require four-byte alignment and exact file bounds.
- [ ] Review: Do not add compression or checksums in v1.
- [ ] Review: Load each validated asset completely into process-owned memory. Constrain allocation by
  the application heap and return `ENOMEM` without partial state.
- [ ] Review: `tabos_sprite_set_load()`/`tabos_sprite_set_unload()` and
  `tabos_tilemap_load()`/`tabos_tilemap_unload()` own binary allocations. Unload zeroes
  the destination object.
- [ ] Review: Generated C and binary forms expose identical IDs and rendering semantics.
- [ ] Review: Modified binary-map cells remain memory-only; do not save them automatically.
- [ ] Review: Unload only after the final `present()` or graphics close because queued commands retain
  source pointers.

- [ ] Review: Install binary assets under `T:/data/<app-name>/`. Extend application build rules so each
  application explicitly declares generated asset outputs; normal installation and `--msc`
  copy only declared runtime assets, never source PNG, GIF, Tiled, or manifest files.

## Implementation and Documentation Review

- [ ] Review: Implement sprite, animation, metasprite, tilemap, and binary parsing as portable SDK code.
  Add no SDL, ESP-IDF, FreeRTOS, PPA, or PIE types to the public API.
- [ ] Review: Use logical-canvas software drawing for normal tile games. Keep the existing final
  canvas upscale as one accelerated presentation.
- [ ] Review: Keep native-mode support functional through queued blits. Add bulk kernel tile submission
  only later if measured API overhead misses the benchmark target.
- [ ] Review: Add an original `tile-demo` reference application using a generated sprite animation,
  metasprite, scrolling multilayer map, editable cell, collision flags, and object markers.
  Load binary assets from `T:/data/tile-demo/`.
- [ ] Review: Update graphics/API, SDK/application-build, asset-authoring, and demo documentation.
- [ ] Review: Record decisions and milestone work in `agents/TABOS_CONTEXT.md`,
  `agents/architecture.md`, `agents/testing.md`, and `agents/roadmap.md`.

## Test and Acceptance Checklist

- [ ] Converter tests cover deterministic output, RGB565 conversion, automatic and explicit
  color keys, static and animated GIF input, GIF transparency/disposal, frame timing and
  repeat counts, multiple tilesets, Tiled transforms and animations, object markers,
  identifier collisions, and every rejected feature.
- [ ] Verify generated-C/binary equivalence for IDs, descriptors, and rendered output.
- [ ] Test binary-loader rejection of bad magic and unsupported versions independently.
- [ ] Test truncated binary assets independently of bad magic.
- [ ] Test binary-loader overflow and invalid offsets, counts, alignment, and GIDs.
- [ ] Inject allocation failures and verify `ENOMEM`, no partial state, and leak-free cleanup.
- [ ] Verify repeated successful load/unload and cleanup after failed loads.
- [ ] Sprite pixel tests cover clipping, pivots, scale, quarter rotations, mirrors, opacity,
  transparency, animation wrap/clamp, metasprite order, and transformed pivots.
- [ ] Tilemap tests cover get/set, empty cells, every Tiled flip combination, negative camera,
  smooth scrolling, viewport edges, layer ordering, animation time, tile flags, objects,
  and invalid layers/cells.
- [ ] Run tests through native and logical canvases; scalar output remains byte-identical
  across host and Tab5 fallback.
- [ ] Extend the maintained `tester` with generated-C and binary asset coverage.
### Build Matrix

- [x] Validate macOS Debug and Release builds (recorded in roadmap; not rerun during this checklist conversion).
- [x] Validate Tab5 Debug and Release builds (recorded in roadmap; not rerun during this checklist conversion).
- [ ] Validate Linux Debug and Release builds.
- [ ] Verify all applications build and declared runtime assets install correctly across the target/configuration matrix.

### Physical Tab5 Acceptance

- [ ] Verify `tile-demo` rendering, layer order, animation, editable cells, and object markers on physical Tab5.
- [ ] Verify seam-free scrolling and exact viewport clipping, including negative camera positions.
- [ ] Verify RGB565 color-key transparency and all Tiled transforms on physical Tab5.
- [ ] Verify repeated load/unload and repeated application launch/exit without resource loss.
- [ ] Verify terminal and input restoration on exit.
### Performance Acceptance

- [ ] Add a repeatable benchmark fixture and timing instrumentation; current `tile-demo` uses a 160x120 canvas and two tile layers, so it does not meet the acceptance workload.
- [ ] Performance acceptance uses a 320x180 logical canvas, 16x16 tiles, three full visible
  tile layers, and 64 sprites, sustaining the Tab5 panel cadence target of 58 FPS. If frame
  construction, excluding the VSYNC wait, exceeds 12 ms, add one bounded bulk tile-layer
  private ABI operation while preserving the same public SDK API.

## Assumptions

- RGB565 remains the public and asset pixel format.
- Rendering uses nearest-neighbor sampling only.
- Maps are finite and orthogonal.
- Object layers contain simple markers only; the game owns entity creation and behavior.
- Runtime PNG, GIF, and Tiled parsing remain excluded.
- Application binaries remain in `T:/bin`; application data belongs in
  `T:/data/<app-name>/`.
- No compatibility shim is required because the application ABI remains pre-release.
