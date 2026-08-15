# TabOS Codex Context

> Status: project context distilled from TabOS planning discussions through 2026-08-10.
> Purpose: give Codex a stable architectural baseline. Treat items marked **Decision** as the current direction, **Proposed** as a likely design that still needs validation, and **Open** as unresolved.

## 1. Project Goal

TabOS is a Zeal OS-inspired operating environment for the **M5Stack Tab5 with the Tab5 Keyboard**.

The goal is not to reproduce Zeal OS literally. The useful model to carry forward is:

- keyboard-first personal computer
- filesystem- and CLI-oriented
- small, understandable native application API
- applications that feel like programs for a coherent computer platform rather than ESP-IDF firmware
- optional graphical environment built on the same public OS services
- native compiled RISC-V applications

The system should hide most ESP32/ESP-IDF-specific machinery from normal TabOS applications.

## 2. Hardware Baseline

### Decision / current hardware assumptions

Target device:

- M5Stack Tab5
- Tab5 Keyboard
- ESP32-P4 application processor
- dual-core 32-bit RISC-V CPU, up to approximately 400 MHz
- approximately 16 MB flash
- approximately 32 MB PSRAM
- 1280x720 MIPI-DSI touchscreen
- microSD storage
- USB host/OTG
- audio support
- camera support
- RTC and onboard sensors
- ESP32-C6 companion device providing Wi-Fi 6 functionality
- Tab5 Keyboard controller based on STM32, connected over I2C with an interrupt line
- keyboard firmware exposes Normal, HID, and Character modes

### Decision: support all known Tab5 display revisions

Tab5 hardware currently exists with three display-controller combinations:

- ILI9881C display with GT911 touch controller
- ST7123 display with ST712x touch firmware version 3
- ST7121 display with ST712x touch firmware version 1

The ESP-IDF platform backend must detect these at runtime so one firmware image works across revisions. Detection first probes GT911; ST712x devices are distinguished by reading touch firmware register `0x0000`. Do not rely on `m5stack_tab5_noglib` v1.2.0~1 board-version detection alone: it treats every ST712x touch address as ST7123 and therefore misidentifies ST7121 hardware.

Current implementation uses the Tab5 BSP for ILI9881C and ST7123, and the official `espressif/esp_lcd_st7121` component for ST7121. ILI9881C uses the BSP 1000 Mbps DSI rate; ST7121/ST7123 use the M5Stack reference rate of 965 Mbps. Shared TabOS graphics render at 1280x720 RGB565 and the hardware backend rotates counter-clockwise into the panel's native 720x1280 scanout orientation.

The detected display name must remain available through the platform API and reported over serial at info level in both debug and release builds. This information will later feed an on-screen boot driver/hardware list.

## 3. Operating-System Architecture

### Decision: FreeRTOS/ESP-IDF is the hardware/runtime foundation

TabOS should initially be implemented **on top of ESP-IDF and FreeRTOS**, rather than replacing FreeRTOS with a from-scratch kernel.

FreeRTOS provides the difficult low-level facilities that TabOS immediately needs:

- task scheduling
- SMP/multicore execution
- synchronization
- timers
- interrupt integration
- memory/runtime support
- mature ESP32-P4 hardware integration through ESP-IDF

TabOS is therefore primarily the **OS personality, API, process/program environment, shell, filesystem model, graphics/input stack, and SDK** layered over that foundation.

This is intentionally different from writing a bare-metal kernel.

### Decision: ESP-IDF is an implementation detail

Normal TabOS applications should not directly depend on arbitrary ESP-IDF APIs.

The intended layering is approximately:

```text
TabOS applications
        |
TabOS public API / ABI
        |
TabOS kernel + system services
        |
FreeRTOS / ESP-IDF
        |
ESP32-P4 + peripherals
```

Internal system components may use ESP-IDF directly where appropriate.

### Decision: shell-first, GUI optional

The shell/CLI is a primary interface, not a debugging afterthought.

A GUI may be added, but it should be a **client of normal TabOS services**, not a privileged architecture that defines the OS.

This keeps the system usable as a keyboard-driven computer and helps preserve the Zeal OS inspiration.

The shell is the first planned official TabOS application. It must not be compiled into or architected as part of the kernel. Kernel and system layers provide reusable terminal, input, process, and filesystem services; the shell consumes the public application-facing forms of those services.

