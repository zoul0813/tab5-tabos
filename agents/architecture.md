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

### Application command discovery [DECIDED]

Applications are installed as extensionless executable files under `T:/bin/`.
The shell default command search path is `T:/bin`; users may redefine it with
DOS-style semicolon-separated PATH entries. Absolute drive paths and relative
paths containing `/` (including `./` and `../`) bypass PATH lookup. The shell
does not append `.bin` to command names. Before a filesystem-loaded child starts,
its executable path is normalized against the parent's inherited process-local
working directory rather than the filesystem-wide working directory.

### Pre-release application ABI [DECIDED]

Applications consume public SDK headers only; the ELF transport table is private to CRT
and SDK runtime sources. The current version value is 3, but the ABI is not released or
frozen. Incompatible changes are allowed while TabOS is being defined, and bundled
applications are rebuilt with the system. No compatibility shims are required for
unreleased binaries. The current runtime uses newlib for C17 allocation and stdio, provides the
documented filesystem subset, monotonic time/cooperative sleep, foreground process
launch/wait, and portable system information. TabOS does not promise full POSIX compatibility.

### RTC and wall clock [DECIDED]

TabOS wall-clock values are signed Unix seconds in UTC. Tab5 reads and writes its
RX8130CE RTC at I2C address `0x32`; host targets follow the host clock and simulate
setting it with a private offset rather than changing the workstation clock. Kernel and
drivers do not apply timezone or daylight-saving policy. Applications receive a portable
calendar API plus standard `time()`, `gettimeofday()`, and `clock_gettime()` access.
The existing monotonic clock remains separate and must be used for elapsed time.

### Hardware device registry [DECIDED]

TabOS owns a fixed-capacity registry of hardware and deterministic host-virtual devices.
The registry has 32 lifetime slots per boot. Public 32-bit device IDs encode a boot
generation and slot; removed slots become tombstones and are not reused until the next
boot. Only detected devices are registered. Active entries carry a fixed-size logical
name, driver name, device class, ready/offline/fault state, informational feature flags,
and last error. Application access is not gated by a capability mask.

The public API supplies copied count/index/ID/name lookup and process-owned lifecycle
subscriptions. Each subscription retains 32 events, drops the oldest event on overflow,
reports overflow on the next successful read, and is discarded during process cleanup.
Portable runtime integration registers initialized `display0`, `keyboard0`, `storage0`,
`rtc0`, `battery0`, and `wifi0` entries from platform diagnostics while keeping typed
drivers below the platform boundary. Host builds expose deterministic virtual counterparts.
Wi-Fi registry state follows network-service offline, fault, and ready transitions. RTC
read, write, and calendar-data failures move `rtc0` to fault with the platform error;
a later successful RTC operation restores ready state. Battery telemetry and charger-control
failures likewise move `battery0` to fault, and successful retries restore ready state.
Battery status carries explicit per-field validity; signed current and power are positive
while discharging and negative while charging.

Detected touch hardware is registered as `touch0`. The process-owned pointer service
normalizes platform coordinates into the logical 1280x720 display space and supplies
bounded multi-contact event queues only to the foreground owner. Focus changes, device
removal, and queue resets cancel active contacts. SDL mouse contact 0 and SDL touch
contacts share this contract; Tab5 controller handles remain below the platform boundary.
The contact ceiling defaults to five and is build-configurable for future hardware.
Tab5 GT911 and ST712x controllers use active-low GPIO23 interrupt callbacks. ISR context
only records readiness and wakes runtime task; controller I2C reads, contact matching,
logical rotation, delivery, and fault cancellation remain serialized in task context.
Idle runtime updates do not read touch controller.

Camera capture follows the same platform boundary. Applications configure process-owned
streams and receive metadata plus opaque generation-tagged leases; the kernel retains
frame storage and exposes bounded copying only. Three-slot stream pools never overwrite
leased frames, replace the oldest ready unleased frame for slow consumers, and count
drops. H.264 instead pauses backend updates until a pool slot is free, preserving encoded
reference pictures; sensor-side skips are outside the pool-drop count. Close or process
teardown reclaims buffers and leaked leases. Host supplies
deterministic RAW8 fixtures. Host and Tab5 use dedicated capture workers; callbacks wake
runtime after frame or fault completion. Tab5 camera I/O, RAW conversion, preview
submission, JPEG encoding, and H.264 encoding remain in worker task context. The Tab5
worker is pinned to CPU0: physical testing found that allowing this V4L2/ISP path to migrate
between cores corrupted RGB565 preview data without reporting a driver error. A two-second
dequeue deadline provides slow stall diagnostics, not normal polling. H.264 capacity is
checked before dequeue, and lease release wakes a capacity-blocked worker. Stop and
shutdown join capture work before frame pools, DMA mappings, or mutexes are destroyed.

