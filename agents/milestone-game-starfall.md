# Milestone: Starfall

## Goal

Build `apps/starfall/` as a polished, standalone retro arcade shooter that proves
TabOS can support real games through its public graphics, raw-input, time, and
filesystem APIs. Starfall must behave consistently on the macOS/Linux hosts and
physical Tab5 hardware.

## Status

The Starfall application is implemented: its game, rendering, input, and storage
modules are present and the extensionless binary is installed as `T:/bin/starfall`.
On 2026-08-30, the macOS `unit.starfall_game` test and `apps/build.sh` both passed.
The remaining work is broader automated coverage plus host and physical-Tab5
acceptance validation.

## Decisions

- [x] Name the game **Starfall**.
- [x] Use a `640x360` RGB565 logical canvas, scaled exactly 2x to `1280x720`.
- [x] Use A/S for horizontal movement because they fit the Tab5 keyboard naturally.
- [x] Use K to start, fire, and restart because Enter and Space are awkwardly positioned.
- [x] Use raw TabOS key events for held movement, firing, modifiers, and key release.
- [x] Build a polished single-player arcade shooter rather than a graphics-only demo.
- [x] Use an original arcade RGB565 visual style rather than copying an existing game.
- [x] Persist the high score on `T:/`.
- [x] Keep audio out of this milestone until TabOS has a public audio API.

## Memory Prerequisite

A `640x360` RGB565 canvas consumes 460,800 bytes before game state and assets. The
previous 256 KiB application heap could not hold it. Starfall raises the bounded default
arena to 1 MiB as an interim step; application-selected heap metadata remains separate
future work.

- [x] Raise the bounded application heap arena from 256 KiB to 1 MiB on host and Tab5.
- [x] Preserve deterministic process cleanup for the application heap arena.
- [ ] Ensure one application cannot request more memory than the platform can safely
  provide at that moment.
- [x] Return `ENOMEM` cleanly when the bounded arena cannot satisfy a request.
- [x] Confirm the Starfall binary and host tests support a `640x360` canvas allocation.
- [ ] Confirm the canvas allocation on physical Tab5 hardware.
- [ ] Add allocation tests covering growth, exhaustion, cleanup, and repeated launches.
- [ ] Record free-memory impact before opening, while running, and after exiting Starfall.

## Application Structure

- [x] Create `apps/starfall/` with `Makefile`, `README.md`, `LICENSE`, `include/`,
  `src/`, and `assets/`.
- [x] Install the extensionless executable as `T:/bin/starfall` through the existing
  `apps/build.sh` discovery process.
- [x] Separate main/runtime control, game state, input state, entities, collision,
  rendering, effects, storage, and deterministic random-number generation into focused
  modules.
- [x] Keep gameplay state independent from rendering so deterministic host tests can
  update the game without opening a display.
- [x] Use fixed-capacity pools for stars, player shots, enemies, particles, and
  explosions.
- [x] Perform no allocation during an active frame.
- [x] Close graphics and return cleanly to the shell on normal exit or recoverable error.

## Asset and Licensing Policy

- [x] Create the starfield procedurally in game code.
- [x] Create original Starfall player, enemy, projectile, explosion, and HUD artwork.
- [x] Create an original limited bitmap font containing only required uppercase letters,
  digits, and punctuation.
- [x] Keep bitmap/font data separate from rendering code and store source assets under
  `apps/starfall/assets/`.
- [x] Add `apps/starfall/assets/README.md` recording the origin and license of every
  asset.
- [ ] Do not copy, trace, or closely imitate sprites, logos, names, characters, or visual
  designs from commercial games.
- [ ] Accept external assets only when verified as CC0 or public domain; merely
  open-source or freely downloadable assets are not sufficient for this milestone.
- [ ] Ensure screenshots, packaged binaries, firmware artifacts, and promotional images
  can be redistributed without third-party attribution or licensing obligations.

## Game Loop and Timing

- [x] Implement a fixed 60 Hz simulation using `tabos_monotonic_ms()`.
- [x] Accumulate elapsed milliseconds in 60 Hz units so timing does not depend on a
  rounded 16 ms step.
- [x] Clamp long elapsed intervals and cap catch-up updates to prevent a spiral after a
  pause, breakpoint, or slow frame.
- [x] Poll all pending raw input events before each simulation update.
- [x] Track held keys from key-down/key-up state rather than relying on repeat events.
- [x] Present at most once per outer loop and allow TabOS/VSYNC to pace visible frames.
- [x] Seed the runtime PRNG from monotonic time while allowing fixed seeds in tests.

## Controls

- [x] A/S move the player left/right while held; vertical movement is intentionally absent.
- [x] K starts/restarts and fires while held during play, limited by weapon cooldown.
- [x] P pauses and resumes on a non-repeat key-down event.
- [x] Q or Escape exits from title, pause, gameplay, or game-over state.
- [x] Support simultaneous movement and firing.
- [x] Clear held-input state during state transitions so keys cannot remain logically
  stuck.

