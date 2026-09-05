# TabOS Implementation Roadmap

> Status: living agent work tracker.
>
> Audience: coding agents. This is not user or contributor documentation.
>
> Update this file whenever work starts, finishes, becomes blocked, is reordered,
> or gains hardware validation. Keep user-facing behavior and instructions in
> `docs/`; keep architectural decisions in `agents/architecture.md` and
> `agents/TABOS_CONTEXT.md`.

## Checklist Rules

- `[x]` means implementation and required automated validation are complete.
- Hardware-dependent work is not complete until its separate hardware check is marked.
- `[ ]` under **Current Work** means active or immediately pending work.
- Reorder future items when dependencies change; do not silently discard them.
- Add newly discovered technical debt to **Maintenance and Technical Debt**.
- Do not treat experimental ELF work as a frozen executable or process ABI.

## Current Work

### Sprite and Tile Graphics

- [x] Add stateless animated draw helpers, completion query, and Tiled repeat metadata. macOS Debug 44/44 tests, four focused Release tests, and standard RV32 application builds pass; maintained tester animation checks still require execution on target.

- [x] Verify generated-C/binary metadata and 79 logical-canvas scene pairs from one fixture, with independent pixel expectations; macOS Debug/Release tests and generated-C RV32 compilation pass.

- [x] Add clip rectangles across SDK, private ABI, scalar, SDL, and Tab5 paths.
- [x] Add portable sprite, animation, metasprite, tilemap, object, property, and flag APIs.
- [x] Add validated versioned `.tsp` and `.tmap` loaders.
- [x] Add deterministic PNG/GIF/Tiled converter and generated C output.
- [x] Add explicit runtime-asset installation and MSC copying under `T:/data/<app>`.
- [x] Add original binary-backed `tile-demo` and host tests.
- [x] Review binary loaders, reject overlapping sections and invalid descriptors, and add 764 malformed-file, allocation-failure, and cleanup cases; macOS Debug/Release validation and standard RV32 app cross-build pass. See `milestone-tiling.md` for evidence.
- [x] Validate Debug/Release builds on macOS and Tab5.
- [ ] Validate Debug/Release builds on Linux.
- [ ] Validate rendering, repeated load/unload, terminal restoration, and performance on physical Tab5.

### RTC and Wall Clock

- [x] Add validated Gregorian calendar and signed Unix-epoch conversion.
- [x] Implement host wall clock and Tab5 RX8130CE backend.
- [x] Expose portable calendar read/write and libc wall-clock functions.
- [x] Add private ELF transport and host interpreter support.
- [x] Extend unit tests and maintained tester coverage.
- [x] Add UTC `date` utility for reading and setting wall-clock time.
- [x] Add text `cal` application using the current UTC wall clock.
- [x] Add standalone 640x360 graphical UTC `clock` application.
- [x] Validate RX8130 detection, calendar read, and unchanged-value write on physical Tab5 hardware.
- [ ] Validate RX8130 wall-clock retention across a physical Tab5 restart.

### Reboot and Shutdown

- [x] Add deferred kernel reboot and power-off request state.
- [x] Add Linux-style application `reboot()` API and private ELF transport.
- [x] Add host restart/exit behavior and Tab5 reset/power-control backends.
- [x] Add immediate `reboot` and `shutdown` core utilities.
- [x] Add host unit, build, and application validation.
- [ ] Validate orderly restart, battery power-off, externally powered halt, and TF integrity on Tab5.

### C17 Application Runtime

- [x] Decide `main(argc, argv)` entry contract and hidden `crt0` lifecycle.
- [x] Decide versioned API-table transport and initial newlib integration.
- [x] Decide process-local descriptor, cwd, errno, and cleanup semantics.
- [x] Decide 16 KiB stack and default 1 MiB bounded growing heap.
- [x] Decide CP437 byte-text, binary file, and stdio buffering semantics.
- [x] Decide blocking stdin plus immediate `O_NONBLOCK`/`EAGAIN` support.
- [x] Implement process-owned descriptors, cwd, errno, and heap arena.
- [x] Implement C runtime startup and newlib syscall stubs.
- [x] Convert independently loaded sample applications to standard `main`.
- [x] Validate libc, filesystem, heap, CP437, nonblocking input, and cleanup on host and Tab5.
- [x] Add modular `apps/tester` application for ongoing public SDK and ABI validation.
- [ ] Add RVC/compressed-instruction support to host execution, then select compressed
  application multilib for shared artifacts.