### Native application quiescence [DECIDED]

Every native private ABI gate is guarded by `platform/esp32p4/application_task.c`;
`application_gates.inc` covers all table fields, with an ABI-size assertion preventing
silent omissions when fields are added. Task-local storage retains the execution
context; portable gates still obtain only their application user data. Teardown sets
stop, suspends the task, and checks `eTaskGetState()` until it is not running on either
core. It may delete only when gate depth is zero. An active gate is resumed after
nonblocking cancellation is issued and parks after releasing all service locks.
Computing guest code outside gates does not need to cooperate. A completion flag alone
is never treated as a stopped acknowledgement. The pinned IDF deletion helper already
suspends/checks task execution, but cannot establish that service locks were released.

Native socket and echo workers use bounded cancellation polls; TLS uses WANT retries.
Callers retain their worker mutex and consume the reply before leaving the guarded
call, preventing stale-response reuse. Close operations remain valid during cancellation
for rollback. A DNS lookup already inside lwIP and the bounded ESP-TLS connect-select
call may delay quiescence; do not delete either task or borrowed state to enforce a
teardown timeout. Host stop abandons copied jobs using its existing continuation rules.

### Generic wait sources [DECIDED]

Asynchronous application waits use opaque, generation-tagged `tabos_wait_source_t`
handles owned by the calling process. A source records its parent service handle and is
invalidated before that parent closes. Stale and foreign-process sources return `EBADF`.
The generic event vocabulary is readable, writable, state-changed, error, and hangup;
each source adapter accepts only meaningful bits. `tabos_wait()` supports zero-time polls,
finite monotonic millisecond deadlines, and infinite waits. Process teardown marks waits
cancelled and wakes native workers before disposing parent resources, so a later process
cannot receive stale completions. Sockets expose sources through
`tabos_socket_wait_source()` and device lifecycle subscriptions through
`tabos_device_subscription_wait_source()`, and pointer streams through
`tabos_pointer_wait_source()`, and camera streams through `tabos_camera_wait_source()`.
A type-indexed internal adapter table supplies
per-source event validation and readiness translation so future services can join without
adding service-specific wait transports.

### Reboot and power-off [DECIDED]

Applications request immediate reboot or power-off through Linux-compatible
`reboot(RB_AUTOBOOT)` and `reboot(RB_POWER_OFF)`. TabOS does not define a generic
`shutdown()` C function because that name belongs to the POSIX socket API. Requests are
deferred to a safe runtime boundary; application cleanup, descriptor close, filesystem
unmount, display shutdown, and platform shutdown complete before reset or power control.
The first accepted request wins. Host reboot reinitializes TabOS in the same process;
host power-off exits. Tab5 power-off uses the second PI4IO expander power signal and
remains halted if external power prevents complete removal.

Fullscreen graphics use an OS-owned RGB565 framebuffer through public SDK calls.
Applications never receive display-driver ownership. They may clear, draw clipped
primitives, blit RGB565 pixels, and explicitly present a frame. Closing or exiting
the graphics application restores the retained terminal display.

[DECIDED] Applications may select an SDK-owned lower-resolution RGB565 canvas by setting
both dimensions in a zero-initialized graphics context before the single open call. Leaving
both dimensions zero selects native mode. TabOS chooses the largest uniform integer scale
that fits, centers the output, and uses a
runtime-changeable black-by-default RGB565 color for letterbox/pillarbox borders. Existing
graphics operations render into application memory and present clears borders then performs
one nearest-neighbor blit through the normal kernel graphics API. This adds no ELF call-table
entry, never exposes physical framebuffer memory, and never stretches pixels nonuniformly.

