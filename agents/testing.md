# TabOS Testing and Multi-Target Development Strategy

> Status: agreed development and testing direction as of 2026-08-10.
>
> Purpose: define how TabOS should be structured, built, and tested across the real M5Stack Tab5 hardware target and native macOS/Linux development targets.
>
> This document is written primarily for Codex and contributors. It should be read together with `architecture.md` and `TABOS_CONTEXT.md`.

## 1. Goal

TabOS should be developed as a **multi-target codebase**, not as firmware that can only be meaningfully exercised on physical hardware.

The same project should be able to build for at least:

```text
tab5
macos
linux
```

The Tab5 build is the real hardware target.

The macOS and Linux builds are native host-development targets intended to run as normal desktop processes.

For macOS and Linux, **SDL3** should provide the main host abstraction for:

- display/window creation
- keyboard input
- mouse/pointer input
- touch-like input where appropriate
- audio
- timing
- controller/input support where useful

The host builds are not intended to simulate every electrical or peripheral detail of the Tab5.

Their purpose is to run as much of the actual TabOS implementation as possible with rapid local iteration.

---

## 2. Core Principle

The preferred architecture is:

```text
                       Shared TabOS code
                              |
             +----------------+----------------+
             |                |                |
             v                v                v
          Tab5             macOS            Linux
             |                |                |
        ESP-IDF /          SDL3 +           SDL3 +
        FreeRTOS           host OS          host OS
```

Most operating-system behavior should be implemented once.

Platform-specific code should be kept behind narrow interfaces.

The macOS and Linux builds must not become separate reimplementations of TabOS.

---

## 3. Why Host Builds Exist

Host builds are a development and testing tool.

They should allow work on major parts of TabOS without repeatedly:

```text
compile firmware
flash device
reboot device
reproduce state
debug through serial output
```

Instead, a large class of changes should support a workflow like:

```text
edit
 ↓
native compile
 ↓
run
 ↓
test/debug
```

This should make normal development substantially faster.

### Current verified workflow

- Host targets use SDL3 and build through `./tools/tabos`.
- `./tools/tabos config` stores TabOS-owned host and Tab5 settings in `.local/tabos.config`; wrapper commands pass them explicitly to every configuration.
- Tab5 firmware requires ESP-IDF v5.4.4. `./tools/tabos setup` installs host prerequisites and a project-local copy under `.local/`; Tab5 wrapper commands activate it automatically when needed.
- Tab5 wrapper commands may use an already-active ESP-IDF only when `IDF_PATH` is set and `idf.py --version` reports the exact required version. This permits the pinned ESP-IDF CI container while preventing accidental builds with another active SDK version.
- Project-local ESP-IDF tools live under `.local/espressif-tools`, keeping them separate from global ESP-IDF installations.
- Hardware flashing uses local `esptool` through `./tools/flash.sh [debug|release]`; debug is the default.
- Hardware serial monitoring uses `./tools/tabos tab5 [debug|release] monitor`; debug is the default and `ESPPORT` selects a device when needed.
- Generated firmware lives in `build/tab5-debug/` or `build/tab5-release/` and uses `TabOS.bin` capitalization.
- Info-level serial logging is required in debug and release so detected hardware remains visible during boot.
- Current host suite has unit, integration, architecture-boundary, and invalid-target tests. Display transforms must remain host-unit-tested.

Host builds are especially useful for:

- shell development
- parser development
- filesystem semantics
- application APIs
- UI
- graphics primitives
- compositor logic
- terminal rendering
- input dispatch
- application lifecycle
- executable tooling
- libraries
- configuration handling
- utility applications
- error handling
- integration tests

### Persistent foreground-process validation

[DECIDED] Process tests must model persistent nested execution rather than restarting
caller after each command. Minimum scenario is shell process 0 executing child, child
executing grandchild, then deterministic reverse-order unwind. Tests must prove:

- blocked parents remain loaded and retain execution state, memory, handles, and identity
- only top process receives focused input and foreground console ownership
- child exit status returns to immediate parent
- child fault cleans child and resumes parent according to defined policy
- parent resumes at execution call site without rerunning entry point
- executable memory remains allocated until owning process fully stops
- cleanup cannot race active application API calls
- process-table and nesting-depth limits fail without corrupting current foreground stack

