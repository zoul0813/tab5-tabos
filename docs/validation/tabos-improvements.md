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
