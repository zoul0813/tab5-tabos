# TabOS Architecture

> Status: agreed project architecture as of 2026-08-10.
>
> This document defines the intended structure of TabOS and the architectural boundaries that implementation work should preserve. It is written primarily for Codex and contributors working in the repository.

## 1. Overview

TabOS is a small native-computing environment for the **M5Stack Tab5 with the Tab5 Keyboard**.

It is inspired by the simplicity and coherence of Zeal OS, but it is not intended to reproduce Zeal OS or mimic a Unix system.

The core architectural idea is:

```text
+--------------------------------------------------+
|                  TabOS Applications              |
+--------------------------------------------------+
|                  TabOS Public API                |
+--------------------------------------------------+
| Shell | FS | Loader | Graphics | Input | Net ... |
+--------------------------------------------------+
|               TabOS System Services              |
+--------------------------------------------------+
|            Platform Abstraction Layer            |
+-------------------------+------------------------+
| ESP32-P4 / ESP-IDF      | Native Host Backend    |
| FreeRTOS                | macOS development      |
+-------------------------+------------------------+
| Hardware                | Host OS / Windowing    |
+-------------------------+------------------------+
```

TabOS owns the user-facing operating-system model.

ESP-IDF and FreeRTOS are implementation foundations, not the application programming model.

---

## 2. Architectural Goals

TabOS should behave like a coherent small computer platform rather than a collection of ESP-IDF firmware components.

The architecture should support:

- independently compiled native applications
- a stable TabOS-facing API
- filesystem-based program and data storage
- a first-class keyboard-oriented shell
- multitasking
- graphical applications
- optional desktop/windowing functionality
- a high-fidelity native development environment on macOS
- clear separation between portable OS logic and hardware-specific implementation
- incremental evolution without exposing ESP-IDF internals to applications

The architecture should remain understandable enough that the OS can be explored, modified, and extended without requiring developers to understand the entire ESP-IDF stack.

---

## 3. Platform Foundation

### 3.1 ESP-IDF and FreeRTOS

The hardware build of TabOS is based on:

```text
TabOS
  ↓
ESP-IDF
  ↓
FreeRTOS
  ↓
ESP32-P4 hardware
```

FreeRTOS is responsible for low-level facilities including:

- scheduling
- synchronization
- timers
- interrupt integration
- SMP execution
- low-level runtime support

TabOS should not initially replace FreeRTOS with a custom scheduler or bare-metal kernel.

### 3.2 Architectural Boundary

Portable TabOS code must not depend directly on ESP-IDF or FreeRTOS unless the dependency is intentionally part of the platform layer.

In particular, public application headers must not casually expose:

- `TaskHandle_t`
- FreeRTOS queues/semaphores
- ESP-IDF driver structures
- ESP-IDF networking structures
- ESP-IDF filesystem implementation types

Application code targets **TabOS**, not ESP-IDF.

---

## 4. Runtime Model

TabOS is a multitasking system layered on the SMP scheduler supplied by FreeRTOS.

A conceptual runtime looks like:

```text
Core 0                         Core 1
------                         ------

TabOS task                     Application task
Application task               Graphics service
Filesystem work                Input service
Network service                Application task
...
```

The exact scheduling of tasks is dynamic.

### 4.1 No Dedicated Graphics Core

One ESP32-P4 CPU core is **not** permanently reserved for graphics.

Display work should instead use:

- normal FreeRTOS scheduling
- task priorities
- hardware DMA
- asynchronous display drivers
- selective task affinity only when measurements justify it

A compositor or display task may become high priority, but it remains part of the multitasking system.

### 4.2 Application Execution

The intended application model is:

```text
Storage
   ↓
Executable loader
   ↓
Native RISC-V program
   ↓
TabOS API
```

Programs should not require the complete TabOS firmware to be rebuilt and reflashed.

The exact mapping between a TabOS process and FreeRTOS tasks remains an implementation detail.

Possible models include:

```text
1 process = 1 task
```

or:

```text
1 process
   ├── main task
   ├── worker task
   └── optional additional tasks
```

The application API should not depend on which model is chosen.

---

## 5. Application Architecture

### 5.1 Native Programs

TabOS applications are intended to be native **ESP32-P4 RISC-V binaries**.