Process 0 is exceptional and must have dedicated invariant tests. Normal return, explicit
exit request, execution fault, and forced termination must each enter kernel panic state.
Tests must prove process 0 is neither unloaded nor restarted, panic does not attempt to
resume nonexistent parent, and failure cause/exit status appears through both captured
serial/log output and framebuffer console/terminal output. Panic rendering must work even
when process-0 console session is stale or unavailable.

Deterministic lifecycle coverage exercises exit request, executable return, execution
fault, and forced termination. Console tests build against the platform mutex contract;
hardware validation remains responsible for detecting task starvation, lock inversion,
and watchdog regressions under the FreeRTOS implementation.

Host RV32 tests must force multiple instruction-slice yields before child completion and
verify retained PC, registers, memory, and parent state. Tab5 tests must keep native child
active while independently proving keyboard polling, timer/cursor updates, display work,
filesystem operations, and other runtime/service progress. Infinite or non-cooperative
native application must not execute on runtime/service task.

Argument tests cover guest `argc`/`argv` memory construction plus shell parsing of plain,
single-quoted, double-quoted, escaped, empty, unterminated, and excessive arguments.

`apps/tester` is the maintained end-to-end SDK validation application. Keep individual
concerns in separate test modules and run the same independently built RV32 artifact on
host and Tab5. New public application APIs should add tester coverage where hardware or
full-runtime behavior cannot be proven by ordinary host unit tests. Tests must clean up
persistent files and directories and return nonzero when any assertion fails.
Process module must remain self-contained: tester parent launches tester child, child
launches tester grandchild, known statuses unwind in reverse, and parent repeats chain to
prove cleanup and reload. Run tester from shell so this also exercises persistent PID 0.

---

## 4. Target Definitions

### 4.1 `tab5`

The real hardware target.

Expected platform stack:

```text
TabOS
 ↓
TabOS ESP32-P4 platform backend
 ↓
ESP-IDF / FreeRTOS
 ↓
M5Stack Tab5
```

This build validates:

- actual RISC-V compilation
- actual ESP32-P4 execution
- real memory behavior
- PSRAM behavior
- hardware display
- real keyboard controller
- touchscreen
- microSD
- audio hardware
- ESP32-C6 networking
- DMA
- timing behavior
- hardware resource constraints

### 4.2 `macos`

Native macOS development build.

Expected platform stack:

```text
TabOS
 ↓
TabOS host platform API
 ↓
SDL3 + POSIX/macOS facilities
 ↓
macOS
```

The executable should run like a normal native macOS application or command-line process.

### 4.3 `linux`

Native Linux development build.

Expected platform stack:

```text
TabOS
 ↓
TabOS host platform API
 ↓
SDL3 + POSIX/Linux facilities
 ↓
Linux
```

macOS and Linux should share almost all host implementation code.

Avoid unnecessary:

```text
platform/macos/
platform/linux/
```

duplication when:

```text
platform/host/
```

can handle both.

OS-specific host code should only exist where the underlying systems genuinely differ.

---

## 5. Platform Layer

Portable code should consume a TabOS platform interface.

Conceptually:

```c
platform_init();
platform_shutdown();

platform_time_us();

platform_display_present(...);

platform_input_poll(...);

platform_audio_write(...);

platform_storage_open(...);
```

These names are illustrative and are not a frozen API.

Implementations may look like:

```text
platform/
├── esp32p4/
│   ├── display.c
│   ├── input.c
│   ├── storage.c
│   ├── audio.c
│   ├── network.c
│   └── runtime.c
│
└── host/
    ├── sdl_display.c
    ├── sdl_input.c
    ├── sdl_audio.c
    ├── host_storage.c
    ├── host_network.c
    └── host_runtime.c
```

The platform interface belongs below normal TabOS subsystems.

Application code must not use the platform interface directly unless it is itself a low-level system component.

---

## 6. SDL3 Host Backend

SDL3 is the preferred abstraction for functionality that maps naturally to desktop multimedia/input facilities.

### SDL3 should be used for

