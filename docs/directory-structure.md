# Directory Structure

This document describes the current TabOS repository layout. It is authoritative for directory purpose and ownership. Update it whenever directories are added, removed, renamed, or given new responsibilities.

## Repository Layout

```text
tabos/
├── .github/workflows/   GitHub Actions builds, tests, and artifacts
├── agents/              Context and implementation guidance for coding agents
├── apps/                Application sources
│   ├── diag/            Optional built-in diagnostics and diagnostic registry
│   │   ├── console/     Console/input diagnostic
│   │   ├── elf_loader/  Embedded-ELF loader/execution diagnostic
│   │   └── filesystem/  Filesystem/storage diagnostic
│   └── hello_elf/       Independently built ELF example application
├── audio/               Portable audio subsystem
├── cmake/               Shared host-build CMake modules
├── config/              Common identity, display, font, input, console, and loader build settings
├── console/             Portable foreground text-console service
├── docs/                User and contributor documentation
├── fs/                  Portable filesystem subsystem
├── graphics/            Portable framebuffer and graphics subsystem
├── input/               Portable input-event subsystem
├── kernel/              Portable runtime, application lifecycle, registry, and service core
├── loader/              Experimental ELF validation, loading, fixtures, and lifecycle adapter
├── net/                 Portable networking subsystem
├── platform/            Environment-specific service implementations
│   ├── esp32p4/         ESP32-P4, ESP-IDF, FreeRTOS, and Tab5 BSP implementation
│   ├── host/            Native host implementation
│   │   ├── posix/       POSIX-specific host services
│   │   └── sdl/         SDL3 display and input services
│   ├── include/         Internal platform interface used by portable code
│   └── posix/           Shared native POSIX storage adapter
├── process/             Portable process table, PID, lifecycle, and focus management
├── rootfs/              Future default TabOS filesystem contents
├── sdk/                 Public application-development surface
│   ├── include/         Public TabOS headers
│   ├── lib/             Application-facing compatibility wrappers
│   ├── posix/include/   POSIX source-compatibility headers
│   ├── linker/          Future linker scripts and executable-format support
│   └── tools/           Future SDK tools
├── targets/             Buildable product entry points and target configuration
│   ├── host/            Native entry point and host artifact packaging files
│   └── tab5/            ESP-IDF Tab5 firmware project and entry point
├── tests/               Host-side automated tests and fixtures
│   ├── component/       Subsystem component tests
│   ├── fixtures/        Shared test data
│   ├── graphics/        Rendering and visual tests
│   ├── integration/     Multi-subsystem integration tests
│   └── unit/            Focused portable unit tests
├── time/                Portable monotonic-clock and polling-timer service
└── tools/               Thin `tools/tabos` CLI and maintainable Python support modules
```

Some directories contain only `.gitkeep` placeholders. Their stated responsibilities are planned boundaries; their APIs are not yet defined.

The command shell will be the first official TabOS application and belongs under `apps/`. It is not part of the kernel. Built-in diagnostic applications live under `apps/diag/`; they are optional test applications compiled into the firmware, not independently loaded programs. Independently built ELF applications, such as `apps/hello_elf/`, remain separate. Kernel and portable subsystems provide terminal, console, input, process, and filesystem services used by applications.

## Targets and Platforms

A target is a complete product that can be built or run. A platform is an environment-specific implementation used by a target.

- `targets/host/` defines the native executable used by both macOS and Linux builds.
- `targets/tab5/` defines the flashable ESP-IDF firmware project for M5Stack Tab5.
- `platform/host/` implements platform services with SDL3 and POSIX APIs.
- `platform/esp32p4/` implements platform services with ESP-IDF and FreeRTOS APIs.

macOS and Linux are distinct build targets in project tooling, but currently share the native entry point and host platform implementation. Platform conditionals provide small operating-system-specific differences where required.

Platform backends are split by service instead of collected in one large translation unit. `platform/esp32p4/` contains runtime, keyboard, display, and executable-memory modules. `platform/host/sdl/` contains runtime/window state, input, display, and executable-memory modules. Private `internal.h` files share only backend-local state and helpers.

Portable `input/input.c` owns queue and HID text translation. Optional event logging lives in `input/input_diagnostic.c`, keeping diagnostic compile gating outside queue logic. `graphics/font.c` owns fixed-width drawing while `config/Font.cmake` describes replaceable raw bitmap data such as `graphics/blueterm.f12`. Build-generated assembly embeds configured asset directly without generating a C bitmap table.

`apps/shell/` contains independently built RV32 shell source. `sdk/tools/build-shell-elf.sh`
produces runnable shell binary separately from firmware build.

## Dependency Direction

Dependencies flow toward platform implementations:

```text
applications
    ↓
public SDK API
    ↓
portable TabOS subsystems
    ↓
internal platform interface
    ↓
host or ESP32-P4 platform implementation
```

Portable code must not include SDL3, POSIX, ESP-IDF, or FreeRTOS APIs directly. Those dependencies stay behind interfaces in `platform/include/`.

## Root Build Files

- `CMakeLists.txt` defines native host builds and shared portable sources.
- `CMakePresets.json` defines macOS and Linux debug/release configurations.
- `targets/tab5/CMakeLists.txt` is the ESP-IDF firmware project entry point.
- `tools/tabos` is the supported thin command wrapper for configuration, setup, build, test, run, flash, and Tab5 serial monitoring. Its implementation is split by responsibility under `tools/tabos_tools/`: configuration schema and persistence, environment setup, target orchestration, and shared process helpers.
- `tools/flash.sh` flashes an existing Tab5 debug or release image using local `esptool`, including images built in a container.
- `AGENTS.md` directs coding agents to internal project context and documentation-maintenance requirements.

Generated output belongs under `build/` or `dist/` and is ignored by Git. Source code and maintained documentation must not be placed in generated-output directories.

## Documentation Boundary

`docs/` contains material intended for users and contributors. Documentation should describe current behavior and remain understandable without agent-specific context.

`agents/` contains design background, constraints, and detailed implementation guidance used by coding agents. It may discuss proposals or future direction and must not be treated as the user documentation set.
