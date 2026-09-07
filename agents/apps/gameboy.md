# Peanut-GB / Game Boy Implementation Plan

Status: proposed implementation plan, 2026-09-07. Implementation has not started.

This expands the Peanut-GB candidate in [Application Port Candidates](../milestone-apps.md). **Kernel/API prerequisites below gate the interactive emulator milestone.** Proposed API names, budgets, controls, and performance thresholds are recommendations, not new `[DECIDED]` architecture requirements. This document does not implement or approve the proposed API changes as existing functionality.

## Outcome and Scope

Provide `T:/bin/gameboy`, an independently compiled RV32 application that plays supported original Game Boy (DMG) ROMs with keyboard controls, scaled video, sound, and persistent cartridge RAM/RTC state. Run the same executable on Tab5 and through the macOS/Linux RV32 host interpreter.

Initial command contract:

```text
gameboy <rom.gb>
gameboy --nosound <rom.gb>
gameboy --help
```

Support ROM-only cartridges and validated subsets of the pinned core's MBC1, MBC2, MBC3, and MBC5 support, within the application's explicit ROM/RAM limits. Permit DMG-compatible dual-mode cartridges only after checking their headers; reject Color-only cartridges. Publish tested compatibility, not a promise that every game works.

Initial features include pause/resume, reset confirmation, quit, volume, a small palette selection, battery-save persistence, and RTC persistence for validated MBC3 games. Defer Game Boy Color/Super Game Boy, link cable, USB controllers, touch gamepad, zipped ROMs, ROM browser, cheats, rewind, fast-forward, and emulator save states. Cartridge battery saves are distinct from whole-emulator save states.

Do not bundle commercial ROMs or a proprietary boot ROM. Start with upstream's boot-ROM-free initialization and use redistributable homebrew/test fixtures with recorded licences. User-supplied ROM loading does not make the ROM data part of the application licence.

## Upstream and Current TabOS Evidence