- creating the TabOS display window
- presenting the TabOS framebuffer
- keyboard events
- mouse events
- translating pointer events into touch-like input
- game controller support if later useful
- audio output
- timers where appropriate
- clipboard integration only if explicitly added as a host feature

### SDL3 should not define TabOS architecture

Portable TabOS code must not directly include SDL headers.

Avoid:

```c
#include <SDL3/SDL.h>
```

inside:

```text
graphics/
input/
apps/shell/
apps/
fs/
kernel portable code
```

SDL belongs in the host platform/backend layer.

Correct relationship:

```text
TabOS graphics
      ↓
platform display interface
      ↓
SDL3
```

not:

```text
TabOS graphics
      ↓
SDL3
```

---

## 7. Display Simulation

The macOS/Linux build should display the Tab5 logical screen in an SDL3 window.

The host display should use the same logical display dimensions and pixel model as the hardware configuration whenever practical.

For the current Tab5:

```text
1280 × 720
```

The SDL window may support scaling independently from the logical framebuffer.

For example:

```text
TabOS framebuffer: 1280x720
SDL window:         1280x720
```

or:

```text
TabOS framebuffer: 1280x720
SDL window:         scaled to desktop
```

Scaling must not alter the coordinate system seen by TabOS applications.

A pointer event received in the scaled host window should be translated back to TabOS display coordinates.

---

## 8. Input Simulation

SDL keyboard input should feed the same TabOS input subsystem used on hardware.

Conceptually:

```text
Tab5:

STM32 keyboard
      ↓
Tab5 keyboard driver
      ↓
TabOS input event
      ↓
application
```

```text
Host:

SDL3 keyboard event
      ↓
SDL host input driver
      ↓
TabOS input event
      ↓
application
```

Everything above the platform driver should ideally be identical.

This makes host testing useful for:

- shell input
- shortcuts
- text input
- key repeat
- modifier handling
- event routing
- GUI focus behavior

Hardware-specific tests are still required for the real STM32 keyboard controller and I2C/interrupt behavior.

---

## 9. Touch and Pointer Simulation

This section describes eventual GUI/input testing, not current implementation priority. Touch and host pointer-to-touch mapping are deferred until the windowing system and application touch API. Terminal, keyboard, and shell work should not wait for touch support.

Current keyboard coverage includes host tests for HID-to-text translation, key/text event ordering, queue overflow policy, and runtime bootstrap with a fake keyboard platform. SDL3 and the Tab5 I2C backend feed the same public queue. Hardware validation must confirm that boot diagnostics show `TAB5 KEYBOARD FW ...; HID MODE`; missing keyboard must remain a warning rather than preventing boot. Builds configured with `TABOS_ENABLE_KEYBOARD_DIAGNOSTICS=ON` also log every normalized key/text event without consuming the queue; this flag defaults off. USB HID keyboards on Tab5 are not yet supported.

Console tests cover exclusive foreground acquisition, background and stale-session rejection, rejected-read non-consumption, cursor pixels and blink phase, clearing, history ring overflow, Page Up/Down/Home/End navigation, automatic return to live output, and scale reflow with retained cells. Timer tests cover one-shot, repeating, late-poll skipping, and cancellation behavior with fake monotonic time. Manual validation must verify shell opt-in consumes Page Up/Down/Home/End and Tab5 Ctrl+Arrow equivalents, a disabled mode delivers those events to the application, children inherit by value, and returning to the parent restores its unchanged mode. `TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP=ON` provides manual cross-target keyboard-to-framebuffer validation and defaults off. Diagnostic application is not shell.

Application lifecycle tests cover descriptor validation, duplicate rejection, registry lookup, startup failure cleanup, PID metadata, process-table retention, PID 0→1→2 nesting, blocked-parent state, console focus transfer, reverse-order status unwind, root-exit panic transition, and shutdown cleanup. Runtime smoke test verifies configured `console-test` remains PID 0 after Ctrl+Q reports completion. Same portable lifecycle code compiles into host and Tab5; host executes deterministic tests while Tab5 cross-build verifies target compatibility.

