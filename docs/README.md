# TabOS Documentation

This directory contains user and contributor documentation for TabOS.

## Guides

- [Project Configuration](configuration.md): persistent host and device options managed by `tools/tabos config`.
- [Directory Structure](directory-structure.md): repository layout, directory responsibilities, and dependency boundaries.
- [Display System](display.md): logical framebuffer contract, host presentation, and Tab5 display behavior.
- [Keyboard Input](input.md): public events, SDL3 translation, and Tab5 Keyboard protocol.
- [Console Service](console.md): foreground ownership, terminal controls, input, and diagnostic app.
- [Application Lifecycle](applications.md): descriptors, built-in registry, foreground execution, and current limits.
- [ELF Loader Experiment](elf-loader.md): stripped RV32 ELF contract, build pipeline, and Tab5 validation.
- [Time and Timers](time.md): monotonic clock and reusable polling timers.
- [Filesystem and Storage](filesystem.md): POSIX-style file API, host root, and Tab5 microSD behavior.
- [Shell](shell.md): build, install, start, and use filesystem-backed PID 0 shell.

## Documentation Policy

Documentation in this directory describes the current project. Update it in the same change whenever project structure, supported workflows, commands, requirements, or user-visible behavior changes.

The `agents/` directory is separate. It contains background context and implementation guidance for coding agents, not user-facing documentation.
