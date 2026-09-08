# Kilo Terminal Editor

Kilo edits one named file using the public TabOS terminal, keyboard, and filesystem
services. The host executes the same independently built RV32 image as Tab5.

```text
kilo T:/etc/wifi.conf
kilo "notes with spaces.txt"
kilo --help
```

Relative paths use the shell's inherited working directory. A missing file starts
empty and is created only by Save. Directories, binary files containing NUL, and
read errors are rejected. Use `./` before a filename starting with `-`.

## Controls

| Key | Action |
| --- | --- |
| Printable text / Tab | Insert CP437 bytes / a preserved tab byte |
| Enter | Split the current line |
| Backspace / Delete | Delete before / at the cursor, including line joins |
| Arrows | Move, with OS key repeat |
| Home / End | Start / end of the logical line |
| Page Up / Page Down | Move one visible page |
| Ctrl+Left / Ctrl+Right | Home / End on Tab5 |
| Ctrl+Up / Ctrl+Down | Page Up / Page Down on Tab5 |
| Ctrl-S | Save |
| Ctrl-F | Incremental search; arrows select next/previous match |
| Enter / Escape in search | Accept / cancel and restore the original view |
| Ctrl-L | Query terminal geometry again and redraw |
| Ctrl-Q | Quit; modified files require a fresh Y to discard |
| N / Escape at discard prompt | Keep editing |

Held save, find, quit, and confirmation shortcuts are ignored. Cooked Aa/Sym
translation remains enabled on Tab5; shell scrollback shortcuts are disabled for
the editor and the parent's policy is restored on return.

The bottom two rows show filename, modified state, logical byte position, and
help or status. Tabs display at four-column stops. Horizontal scrolling keeps long
lines editable. C/C++ files use Kilo's keyword, string, number, and comment colors;
other extensions remain plain text. Control bytes in files, names, and errors are
visibly substituted so they cannot issue terminal commands.

TabOS wraps immediately at the last column. Kilo deliberately leaves that column
blank and positions every painted row explicitly. The normal 80x24 terminal provides
79 editing columns and 22 document rows. Minimum terminal size is 20x4. Host window
scaling does not change cell geometry; asynchronous terminal-scale changes during
editing are unsupported. Exit clears the editor screen and returns to the retained
shell. Previous screen contents are not restored.

## Files and limits

- Maximum document: 256 KiB including line endings.
- Maximum logical rows: 8,192, including an empty row after a final newline.
- Maximum logical line: 16 KiB excluding its terminator.
- Requested application heap: 2 MiB; stack: 32 KiB.
- Search query: 255 bytes; one buffer, no undo, clipboard, selection, or Save As.

File bytes remain CP437/ASCII without Unicode conversion. LF, CRLF, mixed endings,
trailing blank lines, and absence of a final newline survive an unedited save exactly.
New line breaks use the first observed style, or LF for a new file. Existing standalone
CR and other non-NUL control bytes are preserved but displayed inertly.

Rows keep original bytes and one highlight byte per byte; tab expansion is computed
while drawing and does not allocate expanded document copies. Edits allocate affected
replacement rows before changing live state. Limit or allocation errors preserve
valid document state. Saving streams rows without a second document-sized buffer.

## Saving and recovery

Kilo exclusively creates a sibling `.kilo-N.tmp`, writes all data, and checks close.
If the destination exists, it reserves an unused `.kilo-N.bak` name, removes only the
empty reservation, moves the original to that name, then installs the staged file.
This works with host rename and the pinned FatFs backend's refusal to replace an
existing destination. Kilo never unlinks the original before installation.

Staging failures leave the original untouched. Failed installation attempts restore
the backup. A failed rollback retains both original backup and completed edited copy,
with their paths in the error message. Installation failure retains the completed
temporary copy even after successful rollback. Backup removal failure after successful
installation is reported as a warning; the buffer is then saved, not dirty.

The status row clips long messages to terminal width. The most recent recovery message is retained separately and
printed in full when leaving Kilo, including after a successful retry. Inspect
sibling `.kilo-*.tmp` and `.kilo-*.bak` files before removing them. Copy a chosen
recovery file to a different filename first, inspect it, then restore the intended
filename. A retry skips existing recovery files rather than overwriting them.

This transaction recovers from reported I/O errors; it is not crash-atomic and does
not guarantee power-loss durability. The SDK has no `fsync` contract. Keep enough
space for the original and edited file together; retained recovery copies need extra
space. Concurrent external writers are unsupported. On host filesystems replacement
creates a new file identity with staging permissions, so original metadata and
hard-link identity are not preserved. Other hard links retain the old contents.

## Build and validation

`./apps/build.sh` builds and installs Kilo automatically. `make -C apps/kilo metadata`
reports its resource note with the SDK toolchain activated. Installation places license
and provenance files under `T:/share/licenses/kilo/`; `--msc` copies installed license
materials beside application binaries. Current CI host and firmware archives do not
bundle applications. Any separate distribution of Kilo must include these notices.

Native sanitizer tests are registered as `component.kilo`; terminal and input service
regressions run in the ordinary macOS suite. With application artifacts built, run:

```sh
build/macos-debug/tests/tabos_kilo_rv32 build/apps/shell/shell build/apps/kilo/kilo build/apps/tester/tester
```

This optional harness uses a temporary drive and real SDK binaries to check editing,
search, saving, reopening, discard, parent continuation, and idle keyboard suspension.
Physical Tab5 functional acceptance was reported by the operator on 2026-09-07.
Quantitative heap/stack and idle-power measurements were not recorded. Linux host testing was explicitly
excluded from this implementation session by the project owner.