The C runtime test matrix must use the same independently built applications on
host and Tab5. Cover `main(argc, argv)`, return-to-exit status, descriptors
0/1/2, file descriptors 3+, open/read/write/close/lseek/stat/fstat/mkdir/unlink/
rename, inherited but isolated working directories, process-local errno,
line-buffered stdout, unbuffered stderr/stdin, binary-transparent I/O, heap
growth and limit failure, and deterministic cleanup after success and failure.
Test blocking stdin plus `O_NONBLOCK`/`EAGAIN`. Text fixtures use raw CP437 bytes;
host Unicode input outside CP437 must be rejected or explicitly substituted.

ELF loader tests use real RV32 fixture and cover format metadata, segment bounds, executable entry, supported static `SHT_RELA` processing, unsupported relocation rejection, image-size limit, memory copy, unload, and malformed inputs under host sanitizers. Host executes same RV32 bytes through resumable interpreter and must cover multiple instruction slices, API-table calls, argument vectors, console output, return status, illegal instructions, and invalid guest memory access. Tab5 hardware validation covers dual PSRAM aliases, load-bias relocation, final cache synchronization, globals/BSS/newlib state, native API-table calls, arguments, console output, return status, and cleanup. Expected success text begins with `Hello TabOS!`.

Manual console validation must include prompt-boundary Backspace, held Backspace, held printable keys, Enter, and Tab followed by visible text. Host backend synthesizes missing Enter/Tab/repeat text while retaining SDL text input for normal layout and IME behavior; matching SDL text events are suppressed to prevent duplicates.

Desktop mouse/pointer events should be mapped into the TabOS touch event model.

For example:

```text
SDL mouse down  → touch down
SDL mouse move  → touch move
SDL mouse up    → touch up
```

This is an approximation for development purposes.

Where SDL3 provides appropriate touch events, native touch input may also be supported.

Portable TabOS code should receive the same event structure on host and Tab5 wherever possible.

---

## 10. Audio Simulation

The host build should use SDL3 audio for the TabOS audio backend.

Conceptually:

```text
TabOS mixer
    ↓
TabOS audio platform interface
    ↓
SDL3 audio
```

and on hardware:

```text
TabOS mixer
    ↓
TabOS audio platform interface
    ↓
ESP32-P4 audio driver
```

The TabOS mixer and public audio behavior should be shared.

Only the output transport should differ.

---

## 11. Filesystem Strategy

The host build should use a controlled host directory as the TabOS filesystem root rather than exposing the entire developer machine directly.

For example:

```text
build/host/rootfs/
```

could appear inside TabOS as:

```text
/
```

This allows tests to create predictable filesystem layouts such as:

```text
/
├── bin/
├── apps/
├── home/
├── etc/
└── tmp/
```

The host filesystem adapter should translate TabOS paths to files below the configured root.

Portable filesystem behavior should remain shared.

Do not allow host-only behavior to accidentally define the TabOS filesystem API.

---

## 12. Networking

Native macOS/Linux builds may use host sockets behind the TabOS networking abstraction.

Conceptually:

```text
TabOS networking API
        ↓
network subsystem
        ↓
platform network backend
        ↓
+----------------------+------------------+
| host sockets         | P4/C6 networking |
+----------------------+------------------+
```

This permits substantial network stack/API testing without requiring the physical device.

Tests that depend specifically on ESP32-C6 behavior still belong to hardware testing.

---

## 13. Runtime and Task Abstraction

Portable TabOS components should not assume that a FreeRTOS task is the only possible execution primitive.

Where TabOS needs runtime abstractions, the host implementation may map them to:

- pthreads
- C++ threads
- SDL threading primitives
- another minimal host implementation

The choice should remain below the abstraction boundary.

Conceptually:

```text
TabOS task abstraction
       ↓
+--------------------+----------------------+
| FreeRTOS task      | host native thread   |
+--------------------+----------------------+
```

Do not expose `TaskHandle_t` through shared APIs.

---

## 14. Build System Requirements

The project build system should make targets explicit.

A desired developer interface is approximately:

```text
build tab5
build macos
build linux
```

The final command syntax is not yet fixed.

Possible implementations include CMake presets, wrapper scripts, or a small project tool.

Whatever is chosen should make the target obvious and reproducible.

### Build requirements

