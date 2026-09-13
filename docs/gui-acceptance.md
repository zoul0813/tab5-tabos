# Physical GUI Acceptance

Status: physical Tab5 verification pending/incomplete; to be tested later.
User reports GUI workflows manually verified on macOS host simulator on
2026-09-13. Continued development is proceeding without physical confirmation. Automated macOS evidence
and Tab5 compilation do not complete this acceptance. This procedure assumes a
Tab5 already running matching firmware and SDK applications; it does not provide
flashing or device-control instructions.

## Record the Test Setup

Record firmware commit, Debug/Release configuration, application build commit,
Tab5 Keyboard firmware/mode, microSD filesystem, GUI resource limits, and the
boot-reported display/controller revision. Keep results separate for each revision.

| Display / touch combination | Device identifier | Configuration | Result / evidence |
| --- | --- | --- | --- |
| ILI9881C / GT911 | Pending | Pending | Not run |
| ST7123 / ST712x v3 | Pending | Pending | Not run |
| ST7121 / ST712x v1 | Pending | Pending | Not run |

Use disposable documents with known contents. Preserve the run log and record
reproduction steps for each failure. A missing measurement is **pending**, not a
pass. Record observed performance without declaring acceptance against an
unagreed latency or memory-headroom threshold.

## SDK and Native Service Checks

1. From the shell, run `tester --concurrent`. Record the complete result. It covers
   concurrent children, wait/status, session pause/resume, IPC, surface grants,
   rejected transfers and cleanup through the public SDK.
2. Repeat the command and confirm it completes again with no failed assertions,
   watchdog reset or loss of shell input.
3. Record native task/gate cancellation and service-contention evidence separately.
   Passing this command does not exercise every cross-core wait/stop interleaving.
   Those cases require controlled native instrumentation; host fixtures alone
   cannot close this part of acceptance.

## Touch, Windows and Keyboard

Start `T:/bin/desktop` from the shell. Repeat the following on each available
controller revision and record the others as untested.

1. Open Files, Calculator, Text editor and Canvas with single taps. Check corner
   and edge coordinates, orientation, target spacing and text readability on the
   actual five-inch screen.
2. Restore, move, resize, maximize and minimize each window. Tap its dock task to
   restore it. Confirm the dock remains reachable and overlapping windows repaint
   without damaged or stale pixels.
3. In Canvas, draw near all four content edges. Drag outside the window and release,
   then draw again. Confirm no stuck stroke, accidental activation or lost capture.
4. In Files, navigate directories and scroll using touch controls. Confirm navigation
   does not depend on hover, right-click or double-click.
5. With the physical keyboard, enter known text in Editor. Exercise Tab/Shift+Tab,
   arrows, modifiers, Ctrl+Tab and Ctrl+Q. Verify text reaches only the focused
   client and characters are not duplicated.
6. Change focus during a held key or contact, then release it. Check that the new
   client receives no stale held input. Record device-removal/cancellation cases
   only when the relevant setup supports reproducing them.

## Fullscreen Return and Unsaved Work

1. Save a disposable Editor document, then append a distinctive unsaved suffix.
   Draw a distinctive Canvas stroke and leave a window restored at a recorded
   position and size.
2. Use Files/Apps to launch `hello`, then Starfall and Doom when their binaries and
   required game data are installed. Quit each program normally. Confirm the same
   windows, geometry, drawing and unsaved text return.
3. Repeat the cycle ten times as an initial endurance sample. Record the actual
   count, game/configuration and any input, display, audio or resource failure.
4. Request desktop Exit. Cancel Editor's unsaved-work prompt and verify desktop
   remains usable. Save the document and inspect its exact expected contents.
5. Make the disposable document dirty again. Request close, then request close
   again to reach the force-close warning. Cancel first; repeat and confirm force
   close. Check that control returns and the client resources are reclaimed.
6. Confirm orderly desktop Exit returns to the retained shell, which accepts a
   command and can launch desktop again.

Pause timeout, load failure and resource exhaustion need controlled native
fixtures for repeatable device evidence. Do not treat the desktop's rejection of
an invalid path before launch as evidence of recovery after display handoff.
Arbitrary native instruction faults may reset Tab5; host fault-containment tests
are not proof of native process isolation.

## Memory and Timing Record

Take measurements at the same points in every run. Include baseline and peak,
measurement method and units. The public surface statistics cover retained and
staging buffers only; they do not measure total PSRAM or game headroom. Current
12 MiB aggregate / 6 MiB per-process surface limits remain provisional.

| Checkpoint | Total / free / largest PSRAM block | Internal heap | Surface retained / staging / peak | Client heap / stack | Input and composition timing |
| --- | --- | --- | --- | --- | --- |
| Shell baseline | Pending | Pending | Pending | Pending | Pending |
| Empty desktop | Pending | Pending | Pending | Pending | Pending |
| One maximized client | Pending | Pending | Pending | Pending | Pending |
| Editor + Canvas, dirty document | Pending | Pending | Pending | Pending | Pending |
| Resize in progress and adopted | Pending | Pending | Pending | Pending | Pending |
| Increasing windows near configured limits | Pending | Pending | Pending | Pending | Pending |
| Starfall with GUI resident | Pending | Pending | Pending | Pending | Pending |
| Doom with GUI resident | Pending | Pending | Pending | Pending | Pending |
| After repeated close/handoff cycles | Pending | Pending | Pending | Pending | Pending |
| Returned shell | Pending | Pending | Pending | Pending | Pending |

Record compositor/scanout allocations and executable memory in addition to client
canvases and surface storage. Measure idle behavior and upload/composition time;
identify the instrumentation used. These measurements may require additional
native diagnostics; no existing shell command is implied by this table.

Compare equivalent idle checkpoints after each cycle for cumulative growth.
Use measured peaks and fragmentation to choose limits and buffering. Record the
rationale and repeat the affected checks after changing limits. Do not infer a
reserved game budget from a successful launch.

## Result Record

For each check record **pass**, **fail**, or **pending**, the device revision,
configuration, repetition count, observed behavior, log/capture location and issue
reference. Include unresolved measurement thresholds and instrumentation gaps.

Acceptance remains incomplete while required revisions or native recovery,
contention, memory and usability results are pending. See [GUI behavior](gui.md)
for the supported controls and current limitations.
