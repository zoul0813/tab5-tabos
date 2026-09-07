# Kilo Implementation Plan

Status: proposed implementation plan, 2026-09-07. Implementation has not started.

This plan develops the Kilo candidate from [Application Port Candidates](../milestone-apps.md). Proposed API names, resource budgets, and feature choices below are implementation recommendations, not new `[DECIDED]` architecture requirements.

## Outcome and Scope

Provide an independently built `T:/bin/kilo` application for editing configuration, notes, and C source files on Tab5, macOS, and Linux. The host must execute the same RV32 artifact as Tab5. Kilo remains a terminal application using public TabOS services.

Initial command contract:

```text
kilo <path>
kilo --help
```

Require one filename initially. An absent file opens an empty buffer but is created only by Save. Resolve relative paths against the inherited working directory; support drive-qualified paths and shell-quoted filenames. Reject directories, invalid arguments, and genuine open/read errors without pretending they are new files.

Initial features:

- Insert text, split/join lines, Backspace, forward Delete, and tab insertion.
- Arrow movement, Home/End, Page Up/Down, and horizontal/vertical scrolling.
- Ctrl-S save, Ctrl-F incremental search, Ctrl-L redraw, and Ctrl-Q quit with explicit unsaved-change protection.
- Filename, modified flag, cursor position, help, and error messages in two reserved terminal rows.
- Existing C/C++ syntax highlighting, with safe handling of unfamiliar file extensions.
- ASCII/CP437 byte-oriented text consistent with TabOS.

Defer multiple buffers, undo/redo, selection, clipboard, mouse/touch editing, UTF-8 layout, syntax plugins, shell escapes, subprocess tools, unnamed buffers, and Save As. Do not introduce curses, SDL, a GUI, or general POSIX emulation to support this port.

## Evidence and Existing Boundaries

Use [architecture](../architecture.md), [context](../TABOS_CONTEXT.md), [testing](../testing.md), and [C coding style](../coding-style.md) as project requirements. Some historical prose has stale ABI and memory defaults; verify current headers, build rules, and loader limits before implementation.