The build system should:

- compile shared code once per target
- select the proper platform backend
- reject incompatible platform source combinations
- support Debug and Release configurations
- support tests independently from full firmware builds
- make SDL3 a host-only dependency
- keep ESP-IDF dependencies out of host builds
- keep host dependencies out of Tab5 firmware

---

## 15. Compile-Time Target Selection

Prefer explicit build-system source selection over widespread preprocessor branching.

Good:

```text
target_sources(tabos PRIVATE
    platform/host/sdl_display.c
)
```

Avoid turning shared files into large blocks such as:

```c
#ifdef TABOS_MACOS
...
#elif TABOS_LINUX
...
#elif TABOS_TAB5
...
#endif
```

Small compile-time differences are acceptable when genuinely necessary.

Platform-specific implementations should normally live in separate source files.

Useful target defines might include:

```text
TABOS_TARGET_TAB5
TABOS_TARGET_HOST
TABOS_HOST_MACOS
TABOS_HOST_LINUX
```

but those should not become substitutes for proper platform boundaries.

---

## 16. Testing Layers

Testing should be divided into levels.

```text
                     +------------------+
                     | Hardware tests   |
                     +------------------+
                             ↑
                     +------------------+
                     | Integration      |
                     +------------------+
                             ↑
                     +------------------+
                     | Component tests  |
                     +------------------+
                             ↑
                     +------------------+
                     | Unit tests       |
                     +------------------+
```

The lower levels should run frequently and quickly.

The higher levels provide realism.

---

## 17. Unit Tests

Pure logic should be tested without SDL3 or hardware whenever possible.

Examples:

- parsers
- path normalization
- shell tokenization
- data structures
- allocators
- event queues
- clipping calculations
- geometry
- text layout
- executable metadata parsing
- command-line argument handling
- filesystem path handling
- configuration parsing

A unit test should normally build and run as a standard host executable.

Unit-testable code should not depend on ESP-IDF.

---

## 18. Component Tests

Component tests may instantiate a real TabOS subsystem with fake or host platform dependencies.

Examples:

```text
filesystem + temporary rootfs
input subsystem + synthetic events
graphics + memory framebuffer
shell + fake terminal
loader + fixture executable
```

These tests should validate subsystem contracts without requiring the full simulated OS.

---

## 19. Host Integration Tests

The full host build should support integration testing.

Examples:

### Shell

Start TabOS, inject keyboard input:

```text
echo hello
```

and verify expected console output.

### Filesystem

Start with a fixture root filesystem, execute:

```text
ls /apps
```

and validate results.

### Application lifecycle

Launch a host-compatible test application and verify:

- arguments
- stdout
- filesystem access
- exit status
- cleanup

### Graphics

Render known content into the shared graphics pipeline and verify framebuffer results.

When possible, validate framebuffer memory or drawing commands rather than relying solely on screenshots.

---

## 20. Headless Host Testing

SDL3 host code should support a headless or noninteractive mode where practical.

CI must not require a person to interact with a visible window.

Possible mechanisms include:

- SDL dummy/offscreen video driver
- an explicit headless platform display implementation
- in-memory framebuffers
- injected input events

The architecture should not require a graphical desktop merely to exercise portable TabOS logic.

---

## 21. Graphics Regression Tests

Graphics should support deterministic tests.

Prefer verifying rendered framebuffer output.

A test may:

1. create a known framebuffer
2. draw a known UI or scene
3. hash or compare the result
4. fail when output changes unexpectedly

Reference-image tests can be useful, but they should be used selectively because intentional rendering changes require updates.

For lower-level primitives, direct pixel/region assertions are preferable.

Accelerated Tab5 kernels must match scalar output byte-for-byte. Test aligned and
unaligned buffers, tails, overlap, clipping, RGB565 blend boundaries, color keys,
font scaling, cursor inversion, and full terminal redraw. Physical diagnostics must
measure internal RAM and PSRAM separately. Keep only repeatable speedups; host tests
cannot establish PIE performance or context-switch correctness.

Fullscreen graphics tests must verify terminal writes, redraws, cursor timers, and
scrollback navigation cannot alter or present the graphics framebuffer. TTY navigation
keys must reach the graphics application regardless of its inherited TTY mode. Closing
or faulting the application must redraw and present the retained terminal exactly once.