[DECIDED] Fullscreen graphics ownership automatically suspends TTY shortcut interception
and all terminal rendering/presentation. Terminal cell/history state may continue changing
but cannot touch the graphics framebuffer. Raw input reaches the graphics application;
applications do not manage TTY policy merely because they enter graphics mode. Closing or
exiting graphics redraws and presents the retained terminal once.

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

### Event and deadline wake foundation [DECIDED]

Portable runtime scheduling uses a narrow TabOS-owned readiness bitset and absolute
monotonic deadlines. Readiness bits are wake hints and may coalesce; authoritative data
remains in each subsystem queue or state owner. Platform backends provide task-context
notification, an ISR-safe notification form where supported, indefinite wait, and wait
until an absolute deadline. Shutdown sets its readiness bit and wakes the runtime before
platform notification state is released.

The host backend maps the contract to SDL events, including headless operation. Tab5 maps
it to FreeRTOS direct task notifications; FreeRTOS types remain below the platform
boundary. Each blocking wait returns its coalesced readiness bitset to the portable
dispatcher. The dispatcher runs only named event owners and expired deadline owners,
once each in deterministic order per bounded pass, then the platform recomputes the
nearest deadline before blocking again. Key repeat, cursor blink, network retry, and
finite waits publish exact monotonic deadlines. `UINT64_MAX` means no deadline and finite
additions clamp below it; late periodic work runs once and advances to the next future
period. Cursor ownership and network/input changes wake runtime when they add or cancel
deadlines. Native Tab5 application completion and ELF exit/child-exec requests publish
application readiness; process launch and parent restoration do likewise. These
notifications carry no process pointer, so coalesced late wakeups cannot target a
destroyed or generation-reused slot. No compatibility tick remains. Active host RV32
interpretation keeps the runtime immediately runnable for bounded instruction slices
only while the guest can execute. A suspended host wait/I/O gate retains its PC and
arguments and publishes a bounded 10 ms readiness retry deadline, shortened to the
original finite wait deadline. This deadline exists only while an operation is pending.
Socket and established TLS operations use nonblocking backend attempts; explicit guest
nonblocking socket mode still returns EAGAIN. DNS, echo, and TLS setup use at most 16
unfinished detached jobs with owned request/result storage. Only the runtime thread
publishes results, allocates process handles, or accesses guest memory. Cancellation
abandons replies without joining an uninterruptible resolver; completed abandoned TLS
transports are freed by their worker. Worker completion never calls into destroyed SDL
notification state, and echo uses an OS monotonic clock independent of SDL lifetime.
Native Tab5 execution does not use that runnable hint because it runs in its own managed
task. Debug builds count wake bits and expired deadline owners; reporting piggybacks on
the existing health audit rather than creating another periodic deadline.

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

Current process foundation provides fixed-capacity process table, stable process IDs,
parent/state metadata, and nested foreground stack. Launching built-in or filesystem ELF
child blocks and retains parent, transfers console focus, then restores parent and child
status during unwind. Public cooperative C API exposes child request through
`tabos_app_exec()` plus later child-status collection; caller returns from current
callback after successful request. Filesystem ELF image, execution context, path, and
descriptor belong to child process and are released only during child cleanup. Persistent
`elf-hello` diagnostic runs configured ELF twice as children without exiting process 0.

Filesystem-backed shell now loads `T:/bin/shell.bin` directly as process 0. Experimental
ELF API provides console input/output, terminal clear, current-directory and directory
listing operations, child execution, yield, and exit request. Host advances shell through
retained RV32 interpreter slices. Tab5 platform starts native ELF entry in managed FreeRTOS
application task and receives completion notification in runtime task. Shell uses pending child-exec
protocol to remain blocked until process manager restores it with child status.

Process teardown cancels blocking application waits and stops the native execution
context before releasing any process-owned service, descriptor, heap, or executable
mapping. This prevents cleanup from racing a final application API call.
Queued graphics commands borrow application buffers. Teardown discards that queue after
stopping execution; it must not drain blits after guest RAM or the native stack is freed.
Explicit graphics close still fences pending work while application memory is live.

