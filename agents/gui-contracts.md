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

Implemented channel contract:

- A foreground non-root process opens a session before launching clients. Its
  concurrent descendants inherit the session ID; a synchronous fullscreen child
  starts outside it. Only the session owner may listen, and only session members
  may connect. Accepted channel pairs grant each peer private send/receive access.
- The service has 32 generation-tagged endpoint slots. One listener exists per
  session with at most eight pending accepts. Each channel endpoint has eight data
  messages and two independent control messages; each message has 224 data bytes,
  kind, transition token and an OS-filled sender PID. Unused data bytes are zeroed.
- Send and receive copy whole messages. Empty receive and saturated send return
  EAGAIN. Control receives precede ordinary messages; saturation of one client's
  data cannot occupy its control capacity or another client's channel. Full control
  capacity requires bounded retry by the lifecycle coordinator.
- Closing preserves messages already delivered into the peer queue and exposes
  level-triggered hangup. After queued messages drain, receive returns EPIPE.
  Listener close disconnects unaccepted peers; accepted channels remain independent.
  Owner cleanup closes all owned endpoints. Stale/foreign handles return EBADF.
- Generic wait sources report readable accept/messages, writable data capacity and
  peer hangup. Source lookup rechecks endpoint generation on every poll; close
  invalidates the owning source. Existing host/native wait cancellation applies.
- All table, grant and queue operations serialize through a platform mutex. Handles
  never wrap to alias an earlier generation; generation exhaustion refuses allocation.
  APIs expose no native queue handles or cross-process pointers.

## Retained RGB565 surfaces

Prototype implemented; physical resource gate remains open:

- Sixteen generation-tagged surfaces support nonzero dimensions up to 1280 by 720.
  Each owns one zero-initialized committed RGB565 image. An upload transaction
  lazily allocates a full-size staging copy, applies copied rectangles there and
  swaps images atomically at commit. Commit frees the previous image immediately;
  no permanent full-size back buffer remains between transactions.
- Service reads and commits serialize under a platform mutex. Each copied rectangle
  read observes one complete committed revision. A compositor that needs coherent
  whole-window pixels reads that rectangle in one call. No raw buffer escapes.
- Upload bounds and multiplication are checked before copying. Failed service
  uploads abort staging; SDK wrappers also abort on errors while preserving errno.
  Explicit abort releases staging. Neither failure nor abort changes committed pixels.
- Surface owner grants read/info access through a live IPC channel to its peer.
  Read grants do not allow mutation or release. Owner exit frees all committed and
  staging images; reader exit revokes grants. Generations prevent stale reuse.
- Resize creates a replacement while retaining the old image and geometry; desktop
  adopts replacement only after a valid commit. Failed replacement leaves old state.
- Configurable provisional budgets include committed and staging allocations:
  `TABOS_GUI_SURFACE_BYTES=12582912` aggregate and
  `TABOS_GUI_PROCESS_SURFACE_BYTES=6291456` per owner. Stats expose used/peak/limits.
  These bound the prototype; they are not physically measured production limits.
  Client heaps/canvases, compositor, scanout, executable and OS memory remain separate
  consumers and must be measured together before declaring hardware feasibility.

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