---

## 22. Input Tests

Portable input behavior should support synthetic events.

For example:

```c
test_input_key_down(...);
test_input_key_up(...);
```

or an equivalent internal testing interface.

This allows deterministic validation of:

- modifiers
- repeat behavior
- focus
- text input
- keyboard shortcuts
- shell interaction

Tests must not need actual SDL keyboard events unless testing the SDL adapter itself.

---

## 23. Platform Contract Tests

Both host and Tab5 implementations should satisfy common behavioral contracts.

For example:

```text
platform clock is monotonic
filesystem read/write semantics match
input events use valid coordinates
display buffer dimensions are correct
timers do not fire before their deadline
```

Where possible, the same test logic should be reusable against multiple platform implementations.

This helps prevent:

```text
works on macOS
```

from becoming:

```text
but behaves differently on Tab5
```

---

## 24. Hardware Tests

Some behavior can only be validated on the Tab5.

Required hardware testing areas include:

- boot
- PSRAM allocation
- executable memory behavior
- RISC-V application loading
- cache behavior
- display DMA
- MIPI-DSI output
- keyboard I2C protocol
- keyboard interrupt behavior
- touchscreen
- microSD
- C6 networking
- real audio output
- sleep/power behavior
- timing under load
- SMP behavior
- memory pressure
- peripheral concurrency

Host tests do not replace these.

They reduce how often hardware is needed during normal development.

---

## 25. Cross-Target Compile Testing

Portable code should be compiled for all supported targets regularly.

A change is not considered portable merely because the macOS build succeeds.

At minimum, CI or developer validation should cover:

```text
macOS build
Linux build
Tab5 cross-build
```

Where CI access to the ESP-IDF toolchain is practical, every relevant change should at least compile the Tab5 firmware even when physical hardware testing cannot run.

This catches problems such as:

- accidental host-only APIs
- unavailable libc functions
- pointer-size assumptions
- unsupported compiler behavior
- platform header leakage
- incorrect feature detection

---

## 26. CI Strategy

A likely CI matrix is:

```text
                Debug       Release
macOS             ✓            ✓
Linux             ✓            ✓
Tab5 compile      ✓            ✓
```

Not every combination necessarily needs to run on every small commit if CI cost becomes excessive, but the project should regularly exercise all of them.

A normal pull-request pipeline should eventually include:

```text
format/lint
 ↓
host unit tests
 ↓
host component tests
 ↓
macOS build
 ↓
Linux build
 ↓
Tab5 cross-compile
```

Optional or scheduled pipelines may additionally include:

- sanitizers
- static analysis
- graphics regression tests
- hardware-in-the-loop tests

---

## 27. Sanitizers

Host builds should take advantage of tools that are much easier to use on desktop systems.

Debug/test builds should support, where practical:

- AddressSanitizer
- UndefinedBehaviorSanitizer

Potentially later:

- ThreadSanitizer

This is a major reason to keep portable C/C++ code host-buildable.

Bugs discovered with host sanitizers should be treated as real bugs even when they do not immediately fail on the ESP32-P4.

---

## 28. Static Analysis

Portable code should be suitable for normal desktop static-analysis tooling.

Potential tools include:

- compiler warnings
- clang-tidy
- cppcheck
- project-specific linters

Compiler warning levels should be reasonably strict.

Avoid maintaining separate quality standards for firmware and host code.

---

## 29. Determinism

Tests should avoid depending unnecessarily on:

- wall-clock time
- real keyboard timing
- network availability
- desktop window focus
- actual SD cards
- hardware frame timing

Use injectable clocks, synthetic input, temporary filesystem roots, and mock/fake backends where useful.

This keeps tests fast and reliable.

---

## 30. Testability as an Architectural Constraint

Code should not be made portable only after it becomes difficult to test.

When designing a subsystem, ask:

```text
Can this logic run in a host unit test?
```

If the answer is no because it directly references:

```text
ESP-IDF
FreeRTOS
SDL3
macOS
Linux
hardware registers
```

then the dependency may be at the wrong architectural layer.

