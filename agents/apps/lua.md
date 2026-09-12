# Lua Implementation Plan

Status: CLI implementation and automated validation completed 2026-09-12; physical Tab5 acceptance pending. Graphics/input/audio/process bindings remain gated on CLI acceptance. Linux builds/tests excluded by user direction for this implementation.

Current implementation uses the proposed 5.5.1 baseline and resource budgets, with
48-level C-call/pattern limits. Kilo's keyboard wait source is reused without a new
public ABI. UTC table input validates rather than normalizes overflowing calendar
fields; `os.exit` always closes the state and rejects recursive exit during finalization.
See [user documentation](../../docs/lua.md), [provenance](../../apps/lua/UPSTREAM.md),
and [validation](../../docs/validation/lua-cli-2026-09-12.md) for exact behavior and evidence.

This expands the Lua candidate in [Application Port Candidates](../milestone-apps.md). Version selection, resource budgets, binding names, and staged feature choices below are recommendations, not new `[DECIDED]` architectural requirements.

## Remaining Work

Track outstanding work here; completed CLI work remains in [Validation and Delivery](#validation-and-delivery).
Start graphics, input, audio, and foreground child execution only after CLI acceptance.
Linux builds/tests remain excluded by user direction. Each binding slice needs its
own implementation, documentation/example, automated coverage, and physical validation.

### CLI Acceptance on Tab5

- [ ] Verify physical keyboard editing, Aa/Sym typing, multiline input, Ctrl-U cancellation, and Ctrl-C/Ctrl-D REPL exit.
- [ ] Run examples and nested modules from microSD; verify binary file reads/writes.
- [ ] Verify recovery from allocation, parser-depth, and recursion failures; repeat launches and `os.exit(7)` with a usable shell afterward.
- [ ] Measure interruption latency in loops and coroutines; verify runtime service responsiveness.
- [ ] Record stack/heap high-water usage, count-hook overhead, and representative computation/allocation performance.
- [ ] Record physical validation evidence and mark the CLI milestone accepted.

### Graphics Bindings

- [ ] Implement SDK-owned logical canvas lifecycle, RGB565 primitives/blits, explicit present/close, and dimension/buffer validation using packed buffers or userdata.
- [ ] Implement idempotent resource cleanup on explicit close, Lua errors, finalization, and process teardown; define cancellation and buffer lifetime for blocking operations.
- [ ] Document the Lua graphics API and ship a small runnable example.
- [ ] Add deterministic host coverage for drawing, argument validation, resource failures, and cleanup; exercise bindings through the real RV32 runtime.
- [ ] Validate rendering, presentation, cleanup, memory use, and representative frame performance on physical Tab5; record results.

### Input Bindings

- [ ] Expose keyboard press/release and pointer events through the existing shared input ownership policy.
- [ ] Implement console/graphics input transitions without competing consumers; preserve interruption handling and restore console mode on close/error.
- [ ] Document the Lua input API and ship a small interactive example.
- [ ] Add deterministic host coverage for event delivery, input transitions, interruption, and restoration; exercise bindings through the real RV32 runtime.
- [ ] Validate keyboard/pointer interaction and return to a usable shell on physical Tab5; record results.

### Audio Bindings

- [ ] Implement bounded signed-16-bit PCM playback with explicit backpressure and close, without per-sample Lua callbacks or hidden resampling.
- [ ] Define buffer ownership/lifetime and cancellation; implement idempotent cleanup on close, Lua errors, finalization, and process teardown.
- [ ] Document the Lua audio API and ship a small playback example.
- [ ] Add deterministic host coverage for buffer validation, backpressure, cancellation, failures, and cleanup; exercise bindings through the real RV32 runtime.
- [ ] Validate playback, backpressure, cancellation, and resource cleanup on physical Tab5; record results.

### Foreground Child Execution

- [ ] Wrap `tabos_exec(path, argv)` with path/argument validation, argument count/byte limits, and negative-error conversion.
- [ ] Preserve the loaded parent Lua state while the child runs; restore focus after child exit and define blocking-call cancellation behavior.
- [ ] Document the Lua execution API and ship a small child-launch example; retain the distinction from shell-string execution and background jobs.
- [ ] Add deterministic host coverage for invalid arguments, launch failures, exit statuses, and parent-state/focus restoration; exercise bindings through the real RV32 runtime.
- [ ] Validate child launch, exit, repeated execution, and Lua/shell restoration on physical Tab5; record results.

### Later Follow-Up

These items are outside CLI acceptance. Other explicitly deferred capabilities
remain listed in [Outcome and Milestone Boundaries](#outcome-and-milestone-boundaries).

- [ ] Add persistent REPL history, with documented storage and failure behavior plus automated coverage.

## Outcome and Milestone Boundaries

Provide an independently built `T:/bin/lua` application that runs source scripts and offers an interactive prompt on Tab5, macOS, and Linux. The host runs the same RV32 executable as the device. Users can edit scripts with any text editor, including the separately planned [Kilo](kilo.md), and run them without rebuilding firmware.

Deliver two distinct milestones:

1. **CLI Lua:** language runtime, files, pure-Lua modules, usable REPL, bounded memory, errors/interruption, and a small system-information/time module.
2. **TabOS scripting bindings:** graphics, input, audio, and foreground child execution, added in separate validated slices after CLI acceptance.

The CLI milestone does not depend on Kilo being implemented. Reuse any generic keyboard-wait work completed for Kilo, but do not require its terminal-size or ANSI improvements just to execute scripts.

Defer LuaJIT, LuaRocks, dynamic native modules, background jobs, package downloads, a script editor, network/camera bindings, firmware embedding, and a stable third-party Lua C-module ABI. Lua coroutines remain language-level cooperative execution within one foreground process, not TabOS processes or background tasks.

## Upstream and Compatibility Baseline

Propose **PUC Lua 5.5.1**, the current release listed by upstream on the assessment date. Lua 5.4.9 is listed as the final 5.4 release; do not choose it merely because older porting examples use 5.4. Recheck the selected release's published bug list before importing, and record any intentional fixes. See [version history](https://www.lua.org/versions.html), [5.5 build documentation](https://www.lua.org/manual/5.5/readme.html), and [license](https://www.lua.org/license.html).

Vendor the official source archive with its MIT notice. Record the archive URL, version, verified SHA-256, import contents, local configuration, and patch inventory in `apps/lua/UPSTREAM.md`. Builds must work offline after checkout. Preserve upstream formatting in unchanged vendor files; follow project C style in adapters and maintained local modifications. Do not mechanically reformat the entire interpreter.

Keep the standard 64-bit integer and double-precision number configuration initially. The shared TabOS application build uses RV32I and `ilp32`; compiler/newlib software arithmetic must supply unsupported operations. A native ESP32-P4 floating-point build would not match the current host interpreter contract. Measure before considering a reduced-precision profile, which would change script behavior and binary-chunk compatibility. See [Lua configuration](https://www.lua.org/source/5.5/luaconf.h.html).

Prefer source distribution for scripts. For the first milestone, accept text chunks only through CLI, `load`, `loadfile`, `dofile`, and module loading; reject binary chunks consistently with a clear error. Omit `string.dump` and do not ship `luac` initially. This is a documented compatibility restriction, not a security sandbox or an implication that desktop bytecode works on TabOS.

## Current TabOS Fit and Gaps

Read [architecture](../architecture.md), [context](../TABOS_CONTEXT.md), [testing](../testing.md), and [coding style](../coding-style.md) before implementation. Recheck implementation against historical prose: current build metadata defaults and older documentation do not always agree.

| Existing surface | Plan consequence |
| --- | --- |
| `sdk/make/application.mk` | Build C17 RV32I/`ilp32`, use current ABI metadata, install an extensionless executable. Current default heap/stack are 256 KiB/16 KiB; Lua should request explicit limits. |
| Newlib and `sdk/libc/syscalls.c` | Allocation, stdio, file descriptors, seek, and file operations provide a useful base; link and exercise actual functions instead of assuming full POSIX. |
| `sdk/lib/clock.c`, `sdk/include/tabos/runtime_time.h` | UTC wall-clock and monotonic time exist. `os.clock` needs a CPU-time contract and must not silently become uptime. |
| `sdk/include/tabos/input.h`, `sdk/lib/input.c` | Cooked CP437 events and physical key events share a foreground queue. Stdin and event reads must not compete for it. Existing input wait retries through yield. |
| `sdk/include/tabos/wait.h` | Generic service waits exist, but there is no current keyboard source and zero-item timer-only waits are rejected. Do not invent either as an already-supported operation. |
| `sdk/lib/process.c` | `tabos_exec` retains and blocks the caller while a child runs; it is not shell-string execution or Unix process replacement. |
| Filesystem drive namespace | Module filenames may contain `T:`; use semicolon-separated Lua path templates and TabOS filesystem resolution. |
| `apps/build.sh` | Application Makefiles are discovered automatically. Runtime assets currently stage as flat per-application files. |

## Phase 1: Reproducible Runtime Build

Suggested structure:

```text
apps/lua/
  Makefile
  LICENSE
  README.md
  UPSTREAM.md
  vendor/lua/             # Pinned upstream source and notices
  include/lua_tabos/      # Local configuration and adapter contracts
  src/main.c
  src/console.c
  src/runtime.c
  src/libraries.c
  src/module_tabos.c
  examples/hello.lua
  examples/files.lua
  examples/system.lua
docs/lua.md
```

Build the VM and selected libraries into the application using the shared SDK Make rules. Use a thin application driver based on upstream CLI behavior; avoid compiling both that driver and upstream `lua.c`, or accidentally including `luac.c`. Enumerate sources explicitly rather than compiling every `.c` file in the archive.

Use generic ISO C configuration. Disable POSIX/Linux/macOS feature switches, dynamic loading, readline discovery, and signal-based interruption. Host-native tests must use the same TabOS Lua configuration; host platform detection must not enable extra libraries accidentally. Review [upstream CLI](https://www.lua.org/source/5.5/lua.c.html) when separating reusable CLI behavior from desktop integration.

Link newlib math support after objects/sources. The current shared Make rule has no trailing application-library variable; if needed, add a narrowly scoped `TABOS_LDLIBS` facility, use `-lm`, and include its value in effective-build configuration tracking. Do not put libraries before their references or bypass shared dependency/metadata rules with an unrelated build system.

First proof: build and run `lua -v` and `lua -e 'print(1 + 2)'` through the real RV32 loader. Check integer operations, floating arithmetic, `setjmp`/`longjmp` error unwinding, math linkage, ELF size, and metadata before expanding the application. No public TabOS ABI change should be required for this proof.

## Phase 2: CLI and Standard-Library Contract

Initial commands:

```text
lua
lua -v
lua -e 'print(1 + 2)'
lua script.lua first second
lua -i script.lua
lua -l module script.lua
lua -- filename-starting-with-a-dash.lua
```

Preserve supported option ordering, `arg`, script varargs, and exit-status behavior against the selected upstream CLI. No-argument invocation enters the REPL. `-i` keeps the state after a successful script. Defer `lua -` and redirected/piped input until an explicit console-EOF/stream contract is supported; return a clear unsupported-option error rather than waiting for an EOF that cannot arrive. Provide `--help` describing the TabOS profile.

Start standard libraries from an explicit registration table instead of blindly calling `luaL_openlibs` and deleting functions afterward. Compile-time adaptations must also remove unsupported symbol references from registered libraries.

| Library or operation | Initial contract |
| --- | --- |
| Base, table, string, math, coroutine | Preserve ordinary upstream behavior, subject to text-only loading and documented resource limits. No `string.dump` initially. |
| UTF-8 library | Keep byte-processing helpers. They do not convert the CP437 console into a Unicode terminal; UTF-8 file bytes remain usable as data. |
| `io.open`, file read/write/seek/flush/close, file-based lines | Support through newlib after actual cross-target validation. Preserve binary data including NUL in files. Test file modes and error returns. |
| Console `io.read`, `io.stdin:read`, `io.lines()` | Initially support line forms `l`/`*l` and `L`/`*L` through the shared console broker. Return an explicit error for unsupported console read formats. Ordinary file read formats remain upstream-compatible. |
| `io.popen`, `io.tmpfile` | Unsupported initially; no shell pipelines or unverified temporary-file lifecycle. |
| `os.remove`, `os.rename`, `os.difftime` | Support via validated filesystem/time behavior; preserve Lua-style error results. |
| `os.time`, `os.date` | UTC-only adaptation using the portable clock contract. Validate table conversion, ranges, and supported format specifiers. Do not inherit workstation timezone. |
| `os.exit` | Map status to application exit; ensure owned Lua/TabOS resources and TTY state are cleaned according to the documented TabOS profile. |
| `os.clock` | Unsupported until CPU-time semantics exist; provide monotonic milliseconds separately. |
| `os.execute`, `os.tmpname`, locale changes | Unsupported initially. Do not emulate these with shell parsing, guessed temporary paths, or global locale mutation. |
| `os.getenv` | Return nil for absent environment variables. Do not expose the host environment or pretend shell PATH is a general inherited environment. |
| `package` | Preloaded modules and pure-Lua source modules only. No native loader searchers; empty `package.cpath`; explicit unsupported result for `package.loadlib`. |
| `debug` | Omit the public library initially. Internal traceback generation remains available through the C API; user hooks must not replace interruption handling. |

Document exact unsupported-function behavior and test it: use conventional nil/message/error-code tuples for OS failures and a clear Lua error for unsupported language-facing operations. Keep it consistent across all targets. This profile is a usable Lua port with explicit differences, not a claim of full desktop standard-library compatibility.

## Phase 3: Pure-Lua Modules and Files

Proposed default `package.path`:

```text
./?.lua;./?/init.lua;T:/lib/lua/5.5/?.lua;T:/lib/lua/5.5/?/init.lua
```

Relative entries use the process working directory; do not silently change cwd to the script directory. A script may explicitly adjust `package.path`. Resolve dotted module names using ordinary Lua conventions and retain `package.loaded`/`package.preload` behavior. System library directories may be absent and require no creation during startup.

Do not consult `LUA_INIT`, `LUA_PATH`, or `LUA_CPATH` from the workstation. Configure this at the Lua adapter boundary so the host and Tab5 behave identically. Reject embedded NUL in path arguments crossing into C APIs instead of silently accessing a truncated filename.

Ship examples through the existing flat runtime-asset mechanism into `T:/data/lua/`. Exercise nested pure-Lua module paths through test fixtures and documented manual installation. If bundled modules later require a directory tree, extend staging deliberately with build/install tests rather than assuming the current copier preserves directories.

## Phase 4: REPL, Interruption, and Resource Limits

### Console and keyboard ownership

Build one application-local console broker shared by the REPL, console I/O, and interrupt handling. It consumes cooked input events exclusively. Keep Aa/Sym text translation enabled, preserve unrelated TTY bits, and disable inherited scroll-key interception where Lua handles those keys. Restore the inherited mode on every exit path.

Provide prompt echo, Backspace, Enter, multiline continuation, expression-result display, and Ctrl-C/Ctrl-D exit from the REPL, including pending input and running interactive chunks. Ctrl-U cancels pending input and multiline chunks. Use bounded input buffers and report overlong input without executing a truncated command. The REPL also supports physical Backspace/Delete, Left/Right, Home/End, 16 session-local history lines with Up/Down and draft restoration, and horizontal scrolling. Ctrl-C/Ctrl-D exit continuation prompts with or without text. Interactive startup displays the upstream version/copyright banner; Escape has no exit action. Persistent history remains follow-up work.

If regular Lua `io` code would otherwise read console descriptors through newlib, route those console operations through the same broker. Cover `io.read`, `io.stdin:read`, console line iteration, and any explicit standard-input file handle. File reads remain ordinary stdio. Do not combine a hook that drains raw events with an independent `fgets(stdin)` loop.

### Running-script interruption

Install an instruction-count hook through the embedding C API. At bounded intervals it checks Ctrl-C and yields through TabOS services when needed. Buffer non-command text/navigation events for the console reader instead of swallowing them. Bound buffered typeahead, define an overflow policy with a visible indication, and keep Ctrl-C detectable even if the text buffer is full.

Apply the interruption policy to every coroutine, including newly created threads; verify hook inheritance rather than assuming only the main thread needs a hook. Prevent public debug hooks from replacing it. Never yield a Lua coroutine across a non-yieldable C boundary; an OS scheduling yield and a Lua coroutine yield are different operations.

Run chunks through protected calls, report tracebacks, and return to a usable prompt after ordinary errors. In script mode, report failure to stderr and return nonzero to the shell. A script can catch errors, and a long C-library call does not execute VM hooks: this is cooperative cancellation, not a hard execution limit or sandbox. Document and test the limits; do not advertise guaranteed interruption of arbitrary hostile code.

Use the proposed shared keyboard wait-source work from the Kilo plan for idle REPL/console waits. If absent, implement that narrow SDK service as a separately validated prerequisite. No busy redraw loop, fixed-frequency keyboard polling, or zero-item `tabos_wait` assumption. Measure count-hook cost on device and tune by elapsed monotonic time so scheduling does not impose a tick delay every few instructions.

### Memory and stack

Propose a 4 MiB application heap and 64 KiB stack initially, with a 3 MiB Lua-managed allocation ceiling. Reserve remaining heap for newlib, input buffers, diagnostics, and future adapter state. Treat these as measurement-driven starting values, not proven requirements.

Use a checked `lua_Alloc` wrapper with accurate realloc accounting, overflow checks, and allocation-failure semantics. Respect Lua's allocator contract for new allocations, shrinking, and freeing. Failed growth retains the original allocation; freeing must work during OOM recovery. Protect state initialization and library registration, not only script evaluation. Diagnostics must still work when allocating a traceback fails.

Set a conservative Lua C-call recursion limit appropriate to the measured native stack. Exercise parser depth, recursive Lua/C calls, pattern matching, garbage collection, and coroutine nesting; heap caps alone do not prevent C stack overflow. Keep input and scratch buffers off the stack where appropriate.

Close the Lua state and owned resources on ordinary exit. Handle finalizer errors and repeated initialization failures. Tab5 native faults remain device-fatal under the current architecture, so host sanitizer and RV32 validation are required before physical stress testing.

## Phase 5: Small TabOS Module

Add a statically registered `require("tabos")` module with a deliberately small CLI surface:

- `tabos.info()` returns copied portable system information.
- `tabos.monotonic_ms()` returns elapsed monotonic milliseconds as a Lua integer.
- `tabos.sleep_ms(ms)` validates a finite nonnegative duration and uses the existing cooperative sleep API, documenting its current yield-based implementation. Do not claim deep sleep or event-only timing.

Use Lua argument errors for wrong types/ranges and consistent nil/message/error-code results for service failures. Check signed/unsigned widths before calling the SDK. Avoid exposing pointers, private transport tables, FreeRTOS handles, or platform structs. Return owned strings/tables, not borrowed buffers.

This module should require no new public TabOS ABI beyond any separately implemented keyboard readiness support. Benchmark computation-heavy and allocation-heavy examples on hardware; host RV32 interpreter timing does not predict native Lua performance.

## Follow-On: Graphics, Input, Audio, and Process Bindings

Start these only after the CLI milestone passes. Each slice needs API documentation, a small example, deterministic host coverage, and physical validation where relevant.

| Slice | Initial design |
| --- | --- |
| Graphics | SDK-owned logical canvas, RGB565 primitives/blits, explicit present, explicit close. Validate dimensions and buffer sizes. Use packed buffers or userdata rather than Lua tables per pixel. |
| Input | Expose keyboard/pointer events through one ownership policy. Switching from REPL text to graphics input must not create competing consumers. Restore console mode on close/error. |
| Audio | Playback of bounded signed-16-bit PCM buffers with explicit backpressure handling and close. Avoid per-sample Lua callbacks and hidden resampling. |
| Foreground child execution | Wrap `tabos_exec(path, argv)` with validated argument count/byte limits and negative-error conversion. Parent Lua state remains loaded; child exit restores focus. Do not reinterpret it as `os.execute(command_string)` or a background job. |

Use full userdata with idempotent explicit close and appropriate finalization for resource-owning objects. Cleanup must survive Lua errors and process teardown; garbage collection is a fallback, not timely resource management. Bindings that can block must define cancellation and buffer lifetime before exposing the call. Keep later networking/camera/device extensions out of CLI completion criteria.

## Validation and Delivery

Use the [official Lua test suites](https://www.lua.org/tests/) matching the selected version as a source of language regression coverage. Run an audited applicable subset; record exclusions for missing OS features, native test modules, resource-heavy cases, and the documented TabOS profile. Do not claim the full upstream suite passes based on a version printout.

| Layer | Required evidence |
| --- | --- |
| Runtime/build | Exact vendor version, offline build, numeric configuration, correct library link order, metadata, ELF size, and header/config dependency rebuilds. |
| Native sanitizer tests | Allocator failures, registration/cleanup, argument conversion, line broker, typeahead, option parsing, module paths, NUL-path rejection, and recursion limits. |
| Language tests | Integer boundaries, floats/math, strings including NUL, tables, closures, metatables, errors, GC, coroutines, text loading, and rejected binary chunks. |
| Library/profile tests | File modes/read formats/seek/flush/close, UTC dates and time bounds, unsupported APIs, ignored host environment, module caching and search order. |
| Session tests | REPL echo/editing/multiline, syntax/runtime errors, Ctrl-C in loops and coroutines, console-read cancellation, Ctrl-D, OOM recovery, exit status, and restored TTY/cursor state. |
| Actual RV32 host integration | Launch from persistent shell, run scripts and `-e`, verify file bytes and stdout/status, test nested failures and repeat launches, then return to a usable parent prompt. |
| Physical Tab5 | Aa/Sym typing, Ctrl-C latency, microSD module/file access, stack/heap high-water usage, repeated launches, service responsiveness, and representative script performance. |

Keep native tests configured like the target and run under ASan/UBSan. Use temporary drive roots and deterministic clocks/events. Test files and example scripts must not modify user configuration. Model injected I/O failures separately from destructive hardware experiments.

Build through `./apps/build.sh` and shared Make rules with the pinned project toolchain; inspect `make -C apps/lua metadata`. Validate macOS/Linux Debug and Release host builds and Tab5 cross-builds through the existing project workflow. Follow the optional real-RV32 harness convention where separately built application artifacts are not prerequisites of ordinary CTest. New shared SDK APIs require maintained `apps/tester` coverage; Lua semantics remain application tests.

Suggested delivery sequence: pinned build and arithmetic proof; library/profile and module loading; bounded REPL/interruption; memory/error hardening and small TabOS module; cross-target acceptance and docs. Graphics/audio follow separately.

- [x] Pin upstream archive, verify checksum, retain license, and build the independent RV32 executable.
- [x] Implement and document supported CLI, standard libraries, source loading, and module paths.
- [x] Validate unified console input, idle waiting, coroutine interruption, and parent restoration.
- [x] Validate numeric semantics, memory limits, recursion, OOM handling, and file errors.
- [x] Add the small TabOS module and installable example scripts.
- [x] Pass native sanitizer tests, applicable upstream tests, and actual RV32 host integration.
- [x] Complete macOS Debug/Release and Tab5 Debug/Release builds; Linux excluded by user direction.
- [x] Write `docs/lua.md`; update `docs/README.md`, `docs/applications.md`, and any affected SDK/input documentation.
- [x] Update `agents/roadmap.md` when implementation starts and as validation completes; record accepted shared API changes in architecture/context/testing documents.

Outstanding physical acceptance and measurements are tracked in [CLI Acceptance on Tab5](#cli-acceptance-on-tab5).

CLI completion means a user can run a script with arguments, read/write files, require a source module, interact at the prompt, recover from ordinary errors or memory exhaustion, interrupt a cooperative running chunk, and return to the shell on host and Tab5. Successful compilation alone is insufficient.