ELF ABI version 3 entry and nested execution carry bounded `argc`/`argv`. Child loader
state owns copied arguments for full process lifetime. Tokenization, quoting, and escaping
are application policy implemented by shell, not kernel process behavior.
SDK `<tabos/process.h>` wraps pending ELF call-gate protocol as synchronous
`tabos_exec(path, argc, argv)`: parent stays loaded and blocked until child cleanup returns
status. Maintained tester self-launches through child and grandchild levels, validates
reverse status delivery, then repeats chain to exercise cleanup and reload.

First ELF implementation accepts static RV32 `ET_EXEC` image with bounded
`PT_LOAD` segments and retained `SHT_RELA` records, but no dynamic linking. Loader can consume a bounded file
through TabOS filesystem API, copies its image into platform-provided executable memory,
and invokes entry with versioned API table. Checked-in hello bytes remain only a focused
loader/test fixture. Tab5 loads the image through a writable PSRAM allocation and maps the
same physical pages into the ESP32-P4 PSRAM linear region with an `EXEC | READ` request.
Loader rebases absolute data and instruction-address relocations to this executable alias
before final cache synchronization. Application builds use non-PIC code, disable linker
relaxation and small-data addressing, emit static relocations, then remove debug sections
without stripping relocation or symbol tables.
ESP-IDF rejects an explicit `EXEC | WRITE` request, but the selected P4 linear region is
connected to both instruction and data buses. Hardware validation must prove writes to
globals, BSS, and newlib state through that alias. Host executes the same RV32 artifact
through resumable interpreter slices. This remains experimental, not final
executable-format or process-isolation decision.

[DECIDED] Initial process model uses persistent nested foreground processes. Shell is
persistent root process, initially process 0. Executing child blocks but does not unload
parent. Child becomes sole focused user process and may execute another child. Exit or
fault pops child, releases its resources, returns status to parent, restores parent's
console/input focus, and resumes parent at call site. This resembles synchronous
spawn-and-wait even when public API is named `exec`; it does not use POSIX image-replacing
`exec` semantics.

Conceptually:

```text
kernel and service tasks: always runnable

foreground stack:
    child-of-child       running; owns focus
    child                blocked; fully retained
    shell (process 0)    blocked; fully retained
```

[DECIDED] Initial Tab5 mapping is one managed FreeRTOS task per native user process.
Parent blocks through TabOS process synchronization rather than arbitrary suspension.
Runtime/service task continues input polling, timers, display, filesystem, network, and
lifecycle work. Native instructions still execute directly on ESP32-P4. Host represents
each process with retained RV32 interpreter context and advances only foreground process
in bounded instruction slices. Public application API must not expose FreeRTOS or host
thread types.

Initial scope deliberately excludes background jobs, multiple runnable user processes,
pipelines, signals, and worker threads inside one process. Process table and ownership
model must leave room for these later.

[DECIDED] Process 0 is kernel-required root shell and has liveness invariant: it cannot
exit normally. Return from entry, explicit exit request, execution fault, forced
termination, or any other transition toward exited state must trigger kernel panic.
Kernel must not unload or automatically restart process 0. Panic path must preserve
enough display/runtime functionality to report clear process-0 failure, cause, and exit
status when available through both platform serial/log output and framebuffer
console/terminal. Panic reporting must not depend on process 0 retaining valid console
ownership.

Current process state records explicit exit-request, executable-return, execution-fault,
and forced-termination causes. Console state shared between runtime and native
application tasks is protected by the platform synchronization boundary: SDL mutex on
host and priority-inheriting FreeRTOS mutex on Tab5.

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

[DECIDED] The initial transport is a versioned API table reached through SDK
stubs. The ELF entry point is private `crt0`; normal C17 applications implement
`int main(int argc, char **argv)`. `crt0` retains the API pointer, initializes
the runtime, invokes `main`, and exits with its return status.

```text
main(argc, argv)
    ↑
newlib + TabOS syscall stubs
    ↑
versioned TabOS API table
    ↑
kernel services
```

[DECIDED] The first process C runtime provides POSIX-style descriptors and file
calls. Descriptors 0/1/2 are console stdin/stdout/stderr; 3+ are process-owned
files/devices. Working directory and errno are process-local, children inherit
the parent's working directory, and exit closes all descriptors. Blocking
stdin is default, with `fcntl(..., O_NONBLOCK)` supported immediately and
empty nonblocking reads returning `EAGAIN`.