### Current implementation: shared boot console

The kernel currently constructs one structured boot report and sends it to both the platform serial/log sink and a portable framebuffer terminal. Portable graphics embeds raw fixed-width bitmap font assets through assembler `.incbin` and provides a scaled RGB565 terminal renderer with clearing, wrapping, colored text, and scrolling. `config/Font.cmake` defines asset path, glyph width/height, glyph count from 1 through 256, and cell width/height. Packed rows support widths above 8; missing high glyphs fall back to glyph zero. Default is the 256-glyph 8x12 CP437 `graphics/blueterm.f12`. All targets use the same compiled default terminal scale from `TABOS_TERMINAL_SCALE` in `config/Display.cmake`; its default is 2. At this scale, 16x24 glyphs occupy 16x30 terminal cells, yielding exactly 80x24 on 1280x720. Glyphs have no added horizontal spacing. Host rendering must match Tab5 size and wrapping. The public `<tabos/terminal.h>` API can select scale 1 through 8 before or after runtime startup. The runtime retains the active scale and boot report, allowing immediate reflow/redraw; persistence across device restarts is deferred until filesystem/configuration support. Current report entries cover system version, target, detected display, framebuffer, processor, memory, storage state, and kernel runtime state. Tab5 supplies internal heap, PSRAM, physical flash capacity, and mounted microSD filesystem capacity/free space. Host reports its controlled root filesystem capacity/free space. Missing Tab5 microSD is nonfatal and reported as not mounted. The static boot frame is presented once; LCD hardware performs scanout refresh while the platform loop sleeps. This boot console is infrastructure, not the shell application.

The public `<tabos/console.h>` API now supplies one cooperative foreground console session. Only current session can write, clear, inspect cursor, navigate scrollback, or read normalized input; rejected reads do not consume events. Terminal controls support newline, carriage return, four-column tabs, destructive backspace, wrapping, clearing, and scrolling. Terminal state is a colored cell ring retaining visible rows plus `TABOS_TERMINAL_SCROLLBACK_LINES` (default 256) history rows. Viewport is separate from live cursor; Page Up/Down/Home/End navigate, cursor hides above live output, and new output returns to end. Tab5 Keyboard lacks these four physical HID keys, so diagnostic mapping uses Ctrl+Up/Down for Page Up/Down and Ctrl+Left/Right for Home/End; plain arrows remain unclaimed. Runtime scale changes reflow retained hard/soft lines and redraw cells. Cursor blinks at configurable half-period through reusable polling timer service; input/output restores visible phase. Terminal dirty-cell rendering avoids full glyph redraw for ordinary writes and blink changes. Writes still present framebuffer immediately on every target. Optional `TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP` builds a target-neutral echo/test application under `apps/`; it defaults off and is explicitly not shell.

Portable application foundation now defines public descriptor and cooperative lifecycle API in `<tabos/application.h>`. Descriptor carries ABI version, name, version, capabilities, entry, update, and cleanup callbacks while remaining independent of future ELF container. Fixed-capacity built-in registry currently holds static descriptors. Runtime launches at most one foreground application, acquires requested console capability on its behalf, dispatches updates, honors requested exit status, calls cleanup, releases console, then remains idle for future launcher. Optional diagnostic is registered as `console-test`; Ctrl+Q exercises clean exit. This is initial host-native/built-in application mode, not final process/task or loader ABI.

### Decision: persistent nested foreground processes

TabOS will initially support multiple loaded processes but only one focused application
at a time. Shell is persistent root process (initially process 0). When foreground
process executes another program, parent remains loaded with all execution and resource
state retained, blocks in background, and child becomes foreground. Child may launch
another child using same rule. When child exits or faults, kernel unloads child, returns
status to parent, restores parent's focus and console/input ownership, and resumes parent
without restarting it.

This is Zeal-style nested execution, not POSIX `exec()`, which replaces caller image.
Conceptually TabOS operation is synchronous spawn-and-wait exposed as `exec`: push child
onto foreground process stack, block parent, then pop child and wake parent. Initial
model has no background jobs or concurrently runnable user programs. Kernel, input,
display, timer, filesystem, network, and other system/service tasks must remain runnable
while foreground application runs. On Tab5, native application code must therefore run
in managed FreeRTOS application task rather than synchronously on runtime/service task.
Host RV32 interpreter retains guest state across runtime-update slices.