## Gameplay

- [x] Add title, playing, paused, and game-over states.
- [x] Give the player three lives and a visible score/high-score HUD.
- [x] Add bounded horizontal movement along the bottom of the logical playfield.
- [x] Add a rate-limited player weapon with reusable projectile slots.
- [x] Add straight, zigzag, and diving enemy movement patterns.
- [x] Increase difficulty through wave number, spawn rate, and enemy speed.
- [x] Use deterministic AABB collision detection for ships and projectiles.
- [x] Award defined points for each enemy type.
- [x] Remove a life on collision and grant temporary
  invulnerability.
- [x] End the run when all lives are lost and allow immediate restart.
- [x] Prevent entity-pool exhaustion from corrupting state; skip spawns or shots when a
  pool is full.

## Rendering and Effects

- [x] Render into a `640x360` application-owned RGB565 canvas obtained through
  `tabos_graphics_open()`.
- [x] Draw a three-layer parallax starfield with distinct speeds, sizes, and brightness.
- [x] Use pixel-aligned original ship, enemy, and projectile graphics.
- [x] Add bounded particle/explosion effects without per-frame allocation.
- [x] Render score, high score, lives, wave, menus, and status using the private bitmap
  font renderer.
- [x] Flash the logical playfield border briefly when the player takes damage.
- [x] Set the letterbox color to red during damage feedback, blue while paused, and black
  during ordinary play. The exact 16:9 mode normally has no visible border, but the state
  remains correct for alternate physical displays.
- [ ] Ensure no terminal framebuffer, cursor, or scrollback shortcut appears while the
  game owns graphics mode.

## High-Score Storage

- [x] Store the high score as validated ASCII at
  `T:/data/starfall/highscore.dat`.
- [x] Create `T:/data/` and `T:/data/starfall/` when missing; tolerate `EEXIST`.
- [x] Treat missing, malformed, negative, overflowed, or truncated score data as zero.
- [x] Clamp parsed scores to the game's supported maximum.
- [x] Save only when a run exceeds the loaded high score.
- [x] Write `highscore.tmp`, close it successfully, then rename it over
  `highscore.dat`.
- [x] Treat read-only, absent, full, or failing storage as nonfatal and keep the game
  playable for the current session.

## Automated Tests

- [x] Add an initial host-native deterministic test for movement, firing, damage,
  invulnerability, pause, game over, and restart.
- [ ] Test raw key-down/key-up state, simultaneous movement/fire, repeat filtering for
  toggles, and held K firing cadence.
- [ ] Test fixed-step accumulation, elapsed-time clamping, and bounded catch-up.
- [ ] Test all enemy movement patterns and deterministic spawning with a fixed seed.
- [ ] Test projectile/enemy/player collisions, lives, invulnerability, scoring, wave
  progression, game over, restart, and pause.
- [ ] Test full entity pools and verify graceful skipped spawns.
- [ ] Test high-score parsing, corruption fallback, update rules, directory creation,
  temporary-file replacement, and nonfatal storage failures.
- [x] Build all applications with `apps/build.sh`.
- [x] Run `unit.starfall_game` on macOS.
- [ ] Run the complete macOS test suite after Starfall-specific coverage is expanded.
- [ ] Build Linux debug and release targets.
- [x] Build Tab5 debug firmware.
- [ ] Build Tab5 release firmware.

## Host Acceptance

- [ ] Starfall launches from the shell as `starfall`.
- [ ] The `640x360` image fills the `1280x720` logical display at exact 2x scaling.
- [ ] Movement, firing, pause, restart, and exit controls respond immediately.
- [ ] Gameplay speed remains stable when the SDL mouse is stationary or moving.
- [ ] No terminal flicker, tearing, input leakage, sanitizer failure, or kernel panic
  occurs.
- [ ] High score survives application restart.

## Tab5 Acceptance

- [ ] Copy the Starfall binary rebuilt with the current system SDK to `T:/bin/starfall`.
- [ ] Confirm gameplay speed and layout match the host simulation.
- [ ] Confirm held A/S movement and held K firing work on the physical keyboard.
- [ ] Confirm simultaneous movement and firing work without delayed input.
- [ ] Confirm no tearing, terminal flash, watchdog reset, application fault, or kernel
  panic occurs during an extended play session.
- [ ] Confirm title, pause, damage, game-over, and restart transitions render cleanly.
- [ ] Confirm high score survives application exit and device restart.
- [ ] Measure frame/update timing and free PSRAM during representative heavy scenes.

## Deferred Follow-Ups

- [ ] Add music and sound effects after TabOS gains a public audio API.
- [ ] Add controller, USB HID, or touch controls after those public input paths exist.
- [ ] Add bosses, power-ups, multiple weapons, and additional enemy art after the first
  complete game is validated.
- [ ] Add replay, networking, leaderboards, or background execution only as separate
  milestones.
