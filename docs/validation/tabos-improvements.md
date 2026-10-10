# TabOS improvements validation

Baseline: main `aaa4d72`. Native apps must be rebuilt at each private
transport transition; final deployment and rollback use
matched firmware/application sets. Existing main-worktree audit changes are untouched.

## Runtime and SDK

macOS Debug sanitizer build passed. Nine focused tests passed: native task,
RV32 execution, power manager, architecture boundaries and build tracking.
All default SDK applications built. The actual RV32 shell/Kilo/tester integration
passed, including newlib fdopen rejection (EBADF), descriptor retention after
failure, successful reading and fclose ownership. No physical speed claim.

## Input recovery (transport 23)

macOS Debug sanitizer build and all 12 focused input/console/RV32/architecture
checks passed with local networking enabled. The added real SDL focus-loss and
window-close regression also passed. SDK wrapper tests cover null/missing gates,
error propagation and resynchronization; tester checks nested focus generations.

## Graphics (transport 25)

macOS Debug sanitizer build and 15 focused graphics/raster/display/native-task/
RV32/loader/architecture checks passed (loader fixture layout assertion updated
and rerun after regeneration). Tab5 Debug cross-build passed. Source review found
and fixed graphics teardown ordering: submitted readers are fenced before native
task destruction can free stack-backed borrowed sources. Existing blocking
present remains available; tester additionally exercises scaled submit/borrow/wait.
Physical latency, source lifetime and panel checks remain pending.

## Device tooling

Twelve focused host checks passed, plus eight transport/workload Python cases.
Normal Tab5 Debug firmware cross-build passed with test controls disabled.
General tester and graphics-benchmark workloads replace application-specific
workloads; screenshots are excluded from timed comparisons. Normal serial MSC
control blocks indefinitely; only test firmware wakes to expire injected keys.
Subsequent physical probing established an idle-shell test session.
Release firmware also cross-built with device-test controls enabled in its own
SDK configuration; standard board defaults were not changed.
A physical attempt exposed the runner's transient process-count launch race.
The runner now requires the next shell prompt before declaring completion;
a regression covers acknowledgement-before-launch, bringing Python cases to nine.

Three graphics-benchmark runs through the serial controller completed on the
installed development firmware (360 MHz actual/configured, 256 KiB L2). This
firmware has not been identified by a read-back hash and is not the main baseline
or the candidate built here, so these are tooling smoke measurements only.

| Run | Present only (60 frames) | Clear + present (60) | Scene (120) |
| --- | ---: | ---: | ---: |
| 1 | 1919 ms | 916 ms | 1834 ms |
| 2 | 1934 ms | 917 ms | 1833 ms |
| 3 | 1904 ms | 916 ms | 1834 ms |

Minimum free internal memory was 121735 bytes across these runs; largest free
block was 49152 bytes. Console capture, phase parsing, diagnostics and return
to shell succeeded. Raw records are in `/private/tmp/tabos-hardware-baseline-a1`
through `a3`. No firmware or application replacement was performed.

## Bounded compute (transport 26) and final integration

Native worker tests cover invalid callbacks/data, both signal-allocation failures,
task-allocation failure, completion acquisition, busy rejection until wait,
repeated jobs, forbidden SDK calls, and forced dual-task teardown. SDK wrapper
tests cover forwarding, error propagation and absent runtime/gates. The maintained
tester adds public compute checks, nested execution, and synchronous host fallback.

Final macOS Debug and Release builds with AddressSanitizer/UndefinedBehaviorSanitizer
each passed all 101 CTest checks (27.84 s and 27.42 s). This includes architecture,
application build tracking, input, graphics, loader, tools and compute checks.
The first Debug pass lacked SDKROOT for its nested build-tracking compiler; the
complete final pass supplied the installed SDK and passed. Network integration
checks ran with loopback access. All default SDK applications rebuilt for private
transport 26. Actual RV32 shell/Kilo/tester integration passed, including filesystem,
input and compute fallback. Normal Tab5 Debug and test-enabled Tab5 Release
cross-builds passed with isolated configuration files.

Application ABI remains 3. Soccer, Starfall and other existing applications keep
their blocking presentation and physical/text input APIs; rebuild them with the
final SDK. Async/borrowed graphics, logical input recovery and compute adoption
are optional. Deploy and roll back firmware and rebuilt applications as a matched
set. Hardware test controls remain off in normal firmware.

## Outstanding acceptance checks

- The initial GitHub Linux build failed because the SDK clock test did not
  request POSIX clock declarations under strict C17. Its target now defines
  `_POSIX_C_SOURCE=200809L`; check the updated PR for the subsequent CI result.
  No local Linux container runtime was available.
- Three matched main/candidate physical runs remain pending. The installed
  development-firmware smoke measurements above cannot establish a speedup or
  validate candidate firmware. Use identical settings, inputs and rebuilt
  application sets, record firmware/application hashes, omit screenshots during
  timing, and retain backups for rollback.
- Candidate hardware compute execution/teardown, borrowed source lifetimes,
  screenshot capture, MSC/reboot/upload/eject and flash acceptance remain pending;
  unit/integration tests and cross-builds are not substitutes for these checks.
- Panel-specific validation remains pending for ILI9881C, ST7123 and ST7121.

No experimental tick/cache-line configuration or stack-placement change was
selected. Existing main-worktree audit/documentation edits remain untouched.

## Reconciliation with upstream main

The five improvement commits were rebased onto reconciled main `a35b607`, which
preserves the eight local audit fixes and upstream Pool/Soccer additions. The
rebase retained all five improvement areas; the runtime/SDK commit also fixes
the Linux clock-test feature declaration noted above.

On the combined tree, macOS Debug and Release sanitizer suites each passed all
113 CTest checks (42.23 s and 28.03 s). All default applications rebuilt for
transport 26, including Pool and Soccer. No game source changes were required.
Both rebuilt games passed their actual RV32 gameplay harnesses, including
normal exit and restoration of the parent shell.
