# GUI Service Contracts

Status: implementation contracts, 2026-09-12. These specify the GUI successor to
foreground-only execution; they do not claim that every service is implemented.
Track implementation and evidence in [GUI tasks](apps/gui.md).

## Process lifecycle

- Runtime owns process-table mutations. Native gates publish copied requests and
  consume replies through release/acquire synchronization; they never launch or
  destroy another process on an application task.
- Spawn returns the actual positive PID after loading succeeds. Paths, arguments,
  working directory and TTY policy are copied before return. A failure returns a
  negative TabOS error and leaves no child or uncollectable status.
- Spawn retains the runnable parent and grants no console, raw keyboard, pointer
  focus or display ownership to the child. Synchronous exec retains its existing
  foreground transfer and reverse-unwind semantics. Shell uses synchronous exec.
- PIDs increase monotonically within a boot and never alias a previous child.
  Refuse allocation before reaching the invalid sentinel or signed SDK PID limit.
  The fixed 16-slot table includes unreaped children; exhaustion is a clean error.
- Exit stops execution before reclaiming resources. An asynchronous child retains
  only its identity, parent and exit status until its parent reaps it exactly once.
  Wait on a live child is pending; a foreign, absent or already-reaped PID returns
  ECHILD. Child-exit readiness is level-triggered until reaping.
- Parent exit terminates descendants from leaves upward and discards their exit
  records. PID 0 termination always panics. Session-owner failure tears down its
  entire descendant tree, including an active fullscreen handoff.
- Host dispatch visits a snapshot of runnable PIDs in rotating order, at most one
  bounded interpreter slice per PID per pass. New children wait for the next pass;
  exited/reused slots cannot receive stale work. Runtime deadlines take the minimum
  across runnable contexts. Native tasks retain per-task caller identity and gates.

## GUI sessions and ownership

- Desktop enters a session while it owns the foreground console/display. GUI
  descendants inherit its session identifier; membership is not a privilege.
  Focus and display grants remain separate from process execution state.
- Desktop routes copied normalized input to focused windows. Background apps may
  use ordinary filesystem/timer/services, but cannot consume the raw input stream
  or open fullscreen graphics. Foreground grants are checked by each service.
- Pause and shutdown close admission before enumerating members. New descendant
  launches fail during these transitions. The coordinator remains runnable.
- Fullscreen launch uses a distinct foreground child chain outside the paused
  client set. It inherits ordinary resource limits, never GUI surface accounting.
  On unwind restore the retained desktop, clear stale input and resume clients.
- Pause uses a monotonically increasing transition token, bounded control events
  and a two-second absolute deadline. Acknowledgements name the token. Late replies
  cannot park clients after rollback. Exit counts as removal, not a pending reply.

## Copied IPC

- Endpoints and grants use generation-tagged positive handles; callers never pass
  kernel pointers. Endpoint owner alone receives/closes; granted peers may send.
  The kernel supplies sender identity and copied payload length.
- Fixed endpoint/message capacities bound memory. Data send returns EAGAIN when
  full. Receive copies one complete message or leaves it queued if the destination
  is too small. Empty receive returns EAGAIN; stale/foreign handles return EBADF.
- Lifecycle state has separate bounded storage per admitted peer. Close, pause,
  resume and disconnect cannot be crowded out by ordinary input/damage messages.
  Transition tokens permit coalescing superseded state without dropping the
  current required acknowledgement. Repeated control sends cannot allocate memory.
- Discovery publishes the desktop endpoint only within its session. Client endpoint
  handoff explicitly grants the desktop reply access; names alone grant no access.
- Generic waits report readable data/control and peer hangup. Closing an endpoint
  revokes grants, wakes waiters and publishes disconnect without relying on space
  in the ordinary queue. Teardown closes all owned endpoints and grants.

## Retained RGB565 surfaces