### Process Ownership and Failure Containment

- [x] Audit process, ELF mapping, execution task, descriptor, heap, graphics,
  console, and nested-parent ownership.
- [x] Make filesystem ELF resource teardown idempotent across partial startup,
  normal exit, requested exit, and platform-detected faults.
- [x] Add heap boundary guards and cleanup diagnostics.
- [x] Enforce bounded default per-process heap and stack limits.
- [x] Extend the maintained tester with leaked-resource and nested-failure cases.
- [x] Preserve host invalid-memory fault containment and PID 0 panic behavior.
- [x] Document that native Tab5 application faults remain device-fatal until a
  user-mode/PMP execution boundary and recoverable trap path are implemented.
- [ ] Design and implement the Tab5 user-mode/PMP boundary and recoverable native-fault path.

- [x] Replace generated C glyph tables with directly embedded raw bitmap font data.
- [x] Support compile-time fixed-width font width, height, cell size, and 1-256 glyphs.
- [x] Render glyph zero when requested byte is outside configured glyph count.
- [x] Expose font settings through `./tools/tabos config` and validate asset size.
- [x] Keep default CP437 font at 8x12 with 256 glyphs and exact 80x24 layout at scale 2.
- [x] Validate configurable font work with macOS Debug tests and macOS Release build.
- [x] Cross-build current configurable font work for Tab5 Debug and Release.
- [x] Flash current configurable font build and verify default CP437 output on Tab5 hardware.
- [x] Verify a reduced-glyph font uses glyph zero for missing high characters on host.

## Completed Foundation

### Repository and Build System

- [x] Create portable project directory structure and supporting files.
- [x] Separate complete build targets under `targets/` from platform implementations under `platform/`.
- [x] Support explicit `macos`, `linux`, and `tab5` targets.
- [x] Support Debug and Release configurations.
- [x] Provide `./tools/tabos` setup, configuration, build, test, and clean workflow.
- [x] Install and activate project-local ESP-IDF v5.4.4 through `./tools/tabos`.
- [x] Provide `tools/flash.sh [debug|release]`, defaulting to Debug.
- [x] Provide Tab5 serial monitoring through `./tools/tabos tab5 [debug|release] monitor`.
- [x] Support application builds from repository paths containing spaces.
- [x] Default fresh host and Tab5 builds to the filesystem-backed `T:/bin/shell`
  without requiring interactive project configuration.
- [x] Keep generated build output and local tool state ignored by Git.
- [x] Centralize project identity and common strings under `config/`.
- [x] Document repository structure and supported commands for users under `docs/`.

### Continuous Integration and Artifacts

- [x] Build macOS Debug and Release in GitHub Actions.
- [x] Build Linux Debug and Release on matching Ubuntu runner environment.
- [x] Cross-build Tab5 Debug and Release with ESP-IDF.
- [x] Use current Node-based GitHub actions without Node 20 deprecation warnings.
- [x] Cache or package SDL3 efficiently enough to avoid unnecessary Linux container startup.
- [x] Publish downloadable macOS, Linux, and Tab5 artifacts.
- [x] Include executable `run.sh` launchers in macOS and Linux artifacts.
- [x] Let macOS launcher remove quarantine attribute before starting unsigned host binary.
- [x] Package correctly capitalized Tab5 firmware image and flash metadata.

### Platform Boundaries

- [x] Keep portable core independent from SDL3, ESP-IDF, and FreeRTOS headers.
- [x] Share macOS and Linux SDL3 host backend.
- [x] Split host platform into runtime, display, input, and executable modules.
- [x] Split ESP32-P4 platform into runtime, display, keyboard, and executable modules.
- [x] Split optional keyboard diagnostics out of generic input queue implementation.
- [x] Reject incompatible target/platform source combinations.
- [x] Add architecture boundary and invalid-target tests.

### Host Runtime

- [x] Open 1280x720 logical SDL3 display using shared RGB565 framebuffer.
- [x] Match host terminal rendering, scale, wrapping, and behavior with Tab5.
- [x] Remember SDL window position across host runs.
- [x] Store host window state in platform-appropriate SDL preferences storage.
- [x] Support headless host smoke testing.
- [x] Preserve host-only development behavior below platform boundary.

