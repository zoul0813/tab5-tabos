# Desktop and GUI Applications

The optional TabOS desktop runs as an ordinary RV32 application above the persistent
shell. Files, Calculator, Text editor and Canvas are separate processes using the
public GUI SDK. Physical Tab5 usability and memory acceptance remain pending.

## Build and Start

Run `./apps/build.sh build` to build the desktop and clients with the other bundled
applications. Install with the existing application installation workflow described
in [the SDK guide](sdk.md), then run `T:/bin/desktop` from the shell. Desktop does
not replace or automatically start instead of the shell.

## Windows and Input

Windows open maximized above the persistent 80-pixel bottom dock. Title controls
close, restore/maximize and minimize. Tap a dock task to restore and focus it. Drag
a restored title bar to move; drag the lower-right grip to preview resize, releasing
to apply. Failed allocation or upload retains the previous geometry and pixels.
Restored windows have a 320-by-400 minimum, including the 48-pixel title bar.

Touch controls work with single taps. Keyboard Tab/Shift+Tab traverses controls;
Enter/Space activates buttons. Ctrl+Tab switches windows, Ctrl+Q requests close and
Ctrl+Escape opens Files. Mouse hover and wheel events use the same pointer service;
wheel scrolls lists and moves the multiline editor caret through the document.
Text uses CP437 and a physical keyboard; no on-screen keyboard is provided.

Pointer motion is coalesced while delivery is pending. Clicks, wheel steps and keys
are retried in order when an application is briefly busy. If the bounded pending
queue fills, the desktop cancels held input and reports that the application is
not consuming input. Closing an application does not produce a queue-full dialog.

## Included Applications

- **Files:** starts at `T:/bin`. Enter a directory and tap Go, use Up or Apps,
  select a row, then Open/Run. Directory listings are bounded by the public listing
  service and 128 displayed entries. The scrollbar and keyboard navigate the list.
  Extensionless executables are inspected before launch. Documents are opened from
  Text editor; file associations are deferred.
- **Calculator:** touch keypad or type an expression. Supports signed decimal
  numbers and `+ - * /`, with multiplication/division precedence. Enter evaluates
  a focused expression. Invalid syntax, nonfinite results and division by zero
  report errors. Parentheses and scientific functions are outside this calculator.
- **Text editor:** File menu, Open, Save and Save as; Ctrl+N/O/S shortcuts. Edits up
  to 32767 bytes of CP437 text. Arrow keys, Home/End and Ctrl+Home/End move the
  visible caret; long lines wrap. Open rejects oversized/binary files without
  replacing current text. Dirty New/Open/Close asks Save, Cancel or Discard.
  Save stages a complete file, backs up an existing destination and rolls back on
  replacement failure, supporting FAT storage. Failed save retains dirty text.
  If rollback itself fails, the status names the backup and edited-copy suffixes;
  preserve these files for recovery. This is not a power-loss durability guarantee.
- **Canvas:** retained colored sketch, Clear, Ink and Grid controls. Strokes scale
  on resize. Run hello demonstrates returning from a fullscreen program with the
  same drawing retained.

## Fullscreen Programs and Recovery

The SDK-generated GUI marker routes marked programs to independent windows.
Unmarked programs retain the normal fullscreen/console launch path. Desktop closes
launch admission and asks every session member, including descendants, to reach a
safe point. If a client cannot pause within two seconds, launch is cancelled and
the desktop reports a blocking PID. Close that client and retry.

While fullscreen code runs, the desktop and clients remain resident and parked.
They resume after the entire nested foreground chain returns. Memory is not saved
to disk or released to reserve space for a game. Actual allocation failure returns
to the desktop with a close-apps-and-retry message.

Exit on the dock closes clients in order and returns to the shell. Unsaved-work
Cancel aborts desktop exit. Request close again for an unresponsive client to get
an explicit Force close warning. Recoverable desktop failure tears down its
descendants and restores the shell. Arbitrary native Tab5 faults can still require
system recovery; this is not native process isolation.

## Resource Limits and Validation

Surface storage and staging use provisional configurable limits: 12 MiB aggregate
and 6 MiB per process, with eight desktop windows. Client canvases, executable heaps,
desktop composition buffers and scanout are additional memory. These limits do not
reserve game memory and are not yet selected from physical Tab5 measurements.

Dedicated macOS unit/component tests exercise real RV32 desktop/client execution,
pixels across resize/handoff, editor save and close cancellation, coherent surfaces,
input routing and process cleanup. These tests do not establish physical touch
quality, input latency, PSRAM headroom or game performance on Tab5.

### SDK Service Validation

Run `tester --concurrent` from the shell to exercise concurrent processes and GUI
session services. Its IPC checks include listener readiness, finite channel waits,
control delivery after peer close, hangup readiness and stale wait-source rejection
after channel reuse. The command runs three rounds and reports failed assertions.

The same command checks rejected surface dimensions, rectangles and null buffers,
verifies failed uploads preserve committed pixels, and confirms surface read grants
become invalid after their owning child exits. It checks allocation cleanup as well.
