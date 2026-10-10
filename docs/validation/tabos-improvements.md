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