The expected developer flow is:

```text
C / C++ source
      ↓
TabOS SDK
      ↓
RISC-V GCC / compatible compiler
      ↓
TabOS executable
      ↓
copy/install to TabOS filesystem
      ↓
execute
```

Applications should normally include headers such as:

```c
#include <tabos/io.h>
#include <tabos/fs.h>
#include <tabos/input.h>
#include <tabos/graphics.h>
```

rather than ESP-IDF headers.

### 5.2 Public ABI

Applications communicate with the OS through a stable TabOS ABI.

The concrete mechanism is not yet frozen.

Possible implementations include:

```text
Application
    ↓
linked API stub
    ↓
TabOS API table
```

or:

```text
Application
    ↓
syscall/trap
    ↓
kernel dispatcher
```

or:

```text
Application ELF imports
    ↓
TabOS dynamic symbol resolver
```

Regardless of implementation, the public ABI should remain independent of internal TabOS subsystem organization.

### 5.3 Executable Format

ELF is the preferred starting point for experimentation because the RISC-V GCC/binutils toolchain already understands it.

The architecture should allow the eventual executable format to be either:

```text
ELF
```

or:

```text
ELF → TabOS executable conversion
```

A custom format should only be introduced when it provides a concrete advantage.

---

## 6. TabOS System Services

TabOS functionality should be divided into system services with narrow APIs.

Conceptually:

```text
                 +------------------+
                 |  Application API |
                 +---------+--------+
                           |
        +------------------+------------------+
        |                  |                  |
        v                  v                  v
   Filesystem          Graphics            Input
        |                  |                  |
        +------------------+------------------+
                           |
                       OS runtime
```

Primary subsystems include:

- program loader
- process/program lifecycle
- console and terminal I/O
- filesystem
- shell
- input
- graphics
- audio
- networking
- clocks and timers
- memory allocation
- IPC
- device/system information

These services should depend on platform interfaces rather than directly embedding board-specific code.

---

## 7. Platform Abstraction

TabOS should support at least two platform backends:

```text
platform/
    esp32p4/
    host/
```

### 7.1 ESP32-P4 Backend

The hardware backend is responsible for adapting TabOS services to:

- ESP-IDF
- FreeRTOS
- MIPI-DSI display
- touch controller
- Tab5 Keyboard controller
- microSD
- ESP32-C6 networking transport
- audio hardware
- RTC
- additional onboard devices

The display backend currently owns runtime detection for ILI9881C, ST7123, and ST7121 Tab5 variants. It exposes the detected controller name through the platform boundary so portable boot-report code can list initialized hardware without depending on ESP-IDF types.

### 7.2 Host Backend

The native host backend allows portable TabOS code to run directly on macOS.

It should provide equivalents for:

- host filesystem access
- keyboard events
- display/window output
- timers
- threads/tasks
- sockets
- audio where useful

The host backend is intended for rapid development and testing.

It must not become a separate OS implementation.

The desired model is:

```text
                   Shared TabOS code
                         |
            +------------+------------+
            |                         |
      ESP32-P4 backend            Host backend
            |                         |
        ESP-IDF                    macOS APIs
```

---

## 8. Filesystem Architecture

The filesystem is a core operating-system service.

Applications should interact with TabOS filesystem APIs rather than ESP-IDF filesystem APIs.

A likely logical namespace is:

```text
/
├── bin/
├── apps/
├── home/
├── etc/
├── tmp/
└── dev/
```

These names are provisional.

### Responsibilities

The filesystem layer should handle:

- pathname resolution
- current working directory
- mount points
- file access
- directories
- metadata
- removable media
- program loading paths

The implementation may delegate actual storage to FAT, LittleFS, or other ESP-IDF-compatible filesystems.

The public API should not depend on the filesystem used underneath.

---

## 9. Shell Architecture

The shell is a first-class TabOS application/system component.

More specifically, the shell is an application, not kernel code. It belongs under `apps/` and consumes kernel/system services through application-facing APIs. The kernel may provide a boot console and terminal service, but must not own shell parsing, commands, or policy.

It should use public or near-public TabOS APIs wherever practical.

Conceptually:

```text
Keyboard
   ↓
Input service
   ↓
Terminal
   ↓
Shell
   ↓
Program loader
   ↓
Application
```

The initial shell does not need full POSIX compatibility.

Expected capabilities include:

- executing programs
- arguments
- filesystem navigation
- environment/configuration variables
- built-in commands
- application exit status

Pipes, redirection, job control, and similar Unix features should be added only when the underlying process and I/O models support them cleanly.

---

## 10. Input Architecture

Hardware-specific input is converted into normalized TabOS events.

```text
Tab5 Keyboard ─┐
               │
Touchscreen ───┼──> platform input drivers
               │
Other input ───┘
                       ↓
                 TabOS input service
                       ↓
                  event distribution
                       ↓
                   applications
```

Applications should not access the STM32 keyboard controller directly.

A generic input event model should eventually support:

- key down/up
- modifier state
- text/character input
- touch down/up/move
- coordinates
- gestures where appropriate
- system shortcuts

Keyboard latency and reliability have priority because TabOS is intended to support a keyboard-first workflow.

Touch input is intentionally deferred until the future GUI/windowing and application-input work. Initial terminal and shell milestones require keyboard support, not touch. Current ST712x I2C access in the display backend is hardware-revision detection only and must not be mistaken for a general touch subsystem.

Keyboard input now uses a public platform-neutral event queue with key-down, key-up, modifier, repeat, and UTF-8 text semantics. SDL3 and the Tab5 I2C keyboard are backend producers. Future USB HID support on Tab5 must feed the same queue and may coexist with the built-in I2C keyboard; applications must not depend on input-device-specific protocols.

---

## 11. Graphics Architecture

Graphics are provided as a TabOS service.

Applications should not directly initialize or manage the MIPI-DSI hardware.

```text
Application
    ↓
TabOS graphics API
    ↓
surface / framebuffer layer
    ↓
optional compositor
    ↓
display service
    ↓
platform display driver
    ↓
MIPI-DSI hardware
```

### 11.1 Initial Graphics Model

The precise API is still unresolved, but the architecture should allow several levels:

```text
high-level drawing API
         ↓
surface API
         ↓
framebuffer/display service
```

Fullscreen applications may eventually receive optimized paths, but must not own the display hardware in a way that breaks the OS.

### 11.2 Future GUI

A window manager or desktop environment should be implemented as a client of normal TabOS graphics and input services.

It should not introduce an alternate privileged API.

Conceptually:

```text
Applications
     ↓
Window/GUI services
     ↓
Graphics + Input APIs
     ↓
TabOS
```

---

## 12. Networking Architecture

The ESP32-P4 does not directly provide the Tab5 Wi-Fi implementation; networking involves the ESP32-C6 companion device.

This complexity should remain hidden from applications.

```text
Application
    ↓
TabOS network API
    ↓
network service
    ↓
ESP32-P4 / ESP32-C6 transport
    ↓
Wi-Fi / network
```

The public API may eventually resemble BSD sockets, but this should be chosen based on usefulness rather than compatibility alone.

---

## 13. Audio Architecture

Audio should follow the same service boundary:

```text
Application
    ↓
TabOS audio API
    ↓
audio service / mixer
    ↓
platform driver
    ↓
hardware
```

Applications should not directly configure audio peripherals.

A central mixer is likely desirable so multiple applications and system sounds can coexist.

The exact streaming and mixer API remains unresolved.

---

## 14. Memory Architecture

Memory management must account for the ESP32-P4 memory hierarchy, internal RAM, external PSRAM, caches, and executable-memory restrictions.

Conceptually, TabOS will need to distinguish among:

```text
kernel/system memory
application code
application data
graphics buffers
filesystem/cache buffers
DMA-compatible memory
```

The loader must not assume arbitrary PSRAM is executable until this has been verified experimentally.

The exact executable-memory strategy is one of the highest-priority architecture questions.

---

## 15. IPC and Handles

Subsystem APIs should avoid leaking FreeRTOS primitives.

For example, applications should not receive a FreeRTOS queue handle.

Prefer TabOS-owned abstractions:

```c
tab_handle_t
tab_file_t
tab_process_t
tab_surface_t
tab_timer_t
```

or similar.

The specific handle model is not yet decided.

IPC may eventually include:

- message queues
- pipes
- events
- shared memory
- service calls

The smallest useful model should be implemented first.

---

## 16. Error Handling

Public APIs should use TabOS-defined error values.

Do not expose ESP-IDF `esp_err_t` directly through public application APIs.

For example:

```c
tab_result_t result;
```

or a small errno-like model.

The exact representation can be decided later, but errors crossing the public ABI must belong to TabOS.

---

## 17. Source Tree Boundaries

A likely repository structure is:

```text
tabos/
├── kernel/
│   ├── runtime/
│   ├── process/
│   └── api/
│
├── platform/
│   ├── esp32p4/
│   └── host/
│
├── drivers/
│   ├── display/
│   ├── keyboard/
│   ├── touch/
│   ├── storage/
│   ├── audio/
│   └── network/
│
├── fs/
├── loader/
├── input/
├── graphics/
├── audio/
├── net/
├── libc/
│
├── sdk/
│   ├── include/
│   ├── lib/
│   ├── linker/
│   └── tools/
│
├── apps/
│   └── shell/
├── tests/
└── host/
```

The exact directories may change.

The dependency direction is more important:

```text
apps
 ↓
public API
 ↓
TabOS subsystems
 ↓
platform interfaces
 ↓
platform implementation
```

Dependencies should not flow upward.

---

## 18. Dependency Rules

Codex should preserve the following rules.

### Allowed

```text
application
    → public TabOS API

filesystem
    → platform storage abstraction

graphics
    → platform display abstraction

host backend
    → macOS APIs

esp32p4 backend
    → ESP-IDF / FreeRTOS
```

### Avoid

```text
application
    → ESP-IDF

application
    → FreeRTOS

filesystem
    → Tab5-specific hardware

graphics API
    → MIPI driver structures

portable kernel code
    → macOS APIs

portable kernel code
    → ESP-IDF headers
```

Hardware dependencies belong at the edge of the architecture.

---

## 19. Host Development Architecture

Most high-level TabOS code should be testable without flashing the device.

The native host runner should support development of:

- shell
- utilities
- application APIs
- filesystem semantics
- graphics primitives
- compositor logic
- UI
- input routing
- executable metadata/tools
- libraries

Typical workflow:

```text
edit
 ↓
native compile
 ↓
run TabOS host environment
 ↓
test immediately
```

Hardware workflow remains:

```text
edit
 ↓
cross compile
 ↓
build ESP-IDF firmware / TabOS app
 ↓
run on Tab5
```

Both environments should share the same portable implementation wherever possible.

---

## 20. QEMU / CPU Emulation

QEMU is not the primary architectural dependency for TabOS development.

It may eventually be useful for:

- RISC-V CPU validation
- target binary execution
- loader testing
- low-level debugging

It should not be assumed to emulate the complete Tab5 hardware platform.

Native host simulation remains the preferred rapid-development path.

A future host environment may also embed or invoke a RISC-V emulator so actual target binaries can run inside the host environment.

That is not yet an established design.

---

## 21. Boot Architecture

A conceptual boot path is:

```text
ESP32-P4 reset
     ↓
ESP-IDF startup
     ↓
TabOS platform initialization
     ↓
core system services
     ↓
filesystem mounting
     ↓
input/display/network services
     ↓
startup program
     ↓
shell / launcher / desktop
```

The exact user-facing startup program is configurable and remains unresolved.

The shell should always remain available as a basic recovery and development environment.

---

## 22. System vs Application Components

Not every executable must necessarily be dynamically loaded.

Some critical components may initially be statically linked into the firmware.

For example:

```text
TabOS runtime
filesystem
loader
display service
input service
shell
```

Other programs should increasingly become independently loadable applications.

Architecture should avoid coupling a component to firmware simply because it was initially built that way.

---

## 23. Hardware Assumptions

The current target assumes:

- M5Stack Tab5
- Tab5 Keyboard
- ESP32-P4 application processor
- dual-core RISC-V
- approximately 32 MB PSRAM
- approximately 16 MB flash
- 1280×720 MIPI-DSI touchscreen
- microSD storage
- ESP32-C6 networking companion
- STM32 keyboard controller connected using I2C/interrupt signaling
- audio hardware
- RTC and onboard peripherals

