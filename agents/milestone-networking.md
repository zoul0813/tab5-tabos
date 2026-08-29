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
- [ ] Prove DNS, TCP, and UDP.
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

- [ ] Reserve future `[ipv4]`, `[ipv6]`, and `[dns]` sections for static addresses,
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
- [ ] Serialize configuration reads and writes so concurrent processes cannot
  corrupt `wifi.conf`.
- [x] If `T:` is removed after configuration loads, retain credentials in memory
  for current session but report that persistent configuration is unavailable.
- [x] Never place password in argv, shell history, boot report, serial logs, or
  normal status output.

### Portable Network Service

- [x] Add public `<tabos/network.h>` exposing Wi-Fi status and saved connect/disconnect.
- [ ] Extend public `<tabos/network.h>` with scan, interactive connect, DNS,
  bounded ICMP echo, and bounded BSD-like socket operations.
- [ ] Support IPv4 and IPv6, TCP and UDP, `socket`, `bind`, `listen`, `accept`,
  `connect`, `send`, `receive`, `shutdown`, options, and close.
- [ ] Use portable addresses and opaque process-owned handles. Never expose
  ESP-IDF, lwIP, POSIX handles, or native structures.
- [ ] Support blocking calls and nonblocking `EAGAIN`.
- [ ] Implement ICMP echo through portable network API rather than exposing raw
  sockets. Return resolved address, sequence, response size, round-trip time,
  timeout, and network errors.
- [ ] Close sockets and cancel pending work on normal exit, faults, and partial
  startup failures.
- [ ] Defer TLS/HTTPS, static addressing, custom DNS, mDNS, SoftAP, routing, and
  Bluetooth.

### Reusable Wait Foundation

- [ ] Add `<tabos/wait.h>` with process-owned sources, zero-time poll, finite
  monotonic timeout, infinite wait, and readable, writable, error, and hangup
  flags.
- [ ] Use wait sets instead of exposing `select()` or native polling.
- [ ] Cancel waits before process socket cleanup; stale handles fail safely.
- [ ] Add networking capability metadata and feature query.
- [ ] Preserve Application ABI v1. Append private ELF transport calls compatibly so
  old binaries continue working.
- [ ] Defer full device registry and unrelated hardware capabilities.

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
- [ ] Map host sockets to native macOS/Linux sockets.
- [x] Add external `netctl` application with:

- [x] `netctl status`
- [ ] `netctl scan`
- [ ] `netctl connect`
- [x] `netctl connect` (saved profile)
- [ ] `netctl connect --prompt`
- [x] `netctl disconnect`
  - `netctl forget`
  - `netctl autoconnect on|off`

- [ ] Make `netctl connect` prompt for SSID and hidden password. After successful
  connection, ask whether credentials should be saved.
- [ ] Never overwrite saved working credentials after failed connection.
- [ ] Make `forget` remove SSID and password while preserving other config sections.
- [ ] Restore terminal mode on success, error, interruption, and process cleanup.
- [ ] Make `netctl status` show transport/firmware state, connection state, SSID,
  signal strength, IPv4 and IPv6 addresses, prefix/netmask, gateway, DNS servers,
  autoconnect state, saved-config availability, and last failure. Never show or
  imply password content.
- [ ] Bound scan results, order them by signal strength, and show SSID, security,
  channel, and signal strength without exposing backend-specific records.
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

## Test Plan

- [x] Test missing drive/file, valid parsing, comments, escapes, malformed values,
  unknown-key preservation, version rejection, atomic replacement failure,
  `auto_connect` behavior, forget, and redaction.
- [ ] Test address conversion, DNS results, table exhaustion, stale handles,
  blocking/nonblocking behavior, wait readiness/timeouts/cancellation,
  reconnect, the three-attempt autoconnect limit, explicit retry cancellation,
  connection state transitions, and error mapping.
- [ ] Test host IPv4/IPv6 loopback TCP, UDP, listen/accept, DNS, partial I/O, peer
  shutdown, disconnect during wait, and reconnect.
- [ ] Prove process exit and fault close sockets, cancel waits, restore TTY mode,
  preserve parent resources, and leave foreground stack intact.
- [ ] Extend independently built RV32 tester to exercise DNS, TCP, UDP, nonblocking
  sockets, ICMP echo, wait sets, and cleanup.
- [ ] Test `netctl status` redaction and fields plus `ping` IPv4/IPv6 success,
  timeout, unknown host, disconnect, early interruption, summary, and exit
  status behavior.
- [x] Keep old ABI-v1 fixtures unchanged. Add architecture checks rejecting native
  networking headers above platform boundary.
- [x] Run macOS and Linux Debug and Release tests and Tab5 Debug and Release
  cross-builds.
- [x] Validate physical saved autoconnect, DHCP, and display/runtime progress; validate physical scan, interactive connection, and protocol coverage remains pending.

## Assumptions

- [x] Version 1 supports one saved Wi-Fi network profile.
- [x] Station mode only.
- [ ] DNS, TCP, and UDP define basic networking.
- [x] Saved credentials are optional.
- [x] `auto_connect` defaults enabled.
- [x] Autoconnect stops after three failed attempts.
- [x] Static IP and DNS fields remain reserved and uninterpreted.
- [x] Generic device registry remains deferred.