[DECIDED] Each process has a 16 KiB stack and a lazily used, contiguous heap
arena capped at 1 MiB by default. `_sbrk` advances within that arena. ELF
describes static load/BSS requirements only; later executable metadata may
override resource limits.

[DECIDED] Runtime text is single-byte CP437. Standard streams and files carry
bytes without UTF-8 decoding or newline translation. stdin is unbuffered,
stdout line-buffered, stderr unbuffered, and regular files use normal libc
buffering. The SDK does not provide `gets()`; use `fgets()`.

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

A logical namespace exists separately within each assigned drive:

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
- drive registration and routing
- file access
- directories
- metadata
- removable media
- program loading paths

The implementation may delegate actual storage to FAT, LittleFS, or other ESP-IDF-compatible filesystems.

The public API should not depend on the filesystem used underneath.

[DECIDED] Storage uses a drive-letter namespace. Internal Tab5 flash is `A:` and TF/microSD is `T:`. Other storage backends assign letters in their respective drivers. Canonical paths use `A:/path` form. `/path` means absolute path on current drive and `path` means relative path on current drive. DOS-style `A:relative` semantics are deferred.

[DECIDED] The initial application surface is a deliberately bounded POSIX source-compatible
subset. SDK compatibility headers map familiar names such as `open`, `read`, and
`stat` to a TabOS-owned prefixed ABI; TabOS does not expose host or ESP-IDF libc
objects as its ABI. The portable core owns path normalization, descriptors, errors,
and dispatch. Drive table enumerates backend-owned letters; host exposes controlled `A:` and `T:` directories, while Tab5 currently exposes BSP-mounted microSD FAT as `T:`. Missing drives return `ENODEV`; cross-drive rename returns `EXDEV`. Internal-flash `A:` implementation, permissions, links, and removal recovery remain pending.
Portable metadata includes device/file identity for same-file comparisons. Host
backends retain native device/inode identity and therefore recognize hard links.
ESP-IDF FAT reports no inode, so portable core supplies case-folded normalized-path
identity and updates open-file fallback identity across rename. Identity values are
comparison data for current files, not persistent storage IDs.
[DECIDED] Same-drive rename replaces an existing destination. Backends such as the
pinned FatFs implementation that reject replacement move the destination to a reserved
drive-root recovery name, install the source, and restore the destination if installation
fails. This multi-step fallback preserves recoverable data but is not power-loss atomic;
a cleanup or rollback failure may leave a `.tabos-rename-*.bak` recovery file.
Tab5 FAT uses heap-backed long-filename buffers with a 255-character maximum so
the backend honors the public filesystem name limit instead of silently imposing
8.3 names.

Current built-in startup applications execute synchronously on ESP-IDF main task.
Tab5 reserves an 8192-byte main-task stack because nested TabOS filesystem calls
enter FatFs VFS formatting code that exceeds ESP-IDF's 3584-byte default. Future
application-task model must assign explicit per-application stack budgets instead.

---

## 9. Shell Architecture

### Persistent command history [DECIDED]

The shell owns a fixed 32-entry command history and Up/Down recall policy. It preserves
unparsed printable ASCII command lines, skips blank lines and consecutive exact
duplicates, and restores the current draft after browsing. History loads from
`T:/user/history.txt` and saves before command execution through standard SDK file
operations using a temporary file and rename. Storage failure does not stop the shell.
Plain Up/Down recalls commands; Ctrl+Arrow remains the existing opt-in TTY scrollback
policy. History is application state, not a kernel or terminal service.

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
Portable input queue and repeat state use the platform mutex abstraction. Tab5 uses a
priority-inheriting FreeRTOS mutex because application and runtime tasks can contend on
one core; task-level atomic spinning is forbidden for this queue.

Pointer input uses a separate public `<tabos/pointer.h>` stream API with down, move, up,
and cancel events. Events carry boot-local device and stable contact IDs, logical display
coordinates, buttons, and optional normalized pressure. Consumption follows existing
foreground process ownership. Gesture recognition and window routing remain future
clients of this service rather than kernel pointer semantics.