### Tab5 Display

- [x] Initialize MIPI-DSI display and backlight.
- [x] Detect ILI9881C/GT911 display revision at runtime.
- [x] Detect ST7123/ST712x-v3 display revision at runtime.
- [x] Detect ST7121/ST712x-v1 display revision at runtime.
- [x] Use BSP path for ILI9881C and ST7123.
- [x] Use official ESP-IDF ST7121 component for ST7121.
- [x] Apply correct DSI rates for known controller variants.
- [x] Rotate shared 1280x720 framebuffer into native 720x1280 panel orientation.
- [x] Report detected display over serial at info level in Debug and Release.
- [x] Show hardware DSI color bars on ST7121 hardware.
- [x] Verify readable framebuffer colors on ST7121 Tab5 hardware.
- [ ] Verify current firmware on physical ILI9881C Tab5 revision.
- [ ] Verify current firmware on physical ST7123 Tab5 revision.

### Boot Diagnostics

- [x] Build one structured boot report shared by serial and framebuffer output.
- [x] Report TabOS version and build target.
- [x] Report detected display and framebuffer format/dimensions.
- [x] Report processor details.
- [x] Report internal heap, PSRAM, and physical flash capacity when known.
- [x] Report storage as unavailable until real filesystem mount exists.
- [x] Report keyboard initialization state.
- [x] Report kernel/runtime initialization state.
- [x] Use normal casing while retaining `OK`, `WARN`, and error status capitalization.
- [x] Present static boot framebuffer once and rely on LCD scanout when unchanged.
- [x] Add mounted filesystem capacity and free-space entries after storage implementation.
- [ ] Feed future driver/service initialization records into same boot report.

### Font and Text Rendering

- [x] Embed raw bitmap font data with assembler `.incbin`.
- [x] Support full 256-byte CP437 character set.
- [x] Keep glyph data separate from renderer implementation.
- [x] Support packed MSB-first rows, including glyph widths above eight pixels.
- [x] Support configurable glyph dimensions and terminal cell dimensions.
- [x] Support fonts containing 1-256 glyphs.
- [x] Fall back to glyph zero for missing high glyphs.
- [x] Support runtime terminal scale changes from 1 through 8.
- [x] Retain and redraw terminal contents after scale changes.

### Terminal and Console

- [x] Provide public console write and input APIs.
- [x] Implement colored character-cell buffer with foreground and background colors.
- [x] Implement cursor position, newline, carriage return, tab, backspace, and wrapping.
- [x] Protect prompt boundary in diagnostic console behavior.
- [x] Implement clear-screen behavior.
- [x] Implement configurable scrollback ring.
- [x] Separate live cursor from scrollback viewport.
- [x] Implement Page Up, Page Down, Home, End, and return-to-live behavior.
- [x] Map Tab5 Ctrl+Up/Down to Page Up/Down.
- [x] Map Tab5 Ctrl+Left/Right to Home/End.
- [x] Make scrollback shortcuts process-owned, opt-in TTY policy with child inheritance.
- [x] Hide cursor while viewing history.
- [x] Implement visible blinking cursor.
- [x] Build reusable monotonic polling timer service for cursor and future uses.
- [x] Implement dirty-cell rendering for ordinary text and cursor updates.
- [x] Preserve matching host and Tab5 console semantics.
- [x] Add tests for history, cursor, wrapping, navigation, reflow, and ownership.

### Keyboard and Input

- [x] Define platform-neutral key, text, modifier, repeat, and key-up/down events.
- [x] Implement thread-safe fixed-capacity input queue.
- [x] Implement SDL3 host keyboard backend.
- [x] Map macOS Option and Windows/Linux Win/Super keys to portable Sym input.
- [x] Implement Tab5 I2C keyboard backend in Normal matrix-event mode for independent multi-key state.
- [x] Expose Tab5 Sym and Aa/Shift physical keys and modifier state.
- [x] Implement process-owned cooked/raw input policy and Tab5 one-shot Sym/Aa translation.
- [x] Report Tab5 keyboard firmware and mode.
- [x] Keep missing Tab5 keyboard nonfatal and report warning.
- [x] Implement host Enter, Tab, held-key, and duplicate-text handling.
- [x] Implement optional non-consuming keyboard diagnostic monitor behind CMake flag.
- [x] Disable keyboard diagnostics by default after hardware validation.
- [x] Verify physical Tab5 keyboard produces correct serial diagnostic events.
- [ ] Add Tab5 USB HID keyboard backend using same input queue.
- [ ] Replace Tab5 keyboard polling with interrupt-driven wakeup if measurements justify it.