Process 0 is required system root and must never exit. If it returns, requests exit,
faults, or is otherwise terminated, kernel must enter panic state rather than unload,
restart, or normally unwind process 0. Panic must report process-0 failure and available
cause/exit status to both serial/log sink and framebuffer console/terminal.

Experimental loader now accepts bounded little-endian RV32 `ET_EXEC` ELF with loadable segments, executable entry, and no dynamic segment or relocations. Checked-in hello fixture is independently compiled by GCC for RV32IMA/`ilp32`, stripped to 348 bytes, loads 125 bytes, and calls versioned API table containing console write and exit request. Host executes same artifact through RV32 interpretation and reserved API call gates; guest CPU and memory state persist across bounded runtime-update instruction slices, so no application-lifetime instruction ceiling exists. Tab5 executes it natively. `TABOS_ENABLE_ELF_LOADER_EXPERIMENT=ON` selects `elf-hello` startup application on either target; it is mutually exclusive with console diagnostic. Hardware validation proved execution from PSRAM by loading through a writable data mapping, synchronizing cache, and creating a read/execute MMU alias for the same physical pages. API string pointers are translated back to the readable data alias. This is a proven experiment, not yet the final loader/process ABI.

## 4. Multitasking and CPU Cores

### Decision: do not permanently dedicate one P4 core to graphics

The current direction is to let FreeRTOS schedule kernel/system work and applications across the ESP32-P4 cores rather than treating one core as a fixed "GPU core."

Graphics will require asynchronous system work such as:

- display refresh / buffer submission
- composition
- DMA/display-driver coordination
- possibly GUI/compositor tasks

Those can run as FreeRTOS tasks, potentially with affinity or priority tuning when measurements justify it.

Applications and OS services should remain able to multitask.

### Proposed

Start without aggressive core pinning. Introduce affinity only for measured latency, driver, or cache/contention reasons.

A likely pattern is:

- application tasks scheduled normally
- high-priority input/display service tasks where needed
- hardware DMA/display peripherals doing as much bulk transfer work as possible
- compositor/render task awakened by damage/frame work rather than busy-spinning

## 5. Native TabOS Programs

### Decision: native RISC-V applications are a core goal

TabOS programs should be compilable with a normal C/C++-capable RISC-V toolchain, most likely the GCC toolchain already used by ESP-IDF or a compatible standalone configuration.

Applications should compile to **native ESP32-P4 RISC-V machine code**.

The long-term developer experience should resemble:

```text
source -> TabOS SDK/toolchain -> TabOS executable -> copy/install -> run
```

rather than:

```text
source -> rebuild entire ESP-IDF firmware image -> flash device
```

### Decision: applications should target TabOS, not ESP-IDF

A TabOS SDK should provide headers, libraries, linker configuration, executable tooling, and public APIs.

Application source should normally include TabOS APIs rather than ESP-IDF/FreeRTOS APIs.

### Proposed: executable loader

TabOS needs a loader capable of loading independently built application binaries from storage.

The exact executable format is not settled.

Likely requirements include:

- architecture/version marker
- entry point
- loadable code/data
- BSS description
- relocation support if applications are not linked to fixed addresses
- imported TabOS symbols or a stable syscall/API mechanism
- optional metadata such as name/version/icon/capabilities

ELF is attractive during development because GCC/binutils already support it. A smaller TabOS-specific executable format could be introduced later if ELF parsing/storage overhead proves undesirable.

### Open: application isolation

It has not yet been established whether independently loaded programs will have meaningful hardware memory protection.

Do not assume Unix-style process isolation.

The ESP32-P4/ESP-IDF memory architecture, executable-from-PSRAM capabilities, MMU/cache behavior, and available protection mechanisms need investigation before locking isolation and fault-containment guarantees. Initial nested foreground process/task model does not imply Unix-style memory protection.

## 6. Public API / ABI

### Decision: establish a stable TabOS-facing boundary

Applications need a controlled API boundary so TabOS internals can evolve without requiring applications to use ESP-IDF internals.

Likely API families:

- console / terminal I/O
- filesystem
- directories and file metadata
- keyboard/input/events
- time and timers
- task/process lifecycle
- memory allocation
- graphics/display surfaces
- audio
- networking
- inter-task/inter-process communication
- device/system information

### Open: syscall mechanism

The concrete ABI has not been selected.

Candidates to investigate include:

- direct calls through a stable system API table
- linked stubs that dispatch through an API table
- a trap/syscall mechanism if the hardware/runtime makes this useful
- dynamic symbol resolution from loaded ELF applications

The design should optimize for:

1. simplicity
2. ABI stability
3. native performance
4. host emulation/simulation
5. ability to evolve TabOS internals independently of applications

Do not prematurely imitate POSIX if a smaller TabOS-native API is clearer.

## 7. Filesystem and Storage

### Decision

A filesystem and command-line environment are fundamental TabOS services.

microSD is expected to be an important user/program storage device.

### Proposed

Expose a conventional hierarchical filesystem API to applications even if the implementation uses ESP-IDF filesystem components underneath.

Potential logical areas include:

```text
/bin
/apps
/home
/etc
/tmp
/dev
```

These names are illustrative, not yet frozen.

### Open

Still to decide:

- primary on-disk filesystem
- internal flash vs microSD responsibilities
- mount model
- current-working-directory semantics
- device namespace
- removable-media behavior
- package/application installation layout
- permissions/security model

## 8. Shell and Terminal

### Decision

TabOS should boot into or make readily available a keyboard-oriented shell.

The shell is part of the intended user experience and should exercise the same filesystem/program APIs available to other software.

### Proposed initial capabilities

- execute native TabOS programs
- arguments
- current directory
- environment or equivalent configuration mechanism
- filesystem commands
- redirection/pipes only when the underlying process/I/O model is ready

Do not make full Unix shell compatibility an initial requirement.

## 9. Graphics

### Decision

Graphics are an OS service rather than something each application should configure through ESP-IDF.

The 1280x720 touchscreen makes graphics important, but TabOS should remain usable without requiring a desktop GUI.

### Proposed layering

```text
application / GUI program
        |
TabOS graphics API
        |
surface/framebuffer/compositor service
        |
ESP-IDF display + DMA drivers
        |
MIPI-DSI display
```

A future window system should use the public graphics/input services rather than bypassing them.

### Open

Still to determine:

- framebuffer pixel format
- single vs double/triple buffering
- where framebuffers live
- direct framebuffer access vs surfaces
- compositor architecture
- damage tracking
- application graphics API level
- 2D acceleration opportunities
- text/font system
- whether fullscreen applications can bypass composition safely
- achievable frame rates and PSRAM/display bandwidth

## 10. Keyboard, Touch, and Input

### Hardware assumption

The Tab5 Keyboard is connected through an STM32 controller over I2C with an interrupt mechanism and supports multiple keyboard modes.

### Decision

TabOS should normalize hardware-specific input into an OS-level event/input API.

Applications should not need to speak directly to the keyboard controller.

### Proposed

Input events should eventually cover:

- key press/release
- modifiers
- character/text input
- touch
- pointer-like coordinates where useful
- system shortcuts

The keyboard-first philosophy means reliable low-latency keyboard behavior has priority even though the Tab5 is also a touchscreen device.

### Decision: touch input is deferred

Touch is not part of the initial terminal interface. Do not prioritize a general touch driver, touch kernel events, host pointer emulation, or touch-facing application API while building the boot console, terminal, keyboard path, and shell. Touch support belongs later with the GUI/windowing system and application input API.

The current ST712x touch-controller access exists only to identify the attached display revision. Keep that narrow probe separate from future kernel touch/input support.

### Current implementation: keyboard input

The public `<tabos/input.h>` API exposes physical key-down/key-up events, modifiers, repeat state, UTF-8 text events, and blocking/nonblocking reads through a thread-safe 64-event queue. SDL3 supplies host physical/text events. Tab5 uses ExtPort1 I2C controller 0 on GPIO0/GPIO1, probes address `0x6D`, reports firmware register `0xFE`, and reads HID-mode reports. It polls at 10 ms; GPIO50 interrupt support is a later optimization that must not change public semantics. Tab5 text translation is currently US ANSI. Missing keyboard hardware is a boot warning, not a fatal initialization error. Optional CMake flag `TABOS_ENABLE_KEYBOARD_DIAGNOSTICS` logs normalized events without consuming them and defaults off. USB HID keyboards on Tab5 are a future backend; they should coexist with the I2C keyboard through the same queue. Touch remains excluded.

## 11. Networking

### Hardware assumption

Networking is mediated through the ESP32-C6 companion hardware rather than being a simple native ESP32-P4 Wi-Fi peripheral.