Hardware adapters are naturally exceptions.

The goal is not to abstract everything.

The goal is to isolate things that genuinely differ between targets.

---

## 31. SDL3 Testing Boundary

Tests for portable TabOS behavior should not require SDL3.

SDL3 itself should be tested at the adapter boundary.

For example:

```text
SDL_EVENT_KEY_DOWN
       ↓
host SDL adapter
       ↓
TABOS_INPUT_KEY_DOWN
```

A test of that translation may use SDL-specific structures.

A test of TabOS key handling should begin with:

```text
TABOS_INPUT_KEY_DOWN
```

and remain SDL-independent.

The same applies to display and audio.

---

## 32. Host Applications vs Target Applications

There are two useful host-development modes.

### Mode A — Host-compiled applications

Compile applications as native macOS/Linux code against the host TabOS API.

Advantages:

- extremely fast builds
- native debugger support
- sanitizers
- easy testing

This should be the initial development workflow.

### Mode B — Real RISC-V TabOS applications

Eventually, the host environment may support executing the actual RISC-V binaries built for TabOS.

That would provide stronger fidelity for:

- ABI testing
- loader testing
- executable-format testing
- target-specific behavior

This might use:

- QEMU
- an embedded RISC-V emulator
- another execution engine

This is a future capability, not a prerequisite for the initial host system.

---

## 33. Application Source Portability

Normal TabOS application source should ideally compile for:

```text
Tab5
macOS host
Linux host
```

without source changes.

For example:

```c
#include <tabos/tabos.h>

int main(int argc, char **argv) {
    tabos_printf("Hello, TabOS!\n");
    return 0;
}
```

The compiler and linkage differ by target.

The application-facing API should not.

This is a central SDK design requirement.

---

## 34. Host-Only Development Features

The host environment may provide development conveniences, but they should not silently become portable TabOS behavior.

Examples might include:

- verbose debug logging
- framebuffer capture
- synthetic event injection
- test control sockets
- inspector/debug windows
- filesystem fixture mounting

These should be clearly marked as host/testing facilities.

Applications should not accidentally depend on them unless intentionally building host-only tooling.

---

## 35. Debugging

Host builds should support normal native debugging.

On macOS/Linux this should allow tools such as:

```text
lldb
gdb
```

depending on platform/toolchain.

This is expected to be the easiest way to debug much of the portable TabOS implementation.

Target debugging will require a separate ESP32-P4/JTAG/debugger strategy.

Where possible, difficult logic should first be reproduced under the host build.

---

## 36. Repository Layout

A likely layout supporting this strategy is:

```text
tabos/
├── kernel/
├── fs/
├── graphics/
├── input/
├── audio/
├── net/
├── loader/
│
├── platform/
│   ├── esp32p4/
│   └── host/
│       ├── sdl/
│       └── posix/
│
├── sdk/
├── apps/
│
├── tests/
│   ├── unit/
│   ├── component/
│   ├── integration/
│   ├── fixtures/
│   └── graphics/
│
└── tools/
```

Exact names are not frozen.

The important separation is:

```text
portable code
platform code
tests
applications
```

---

## 37. Example Dependency Graph

Preferred:

```text
shell
  ↓
console API
  ↓
terminal subsystem
  ↓
graphics/input
  ↓
platform interfaces
  ↓
+-----------------+----------------+
| ESP32-P4        | SDL3 host      |
+-----------------+----------------+
```

Incorrect:

```text
shell
  ↓
SDL3
```

Incorrect:

```text
shell
  ↓
ESP-IDF
```

Incorrect:

```text
graphics
  ↓
#ifdef __APPLE__
```

Platform differences should remain at the bottom of the dependency graph.

---

## 38. What Host Builds Are Not

The host build is not intended to be:

- a pixel-perfect emulator of every Tab5 peripheral
- a simulation of ESP32-P4 electrical behavior
- an ESP-IDF replacement
- proof that code will work on hardware
- a separate desktop port with different semantics

It is a native implementation of the **platform boundary** used to run the real shared TabOS code.

---

## 39. What Must Still Be Tested on Hardware

Even when a subsystem appears correct on macOS/Linux, hardware validation remains necessary for areas affected by:

- memory placement
- cache coherency
- DMA
- interrupts
- SMP timing
- FreeRTOS scheduling
- hardware clocks
- peripheral protocols
- PSRAM performance
- display bandwidth
- C6 communication
- flash behavior
- SD behavior
- physical input
- actual audio output

The host build should reduce hardware iteration, not eliminate hardware validation.

---

## 40. Initial Implementation Sequence

A practical implementation sequence is:

### Step 1

Create a minimal shared TabOS core that builds on macOS and Linux.

### Step 2

Create an SDL3 host application that:

- opens a 1280×720 logical display
- initializes TabOS
- feeds SDL input into the TabOS input system
- presents the TabOS framebuffer

### Step 3

Add shared:

- console
- shell
- filesystem abstraction
- timers
- event dispatch

### Step 4

Build the same shared core for ESP32-P4.

### Step 5

Implement Tab5 platform adapters for:

- display
- keyboard
- touch
- filesystem/storage

### Step 6

Ensure a simple program such as:

```text
hello
```

behaves similarly on:

```text
macOS
Linux
Tab5
```

### Step 7

Add automated tests and CI around the shared behavior.

---

## 41. First Cross-Platform Milestone

A useful first milestone is:

```text
$ hello
Hello from TabOS
```

working on all three targets.

For macOS:

```text
native host executable
SDL3 window
host filesystem
```

For Linux:

```text
native host executable
SDL3 window
host filesystem
```

For Tab5:

```text
ESP32-P4 firmware
real display
real keyboard
microSD/internal filesystem
```

The shell, command dispatch, application-visible API, and observable behavior should be substantially shared.

---

## 42. Codex Rules

When implementing TabOS, Codex should preserve the following testing and portability rules:

1. Assume `tab5`, `macos`, and `linux` are supported project targets.
2. Keep normal TabOS code buildable for host systems whenever feasible.
3. Use SDL3 for macOS/Linux display, input, and audio integration.
4. Do not include SDL3 headers in portable TabOS subsystems.
5. Do not include ESP-IDF or FreeRTOS headers in portable TabOS subsystems.
6. Put platform-specific implementations behind explicit interfaces.
7. Prefer shared host code for macOS and Linux.
8. Do not scatter `#ifdef __APPLE__`, `#ifdef __linux__`, or ESP target checks throughout shared code.
9. Add unit tests for pure logic.
10. Add component tests for subsystem behavior.
11. Make integration tests runnable on host without physical hardware.
12. Support headless test execution where possible.
13. Treat the Tab5 cross-build as an important portability test.
14. Do not assume host success proves hardware correctness.
15. Keep application source portable across Tab5/macOS/Linux where the TabOS API permits.
16. Prefer deterministic synthetic input and controlled test filesystems.
17. Use sanitizers on host builds where practical.
18. Keep host-only development conveniences outside the public TabOS API.
19. Test SDL3 behavior at the adapter boundary rather than coupling SDL3 to higher-level tests.
20. Update this document when target or testing policy changes.

---

## 43. Testing North Star

### Symbol-boundary checks

Architecture tests should enforce decided naming direction during migration:

- public SDK declarations use `tabos_*`
- public SDK headers do not expose internal layer/subsystem symbols
- new internal declarations do not use generic `tab_*`
- internal-only declarations do not claim `tabos_*` without explicit ABI decision
- shared ESP code, chip-specific code, and Tab5 board code use matching ownership prefix

Migration checks may allow explicit legacy inventory temporarily, but allowlist must only
shrink. Mechanical renames must preserve behavior and pass all target builds/tests.

The desired development model is:

```text
Most code:
    write once
    compile natively
    test quickly
    run on Tab5

Hardware-specific code:
    isolate
    test on Tab5
```

A developer working on the shell, filesystem, graphics model, UI, utilities, or application APIs should normally be able to work on a Mac or Linux machine without having the physical Tab5 attached.

The real Tab5 remains the final source of truth for hardware behavior.

The purpose of the macOS/Linux SDL3 builds is to make the shared TabOS implementation fast to develop, easy to debug, and continuously testable without compromising the architecture of the actual device.
