# TabOS Documentation

This directory contains user and contributor documentation for TabOS.

## Guides

- [Project Configuration](configuration.md): persistent host and device options managed by `tools/tabos config`.
- [Directory Structure](directory-structure.md): repository layout, directory responsibilities, and dependency boundaries.
- [Display System](display.md): logical framebuffer contract, host presentation, and Tab5 display behavior.
- [Keyboard Input](input.md): public events, SDL3 translation, and Tab5 Keyboard protocol.
- [Touch and Pointer Input](pointer.md): process-owned pointer streams, logical coordinates, and host/Tab5 backends.
- [Starfall](starfall.md): build, run, controls, storage, and asset provenance for the demo game.
- [Pool](pool.md): house 8-ball against the computer or a second local player; see the [house rules](pool-rules.md).
- [Soccer](soccer.md): six-a-side timed arcade soccer with controls, rules, sound, and profiling.
- [Console Service](console.md): foreground ownership, terminal controls, input, and diagnostic app.
- [Application Lifecycle](applications.md): descriptors, built-in registry, foreground execution, and current limits.
- [SDK Tester Application](tester.md): end-to-end public SDK and ABI validation on host and Tab5.
- [ELF Loader Experiment](elf-loader.md): stripped RV32 ELF contract, build pipeline, and Tab5 validation.
- [DOOM](doom.md): optional pinned GPLv2 DOOM build and data policy.
- [Time and Timers](time.md): monotonic clock and reusable polling timers.
- [Reboot and Shutdown](power.md): orderly restart, power-off commands, and application API.
- [Filesystem and Storage](filesystem.md): POSIX-style file API, host root, and Tab5 microSD behavior.
- [Lua](lua.md): source scripts, modules, interactive prompt, and TabOS runtime profile.
- [Kilo](kilo.md): terminal editing, search, byte-preserving saves, and recovery.
- [Shell](shell.md): build, install, start, and use filesystem-backed PID 0 shell.
- [Networking](networking.md): saved Wi-Fi configuration, network status, and connection control.
- [Audio Service](audio.md): PCM streams, waits, mixing, routes, and the audio test utility.

## Documentation Policy

Documentation in this directory describes the current project. Update it in the same change whenever project structure, supported workflows, commands, requirements, or user-visible behavior changes.

The `agents/` directory is separate. It contains background context and implementation guidance for coding agents, not user-facing documentation.