- Create validates nonzero dimensions and overflow before allocation. Owner uploads
  copied rectangles; an explicit compositor grant permits copied rectangle reads.
  Grants never expose raw cross-process buffers. Release invalidates all handles.
- Committed pixels remain immutable to readers during an upload transaction.
  Uploads accumulate in bounded staging; commit validates and applies the whole
  transaction under service synchronization. Failure/abort leaves committed pixels
  unchanged and releases staging. Reads serialize with commit; no partial frame.
- Resize creates a replacement surface while preserving the old one. Desktop
  adopts the new geometry only after a valid committed replacement arrives. Failed
  allocation/upload/close cancellation keeps the old window usable.
- Initial implementation must measure alternatives before fixing quotas: bounded
  rectangle staging, tiled copy-on-write, and a temporary full replacement. Account
  for simultaneously live SDK canvas, staging, retained pixels and resize overlap.
  No assumption of a permanent full-size back buffer per client is authorized.

## ELF launch marker

- Extend the existing TABOS SHT_NOTE metadata with a versioned GUI launch flag;
  SDK emits it from an explicit application Makefile setting, default off.
- The public pre-execution query uses the normal bounded ELF inspection path.
  Missing legacy metadata means console/fullscreen. Unknown versions, duplicate
  records, reserved-bit misuse and malformed sizes fail explicitly.
- Launch metadata is policy information only. It does not grant IPC, display,
  memory, device or process privileges and does not bypass normal load validation.

## Resource feasibility and restricted validation

A 1280 by 720 RGB565 buffer is 1,843,200 bytes. With a provisional 80-pixel dock
and 48-pixel title bar, maximized content is 1280 by 592, or 1,515,520 bytes.
Four such canvases plus four retained surfaces cost 12,124,160 bytes before
staging, resize overlap, compositor/scanout, executables, stacks and OS services.
DOOM requests an 8 MiB heap; Starfall requests 1 MiB. These are requested limits,
not measured live use or proof that concurrent resident launch fits 32 MiB PSRAM.

The user prohibits Linux builds/tests, flashing Tab5, and running/controlling host
sim for this implementation. macOS/Tab5 builds and macOS automated test suites are
allowed. Physical PSRAM peaks, upload/composition timing, display revisions and
finger usability remain pending. Analytical estimates and portable benchmarks must
not be relabeled physical measurements. Resource prototypes may proceed, but the
Phase 0 physical resource gate and final hardware acceptance remain open.

## Implementation audit

- `process/process.c`: 16 slots, foreground-only updates/exit handling, one nested
  stack; identity helpers incorrectly tied to foreground for concurrent execution.
- `loader/elf_application.c`: copied exec request mailbox already crosses native
  task/runtime boundary; per-image descriptors, waits, sockets, heap and directory.
  Entry currently requires a console; raw graphics-open also needs ownership audit.
- `sdk/lib/process.c`: spawn returns synthetic PID 1 and saves borrowed arguments;
  wait re-enters exec. Replace both wrappers; preserve synchronous exec explicitly.
- `platform/host/sdl/executable.c`: each context retains CPU/gate continuation;
  current caller is set around runtime-thread gate dispatch. No host worker may
  dispatch guest gates concurrently using that global caller variable.
- `platform/esp32p4/application_task.c`: task-local context and guarded native
  gates already enforce stop-before-free. New gates must extend the generated list.
- `console/console.c` and pointer service: token/owner checks protect input;
  fullscreen graphics close currently returns terminal, requiring owner restoration.
- `sdk/crt/metadata.S`, `loader/elf_loader.c`: existing bounded resource note and
  validation are the marker integration point. SDK and host/native private gates
  must evolve together; bundled binaries rebuild under the pre-release ABI policy.
- Architecture section 5.2 still describes historical 1 MiB default and metadata
  as future work; current loader configuration and SDK metadata are authoritative.
  Historical foreground-only statements remain scoped to synchronous execution
  until the concurrent implementation is verified.