### Application Lifecycle

- [x] Define public application descriptor with name, version, ABI version, capabilities, and lifecycle callbacks.
- [x] Create fixed-capacity built-in application registry.
- [x] Start at most one cooperative foreground application.
- [x] Transfer console ownership to active application.
- [x] Support application exit, cleanup, status retention, and return to idle runtime.
- [x] Convert console diagnostic into registered `console-test` application.
- [x] Keep shell out of kernel and diagnostic application code.
- [x] Add lifecycle, registry, ownership, error-path, and cleanup tests.
- [x] Verify lifecycle behavior on host and Tab5.

### Experimental Independent ELF Loading

- [x] Build independently compiled minimal RV32 ELF fixture with GCC.
- [x] Parse bounded little-endian RV32 `ET_EXEC` images.
- [x] Validate program headers, segment bounds, entry point, image size, and relocations.
- [x] Copy loadable segments and zero required memory.
- [x] Pass versioned TabOS API table to application entry point.
- [x] Expose experimental console write and exit request API calls.
- [x] Add host parser, copy, malformed-image, limit, and cleanup tests.
- [x] Allocate writable PSRAM and create executable MMU alias on ESP32-P4.
- [x] Synchronize instruction/data caches before execution.
- [x] Retain and apply static RISC-V relocations for native PSRAM load bias.
- [x] Translate application string pointers back to readable data alias.
- [x] Execute independent hello ELF on Tab5 hardware.
- [x] Verify independently loaded hello ELF on physical Tab5.
- [ ] Decide whether final executable remains ELF or becomes converted TabOS format.
- [x] Define initial static relocation model; defer dynamic/imported symbol binding.
- [ ] Define final application ABI and versioning policy.
- [x] Define application memory ownership, deterministic cleanup, and host fault containment.
- [ ] Define Tab5 native crash containment after protection capabilities are validated.

## Next Milestone: Filesystem and Storage Foundation

### Decisions and API

- [x] Decide initial filesystem error/result model.
- [x] Decide initial opaque file and directory handle model.
- [x] Define portable public filesystem API for open, close, read, write, seek, stat, and directory iteration.
- [x] Define backend-owned drive registration contract.
- [x] Define path syntax and normalization rules.
- [x] Choose drive-letter namespace instead of Unix mount points.
- [x] Reserve `A:` for internal flash and `T:` for TF/microSD.
- [x] Implement drive table and backend-owned drive registration.
- [x] Migrate path normalization to drive-qualified paths.
- [x] Map host `A:` and `T:` to isolated controlled directories.
- [x] Expose Tab5 TF/microSD as `T:`.
- [ ] Add Tab5 internal-flash filesystem as `A:`.
- [x] Use `T:` microSD as initial available Tab5 drive.
- [x] Treat card absence at boot as nonfatal; live removal recovery remains future work.
- [x] Keep ESP-IDF, host file descriptors, and platform filesystem types out of public API.

### Portable Filesystem Core

- [x] Implement path validation and normalization.
- [x] Implement drive-letter routing.
- [x] Implement file/directory handle ownership and cleanup.
- [x] Implement application-facing filesystem API dispatch.
- [x] Add deterministic tests for paths, handles, errors, and boundary cases.

### Host Storage Backend

- [x] Map TabOS root to controlled host directory rather than exposing host filesystem.
- [x] Prevent path escape outside configured host root.
- [x] Support files, directories, metadata, and required seek semantics.
- [x] Create deterministic temporary-root component tests.
- [ ] Verify identical behavior on macOS and Linux.

### Tab5 Storage Backend

- [x] Implement microSD bring-up through ESP-IDF platform backend.
- [x] Select BSP microSD FAT as initial on-disk filesystem.
- [x] Handle absent card as nonfatal boot condition.
- [x] Report mounted capacity and free space through boot diagnostics.
- [x] Add opt-in `filesystem-test` application covering create, write, seek, read,
  reopen, metadata, rename, enumeration, and cleanup through public filesystem API.
