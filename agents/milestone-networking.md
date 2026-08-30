# TabOS Basic Wi-Fi Networking

## Summary

Deliver end-to-end ESP32-C6 Wi-Fi: optional saved configuration at
`T:/etc/wifi.conf`, volatile interactive connection, autoconnect policy, DNS,
dual-stack TCP/UDP sockets, host parity, application API, diagnostics, cleanup,
and tests.

Use official `esp_hosted` and `esp_wifi_remote` over 4-bit SDIO.

## Implementation

### Hardware Proof

- [x] Enable Tab5 C6 power through BSP Wi-Fi feature.
- [x] Configure SDIO slot 1: CLK GPIO12, CMD GPIO13, D0-D3 GPIO11/10/9/8, reset
  GPIO15.
- [x] Add ESP-IDF 5.4.4-compatible `esp_hosted` and `esp_wifi_remote`.
- [x] Query C6 firmware/version, join AP, and obtain DHCP address.
- [x] Prove DNS and ICMP with physical `ping`, TCP with the Tab5 listener and host
  `nc`, and UDP with the Tab5 listener and host datagram exchange.
- [x] Use compatible factory C6 firmware. Never auto-flash or invent custom
  transport. Record exact blocker if official stack and factory firmware
  mismatch.

### Persistent Network Configuration

- [x] Store optional INI config at `T:/etc/wifi.conf`.
- [x] Use initial schema:

  ```ini
  version=1

  [wifi]
  ssid="network name"
  password="secret"
  name="TabOS"
  auto_connect=true
  ```

- [x] Reserve future `[ipv4]`, `[ipv6]`, and `[dns]` sections for static addresses,
  gateways, DNS servers, and related policy.
- [x] Accept blank lines, comments, quoted escaped values, reordered keys, and
  unknown future keys and sections.
- [x] Modify owned `[wifi]` keys while preserving comments and unknown content.
- [x] Save atomically through same-directory temporary file, flush and close it,
  then rename it. Failed writes leave old config intact.
- [x] Treat missing `T:`, missing file, malformed config, or failed autoconnect as
  nonfatal. Report useful error without printing password.
- [x] Default `auto_connect` to `true` when key is omitted. `false` loads saved
  credentials but requires manual saved connection.
- [x] Permit an empty password for open networks.
- [ ] Serialize configuration mutations before adding `netctl` commands that
  write `wifi.conf`; current saved-connect/status operations only read it.
- [x] If `T:` is removed after configuration loads, retain credentials in memory
  for current session but report that persistent configuration is unavailable.
- [x] Never place password in argv, shell history, boot report, serial logs, or
  normal status output.

### Portable Network Service

- [x] Add public `<tabos/network.h>` exposing Wi-Fi status and saved connect/disconnect.
- [x] Extend public `<tabos/network.h>` with DNS, bounded ICMP echo, and bounded
  BSD-like socket operations.
- [ ] Add portable scan and volatile interactive-connect APIs.
- [x] Support IPv4 and IPv6, TCP and UDP, `socket`, `bind`, `listen`, `accept`,
  `connect`, `send`, `receive`, `shutdown`, nonblocking mode, and close.
Additional socket options are deferred until a concrete portable option set is
required.
- [x] Use portable addresses and opaque process-owned handles. Never expose
  ESP-IDF, lwIP, POSIX handles, or native structures.
- [x] Support blocking calls and nonblocking `EAGAIN`.
- [x] Implement ICMP echo through portable network API rather than exposing raw
  sockets. Return resolved address, sequence, response size, round-trip time,
  timeout, and network errors.
- [x] Close process-owned sockets during application cleanup, including failed
  child exits and partially initialized socket ownership.
- [x] Reject stale socket handles after a per-process slot is reused.
- [x] Interrupt pending blocking socket work and serialize worker teardown before
  releasing process-owned sockets.
TLS/HTTPS, static addressing, custom DNS, mDNS, SoftAP, routing, and Bluetooth
are explicitly deferred beyond this milestone.

### Reusable Wait Foundation

- [x] Add `<tabos/wait.h>` with process-owned sources, zero-time poll, finite
  monotonic timeout, infinite wait, and readable, writable, error, and hangup
  flags.
- [x] Use wait sets instead of exposing `select()` or native polling.
- [x] Cancel waits before process socket cleanup; stale handles fail safely.
- [ ] Add networking capability metadata and feature query.
- [x] Preserve Application ABI v1. Append private ELF transport calls compatibly so
  old binaries continue working.

The full device registry and unrelated hardware capabilities are explicitly
deferred beyond this milestone.

### Backends and Shell Behavior

- [x] Map Tab5 service to lwIP over ESP-Hosted. Link and address callbacks never
  block runtime task.
- [x] During boot, load config after `T:` mount. Autoconnect asynchronously when
  enabled; shell and other services start without waiting for network success.
- [x] Model network state as offline, starting, scanning, connecting, online,
  disconnecting, or failed. Preserve last failure for diagnostics.
- [x] Attempt autoconnect at most three times. Use bounded delay between attempts,
  then enter failed state without an endless retry loop. Explicit connect or
  reboot starts a new three-attempt sequence.
- [x] Make explicit disconnect cancel pending connection and retry work and suppress
  further autoconnect until explicit connect or reboot.