### Decision

Applications should consume TabOS networking APIs rather than depending directly on the C6 transport or ESP-IDF implementation.

### Open

Determine:

- how ESP-IDF exposes P4<->C6 networking on the selected SDK
- socket API compatibility
- whether TabOS exposes BSD-like sockets or a smaller abstraction
- Wi-Fi configuration/user experience
- background network service ownership

## 12. Host Development and Emulation

### Decision: maximize development away from physical hardware

A major project goal is to make most TabOS development possible locally on macOS for faster iteration.

The system architecture should deliberately support this.

### Proposed: native host build/simulator

A large portion of TabOS can be designed so the same higher-level code builds as a **native macOS process**, with a host abstraction replacing ESP-IDF-specific hardware services.

This is preferable for rapid work on:

- shell
- filesystem semantics
- executable metadata/tooling
- graphics compositor logic
- UI
- application APIs
- input/event routing
- libraries
- utilities
- applications

Conceptually:

```text
                    +----------------------+
TabOS core/API ---->| ESP32-P4 backend     |--> hardware
        |
        +---------->| macOS host backend   |--> host window/files/input
                    +----------------------+
```

Keep platform-dependent code behind narrow interfaces.

### Proposed: QEMU is secondary, not the main workflow

QEMU may be useful where ESP32-P4 support is sufficient, but it should not be assumed to emulate the complete Tab5 board, MIPI display, keyboard controller, C6 networking arrangement, and other peripherals accurately.

A purpose-built native host backend is likely to provide much faster and more productive iteration for most OS/application behavior.

QEMU or another CPU-level emulator can later be useful for validating actual RISC-V binaries and lower-level behavior.

### Important constraint

Do not let the host simulator become a separate implementation of TabOS. Shared OS logic should be maximized; only hardware/platform adapters should differ.

## 13. Suggested Source Architecture

This is a proposed organizational direction, not a frozen repository layout:

```text
tabos/
  kernel/
    scheduler-facing glue
    program lifecycle
    system services
    api dispatch
  platform/
    esp32p4/
    host/
  drivers/
    display
    keyboard
    touch
    storage
    audio
    network
  fs/
  graphics/
  input/
  net/
  libc/
  sdk/
    include/
    lib/
    linker/
    tools/
  loader/
  apps/
    shell/
  host/
  tests/
```

The key architectural rule is more important than the exact directories:

**portable TabOS logic must not casually acquire ESP-IDF dependencies.**

## 14. Constraints and Design Principles

### Established

- Target the M5Stack Tab5 + Tab5 Keyboard first.
- Use ESP-IDF/FreeRTOS as the initial low-level platform.
- Do not start by writing a replacement scheduler/kernel from bare metal.
- Keep ESP-IDF APIs out of normal application code.
- Support native compiled RISC-V applications.
- Make programs independently buildable/loadable rather than requiring firmware reflashes.
- Preserve a keyboard-first CLI/filesystem-oriented computer model.
- Treat GUI as an optional client of OS APIs.
- Support multitasking; do not statically reserve an entire CPU core for graphics without evidence.
- Architect for a high-fidelity native host development environment on macOS.
- Keep platform-specific hardware code isolated so host and device builds share most logic.

## 15. Rejected / Deferred Approaches

### Rejected for the initial architecture: bare-metal replacement kernel

Replacing FreeRTOS immediately would consume substantial effort recreating scheduling, synchronization, interrupt/runtime integration, and ESP32-P4 hardware support without directly advancing the distinctive TabOS environment.

This can be revisited only if FreeRTOS imposes a demonstrated architectural limitation.

### Rejected as the application model: ESP-IDF applications

TabOS applications should not simply be ESP-IDF components linked into the firmware. That would undermine independent application distribution and the stable TabOS API/ABI.

Built-in system components may still be compiled into firmware where appropriate.

### Rejected as a default design: dedicated graphics CPU core

The P4's second core should not automatically be sacrificed to a permanent graphics loop. Use normal scheduling, peripherals/DMA, priorities, and measured affinity decisions.

### Deferred: full Unix/POSIX clone

POSIX concepts may be borrowed when useful, but compatibility is not the primary goal. TabOS should remain small and coherent rather than accumulating Unix behavior by default.

### Deferred: QEMU as complete Tab5 emulation