- [x] Verify filesystem diagnostic against real controlled host root.
- [x] Cross-build filesystem diagnostic for Tab5 Debug.
- [x] Flash filesystem diagnostic and verify core operations on physical microSD.
- [x] Verify file read/write, directory listing, seek, rename, and cleanup on hardware.
- [ ] Verify remount and live card-removal behavior on hardware.
- [ ] Evaluate internal flash filesystem after microSD baseline works.

### Tab5 USB Storage Mode

- [x] Decide Delete-held-at-boot behavior for pre-shell USB storage access.
- [x] Add an early Tab5 boot-key sampling window before mounting `T:`.
- [x] Add ESP32-P4 TinyUSB MSC device service backed by the TF/microSD block device.
- [x] Keep `T:` unmounted and inaccessible to TabOS for the full export session.
- [x] Display a dedicated full-screen USB storage mode and safe-eject message.
- [x] Detect host safe eject and restart the Tab5 into normal boot.
- [x] Define USB disconnect as the fallback when the host sends no eject event.
- [ ] Validate safe-eject and USB-disconnect restart behavior on macOS hardware.
- [ ] Test repeated export, write, eject, restart, remount, and filesystem integrity on hardware.
- [ ] Reuse the service from the future shell-launched `usb-storage` application.
- [ ] After `A:` exists, optionally export its data partition as another selectable MSC LUN.
- [ ] Never expose firmware, bootloader, partition-table, or NVS flash regions.

## Current Milestone: Execute RV32 Applications on Host

- [x] Decide that host simulation executes the same RV32 artifact used by Tab5.
- [x] Select a small permissively licensed RV32 interpreter for initial host execution.
- [x] Integrate a pinned RV32IMA interpreter behind the platform executable boundary.
- [x] Bridge the experimental TabOS ELF API table without adopting Linux syscalls.
- [x] Execute `hello_elf` completely in host tests and verify output and exit status.
- [x] Bound guest instruction execution so runaway applications cannot hang host tests.
- [x] Retain guest CPU and memory state across bounded runtime-update slices.
- [x] Report illegal instructions and invalid guest memory accesses without crashing host.
- [ ] Add optional instruction/register tracing for application debugging.
- [ ] Evaluate GDB remote debugging after basic interpreter execution is stable.

This milestone gates filesystem-backed applications and shell utilities. Host development
must execute the real RV32 program artifact so application, loader, and ABI behavior can
be tested without repeated Tab5 flashing. Host-native recompilation may remain useful,
but is not a substitute for this execution path.

## Next Milestone: Persistent Nested Foreground Processes

- [x] Decide initial persistent nested foreground process model.
- [x] Decide initial Tab5 mapping of one managed FreeRTOS task per native user process.
- [x] Replace single active application context with fixed-capacity process table.
- [x] Reserve filesystem-backed persistent shell as root process, initially process 0.
- [x] Enforce process-0 liveness invariant across return, exit request, fault, and forced termination.
- [x] Add kernel panic state that reports process-0 failure to serial and framebuffer terminal.
- [x] Implement foreground process stack and parent/child relationships.
- [x] Add resumable ELF execution call gate that waits for child status at call site.
- [x] Add cooperative C child execution request and child-status collection API.
- [x] Transfer console and focused input ownership to stack top only.
- [x] Restore parent focus, execution, and child exit status when child completes.
- [x] Run native Tab5 ELF entry in application task while runtime/services continue.
- [x] Retain one independent host RV32 interpreter context per loaded process.
- [x] Keep executable memory and execution context until owning process exits.
- [x] Verify mixed code/data ELF writes through the ESP32-P4 executable PSRAM alias.
- [ ] Split ELF code and writable data mappings later if hardware validation shows the
  dual-alias arrangement cannot safely support writable application state.
- [ ] Define cooperative stop versus process-requested exit semantics.
- [x] Audit current foreground-only console, filesystem, loader, and lifecycle synchronization.
- [ ] Validate shell -> child -> grandchild nesting and unwind order on host and Tab5.
- [x] Validate every process-0 termination path panics without unload or automatic restart.
- [ ] Validate input, cursor, timers, display, filesystem, and network service progress
  while foreground native application remains active.

## Current Milestone: Filesystem-Backed Shell