Pin an exact [Peanut-GB](https://github.com/deltabeard/Peanut-GB) commit. Vendor its header and required notices; separately pin and retain the licence of the selected APU. Upstream describes a portable C99 DMG core, mapper/RTC support, and accuracy limitations. Its examples are references, not a dependency on SDL2 or desktop threads.

The [core header](https://github.com/deltabeard/Peanut-GB/blob/master/peanut_gb.h) provides ROM/RAM callbacks, scanline output, errors, and frame execution. The inspected revision advances RTC internally; the old `gb_tick_rtc` entry point is deprecated and ineffective. Confirm behavior at the pinned commit before adapting RTC persistence.

The included [MiniGB APU header](https://github.com/deltabeard/Peanut-GB/blob/master/examples/sdl2/minigb_apu/minigb_apu.h) defaults to 32,768 Hz and a fixed integer samples-per-frame count. TabOS does not advertise 32,768 Hz. Neither an unsupported rate nor a truncated sample count should be copied into the port unchanged.

Read [architecture](../architecture.md), [context](../TABOS_CONTEXT.md), [testing](../testing.md), and [coding style](../coding-style.md) before implementing prerequisites or application code. Current implementation evidence:

| Surface | Current behavior and consequence |
| --- | --- |
| `input/input.c` | A 64-event queue discards the oldest event when full. The internal `held_key` tracks repeat generation, not a complete pressed-key set. No public snapshot or overflow/reset notification exists. |
| `sdk/include/tabos/input.h` | Key-down/up, modifiers, and repeats exist. Gameplay can use physical events, but cannot reliably reconstruct state after a lost release. |
| `sdk/include/tabos/wait.h` | Audio/pointer/device/socket sources and finite timeouts exist; keyboard has no wait-source adapter. Zero-item timer-only waits are rejected. |
| `sdk/lib/input.c`, `sdk/lib/runtime.c` | Input wait and sleep retry through yield. Tab5 yield delays a tick; these are not precise absolute-deadline waits. |
| `docs/graphics-api.md` | Explicit logical canvases, RGB565, nearest-neighbor scaling, and completion-fenced present already exist. Present may block at display cadence; the host fallback is currently 58 Hz. |
| `sdk/include/tabos/audio.h`, `docs/audio.md` | Nonblocking signed-16-bit mono/stereo PCM, queued-byte counts, writable waits, flush, volume, and close exist. Streams share one hardware sample rate; conflicts return `EBUSY`. |
| `sdk/include/tabos/clock.h` | UTC epoch time exists for elapsed offline RTC time. No new board RTC API is required. |
| `sdk/make/application.mk`, loader | Explicit heap/stack metadata and same-artifact RV32 builds exist. ROM data is loaded as a file, not compiled into the executable image. |

## Mandatory Prerequisites

Implementation order is P1/P2, then P3/P4 service validation, then playable integration. A build-only emulator/core test may run earlier. Do not mark the port ready by replacing failed prerequisites with polling or platform-specific application code.

### P1: Foreground Keyboard Readiness

Add a public keyboard wait-source adapter, proposed as `tabos_input_wait_source()`, integrated with the existing generic wait API. Reuse this work if already implemented for [Kilo](kilo.md), [Lua](lua.md), or [Puzzles](puzzles.md); those plans alone do not make the API available.

Requirements:

- Process-owned, generation-tagged source with foreground-only consumption rules and explicit cancellation/hangup behavior on teardown or ownership loss.
- Readable readiness observes queued input without consuming it. Queue insertion and wait registration must not have a lost-wakeup race.
- One wait can combine keyboard input, pending audio backpressure, and a finite frame deadline. An idle pause menu can wait indefinitely for input.
- Runtime input notifications wake native Tab5 waits and suspended host RV32 gates. Do not introduce a periodic keyboard-readiness timer.

Likely layers: portable input/wait services, process ownership, `sdk/include/tabos/input.h`/`wait.h`, SDK wrappers, private ELF transport, loader dispatch, native gate guards, and host interpreter dispatch. Names and exact file ownership must follow current code.

Acceptance: deterministic empty/readable/drained transitions, event arriving during registration, simultaneous input/audio readiness, foreground isolation, stale handles, forced teardown, and actual RV32 wake/return. Add maintained `apps/tester` coverage and cross-build Tab5 before dependent application work.

### P2: Reliable Pressed-Key State and Resynchronization

Add a copied public physical-key state snapshot plus a documented input reset/overflow generation or equivalent resynchronization contract. A proposed API is `tabos_input_get_state(...)`; finalize a TabOS-owned representation covering ordinary keys and modifiers, not an SDL scancode array.

Maintain authoritative pressed state at event production, independently of queue retention and key repeat. Protect it with existing input synchronization. Make state/generation reads coherent; define how consumers discard or reconcile older queued events so replay cannot reintroduce a released key after a snapshot.

Clear or reconcile state on device reset/disconnection, host window focus loss, and foreground transitions. A blocked parent must not observe a child's input. Define whether already-held keys enter a newly focused application; for Game Boy menus and launch, require a release before a held command can trigger a new action.

Do not infer state from the repeat generator's single `held_key`, assume unlimited keyboard rollover, or expose the global input queue to applications. Preserve shell cooked translation and Aa/Sym behavior. Future multiple keyboard producers must have a stated aggregation rule; avoid claiming multi-device correctness that is not implemented.

Acceptance: simultaneous direction+A/B, multiple ordinary keys, modifiers, overflow that drops a release, repeated key-down without toggling, focus loss without key-up, input-device failure/reset, menu transitions, and parent restoration. Verify physical Tab5 combinations and record matrix limitations. This prerequisite prevents stuck movement/fire after event loss.

### P3: Deadline-Wait and Presentation Timing Gate

Validate an interruptible deadline wait using P1 plus existing `tabos_wait` finite timeouts and `tabos_monotonic_ms`. Keep an absolute, fractional frame schedule in the application and derive remaining relative timeout immediately before each wait. Recheck time after every wake. Existing millisecond resolution is adequate for an initial rational schedule; a microsecond clock is not automatically required.

Required service behavior: finite waits do not busy-spin, input wakes them promptly, cancellation releases resources, and wake latency is measured on Tab5. The application must not call `tabos_wait(NULL, 0, ...)`, which is currently invalid. No timer-only extension is needed while a keyboard source is present. If actual finite-wait behavior fails this contract, repair the generic wait implementation first.

Measure `present()` duration and input/service progress with 160×144 scaled output, with and without audio, on native Tab5 and host. Visible refresh cannot define emulated speed: 58 Hz fallback, 60 Hz panels, and other host refresh rates differ from the DMG cadence. Frame skipping and bounded catch-up must cope with this without dropping emulated CPU/APU time.

**Conditional prerequisite:** if existing blocking presentation causes sustained audio starvation or unacceptable input latency despite bounded buffering and dropped visual frames, add a portable nonblocking presentation/completion-wait mode before accepting real-time sound. Specify frame ownership, queue limits, completion fences, stale/canceled handles, and host/native parity. Keep the current blocking API intact. Do not bypass VSYNC or borrow display hardware inside the app. Only implement this extension when measurements demonstrate it is needed.

Acceptance: long-run rational-clock tests, early/spurious wake tests, bounded late-frame recovery, alternate host refresh rates, pause/resume without a catch-up storm, and recorded device frame/present/wait timing. Headless host tests establish correctness, not native performance.

### P4: Existing Audio and Cleanup Contract Validation

No new mixer or application threading API is required initially. Prove existing audio writes handle partial progress and `EAGAIN`, writable waits wake correctly, status reflects queue occupancy, and flush removes stale PCM on pause/reset. Verify sample-rate conflicts and clean resource reclamation after child exit/failure.

Do not include a permanently writable audio source in every wait: it would immediately wake even when no PCM is pending. Wait for writable only after actual backpressure; otherwise use keyboard readiness and the next production deadline. Sample-accurate playback timestamps or configurable low-watermark events are optional future improvements, not assumed prerequisites.

If these existing contracts fail, fix the owning audio/wait service and validate it before sound integration. Add meaningful regression cases to maintained tests instead of hiding defects with unbounded application buffers.

### Prerequisite Summary

| ID | Kernel/API work | Gate |
| --- | --- | --- |
| P1 | New keyboard wait source, ownership, native/host transport | Before interactive gameplay and pause loop. |
| P2 | New coherent pressed-state/resync contract; backend reset/focus integration | Before held-button controls are accepted. |
| P3 | Validate existing deadline waits and presentation; repair failures | Before timed gameplay is accepted. |
| P3 conditional | Nonblocking present/completion API only if measurements require it | Before real-time sound acceptance if blocking present fails latency budget. |
| P4 | Validate existing audio buffering/waits/flush/cleanup; repair failures | Before sound is accepted. |

Every added public feature needs SDK documentation, private transport updates without application exposure, host RV32 dispatch, guarded native gates, ownership/error tests, and `apps/tester` coverage. Follow pre-release ABI policy and rebuild affected bundled apps. Game Boy CPU, mappers, APU emulation, palettes, save format, and RTC emulation remain application code.

## Phase 1: Application Build and ROM Loading

Suggested layout:

```text
apps/gameboy/
  Makefile
  LICENSE
  UPSTREAM.md
  README.md
  vendor/peanut-gb/
  vendor/minigb-apu/
  include/gameboy/
  src/main.c
  src/emulator.c
  src/video.c
  src/input.c
  src/audio.c
  src/timing.c
  src/cartridge.c
  src/storage.c
  src/menu.c
docs/gameboy.md
```

Use `APP_NAME := gameboy`, shared SDK Make rules, explicit source lists, and offline vendored dependencies. Record upstream revisions, file hashes, licences, compile-time options, and every local patch. Compile the single-header core implementation in one translation unit. Preserve upstream source style; apply project style to adapters.

Provisional limits: 8 MiB ROM, mapper-supported cartridge RAM, 12 MiB application heap, and 64 KiB stack. These fit within current request ranges but require memory measurements alongside the retained shell, kernel, display, and audio allocations. Refuse a ROM cleanly when the actual budget cannot accommodate it; lower the supported maximum rather than allocating beyond metadata. Do not hold duplicate whole-ROM buffers.

Load and validate the ROM before starting gameplay. Check minimum/header length, size codes, actual file length, mapper and RAM support, integer arithmetic, and Color-only flags. Use upstream initialization results for compatibility errors. Check read/close failures and retain no partial cartridge. Cap allocations before using untrusted header values.

Keep the ROM resident for fast byte callbacks; no filesystem calls per emulated read. Guard ROM/RAM callback addresses against allocated lengths, preserving verified mapper/open-bus semantics. Implement MBC2's actual save representation rather than trusting a zero RAM-size header. Mark cartridge RAM dirty only when bytes change. Core errors produce a controlled application error and return to the shell; do not continue with corrupt state or write a bad save automatically.

## Phase 2: Video and Controls

Request a 160×144 SDK logical canvas before opening graphics. It uses 46,080 RGB565 bytes and scales 5× to 800×720, centered on the 1280×720 display. Map scanline shade bits through a four-color palette; do not interpret layer flags as extra intensity bits. Preserve the completed frame when the core skips drawing, and explicitly handle LCD disable/blanking.

Write into the logical pixel buffer and present once per selected completed frame. Keep borrowed graphics buffers unchanged until the documented fence returns. Avoid per-pixel ABI calls. Menu overlays must preserve the emulated frame; use an app-owned small licensed font or glyph set, not private terminal internals. Default OS battery/Wi-Fi overlays may be hidden during gameplay and restored on exit.

Proposed controls:

| Key | Action |
| --- | --- |
| Arrows | D-pad. |
| Z / X | A / B. |
| Enter / Backspace | Start / Select. |
| Escape | Pause menu; resume, save, palette, volume, reset, quit. |

Use raw physical key mode and the P2 snapshot/resync contract. Game buttons are active while held; ignore OS text events and repeat as state transitions. Define opposite-direction policy, initially neutralizing both opposite directions. Test direction plus A/B plus Start/Select on physical hardware. Menu commands require fresh presses; clear gameplay latches on pause, reset, focus change, and resume. Restore inherited TTY mode and all process-owned graphics/audio resources on exit.

## Phase 3: Emulated Time and Video Scheduling

Use the pinned core's DMG cycle constants: nominal frame duration is 70,224 / 4,194,304 seconds, about 59.7275 frames per second. Track frame deadlines with integer/rational accumulation rather than rounding every frame to 16 or 17 milliseconds. LCD-disabled execution must still advance emulated time.

Treat emulation, sound production, and display presentation as separate schedules. If presentation falls behind, omit a visible frame while continuing the necessary CPU/APU steps. Bound catch-up work per loop to preserve input handling. On sustained overload, report reduced emulation speed rather than silently discarding CPU time or continually growing audio latency.

Start with normal frame execution and measure the maximum duration of a call. Test pathological/error ROMs and LCD-off behavior. If one call cannot meet the input/cancellation budget, add a narrowly documented bounded-step adapter at the pinned core boundary. Do not depend casually on internal stepping symbols or move the emulator into a kernel/FreeRTOS task of its own.

Pause freezes ordinary emulated CPU/APU time and flushes queued playback. Resume resets wall scheduling baselines and primes bounded audio, without replaying the paused duration as CPU frames. RTC pause/offline elapsed-time policy is handled separately below.

## Phase 4: APU and PCM Playback

Start with the included MIT MiniGB APU, after verifying its licence and build compatibility. Adapt it to execute synchronously in the application's emulation flow; do not copy the SDL example's audio-thread ownership model. Route core sound-register reads/writes into the APU with a documented timing model. Frame-granular audio is an initial accuracy limitation; do not advertise cycle-accurate sound.

Select a TabOS-advertised rate, initially 44,100 Hz with signed-16-bit interleaved stereo. Configure the APU accordingly and adapt its render function to accept the required output count. Carry fractional samples between emulated frames using a rational accumulator; changing only the sample-rate macro leaves fixed-count truncation drift. Validate pitch, channel timing, and overflow behavior after adaptation.

Use one playback stream, not one stream per emulated hardware channel. Mix the four emulated channels in the APU, then feed bounded PCM blocks through `tabos_audio_write`. Retain unwritten tails after partial writes, preserve complete PCM-frame alignment, and honor `TABOS_AUDIO_IO_MAX`.

Start with roughly 40–60 ms of queued sound as a tunable latency target. Measure queue occupancy and underruns, including physical sample-clock drift over long runs. If needed, apply a bounded app-local resampling/production correction; do not change Game Boy CPU speed to chase the host DAC clock or duplicate arbitrary samples without a stated policy. Maintain a hard cap on pending audio.

Handle unsupported audio, open failure, and `EBUSY` by reporting the reason and allowing explicit silent play. Flush on pause/reset and close on exit. Verify headphone routing through existing service behavior. `--nosound` skips the playback stream but must leave sound-register behavior consistent with the selected core configuration.

## Phase 5: Cartridge Saves and RTC

Use a stable full-ROM content digest for save identity, not only title or basename. Keep state beneath `T:/data/gameboy/`. A proposed versioned `.gbsave` container stores cartridge RAM, RTC fields, ROM identity, lengths, and integrity metadata together. It is a battery-state container, not a raw `.sav` or emulator-state dump; raw save import/export is follow-up work.

Validate all lengths, version, digest, mapper, and checksums before applying a save. Missing save starts a new cartridge; corrupt or mismatched save produces a visible error and remains untouched. Do not serialize `struct gb_s`, pointers, padding, or host-dependent C types.

Persist the RTC's counter/register state, halt/carry flags, relevant subsecond state, and a last-valid UTC timestamp using fixed-width fields. Restore elapsed offline time only when the clock is valid and not earlier than the saved timestamp; define day-counter wrap and carry behavior. Do not overwrite cartridge RTC with host calendar time on every frame. Do not double-tick a core that advances RTC internally. A narrow core adapter may be needed to extract/restore RTC fields; keep it pinned and tested.

During pause, apply real elapsed RTC time on resume when appropriate for a running cartridge clock; do not advance a halted RTC. Handle unavailable RTC and wall-clock corrections explicitly. CPU slowdowns should not silently redefine battery-clock semantics; test and document the reconciliation policy.

Save explicitly from the pause menu and on normal quit/reset when dirty, with recoverable failures. Retain changed cartridge RAM in memory if saving fails and offer retry or explicit discard. Debounced periodic checkpoints can follow measured SD/audio impact; do not write synchronously from each RAM callback.

Stage to a uniquely created sibling file, check all writes and close, then install using verified filesystem rename behavior. Use recoverable backup/rename handling where FAT cannot replace directly. Keep the old save until a replacement exists and preserve recovery files if rollback fails. No power-loss-atomicity promise without a corresponding filesystem contract. Reuse proven save logic from other application work where appropriate, without coupling emulator internals to Kilo or Puzzles.

## Validation and Acceptance

Use redistributable deterministic homebrew/test ROMs and record their licences. Upstream test results are useful context, not proof of this adapter. Never require commercial ROMs in CI.

| Layer | Required tests/evidence |
| --- | --- |
| Prerequisites | P1/P2 unit/component, SDK tester, actual RV32 waits/snapshots, native cancellation, and P3/P4 timing/audio results. |
| Cartridge/core | Invalid/truncated headers, ROM/RAM bounds, mapper rejection, MBC2 sizing, banked reads/writes, core errors, LCD-off execution, and allocation failure. |
| Input/video | Simultaneous buttons, overflow resync, focus loss, opposite directions, pause/menu repeat suppression, shade/layer mapping, scanline bounds, scaling, and blanking. |
| Timing/audio | Long-run frame/sample totals, fractional remainders, alternate display rates, bounded catch-up, pause/resume, short writes, backpressure, buffer caps, underruns, channel output, and sample-clock drift. |
| Storage/RTC | ROM identity collisions avoided, repeated save/load, corrupt saves, all write/close/rename failures, offline time, halted clock, backward wall-clock changes, day wrap/carry, and no double ticking. |
| Lifecycle | Shell launch/return, repeated launch, input/audio waits during termination, partial initialization, and restored graphics/TTY/overlays with no leaked resources. |
| Build | Offline import, shared RV32 settings, metadata, ELF size, dependency rebuilds, default application discovery, and installation. |

Run native adapter/core tests under ASan/UBSan, then real RV32 host integration through the loader. Host execution is another interpreter around the emulator and may be slower than device execution: use it for deterministic correctness, not native FPS predictions. Distinguish expected core accuracy limitations from TabOS adapter regressions.

Physical acceptance must record representative compatible ROMs, emulated speed, presented FPS, audio queue/underruns, button-to-response latency, maximum present/compute/save duration, stack high-water use, and total memory. Aim for normal DMG speed with continuous sound and responsive controls on supported ROMs; do not set a broad compatibility claim from one demo. Run a sustained session and repeated launch/save/quit cycles, then reboot and verify battery state.

Use project wrappers and the pinned toolchain for macOS/Linux Debug and Release and Tab5 cross-builds. Build through `./apps/build.sh`; inspect `make -C apps/gameboy metadata`. Keep hardware validation explicitly pending until performed.

## Delivery Checklist

- [ ] P1: implement/verify keyboard readiness and maintained SDK coverage.
- [ ] P2: implement/verify coherent pressed-state and resync behavior on host and Tab5.
- [ ] P3: validate deadline waits and presentation budget; complete conditional API work if required.
- [ ] P4: validate audio buffering, waits, flush, and teardown contracts.
- [ ] Pin core/APU and build independent ROM-loading smoke application.
- [ ] Implement bounded cartridge callbacks, video, controls, and rational frame scheduling.
- [ ] Implement APU adaptation, fractional sample production, and bounded PCM playback.
- [ ] Implement battery/RTC persistence and recoverable save errors.
- [ ] Pass sanitizer tests, actual RV32 integration, and supported-target builds.
- [ ] Complete sustained physical Tab5 gameplay, sound, input, and save acceptance.
- [ ] Write `docs/gameboy.md`; update application/build listings and affected input/wait/graphics/audio SDK documentation.
- [ ] Update `agents/roadmap.md` when prerequisite implementation starts and as validation completes; record accepted API decisions in architecture/context/testing documents.

Completion requires the prerequisites and playable emulator together: reliable held controls, correctly paced emulation, bounded sound, persistent cartridge state, and safe return to the shell. A silent framebuffer demo is an intermediate result, not the completed port.