Do not block development on complete board emulation. Native host simulation should cover the majority of portable OS behavior.

## 16. Major Unresolved Questions

These should be treated as active design work rather than silently assumed by Codex.

1. **Executable format:** ELF directly, converted ELF, or a compact TabOS format?
2. **Loader/relocations:** how are independently linked applications placed and relocated?
3. **ABI:** API table, dynamic symbols, trap/syscall ABI, or another mechanism?
4. **Memory execution:** what memory regions can safely and efficiently hold loaded executable code on ESP32-P4?
5. **Isolation:** what practical memory/process protection can the P4 provide?
6. **Future threading:** when, if ever, may one process own worker tasks beyond its decided initial single managed application task?
7. **Failure containment:** what happens when an application crashes?
8. **libc:** use/newlib wrapper, custom small libc, or hybrid?
9. **filesystem:** which filesystems and mount conventions should be standard?
10. **stdio/handles:** file descriptors, object handles, streams, or a TabOS-specific model?
11. **IPC:** queues/messages/pipes/shared memory and how much FreeRTOS is exposed internally?
12. **graphics API:** framebuffer, retained surfaces, immediate 2D API, or layers of these?
13. **compositor:** whether/when TabOS gets windows and composition.
14. **display buffering:** exact formats, memory placement, bandwidth, and buffering strategy.
15. **network API:** BSD sockets vs a smaller TabOS API.
16. **C6 integration:** exact P4/C6 networking architecture and SDK dependencies.
17. **audio API:** mixer ownership, streaming model, formats, latency.
18. **device API:** whether `/dev`-style objects are useful or unnecessarily Unix-like.
19. **application packaging:** executable plus resources/metadata format and install locations.
20. **security model:** application capabilities, permissions, signing, or no isolation initially.
21. **host runner:** how host-built applications and actual RISC-V TabOS binaries relate.
22. **binary emulation:** whether a RISC-V emulator should be embedded in the host environment for running target binaries.
23. **debugging:** source-level debugging story for native TabOS applications.
24. **SDK build system:** CMake, Meson, Make, or a thin TabOS command wrapping the underlying toolchain.
25. **C++ support:** whether it is first-class initially or follows the C ABI later.
26. **boot UX:** shell directly, launcher, desktop, or configurable startup program.

## 17. Near-Term Architectural Validation

Before building large subsystems, validate these assumptions with small spikes:

1. Build and boot a minimal TabOS firmware under ESP-IDF.
2. Bring up Tab5 display, keyboard, touch, microSD, and basic C6 networking independently.
3. Measure PSRAM/display bandwidth and realistic framebuffer strategies.
4. Prove that code can be loaded from storage into an executable memory arrangement suitable for independent native applications.
5. Compile a tiny external C program with the proposed TabOS SDK and launch it without rebuilding TabOS.
6. Prototype the stable API/ABI with a few calls such as console write, file open/read, input poll/wait, and exit.
7. Build the same portable shell/API code as a native macOS executable using host platform adapters.
8. Decide the executable/ABI model only after the loader experiment establishes what the ESP32-P4 memory architecture permits.

## 18. Guidance for Codex

When modifying TabOS:

- Preserve the decisions above unless the task explicitly changes them.
- Clearly distinguish portable TabOS code from ESP-IDF platform code.
- Do not expose FreeRTOS or ESP-IDF types in public application headers unless explicitly approved.
- Prefer small, testable interfaces.
- Keep host implementations behaviorally aligned with device implementations.
- Avoid designing around hypothetical POSIX compatibility.
- Do not assume hardware isolation or executable-memory behavior without verifying it.
- Do not pin work to a CPU core without a measured or driver-imposed reason.
- For unresolved architecture, document the tradeoff and implement the smallest experiment needed to resolve it.
- Update this document when a previously open question becomes a decision.

## 19. Current Architectural Summary

The intended system is a **small native-computing environment layered on ESP-IDF/FreeRTOS**. FreeRTOS handles low-level scheduling and hardware-runtime concerns; TabOS supplies the user-visible OS abstraction. Applications are independently compiled native RISC-V programs targeting a stable TabOS API. The shell and filesystem are first-class. Graphics, input, networking, and other hardware are mediated by TabOS services. The GUI is optional and non-privileged. Most higher-level OS code should also run in a native macOS host environment, while actual hardware-specific behavior remains in the ESP32-P4 backend.
