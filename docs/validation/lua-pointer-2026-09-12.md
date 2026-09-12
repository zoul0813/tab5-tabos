# Lua Pointer Validation — 2026-09-12

Screen-owned pointer bindings and the single-file `touch.lua` demo use existing
public SDK services. No firmware, public ABI, tile engine, or native game changes
are required. Physical Tab5 acceptance remains pending.

## Automated Evidence

- `./apps/build.sh`: passes; the RV32 Lua application builds and the touch demo
  stages/installs byte-for-byte into `build/apps/lua/data/` and
  `.local/rootfs/T/data/lua/` (2563 bytes).
- `./tools/tabos macos debug build` and `./tools/tabos macos release build`: pass.
- Both configurations pass all 14 selected checks with
  `ctest --test-dir build/macos-debug -R 'lua|pointer|sdk_graphics|naming|application_api' --output-on-failure`
  (substitute `macos-release` for Release). Debug includes ASan/UBSan.
- Native binding coverage includes all event types, pressure presence/absence,
  full-width contact IDs, scaled/letterboxed coordinates and outside edges,
  empty queues, service failures, keyboard coexistence, repeated/stale close,
  missing devices, failed cleanup/retry, GC/scope cleanup, interruption, and
  allocation failure without losing a pending event.
- The shipped demo runs with deterministic input and real SDK drawing. Checks
  cover first-contact ownership, contact 0, rejected letterbox presses, clamped
  dragging, cancellation, release, idle redraw suppression, exit and reopening.
- Real RV32 integration passes the command below. SDL touch events reach Lua
  through the actual service/SDK, preserving down/move/up/cancel, IDs, pressure,
  and canvas mapping. The demo moves its rendered square and changes color on
  release. Q returns to a usable shell. Existing graphics tests now own pointer
  streams through return, errors, `os.exit`, Ctrl-C/Ctrl-D, and forced teardown.

```text
build/macos-release/tests/tabos_lua_rv32 build/apps/shell/shell build/apps/lua/lua apps/lua/examples/snake.lua apps/lua/examples/starfall.lua apps/lua/examples/touch.lua
```

Linux builds/tests and flashing were not performed. Existing RWX ELF and
duplicate-library linker warnings remain unchanged.

## Physical Tab5 Checklist

- [ ] Run `lua T:/data/lua/touch.lua` from microSD and check tap/drag alignment,
  display orientation, all edges, and letterbox rejection on supported revisions.
- [ ] Confirm additional fingers do not steal movement; release/retouch works
  and rapid motion/cancellation cannot leave the square held.
- [ ] Verify Q/Escape and Ctrl-C/Ctrl-D return to a usable terminal, including
  interruption while touching; repeat launches to check cleanup.
- [ ] Record responsiveness, memory use, and behavior after device faults.