- [x] Load external `T:/bin/shell.bin` as process 0.
- [x] Add managed Tab5 ELF task implementation so persistent entry does not run on runtime task.
- [x] Add ELF console text input, raw output, clear, filesystem, exec, and yield calls.
- [x] Add initial `help`, `clear`, `pwd`, `cd`, and `ls` commands.
- [x] Launch `T:/bin/<name>.bin` and explicit paths as foreground children.
- [x] Parse quoted/escaped shell arguments and deliver child-owned `argc`/`argv`.
- [x] Restore shell prompt and child status after child exit.
- [x] Provide standalone shell project with independent SDK-backed build/install rules.
- [x] Validate shell PID 0 loading in headless host runtime.
- [x] Validate interactive editing and commands in macOS SDL host.
- [x] Validate managed shell task, keyboard input, commands, and child launch on Tab5.

## Following Milestone: Load Applications from Files

- [x] Make loader consume filesystem handles rather than embedded byte arrays.
- [x] Preserve bounded reads and executable image size limits.
- [x] Give each filesystem-loaded ELF child ownership of its image and execution state.
- [x] Run configured ELF twice from a persistent launcher diagnostic without exiting PID 0.
- [x] Load and execute independent RV32 hello application from host controlled root.
- [x] Load independent hello application from Tab5 microSD.
- [ ] Define application file naming and discovery rules.
- [x] Define extensionless application names, default `T:/bin` PATH, and relative-path command syntax.
- [ ] Add optional application metadata for requested heap/stack limits when a concrete
  large-application requirement justifies it.
- [ ] Enumerate installed applications without hardcoded registry entries.
- [ ] Add minimal launcher diagnostic that lists and starts applications.
- [ ] Keep launcher diagnostic separate from shell.
- [ ] Verify application launch, output, exit status, cleanup, and repeat launch on host and Tab5.

## Future Milestones

### SDK and Pre-Release Application ABI

- [ ] Freeze and version the first supported application ABI only when TabOS is ready
  to support independently distributed third-party binaries.
- [x] Rebuild all bundled applications with ABI changes during pre-release development.
- [x] Publish public application headers independent from ESP-IDF and FreeRTOS.
- [x] Provide compiler, linker, strip, and packaging workflow for external applications.
- [ ] Optionally provide host-native application build mode for sanitizer-heavy source tests;
  keep RV32 execution as canonical binary/ABI validation.
- [x] Add argument passing and application-visible exit status.
- [x] Decide libc/newlib wrapper strategy.
- [x] Provide malloc/free through newlib and the process-owned growing heap.
- [x] Add monotonic time and cooperative sleep API suitable for applications.
- [x] Add portable system/device information API.
- [x] Document how users build and package applications under `docs/`.

### Process and Runtime Model

- [x] Decide initial process-to-FreeRTOS-task mapping.
- [x] Replace cooperative single-application runtime with nested foreground process stack.
- [x] Add process identifiers and public process metadata.
- [x] Add host application fault containment; document Tab5 native-fault limitation.
- [x] Define application memory ownership and cleanup guarantees.
- [x] Add nested-process, resource-ownership, and cleanup tests.
- [ ] Add a Tab5 recoverable native-fault boundary where hardware permits.
- [ ] Extend service-concurrency validation during long-running native applications.
- [ ] Measure SMP, memory pressure, and application failure behavior on Tab5.

### Shell Application

- [x] Build shell as first official application under `apps/`, never kernel code.
- [x] Implement prompt and editable command line using public console/input APIs.
- [x] Implement command parsing and arguments.
- [x] Implement current working directory.
- [x] Implement basic filesystem commands.
- [x] Execute independently stored applications.
- [x] Report application exit status.
- [ ] Add environment/configuration variables when underlying API exists.
- [ ] Defer pipes, redirection, and job control until process/I/O model supports them.
- [ ] Reach cross-platform `$ hello` / `Hello from TabOS` milestone on macOS, Linux, and Tab5.

### Graphics and Windowing

