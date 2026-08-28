# TabOS Basic Wi-Fi Networking

## Summary

Deliver end-to-end ESP32-C6 Wi-Fi: optional saved configuration at
`T:/etc/wifi.conf`, volatile interactive connection, autoconnect policy, DNS,
dual-stack TCP/UDP sockets, host parity, application API, diagnostics, cleanup,
and tests.

Use official `esp_hosted` and `esp_wifi_remote` over 4-bit SDIO.

## Implementation

### Hardware Proof

- Enable Tab5 C6 power through BSP Wi-Fi feature.
- Configure SDIO slot 1: CLK GPIO12, CMD GPIO13, D0-D3 GPIO11/10/9/8, reset
  GPIO15.
- Add ESP-IDF 5.4.4-compatible `esp_hosted` and `esp_wifi_remote`.
- Query C6 firmware/version, join AP, obtain addresses, then prove DNS, TCP, and
  UDP.
- Use compatible factory C6 firmware. Never auto-flash or invent custom
  transport. Record exact blocker if official stack and factory firmware
  mismatch.

### Persistent Network Configuration

- Store optional INI config at `T:/etc/wifi.conf`.
- Use initial schema:

  ```ini
  version=1

  [wifi]
  ssid="network name"
  password="secret"
  auto_connect=true
  ```

- Reserve future `[ipv4]`, `[ipv6]`, and `[dns]` sections for static addresses,
  gateways, DNS servers, and related policy.
- Accept blank lines, comments, quoted escaped values, reordered keys, and
  unknown future keys and sections.
- Modify owned `[wifi]` keys while preserving comments and unknown content.
- Save atomically through same-directory temporary file, flush and close it,
  then rename it. Failed writes leave old config intact.
- Treat missing `T:`, missing file, malformed config, or failed autoconnect as
  nonfatal. Report useful error without printing password.
- Store password as plaintext because TabOS lacks secret storage, access
  permissions, or encryption. Document this clearly.
- Default `auto_connect` to `true` when key is omitted. `false` loads saved
  credentials but requires manual saved connection.
- Never place password in argv, shell history, boot report, serial logs, or
  normal status output.

### Portable Network Service

- Add public `<tabos/network.h>` exposing Wi-Fi status, scan, connect,
  disconnect, DNS, and bounded BSD-like socket operations.
- Support IPv4 and IPv6, TCP and UDP, `socket`, `bind`, `listen`, `accept`,
  `connect`, `send`, `receive`, `shutdown`, options, and close.
- Use portable addresses and opaque process-owned handles. Never expose
  ESP-IDF, lwIP, POSIX handles, or native structures.
- Support blocking calls and nonblocking `EAGAIN`.
- Close sockets and cancel pending work on normal exit, faults, and partial
  startup failures.
- Defer TLS/HTTPS, static addressing, custom DNS, mDNS, SoftAP, routing, and
  Bluetooth.

### Reusable Wait Foundation

- Add `<tabos/wait.h>` with process-owned sources, zero-time poll, finite
  monotonic timeout, infinite wait, and readable, writable, error, and hangup
  flags.
- Use wait sets instead of exposing `select()` or native polling.
- Cancel waits before process socket cleanup; stale handles fail safely.
- Add networking capability metadata and feature query.
- Preserve Application ABI v1. Append private ELF transport calls compatibly so
  old binaries continue working.
- Defer full device registry and unrelated hardware capabilities.

### Backends and Shell Behavior

- Map Tab5 service to lwIP over ESP-Hosted. Link and address callbacks never
  block runtime task.
- During boot, load config after `T:` mount. Autoconnect asynchronously when
  enabled; shell and other services start without waiting for network success.
- Map host sockets to native macOS/Linux sockets. Simulate TabOS Wi-Fi state
  without changing workstation Wi-Fi.
- Add external `netctl` application with:

  - `netctl status`
  - `netctl scan`
  - `netctl connect`
  - `netctl connect --saved`
  - `netctl disconnect`
  - `netctl forget`
  - `netctl autoconnect on|off`

- Make `netctl connect` prompt for SSID and hidden password. After successful
  connection, ask whether credentials should be saved.
- Never overwrite saved working credentials after failed connection.
- Make `forget` remove SSID and password while preserving other config sections.
- Restore terminal mode on success, error, interruption, and process cleanup.
- Show C6 initialization, saved-config availability, autoconnect result, link
  state, and addresses in boot report. Never show credentials.

## Test Plan

- Test missing drive/file, valid parsing, comments, escapes, malformed values,
  unknown-key preservation, version rejection, atomic replacement failure,
  `auto_connect` behavior, forget, and redaction.
- Test address conversion, DNS results, table exhaustion, stale handles,
  blocking/nonblocking behavior, wait readiness/timeouts/cancellation,
  reconnect, and error mapping.
- Test host IPv4/IPv6 loopback TCP, UDP, listen/accept, DNS, partial I/O, peer
  shutdown, disconnect during wait, and reconnect.
- Prove process exit and fault close sockets, cancel waits, restore TTY mode,
  preserve parent resources, and leave foreground stack intact.
- Extend independently built RV32 tester to exercise DNS, TCP, UDP, nonblocking
  sockets, wait sets, and cleanup.
- Keep old ABI-v1 fixtures unchanged. Add architecture checks rejecting native
  networking headers above platform boundary.
- Run macOS and Linux Debug and Release tests and Tab5 Debug and Release
  cross-builds.
- Validate physical scan, interactive connection, saved autoconnect after
  reboot, disabled autoconnect, DHCP, IPv6 reporting, DNS, TCP, UDP,
  disconnect/reconnect, bad credentials, missing SD card, and continued
  keyboard/display/timer/filesystem progress under traffic.

## Assumptions

- Version 1 supports one saved Wi-Fi network profile.
- Station mode only.
- DNS, TCP, and UDP define basic networking.
- Saved credentials are optional and plaintext.
- `auto_connect` defaults enabled.
- Static IP and DNS fields remain reserved and uninterpreted.
- Generic device registry remains deferred.
