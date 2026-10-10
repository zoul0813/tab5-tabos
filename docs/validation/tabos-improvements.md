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