- [x] Define initial public fullscreen RGB565 graphics API.
- [x] Add clipped fill, pixel, line, rectangle, bitmap blit, and explicit present operations.
- [x] Restore the terminal framebuffer when a graphics application closes or exits.
- [x] Suspend terminal rendering and TTY shortcuts automatically during fullscreen graphics.
- [x] Add a portable interactive graphics demo and tester coverage.
- [x] Define portable queued transform, blend, color-key, and capability APIs.
- [x] Add ESP32-P4 PPA acceleration with transparent software fallback.
- [x] Add private ESP32-P4 PIE span acceleration with transparent scalar fallback.
- [x] Record physical PIE internal-RAM/PSRAM benchmarks and tune thresholds.
- [x] Add direct native-orientation scanout for fullscreen Tab5 graphics applications.
- [x] Add VSYNC-paced native double buffering for tear-free Tab5 graphics.
- [x] Add deterministic pixel-level clipping and framebuffer regression tests.
- [x] Add a portable graphics benchmark application.
- [x] Add application-owned integer-scaled logical canvases with automatic presentation.
- [ ] Validate PPA orientation, transforms, and fallback on physical Tab5 variants.
- [ ] Record host and Tab5 benchmark results.
- [ ] Decide framebuffer placement and buffering strategy from measurements.
- [ ] Add damage tracking and compositor service.
- [ ] Implement window manager as client of public graphics/input services.
- [ ] Support fullscreen application optimization without granting hardware ownership.
- [ ] Add deterministic framebuffer regression tests.
- [ ] Measure display bandwidth and frame rate on Tab5.

### Touch and Pointer Input

- [ ] Define touch/pointer events in public input API.
- [ ] Implement Tab5 touch backend for all supported controller variants.
- [ ] Map SDL mouse/touch events to same portable event model.
- [ ] Add focus and event-routing policy for windowing system.
- [ ] Add synthetic host tests and physical Tab5 tests.

### Networking

- [x] Investigate supported ESP32-P4/ESP32-C6 transport in pinned ESP-IDF.
- [x] Add bounded parsing and loading for version 1 `T:/etc/wifi.conf`.
- [x] Add atomic configuration updates while preserving unknown future settings.
- [x] Add the internal Wi-Fi connection service with three-attempt autoconnect.
- [x] Expose Wi-Fi status, saved connect, and disconnect through the application API and `netctl`.
- [x] Decide on bounded BSD-like sockets with TabOS-owned types and wait sets.
- [x] Implement host socket backend.
- [x] Implement Tab5 ESP32-C6 Wi-Fi, DNS, ICMP, TCP/UDP, and wait-set backends.
- [x] Add certificate-verified TLS client connections and HTTPS utilities.
- [x] Add deterministic portable tests plus physical Wi-Fi, DNS, ICMP, socket, NTP,
  and TLS validation.
- [ ] Add Wi-Fi scan, forget, and interactive configuration workflows.

### Audio

- [x] Inventory and initialize Tab5 audio hardware.
- [x] Define public audio stream/mixer API.
- [x] Implement shared mixer.
- [x] Implement SDL3 host output backend.
- [x] Implement Tab5 audio backend.
- [x] Expose native 8–96 kHz hardware rates with a 44.1 kHz default and shared-clock arbitration.
- [x] Monitor the Tab5 headphone jack and automatically mute the main speaker on insertion.
- [ ] Validate headphone insertion, speaker muting, and unplug restoration on physical Tab5 hardware.
- [x] Add application-local DMX sound-effect mixing and TabOS PCM output for DOOM.
- [x] Reuse and flush DOOM playback streams so effect replacement does not stall rendering.
- [ ] Add DOOM MUS/MIDI synthesis when a suitable bounded software synthesizer is selected.
- [ ] Test mixing, formats, latency, and real hardware output.

### Remaining Hardware and System Services

- [x] Add fixed-capacity internal device registry model with generation-tagged IDs,
  tombstones, copied lookup, and ready/offline/fault state.
- [x] Expose public copied device enumeration and process-owned lifecycle events.
- [x] Register detected display, keyboard, mounted storage, RTC, battery, and Wi-Fi drivers;
  synchronize Wi-Fi offline/fault/ready state and provide deterministic host virtual devices.
- [x] Report detected-but-failed keyboard, RTC, and battery initialization as registry faults.
- [x] Add `devices` core utility and maintained tester coverage for enumeration, lifecycle
  subscriptions, stale handles, and failed-process cleanup.
- [x] Validate `devices` output and device-registry tester coverage on physical Tab5.
- [x] Add the generic process-owned wait API, generation-tagged sources, socket adapter,
  stale-parent rejection, and teardown cancellation.
