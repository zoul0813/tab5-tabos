# RTC and Wall-Clock Milestone

## Goal

Provide one portable UTC wall clock across host targets and Tab5, backed by the
Tab5 RX8130CE RTC. Expose calendar and Unix-epoch access to applications while
preserving the existing monotonic clock for elapsed-time measurement.

## Status

The wall-clock service, `cal`, and graphical `clock` applications are implemented.
On 2026-08-30, `apps/build.sh` produced and installed both as `T:/bin/cal` and
`T:/bin/clock`. Maintained tester validation on physical Tab5 covers detection,
calendar read, and unchanged-value write. Restart retention remains to be checked.

## Decisions

- [DECIDED] Wall-clock values use signed Unix seconds since
  `1970-01-01 00:00:00 UTC`.
- [DECIDED] The hardware RTC stores UTC. TabOS kernel and RTC drivers do not
  apply timezones or daylight-saving rules.
- [DECIDED] Public calendar values contain year, month, day, weekday, hour,
  minute, and second. Years use the full Gregorian year.
- [DECIDED] Applications receive `tabos_clock_get()` and `tabos_clock_set()` in
  addition to standard C/POSIX-style `time()`, `gettimeofday()`, and
  `clock_gettime()` support.
- [DECIDED] `CLOCK_REALTIME` uses the RTC/wall clock. `CLOCK_MONOTONIC` retains
  the existing boot-relative monotonic service. Setting monotonic time is not
  supported.
- [DECIDED] The host backend reads the host UTC clock. Setting simulated RTC
  time changes a process-local host-backend offset and never changes the host
  operating system clock.
- [DECIDED] Tab5 uses the documented RX8130CE on the BSP internal I2C bus at
  address `0x32`. Missing or invalid RTC data is nonfatal and reported as an
  unavailable wall clock.
- [DECIDED] Initial scope excludes timezone databases, locale formatting,
  alarms, periodic RTC interrupts, and automatic host-to-RTC synchronization.
  Network synchronization is provided separately by the `ntpdate` networking
  utility, rather than by the RTC service.

## Checklist

- [x] Add validated Gregorian calendar and Unix-epoch conversion.
- [x] Add platform wall-clock read/write contract.
- [x] Implement host wall clock with safe simulated setting.
- [x] Implement Tab5 RX8130CE read/write backend.
- [x] Add wall-clock calls to the private ELF transport and host interpreter.
- [x] Add public SDK calendar functions and libc time hooks.
- [x] Add unit, host, and maintained tester coverage.
- [x] Add `cal`, a text calendar application using the current UTC wall clock.
- [x] Add `clock`, a standalone graphical UTC wall-clock application.
- [x] Build and install both applications through `apps/build.sh`.
- [x] Document API, UTC semantics, limitations, and hardware validation steps.
- [x] Validate RX8130CE detection, calendar read, and unchanged-value write on
  physical Tab5 hardware.
- [ ] Validate RTC time retention across a physical Tab5 restart.
