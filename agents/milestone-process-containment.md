# Process Ownership and Failure Containment Milestone

## Goal

Make process resource ownership explicit and guarantee deterministic cleanup on
normal return, requested exit, launch failure, and loader-detected execution
faults. Preserve the nested foreground process model: a failed child returns a
nonzero status and its parent resumes; a failed PID 0 enters kernel panic.

## Status

Deterministic process ownership and cleanup are implemented. Host RV32 guest faults
are contained as child-process failures; `unit.application_lifecycle` passed on macOS
on 2026-08-30. Native Tab5 execution still has no hardware memory-protection or
recoverable-fault boundary, so true native crash containment remains deferred.

## Decisions

- [DECIDED] Each filesystem ELF process owns its loaded image and executable
  mapping, platform execution context, application heap, descriptors above
  standard input/output/error, graphics session, copied arguments, working
  directory, and TTY policy.
- [DECIDED] Resource release is idempotent. Both lifecycle cleanup and the final
  application-data destructor may request it, so partial startup and ordinary
  teardown use one ownership path.
- [DECIDED] Child return, explicit exit, loader rejection, and execution faults
  detected by a platform backend unwind the child before console ownership and
  status return to the parent.
- [DECIDED] PID 0 is structurally different: its exit or failure retains the
  panic record and displays kernel panic rather than returning to a nonexistent
  parent.
- [DECIDED] The host RV32 interpreter validates guest addresses and converts
  invalid memory and instruction accesses into process faults.
- [DECIDED] ESP32-P4 native ELF tasks currently run with kernel privilege.
  FreeRTOS task separation is a scheduling and stack-allocation boundary, not a
  memory-protection boundary. An arbitrary native load/store/instruction fault
  may therefore invoke the ESP-IDF panic handler and reboot the device.
- [DECIDED] Do not claim Tab5 crash containment until applications execute in a
  less-privileged mode with a per-process PMP policy and a recoverable trap
  path. Existing ESP-IDF PMP configuration used to split instruction/data RAM
  is not process isolation.
- [DECIDED] Application heaps remain bounded arenas for ABI v1. Bounds failure
  returns allocation failure. Guard regions detect simple underflow/overflow at
  teardown, but guard detection is diagnostic and not a security boundary.

## Checklist

- [x] Audit process, ELF image, task, descriptor, heap, graphics, console, and
  nested-parent ownership.
- [x] Consolidate filesystem ELF resource release into one idempotent path.
- [x] Add heap boundary guards and teardown diagnostics.
- [x] Validate ELF resource metadata and enforce bounded per-process heap and stack
  requests, while retaining safe defaults for legacy applications.
- [x] Test child nonzero status, leaked descriptor cleanup, repeated reload, and
  nested parent restoration with the maintained tester application.
- [x] Test malformed/invalid guest accesses remain process faults on the host
  interpreter.
- [x] Confirm PID 0 failure still enters kernel panic.
- [x] Document public guarantees and current Tab5 native-fault limitation.
- [ ] Design user-mode/PMP execution and recoverable exception delivery before
  attempting true Tab5 native crash containment.