Keyboard input now uses a public platform-neutral event queue with key-down, key-up,
modifier, repeat, and CP437 text semantics. Filesystem ELF applications access raw events
through `<tabos/input.h>` over the private ABI, while terminal stdin retains ANSI arrow
sequences. Both interfaces consume the same foreground queue and must not be mixed by one
application. Per-process TTY policy defaults to cooked input and supports raw input for
physical key consumers. Tab5 Normal matrix events expose Sym and Aa/Shift independently;
cooked Tab5 translation implements one-shot tap and held modifier behavior. SDL3 and the
Tab5 I2C keyboard are backend producers. Future USB HID support
on Tab5 must feed the same queue and may coexist with the built-in I2C keyboard;
applications must not depend on input-device-specific protocols.

### Tab5 boot-time USB storage mode

[DECIDED] The Tab5 platform supports an early boot-mode selection before the
kernel mounts filesystems. Holding the built-in keyboard Delete key selects USB
mass-storage mode. This path initializes only the services needed to detect the
boot request, present a status screen, and export the TF/microSD block device as
`T:` through USB MSC. It does not start the normal application runtime.
The ESP32-P4 high-speed OTG controller is physically routed to the Tab5 USB-A
connector, so this mode disables that connector's host-mode 5 V output before
starting TinyUSB device mode. The host must supply VBUS through a data cable;
the Tab5 USB-C programming/power connection does not enumerate this device.

[DECIDED] USB-A VBUS is safe-off by default. The board power switch remains off
at reset, and early platform initialization explicitly drives its enable low.
No general boot or peripheral initialization path may enable it. Only the USB
host service may enable USB-A 5 V after acquiring exclusive port ownership and
configuring host mode; stopping that service must disable 5 V again.

Storage ownership is exclusive: the filesystem core must not mount `T:` while
USB MSC owns its block device. A confirmed host safe-eject request causes a
controlled system restart. USB disconnect must have an explicitly tested safe
fallback because host operating systems do not all report eject identically.
The future `usb-storage` application must call the same service and follow the
same ownership transition.

The portable console service provides cooperative foreground ownership above this queue. One session at a time may consume console input and update framebuffer terminal. Terminal owns colored cell/history ring, live cursor, independent viewport, reflow metadata, and rendering into platform framebuffer. Session tokens reject background and stale callers but are not a security boundary. Future process manager must own foreground assignment policy. Optional console diagnostic application uses only public console API and remains outside kernel shell policy.

[DECIDED] Terminal navigation shortcuts are per-process opt-in TTY policy, not global
input policy. Shell enables scroll keys through the public `ioctl()` wrapper. Children
inherit a value copy; a child may disable the mode for raw/game input without changing
the blocked parent's retained mode. Enabled navigation keys are consumed before stdin.

Portable monotonic time and deadline timers live behind platform clock source. Console
uses a repeating timer for cursor phase and publishes its next deadline to runtime;
fullscreen graphics cancels that deadline. Late timer polls emit one expiration and skip
missed intervals without replay. Terminal tracks dirty visible cells for ordinary
text/cursor changes and reserves full redraw for viewport, clear, or resize changes.

Network backends expose a callback that only marks status dirty and wakes runtime.
Portable network service copies platform status in runtime context after notification;
idle updates perform no backend status read. ESP-IDF Wi-Fi/IP callbacks and host state
changes use same path. Registry synchronization reads in-memory audio, pointer, camera,
and network service state, while keyboard, RTC, battery, and storage use one 60-second
health audit because current drivers lack change callbacks. Registry itself coalesces
unchanged state, preserving one subscription event per transition.

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

### 11.2 Hardware Acceleration

[DECIDED] Public graphics remain RGB565 and accelerator-independent. Tab5 platform
selects PPA for supported bulk operations, PIE SIMD for measured-beneficial CPU raster
spans, and scalar C otherwise. PIE instructions and ESP-IDF handles remain private.
Host scalar output defines portability behavior and must remain pixel-identical.

### 11.3 Future GUI

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

A central mixer combines process-owned playback streams with saturation and per-stream gain.
Streams use signed 16-bit little-endian PCM and select one of the backend's reported native
sample rates. Playback and capture share one physical clock rate: the first open stream selects
it, conflicting concurrent opens fail with `EBUSY`, and the next open after all streams close may
select a new rate. The default rate is 44.1 kHz. Applications must perform any desired
cross-rate conversion themselves.

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
tabos_handle_t
tabos_file_t
tabos_process_t
tabos_surface_t
tabos_timer_t
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
tabos_result_t result;
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
    tabos_printf("Hello from TabOS\n");
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