- [x] Add device-subscription wait sources and reusable hooks for future hardware-service adapters.
- [x] Add maintained validation for socket/device waits, mixed readiness, ownership,
  stale handles, cancellation, network transitions, and teardown.
- [ ] Run complete generic-wait validation on macOS and Linux; physical Tab5 passes.
- [x] Add RTC and wall-clock service with live `rtc0` fault/ready reporting; physical restart-retention validation remains.
- [x] Add validated battery telemetry fields, charger controls, and live `battery0` fault/ready reporting;
  physical telemetry, charger, external-power, and shutdown validation remains.
- [ ] Add USB host/OTG service beyond keyboard support.
- [ ] Add camera service when application needs justify it.
- [ ] Add supported sensor APIs.
- [x] Add initial device/system information API.

### IPC and Advanced I/O

- [ ] Decide smallest useful IPC model.
- [ ] Add TabOS-owned messages/events/queues without exposing FreeRTOS handles.
- [ ] Add pipes only when process and stream ownership are defined.
- [ ] Add shared memory only if required and safe.

### Host Fidelity and Developer Tooling

- [ ] Add framebuffer capture and deterministic visual comparison tools.
- [ ] Add synthetic input injection for integration tests.
- [ ] Add controlled filesystem fixtures.
- [x] Run actual RV32 binaries on host through a resumable RV32IMA interpreter.
- [x] Batch host RV32 instructions while preserving bounded scheduling, API-gate
  dispatch, exact fault reporting, and resumable execution.
- [ ] Add source-level application debugging workflow.
- [ ] Decide first-class C++ support timing.

## Testing and Release Work

- [x] Enable strict compiler warnings for supported targets.
- [x] Run host unit tests with AddressSanitizer and UndefinedBehaviorSanitizer where supported.
- [x] Provide headless host integration smoke test.
- [x] Cross-compile portable code for Tab5 in CI.
- [x] Build Debug and Release variants for all targets in CI.
- [x] Publish direct-download artifacts.
- [ ] Add Linux runtime smoke tests against packaged artifact.
- [ ] Add SDL adapter-specific input and window-state tests.
- [ ] Add deterministic graphics framebuffer/hash regression tests.
- [ ] Add formatter/linter checks.
- [ ] Add static analysis such as clang-tidy or cppcheck.
- [ ] Add scheduled deeper sanitizer/static-analysis jobs if normal CI becomes too slow.
- [ ] Define repeatable hardware test checklist per subsystem.
- [ ] Evaluate hardware-in-the-loop automation later.

## Maintenance and Technical Debt

- [x] Migrate existing generic `tab_*` and internal-only `tabos_*` symbols to decided
  layer/subsystem prefixes without changing public `tabos_*` ABI.
- [x] Rename portable platform contract from `tab_platform_*` to `platform_*`.
- [x] Distinguish generic `espidf_*`, shared `esp32_*`, chip-specific `esp32p4_*`, and
  board-specific `tab5_*` helpers during platform rename.
- [x] Add architecture lint rejecting new generic `tab_*` declarations and internal-only
  declarations using public `tabos_*` prefix.

- [ ] Keep `agents/roadmap.md` synchronized after every milestone.
- [ ] Keep `agents/TABOS_CONTEXT.md`, `agents/architecture.md`, and `agents/testing.md` synchronized when decisions change.
- [ ] Keep user-facing `docs/` synchronized when commands, APIs, structure, or behavior change.
- [ ] Review experimental ELF loader for final ABI-independent boundaries before extending it.
- [ ] Replace provisional storage namespace examples only after mount-layout decision.
- [ ] Measure terminal dirty-rendering cost on hardware as output volume increases.
- [ ] Measure keyboard polling overhead before implementing interrupt path.
- [ ] Audit tracked files and generated artifacts before releases.

## Explicitly Deferred

- [ ] General touch support until GUI/windowing work begins.
- [ ] Full desktop/window manager until graphics, input routing, and process APIs exist.
- [ ] Full POSIX or Unix compatibility.
- [ ] Pipes, redirection, and shell job control until process/I/O ownership exists.
- [ ] Meaningful application isolation until ESP32-P4 protection capabilities are validated.
- [ ] Dedicated graphics CPU core unless profiling proves need.
- [ ] Complete QEMU Tab5 emulation.
- [ ] Bare-metal replacement for FreeRTOS unless demonstrated limitation requires reconsideration.
