# TabOS Wireless & Network Analysis Apps

Draft for `agents/wireless-apps.md`.

## Purpose

Define a family of TabOS applications for wireless/network **analysis and
test-assistance**, built on the existing portable TabOS kernel APIs. These are
userspace ELF programs under `apps/`, in the same mould as `apps/netutils`.

**Scope boundary (deliberate):** no raw radio access. No monitor mode, no raw
802.11 / BLE frame injection or capture, no promiscuous sniffing. Everything here
works at or above the socket layer. The portable network API may be **extended**
with new in-architecture, non-raw services (e.g. Wi-Fi scan, SoftAP), but raw
radio stays out of scope for now. This keeps the socket-only boundary from
`agents/TABOS_CONTEXT.md:628-638` (`[DECIDED]`) intact.

This excludes the offensive-firmware feature set (Bruce, Evil-M5Project, ESP32
Marauder, Ghost ESP, M5Stick-Nemo, MonsterC5): deauth, beacon spam, handshake
capture, RF replay, NFC/IR. Those need raw radio and/or hardware the Tab5 lacks
(Sub-GHz, NFC, IR). See `agents/monster-c5.md`.

## API surface available to apps today

From `sdk/include/tabos/`:

- `network.h` — DNS resolve, bounded ICMP echo, TCP+UDP sockets (`socket/bind/
  listen/accept/connect/send/receive/send_to/receive_from/shutdown`), nonblocking
  mode, local-endpoint query, Wi-Fi status (state/SSID/IPv4/signal/hostname).
- `tls.h` — process-owned, certificate-verified TLS client (connect/send/receive).
- `wait.h` — portable wait sets over socket/input/device sources, finite and
  infinite timeouts. Use for concurrency instead of `select()`.
- `device.h` — device registry (enumerate, `wifi0`, subscribe to events).
- `clock.h` / `time.h` — timing for RTT, timeouts, timestamps.

Caps worth noting: `TABOS_SOCKET_MAX = 8` concurrent sockets, `TABOS_TLS_MAX = 4`
TLS connections, `TABOS_WAIT_MAX = 16` wait items, `TABOS_NETWORK_IO_MAX = 1024`
per-call I/O. Station mode only; one saved Wi-Fi profile.

Existing reference apps: `apps/netutils/{ping,httpsget,ntpdate,nettest,fetch,irc,
netctl}`. Each is its own source dir + ELF, built via `make -C apps/netutils
<name>`, installed to `.local/rootfs/T/bin/`.

## Tier 1 — buildable on today's API (no kernel change)

Pure sockets / TLS / DNS / ICMP / wait. Each is an `apps/` program.

| App | Purpose | APIs used |
| --- | --- | --- |
| `portscan` | TCP connect scan of host:port-range; optional banner grab | sockets (nonblocking + `wait`) |
| `sweep` | Host discovery across a list/CIDR via ICMP echo and/or TCP probe | `tabos_network_echo`, sockets |
| `dig` | DNS lookup: A/AAAA, reverse, resolve timing | `tabos_network_resolve` |
| `httping` | HTTP(S) availability + latency + status/size over N requests | sockets / `tls.h` |
| `certcheck` | TLS cert inspection: expiry, hostname match, chain reachability | `tls.h` |
| `headers` | HTTP response header grab / server fingerprint | sockets / `tls.h` |
| `netcat` | TCP/UDP client+listener diagnostic (roadmap `[ ]`) | sockets, `wait` |
| `telnet` | Interactive plain-TCP terminal (roadmap `[ ]`) | sockets, `wait`, input |
| `whois` | TCP/43 registry lookup (roadmap `[ ]`) | sockets |
| `wakeonlan` | UDP magic-packet sender (roadmap `[ ]`) | UDP socket |
| `mqtt` | MQTT client for IoT test traffic (roadmap `[ ]`) | sockets / `tls.h` |
| `latency` | Continuous RTT/jitter/loss monitor to a target, summary stats | `tabos_network_echo`, clock |
| `speedtest` | Throughput probe against a chosen LAN/Internet endpoint | sockets, clock |

Several (`netcat`, `telnet`, `whois`, `wakeonlan`, `mqtt`) are already listed as
future utilities in `agents/milestone-networking.md`; this doc gives them a home.

## Tier 2 — small, non-raw API extensions + the apps they unlock

Each extension is portable (host + Tab5), stays above raw radio, and follows the
existing `network.h` pattern (copied status, process-owned handles, bounded data).

1. **Wi-Fi scan API** — enumerate nearby APs: SSID, BSSID, channel, security,
   signal, PHY modes. **Implemented** as `tabos_network_scan()` (private ELF API
   27) with `netctl scan` and `wifiscan`; host-tested and validated on physical
   Tab5. Unlocks:
   - [x] `wifiscan` — nearby-AP lister, filters, bounded repeat, channel survey.
   - [ ] `wifimon` — watch APs over time, track signal/channel (site-survey assist).
     `wifiscan -r` covers simple repeats; a diffing monitor remains open.
   - [ ] A GUI "available networks" picker later.

2. **SoftAP / AP-mode service** — bring up a local access point (portable config,
   no raw frames). Unlocks:
   - `captiveportal` — **legitimate** captive portal for onboarding/testing: serve
     a page, capture form input, for networks you run. (Attack "Evil Portal" is
     out of scope by intent, not just capability.)
   - LAN test rendezvous for the `nettest`-style peer tools.

3. **Socket options (TTL / hop-limit, timeouts)** — small `setsockopt`-like
   additions. Unlocks:
   - `traceroute` — needs per-packet TTL control.

4. **Enriched status fields** — IPv6 addrs, netmask/prefix, gateway, DNS servers,
   transport/firmware state (already roadmap `[ ]`). Unlocks:
   - `ifinfo` / `netinfo` — full local network config readout.

5. **UDP multicast join** — unlocks `mdns`/`ssdp` service browsers (discover
   printers, DLNA, Chromecast, etc. on a test LAN).

Priority order suggestion: **(1) scan** first — highest value for "wireless
analysis", already roadmap-blessed, unlocks the most visibly-wireless apps.

## Explicit non-goals (stay out for now)

- Raw 802.11 / BLE / 802.15.4 frame capture or injection; monitor/promiscuous mode.
- Deauth, beacon spam, probe-flood, handshake/PMKID capture.
- Sub-GHz RF, NFC/RFID, IR — no Tab5 hardware anyway.
- Anything requiring the external MonsterC5 board (tracked separately in
  `agents/monster-c5.md`).

## First step & build pattern

Recommended starter: **`portscan`** (Tier 1, zero kernel change) to establish the
app shape, then **`wifiscan`** once the scan API lands.

Pattern to copy: `apps/netutils/ping` (DNS + ICMP + arg parsing + summary output)
and `apps/netutils/nettest` (TCP/UDP + `wait` sets). New app = new dir under
`apps/netutils/` (or a new `apps/nettools/` group), its own `Makefile` target,
installs to `T:/bin/`.

## Verification (per app)

- Host Debug + Release build and run (`make -C apps/netutils <name>`), exercised
  against a local service (e.g. a Python listener, as `nettest` already uses).
- Tab5 Debug + Release cross-build.
- Physical Tab5 run against a real target on an owned/authorized test network.
- Where a Tier-2 API is added: host fake-backend tests + RV32 tester coverage,
  matching the existing networking test approach in `agents/testing.md`.