### 27.1 Symbol naming

[DECIDED] Public application API and ABI symbols use `tabos_`. Internal symbols do
not use a single abbreviated project prefix. Cross-file internal names identify owning
architectural layer or subsystem directly:

```text
tabos_*       public application API and ABI
kernel_*      kernel, process, scheduling, and lifecycle internals
application_* built-in application registry/adapter internals
console_*     console service internals
terminal_*    terminal model and renderer internals
display_*     portable display/graphics internals
input_*       portable input internals
filesystem_*  portable filesystem internals
loader_*      executable loader internals
platform_*    portable platform contract implemented by selected backend
host_*        host-only backend helpers
espidf_*      ESP-IDF framework glue independent of specific chip behavior
esp32_*       helpers shared across supported ESP32-family chips
esp32p4_*     ESP32-P4-specific helpers
esp32c6_*     ESP32-C6-specific helpers
esp32s3_*     ESP32-S3-specific helpers
tab5_*        M5Stack Tab5 board-specific helpers
test_*        shared test helpers
```

Use `esp32_*` only when implementation genuinely applies across relevant ESP32 chips.
Use `espidf_*` for ESP-IDF framework adaptation that does not encode chip behavior.
Use exact model prefix when code depends on that model's CPU, memory map, peripheral,
cache/MMU, ROM, or errata. Use board prefix such as `tab5_*` for wiring, controller,
display-revision, keyboard, or other board integration even when underlying chip is P4.
Do not label board-specific code merely `esp32_*` or `esp32p4_*`.

Public and internal types follow same ownership rule: `tabos_process_id_t` is public,
while `kernel_process_t`, `platform_riscv32_context_t`, and `tab5_display_revision_t`
are internal. File-local `static` helpers need no project/layer prefix when name is clear
within file. Avoid leading-underscore and double-underscore project identifiers.

Existing generic `tab_*` and internal-only `tabos_*` names were migrated to this
convention. Keep the `tabos_*` prefix for public APIs and keep future internal symbols
in the owning layer/subsystem namespace. Individual pre-release public symbols may still
change when the API design changes.

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
- Follow symbol ownership prefixes above; do not introduce new generic `tab_*` symbols.

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

## Kilo terminal service implementation (2026-09-07)

Kilo uses public copied `TABOS_TTY_GET_SIZE` geometry and a foreground process-owned
`tabos_input_wait_source()` adapter. Keyboard-only waits use retained coalesced wake
signals, with absolute deadlines and host RV32 suspension; other generic service
waits retain their existing behavior. Source handles use existing generation and
teardown rules. Terminal CSI parsing is bounded to eight parameters; cursor addressing
uses the live screen rather than the oldest retained scrollback line. Immediate
wrapping is preserved, so Kilo reserves the final column. Console release resets
attributes and incomplete escape state before parent acquisition.

Kilo is an independent RV32 application, with a 2 MiB heap and 32 KiB stack metadata
request, byte-oriented rows, bounded edits, and an application-local backup/rename save
transaction. No POSIX emulation or hardware dependency was added to the application.

## GPIO service ownership and power baseline

Tab5 GPIO interrupt registration is owned by `platform/esp32p4/gpio_interrupt.c`.
Serialized platform initialization installs one non-IRAM service for the boot; keyboard
and touch own only their pin handlers. A consumer must never uninstall the shared service.
Touch constructors retain GPIO configuration but receive no component callback; TabOS
checks direct attachment and removes it before controller teardown. Concurrent registration
and new consumers require an explicit lifecycle audit.

Power Phase 0 inventory and wake-source restrictions live in `docs/power-baseline.md`.
Portable Phase 1 and 2 power management is internal to kernel. Runtime dispatcher owns state
transitions and combines its absolute deadline with existing service deadlines. Fixed-capacity
participants are ordered once by stable dependency names; asynchronous completions carry a
transition generation so stale replies cannot advance current state. Invalid graphs disable
suspend while normal operation remains available. Platform boundary supplies brightness,
preparation/abort, sleep entry, wake-cause collection, and restoration. No public suspend API,
PM enablement, or Tab5 wake arming exists yet. Missing tested reversible service lifecycle
remains a blocker, including initialized drivers with no application handles.