Exact capabilities must be verified against the board revision and ESP-IDF version used in the project.

Hardware assumptions must not silently become public API contracts.

---

## 24. Architecture Decisions

The following are currently considered established:

1. TabOS targets the M5Stack Tab5 and Tab5 Keyboard first.
2. ESP-IDF is the hardware support framework.
3. FreeRTOS provides the underlying scheduler/runtime.
4. TabOS is not initially a bare-metal kernel.
5. Applications target a TabOS API rather than ESP-IDF.
6. Applications should be independently compiled native RISC-V binaries.
7. Applications should eventually load from filesystem storage without firmware rebuilds.
8. The shell and filesystem are first-class OS features.
9. The GUI is optional and should use normal OS services.
10. Graphics do not permanently own an entire CPU core.
11. Portable TabOS code should run in a native macOS host environment.
12. Hardware-specific code must remain behind platform abstractions.
13. QEMU is secondary to the native host environment for development.
14. Full Unix/POSIX compatibility is not a primary architectural goal.

---

## 25. Unresolved Architectural Decisions

Do not silently resolve these while implementing unrelated work.

They require deliberate design or experiments.

### Program execution

- executable format
- relocation model
- runtime symbol resolution
- API/syscall ABI
- executable memory placement
- application memory protection
- crash containment
- process/task relationship

### Runtime APIs

- handle model
- error representation
- libc strategy
- IPC model
- environment variables
- stdio/file descriptor model

### Filesystem

- default filesystem
- mount layout
- flash vs SD responsibilities
- `/dev`-style model
- package/install format

### Graphics

- pixel format
- framebuffer placement
- buffering strategy
- surface model
- compositor model
- fullscreen optimization
- fonts/text rendering
- hardware acceleration opportunities

### Networking

- BSD sockets vs TabOS-native interface
- ESP32-C6 ownership and transport
- Wi-Fi configuration model

### Development

- SDK build tooling
- host/target binary compatibility strategy
- RISC-V binary emulation
- debugger integration
- C++ support

---

## 26. Architecture Validation Order

The following experiments should drive unresolved architecture decisions.

### Phase 1 — Hardware

Prove:

- display
- keyboard
- touch
- microSD
- PSRAM
- networking
- audio

### Phase 2 — Host Platform

Create a native host build capable of running shared TabOS code.

Prove:

- console
- filesystem abstraction
- input abstraction
- graphics output
- timers
- task/thread abstraction

### Phase 3 — Application ABI

Build a tiny external program such as:

```c
int main(void) {
    tab_printf("Hello from TabOS\n");
    return 0;
}
```

Compile it independently and launch it from storage.

This experiment should determine:

- executable format
- relocation requirements
- API binding
- executable memory requirements

### Phase 4 — Shell Integration

Support:

```text
$ hello
Hello from TabOS
```

without rebuilding or reflashing TabOS.

That is the first major milestone proving the intended architecture.

---

## 27. Rules for Codex

When implementing or modifying TabOS, Codex should follow these rules:

- Preserve subsystem boundaries.
- Keep ESP-IDF dependencies inside platform or hardware-specific code.
- Do not expose FreeRTOS objects through public application APIs.
- Do not expose ESP-IDF errors or structures through public application APIs.
- Prefer portable implementations that work on both ESP32-P4 and host.
- Keep application-facing APIs small and stable.
- Avoid adding POSIX behavior merely because it is familiar.
- Do not permanently pin services to CPU cores without measured justification.
- Do not assume executable PSRAM behavior without verification.
- Do not assume process isolation exists.
- Keep GUI/windowing layered on graphics/input services.
- Keep board-specific behavior out of generic subsystems.
- Prefer experiments over speculative abstractions where hardware behavior is unknown.
- Update this document when an unresolved architecture item becomes an agreed decision.

---

## 28. Architectural North Star

TabOS should feel like a small, coherent computer whose implementation happens to run on an ESP32-P4.

An application developer should think in terms of:

```text
files
programs
processes
input
graphics
sound
networking
TabOS APIs
```

not:

```text
ESP-IDF components
FreeRTOS internals
MIPI driver calls
I2C keyboard registers
ESP32-C6 transport details
```

Maintaining that separation is the central architectural constraint of the project.