- [x] Simulate TabOS Wi-Fi state without changing workstation Wi-Fi.
- [x] Map host sockets to native macOS/Linux sockets.
- [x] Add external `netctl` application.
- [x] Add `netctl status`, saved-profile `netctl connect`, and
  `netctl disconnect`.
- [ ] Add bounded, signal-ordered `netctl scan` output with SSID, security,
  channel, and signal strength.
- [ ] Add `netctl connect --prompt` with hidden-password input, optional save only
  after success, and terminal restoration on every exit path.
- [ ] Add `netctl forget` and `netctl autoconnect on|off`; preserve unknown config
  content and never replace working credentials after a failed connection.
- [x] Make `netctl status` show connection state, SSID, signal strength, IPv4,
  hostname, attempt count, autoconnect state, saved-config availability, and
  last failure without exposing password content.
- [ ] Extend status data with transport/firmware state, IPv6 addresses,
  prefix/netmask, gateway, and DNS servers.
- [x] Show C6 initialization, saved-config availability, autoconnect result, link
  state, and addresses in boot report. Never show credentials.

### Ping Utility

- [x] Add `ping` under `apps/coreutils` as general external utility using public DNS
  and ICMP echo APIs only.
- [x] Support:

  - `ping <host>`
  - `ping -4 <host>`
  - `ping -6 <host>`
  - `ping -c <count> <host>`
  - `ping -W <timeout-ms> <host>`

- [x] Default to four requests with bounded timeout. Resolve names once, print
  resolved address, then show response bytes, sequence, round-trip time, or
  timeout for each request.
- [x] Print transmitted, received, loss percentage, and minimum/average/maximum
  round-trip summary. Return nonzero when name resolution fails, no response is
  received, or local network operation fails.
- [ ] Allow Ctrl+C to stop early, print partial summary, cancel pending echo, and
  restore input state.

### Socket Interoperability Utility

- [x] Add a host Python TCP/UDP echo service and standalone `apps/nettest`
  application that
  validates traffic across the physical Wi-Fi network.
- [x] Validate multi-call TCP transfer, orderly EOF, UDP payload, and UDP reply
  endpoint.
- [x] Validate TCP write-half shutdown across the physical C6 path; the host
  receives all queued bytes before EOF and the Tab5 receives the echoed stream.
- [x] Let Tab5 listen as a one-exchange TCP or UDP echo server for host `nc`
  interoperability checks.
- [x] Run TCP and UDP listener modes on physical Tab5 against host `nc`.
- [x] Run `nettest` client mode against an inbound-capable host.

## Test Plan

- [x] Test missing drive/file, valid parsing, comments, escapes, malformed values,
  unknown-key preservation, version rejection, atomic replacement failure,
  `auto_connect` behavior, forget, and redaction.
- [x] Test IPv4/IPv6 endpoint conversion through host TCP/UDP component coverage.
- [x] Add deterministic DNS result and error tests.
- [x] Add maintained tester coverage for socket-table exhaustion, accepted-socket
  allocation failure, and stale handles.
- [x] Run socket-capacity and accepted-socket failure coverage on physical Tab5.
- [x] Test blocking/nonblocking socket behavior, stale handles, cancellation of a
  blocked receive, socket error mapping, and three-attempt autoconnect state.
- [x] Test zero-time and finite wait readiness plus cancellation of an infinite wait.
- [x] Test reconnect, explicit retry cancellation, disconnect transitions, and
  remaining error paths.
- [x] Test host IPv4/IPv6 loopback TCP and UDP, including listen/accept and peer
  shutdown; test bounded multi-call stream transfer in `nettest`.
- [ ] Add host DNS, disconnect-during-wait, and reconnect tests.
- [x] Prove normal and failed child exit closes sockets, preserves parent
  resources, and leaves the foreground stack intact.
Recoverable application-fault cleanup remains deferred until Tab5 gains a
user-mode/PMP execution boundary; native application faults are currently device-fatal.
- [x] Extend the independently built RV32 tester with TCP, UDP, nonblocking,
  capacity, stale-handle, and cleanup coverage.
- [x] Extend the RV32 tester with zero-time and finite wait-set coverage.
- [ ] Extend the RV32 tester with DNS, ICMP echo, and infinite-wait cleanup coverage.
- [ ] Test `netctl status` redaction and all reported fields.
- [ ] Test `ping` IPv4/IPv6 success, timeout, unknown host, disconnect, early
  interruption, summary, and exit status behavior.
- [x] Keep old ABI-v1 fixtures unchanged. Add architecture checks rejecting native
  networking headers above platform boundary.
- [x] Run macOS and Linux Debug and Release tests and Tab5 Debug and Release
  cross-builds.
- [x] Validate physical saved autoconnect, DHCP, DNS/ICMP, TCP/UDP listener traffic,
  socket capacity, cleanup, and display/runtime progress.
- [x] Validate zero-time and finite socket waits with the RV32 tester on physical Tab5.
- [ ] Validate physical scan, interactive connection, IPv6, and `nettest` client mode.

## Assumptions

- [x] Version 1 supports one saved Wi-Fi network profile.
- [x] Station mode only.
- [x] DNS, TCP, and UDP define basic networking and have physical proof.
- [x] Saved credentials are optional.
- [x] `auto_connect` defaults enabled.
- [x] Autoconnect stops after three failed attempts.
- [x] Static IP and DNS fields remain reserved and uninterpreted.
- [x] Generic device registry remains deferred.