Keyboard and pointer services retain normalized physical activity plus held/contact state
under their existing service mutexes. Runtime samples both after platform ingress and before
foreground execution, then owns resulting power transition. Fullscreen ownership and open
audio/camera streams are sampled as dim/suspend inhibitors; lifecycle changes notify runtime.
Final inhibitor release starts a fresh idle interval. Power status stores desired and last
known effective brightness separately, including validity and failure. Host SDL brightness is
texture presentation modulation; framebuffer and captured pixels remain unchanged.

Debug peripheral activity uses a narrow `platform_runtime_log_activity()` diagnostic
hook beside the existing health-audit wake report. Tab5 counts codec pairs/frames/errors,
headphone attempts/errors, VSYNC and PPA completions with boot-lifetime lock-free unsigned
atomics; no new periodic task/deadline exists. Release compiles out updates; host does
not manufacture physical peripheral measurements. These counts are not PM policy or
synchronization state.

Audio hardware has an initialized-but-idle platform lifecycle. Service admission starts
transport at requested shared sample rate and initial route before exposing first handle.
Last-stream close stops transport after worker exit. Tab5 closes both codec devices, disables
speaker routing, and runs jack polling only for active speaker routing; host SDL likewise owns
audio streams only while TabOS streams exist. Backend-start failure updates device health and
remains retryable after no stream was admitted. Maintenance audit supports suppressed deadlines
and one overdue resume pass. Pinned ESP-IDF v5.4.4 exposes no public retained-buffer MIPI-DPI
pause; controller display-off is not treated as scanout quiescence.

## Lua CLI implementation (2026-09-12)

Lua 5.5.1 is an independent RV32/newlib application under `apps/lua`, using only
public SDK services. No public ABI change is required: the cooked-event console broker
reuses Kilo's keyboard wait source. CLI, console I/O and count-hook cancellation share
one consumer. Pure-source modules and explicit library registration omit desktop-only
integration. Metadata requests 4 MiB heap/64 KiB stack, with a 3 MiB Lua allocation
ceiling and 48-level C-call/pattern limits. Physical high-water and latency measurements
remain pending. The shared Make rules accept tracked trailing `TABOS_LDLIBS`; Lua uses
`-lm`. These application-specific budgets/profile choices do not alter general SDK limits.

[DECIDED] Display-only idle policy dims at 60 seconds, disables backlight at 180 seconds,
and disables panel output at 300 seconds of
total inactivity while CPU, applications, networking, and deadlines continue normally.
Screen-off is a requested display condition within idle, not a system-suspend state.
Brightness and panel enablement are independent platform operations. Backlight must reach
zero before panel disable; panel must enable before brightness restoration. Previous panel display-off suppressed touch
restoration on the tested ST7121 board; operator confirms backlight-only off restores
on a screen tap, with predominantly 0.02 A current (observed range 0.01–0.03 A).
Retain shared touch supplies and
scanout buffers. Touch/keyboard restore output through normal runtime activity handling
until panel-off is requested; after that, pointer activity neither restores nor inhibits
blanking, while keyboard still restores. All deadlines share last physical activity;
held input, fullscreen/media ownership, and panic inhibit idle blanking. No off retry
deadline is added after failure; invalidate corresponding effective status and retry on activity
or policy updates. DMA/VSYNC quiescence remains a separate driver lifecycle requirement.

[DECIDED] Runtime loads optional version-1 INI `T:/etc/power.conf` once after filesystem
initialization and before power-manager initialization. Portable `kernel/power_config.c`
owns bounded parsing/loading through TabOS filesystem APIs; defaults remain 60/180/300
seconds and 75/20 percent. All configurable timeouts are absolute inactivity durations.
Missing fields use defaults; invalid complete policy or I/O failure leaves defaults intact.
No hot reload, periodic storage access, automatic writes, public ABI, or sleep enablement
is added. Normal brightness must remain nonzero; dim brightness is capped at normal by
existing policy. The checked-in `etc/power.conf` is a user-copyable template.