Upstream is [antirez/kilo](https://github.com/antirez/kilo), licensed BSD-2-Clause. Its [source](https://github.com/antirez/kilo/blob/master/kilo.c) combines editing, terminal handling, search, highlighting, and file I/O. Relevant dependencies include `termios`, `TIOCGWINSZ`, cursor-report fallback, and `SIGWINCH`. Its save path uses `ftruncate` and a single write; allocation/error paths need hardening for bounded memory. These are porting areas, not supported TabOS contracts.

| Current repository surface | Consequence for implementation |
| --- | --- |
| `sdk/make/application.mk` | Use the existing C17 RV32I/`ilp32` build and generated metadata. Current defaults are 256 KiB heap and 16 KiB stack. |
| `sdk/include/tabos/tty.h`, `sdk/libc/syscalls.c` | `ioctl` supports GET/SET mode only; no terminal-size query exists. |
| `sdk/include/tabos/input.h`, `loader/elf_application.c` | Cooked mode still exposes physical key events alongside translated CP437 text. Raw mode suppresses text. Event reads and stdin consume the same queue. |
| `sdk/lib/input.c`, `loader/elf_application.c`, platform runtime backends | `tabos_input_wait()` retries polling through `sched_yield()`. Tab5 yield delays one RTOS tick; this is not an indefinite input-readiness wait. |
| `sdk/include/tabos/wait.h` | Generic waits already exist, but no keyboard-input wait-source adapter is exposed. |
| `graphics/terminal.c`, `tests/unit/terminal_ansi.c` | Current CSI parsing holds one numeric value and skips semicolons. It does not correctly implement two-parameter cursor positioning, private cursor visibility, or SGR 39. |
| `sdk/libc/syscalls.c`, `docs/filesystem.md` | File operations include create/exclusive open, write, close, and rename. Do not assume `ftruncate`, `fsync`, or identical replacement semantics across host and FAT. |
| `apps/shell/src/history_file.c` | Useful local pattern for recoverable save errors, but not proof of atomic replacement or power-loss durability. |
| `apps/build.sh` | Automatically discovers application Makefiles; adding `apps/kilo/Makefile` includes Kilo in ordinary builds and declared-output MSC copying. |

## Source and Build Integration

Select and record an exact upstream commit before importing code. Keep the upstream copyright and full BSD license, and record the source URL, revision, imported files, and local changes in `apps/kilo/UPSTREAM.md`.

Use a checked-in, attributed source port: the upstream editor is small, and ordinary application builds should not require network access. Keep initial import distinguishable from adaptation in review. Retain recognizable editing logic; split only where input, rendering, storage, or tests need a clear boundary. Follow project C style for maintained port code without creating an unrelated editor framework.

Suggested layout:

```text
apps/kilo/
  Makefile
  LICENSE
  README.md
  UPSTREAM.md
  include/kilo/editor.h
  include/kilo/input.h
  include/kilo/storage.h
  src/main.c
  src/editor.c
  src/input.c
  src/render.c
  src/storage.c
docs/kilo.md
```

Use `APP_NAME := kilo` and include `../../sdk/make/application.mk`. Produce `build/apps/kilo/kilo`, install to `.local/rootfs/T/bin/kilo`, and retain the unstripped ELF for debugging. No special build-script branch or runtime asset is expected. Include license notices in distributed application materials; verify the release packaging path rather than assuming source-tree inclusion covers binaries.

## Phase 1: Terminal and Input Prerequisites

Complete and test these reusable service changes before building the editor around them. Do not expose internal terminal structs or access the private ELF API table from application code.

### Terminal geometry and ANSI compatibility

- Propose `TABOS_TTY_GET_SIZE` with a copied TabOS-owned rows/columns structure through `ioctl`. Derive geometry from the active terminal's font, cell size, and scale. Validate descriptors and guest pointers, and use the existing foreground ownership rules.
- Wire the public SDK wrapper, private transport, loader gates, native guards, and host RV32 dispatch consistently. Follow the current pre-release ABI policy and rebuild bundled applications when transport changes.
- Add bounded CSI parameter parsing sufficient for `H`/`f` row-and-column positioning, omitted/default parameters, and multiple SGR parameters. Support `?25h`/`?25l` and SGR 39/49. Consume unsupported sequences without rendering their parameter bytes; reject or safely discard overflow and excessive parameters.
- Preserve existing single-parameter shell sequences. Test sequences split across writes, including a split private prefix and numeric parameter.
- Define cursor addressing against the live visible screen even after long shell output has filled scrollback. Inspect the existing `first_line`-based addressing and ring behavior rather than assuming VT100 semantics already hold.
- Prove exact-width rows and the bottom-right cell do not cause accidental scroll during editor painting. Prefer explicit row positioning and no final newline; adapt Kilo's renderer to the verified terminal wrapping contract. Change general wrapping only if required, with shell regressions.
- Let Kilo query size on entry and explicit redraw. Host window scaling does not change terminal cell geometry. Defer asynchronous terminal-scale changes while editing; do not add `SIGWINCH` emulation.

### Event-driven keyboard waiting

Prefer adding a foreground, process-owned keyboard wait-source adapter to the existing generic wait service. A proposed name is `tabos_input_wait_source()`; finalize naming against current SDK conventions before coding.

Readiness must observe the queue without consuming it. Enqueue must wake a blocked waiter without a lost-wakeup gap. Test foreground ownership, invalidation on teardown, stale handles, cancellation, finite deadlines, and host RV32 suspension. Reuse generation-tagged wait infrastructure and existing runtime input notifications; do not add an application timer loop or platform-specific calls to Kilo.

Kilo drains a bounded batch of events, redraws once when state changes, then waits. Use a finite timeout only for an actual pending status-message deadline, based on monotonic time. Key repeat and cursor blink remain OS services. Keep this prerequisite scoped to keyboard readiness rather than rewriting unrelated SDK wait functions.

## Phase 2: Editor Session and Input Adapter

Save the inherited TTY mode, then clear `TABOS_TTY_MODE_SCROLL_KEYS` and `TABOS_TTY_MODE_RAW_INPUT`, preserving unrelated bits. This disables shell scrollback interception while retaining cooked Tab5 Aa/Sym translation.

Consume `tabos_input_event_t` exclusively; do not also read stdin. Insert printable CP437 bytes from text events, including multi-byte event payloads. Handle navigation and commands from key-down events. Give Enter, Tab, Backspace, Delete, and Escape one handling path and ignore their duplicate control-text events. Use explicit unsigned-byte classification, not locale-dependent `isprint(char)` behavior.

Proposed key contract:

| Keys | Action |
| --- | --- |
| Arrows | Move; accept normalized repeats. |
| Home / End | Start / end of current logical line. |
| Page Up / Page Down | Move one editor page. |
| Ctrl+Left / Ctrl+Right | Home / End equivalents on Tab5. |
| Ctrl+Up / Ctrl+Down | Page Up / Page Down equivalents on Tab5. |
| Backspace / Delete | Delete before / at cursor, including line joins. |
| Ctrl-S / Ctrl-F / Ctrl-L | Save / find / redraw. |
| Ctrl-Q | Quit when clean; when dirty, display an explicit discard confirmation. |
| Escape | Cancel search or discard confirmation; preserve edits. |

Ignore auto-repeat for save, find, quit, and discard confirmation so a held shortcut cannot discard changes. An explicit dirty-quit prompt accepting a fresh Y press, with N/Escape cancel, is preferred to a repeat-count mechanism. Suppress any corresponding translated command text while the prompt transitions.

Use one cleanup path for normal return and startup/runtime errors. Restore TTY mode, default colors, and visible cursor; finish on a clean terminal screen and let the retained shell print its next prompt. Do not promise restoration of pre-editor screen contents or add alternate-screen support in this milestone. Ensure forced teardown cannot leave cursor visibility or attributes broken for the parent; fix that as reusable console/process cleanup if needed.

## Phase 3: Bounded Editing and Rendering

Keep one in-memory document with explicit cursor, viewport, and dirty state. Separate logical byte positions from rendered columns so tabs, horizontal scrolling, and search results map correctly. Start with a fixed, documented tab width; preserve tab bytes in files.

Provisional resource settings are a 2 MiB heap and 32 KiB stack, requested through existing metadata. Provisional document limits are 256 KiB input, 8,192 lines, and 16 KiB per logical line. These are ceilings, not a guarantee that all worst-case combinations fit: rendered tab expansion, highlight arrays, row metadata, and temporary edit allocations also count. Validate the budget with fixtures and adjust downward if necessary.

- Check size arithmetic, integer conversions, row counts, and all allocation results.
- Allocate replacements before mutating the live document. Failed inserts, splits, joins, search allocations, or render allocations must leave the document valid and dirty state accurate.
- Avoid recursive highlight propagation across many lines; use bounded iterative work.
- Keep document-sized buffers off the stack. Serialize saves incrementally instead of allocating a second full-document copy.
- Render only after a meaningful edit, navigation, prompt change, or redraw request. Batch output and handle short writes/errors; SDK console writes may internally split the buffer, so one application write is not a guaranteed atomic frame.
- Escape or visibly substitute control bytes in document text, filenames, and errors before terminal output. File content must never become terminal commands.
- Reserve two status rows, clip all content, and reject unusably small geometry cleanly. Test default 80×24 and other supported terminal scales.

## Phase 4: File Loading and Recoverable Saving

Read in binary mode with a bounded line reader; do not depend on unverified `getline` support. Load into temporary state and commit only after successful completion and close. Enforce limits while reading even if initial `stat` size is smaller. Treat only `ENOENT` as a new document.

Preserve LF versus CRLF and whether the final line has a terminator. Track per-line endings if mixed files are accepted; newly inserted line breaks use the first observed style, defaulting to LF for new files. Byte-identical save without edits is required, including trailing blank lines. Treat embedded NUL as unsupported binary input and reject it without writing; other control bytes must remain inert when displayed. Bytes above ASCII remain byte-preserving CP437, without implicit encoding conversion.

Save design:

1. Create a uniquely named sibling temporary file using exclusive creation. Bound filename/path construction and collision attempts; never overwrite a pre-existing temporary or backup file.
2. Stream document bytes, handling partial writes and no-progress/error returns. Check flush/close results before considering replacement. Keep original untouched if staging fails.
3. Verify host and Tab5 FAT replacement semantics. Prefer same-directory rename-over-existing only where the backend contract supports it safely.
4. If FAT cannot replace an existing destination, use a documented backup/rename transaction: move original to an unused sibling backup, move staged file to destination, and restore backup if installation fails. Verify collision behavior before using a reserved backup name. Never unlink the original as a precondition for saving.
5. If rollback fails, retain recoverable files and report their exact paths. Do not delete the only surviving original or edited copy. Preserve buffer and dirty flag on failed installation.
6. Clear dirty state only when the new destination is installed successfully. Backup deletion failure is a cleanup warning after a successful save, not silent success or loss of the saved state.

This provides recoverability from reported I/O errors, not a claim of crash-atomic FAT replacement or power-loss durability. The SDK does not currently promise `fsync`. Document residual backup recovery, extra disk-space requirements, and how replacement affects host hard-link identity/metadata. Do not broaden filesystem semantics silently to make the editor appear portable.

## Phase 5: Validation

Use deterministic synthetic events, temporary drive roots, and injected failures. Register native unit/component tests with the existing test build and run under ASan/UBSan. Test observable behavior and data preservation rather than merely mirroring helpers.

| Layer | Required coverage |
| --- | --- |
| Editor logic | Empty documents, insertion/deletion, split/join, tab positions, line/page boundaries, horizontal scroll, search next/previous/cancel, multiline highlighting, limits, allocation failures, high CP437 bytes. |
| Input adapter | Text plus key events insert once; Aa/Sym cooked input; multi-byte text; navigation repeats; held quit cannot confirm discard; Ctrl+Arrow reaches editor; parent scroll policy restored. |
| Storage component | Existing/missing files, relative/drive-qualified paths, spaces and long names, LF/CRLF/mixed endings, missing final newline, binary rejection, byte-preserving no-op save, short read/write, close failure, full storage, temporary collision, rename/rollback/cleanup failures. |
| Terminal/service regression | Split CSI, geometry, cursor visibility, ring/scrollback addressing, bottom-right behavior, colors, safe control-byte display, shell rendering, input readiness and cancelled waits. |
| Session component | Open, edit, search, failed save, successful retry, quit/discard, repeated launch, cleanup, and usable parent prompt through real terminal behavior. |
| Actual RV32 integration | Launch SDK-built Kilo as a shell child with a temporary `T:` root; inject edits and Save/Quit; verify bytes, exit status, parent continuation, TTY/cursor restoration, and repeat launch. Follow `tests/component/shell_rv32.c` harness conventions. |

Extend `apps/tester` for new public terminal-size and keyboard-wait APIs, keeping tests self-contained and platform-neutral. Application-specific editing tests belong with Kilo's host tests. Keep optional real-RV32 application harnesses separate from ordinary CTest if the existing suite does not build application artifacts itself.

Build and validate macOS and Linux Debug/Release and cross-build Tab5 through the project workflow. Use `./apps/build.sh`, `make -C apps/kilo metadata`, and the relevant `./tools/tabos <target> ...` commands with the pinned toolchain. Include incremental rebuild checks when Kilo headers, Makefile resource settings, or shared SDK headers change.

Physical Tab5 acceptance remains separate:

- Launch from shell after enough output to fill scrollback; verify correct editor positioning.
- Edit configuration and C fixtures with punctuation, Aa/Sym tap/hold, navigation, and repeated keys.
- Save, exit, reopen, then reboot and verify microSD contents.
- Exercise recoverable storage errors without sacrificing user files; do not make live card removal a prerequisite while that service remains unsupported.
- Try near-limit documents and repeated launch/edit/exit cycles; record heap/stack usage, responsiveness, and absence of watchdog or leaked-resource failures.
- Verify idle input waiting does not create periodic editor redraws or a busy application loop.

## Delivery and Completion

Implement in reviewable slices: terminal/input prerequisites; attributed application skeleton; editing/rendering; storage hardening; end-to-end integration and documentation. Record actual results after each slice rather than marking this whole plan complete after the first working screen.

- [ ] Record upstream revision, license, and local adaptation policy.
- [ ] Implement and validate required terminal geometry, CSI, lifecycle, and input-wait support.
- [ ] Build independent Kilo and validate editing, search, highlighting, and keyboard behavior.
- [ ] Validate bounded memory, byte-preserving load/save, and recovery from save failures.
- [ ] Pass native sanitizer tests and actual RV32 host integration.
- [ ] Complete supported-target builds and separate physical Tab5 acceptance.
- [ ] Write `docs/kilo.md`; update `docs/README.md`, `docs/applications.md`, and relevant SDK/input/console documentation for implemented behavior.
- [ ] Update `agents/roadmap.md` when implementation starts and as slices gain validation; record accepted API decisions in architecture/context/testing documents as needed.

Completion means a user can edit a file, recover from an ordinary save failure, save successfully, return to a working shell, and reopen the expected bytes on host and Tab5. A compiling binary or a screenshot alone is insufficient.
