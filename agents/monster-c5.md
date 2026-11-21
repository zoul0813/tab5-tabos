# Feasibility: Running M5MonsterC5 Wi-Fi Tools Under TabOS

## Context

TabOS is a custom OS for the M5Stack Tab5 (ESP32-P4 main MCU + ESP32-C6 Wi-Fi
co-processor). The question: can the M5MonsterC5 ("Monster") wireless toolkit
run as a TabOS *application* rather than as a competing custom Tab5 firmware —
and/or can its individual scanner tools be ported over?

This document is a feasibility assessment only. No implementation is planned yet.

## What M5MonsterC5 actually is

M5MonsterC5 (by LAB5 / "Laboratorium", Poland) is **two separate pieces**, not one
firmware:

1. **Radio engine** — runs on a *separate external ESP32-C5 board* (the Monster
   PCBA) with its own firmware ("JanOS"). The C5 does all the wireless work:
   5 GHz Wi-Fi 6, promiscuous sniffing, raw packet injection, deauth, evil-twin
   / captive portal, WPA3-SAE overflow, etc. This is why the product ships as an
   add-on board with its own antenna, ~$55 on Tindie.
2. **Host-side UI** — a thin client that runs on the M5Stack host (Cardputer ADV
   or Tab5). It sends text/command frames to the C5 and renders the results. It
   does **no radio work itself**. The Tab5 connects to the Monster board via the
   **M5-Bus connector** (which carries a UART line); JanOS exposes a serial
   command protocol.

Key consequence: the Tab5 has only an **ESP32-C6** radio (2.4 GHz only). The
5 GHz capability and the offensive radio primitives live entirely on the external
C5. "Porting Monster to the Tab5's own radio" is not the same project as "running
Monster's UI on the Tab5."

Sources:
- https://www.cnx-software.com/2026/01/21/m5monsterc5-hacking-tool-adds-esp32-c5-and-5-ghz-wi-fi-6-support-to-m5stack-cardputer-adv-and-tab5/
- https://c5lab.github.io/M5MonsterC5-Tab5/tab.stories
- https://github.com/C5Lab/.github
- https://www.tindie.com/products/lab/m5monsterc5-esp32c5-marauder
- https://docs.m5stack.com/en/core/tab5 (Tab5 = ESP32-P4 + ESP32-C6, M5-Bus connector)

## TabOS boundaries that decide feasibility

Confirmed from the codebase/agent docs:

- **Network API is sandboxed BSD sockets only.** `sdk/include/tabos/network.h`
  exposes resolve / echo / TCP+UDP sockets / TLS. There is **no** raw-frame,
  promiscuous, monitor-mode, or injection surface, by design.
  (`agents/TABOS_CONTEXT.md:628-638`, `[DECIDED]` entries.)
- **Networking is mediated through the C6 over ESP-Hosted SDIO.** Apps are
  required to consume TabOS network APIs, not the C6 transport or ESP-IDF
  directly. (`agents/TABOS_CONTEXT.md:632-638`.)
- **Raw GPIO/I2C/SPI/UART is explicitly withheld from external applications.**
  (`agents/hardware-services.md:333, 383`; roadmap "Explicitly Deferred".)
- **A typed serial/RS-485 service exists only as a future roadmap item
  (Phase 7), not built.** (`agents/hardware-services.md:329-334` — all `[ ]`.)
- Apps are ELF programs under `apps/*` built against the `sdk/` headers (see
  `apps/netutils`, `apps/irc`). There is a device-registry API
  (`sdk/include/tabos/device.h`: `wifi0`, etc.) but it reports devices; it does
  not grant raw access.

## Two interpretations, assessed

### Path A — Companion-client app (Monster UI on TabOS, external C5 does radio)

A TabOS app that speaks the JanOS serial protocol to the external Monster board
over the M5-Bus UART, and renders scan/attack results. This matches the user's
"app, not custom firmware" framing and respects every TabOS boundary **except
one**: there is no app-reachable serial/UART transport yet.

- **Blocker:** needs a TabOS typed-serial service (roadmap Phase 7). Raw UART is
  deliberately not exposed to apps, so this cannot be a pure userspace port
  today.
- **Size:** moderate. The gating work is a small, bounded, roadmap-aligned
  kernel service (one typed UART channel with baud/framing, process-owned
  handles, bounded queues, generic waits — exactly the Phase 7 description). The
  app itself is then an ordinary `apps/` program: serial client + text UI.
- **Architectural fit:** good. Adds a service that was already planned; does not
  weaken the socket-only network boundary. Offensive capability stays on the
  external, separately-flashed C5 — TabOS just drives it.
- **Legality/authZ:** the external board does the radio; same authorization
  rules apply as using Monster on any host. On your own hardware/networks this
  is a UI + serial-transport exercise.

### Path B — Native scanner tools on the Tab5's own C6

Port Monster's scanners to run on the Tab5's built-in C6 radio.

- **Blocker:** requires promiscuous/monitor-mode capture and raw 802.11 TX below
  the socket layer. TabOS deliberately exposes only sandboxed sockets. This
  needs a **new privileged raw-radio service** plumbed through ESP-Hosted
  (ESP-IDF supports `esp_wifi_set_promiscuous` + `esp_wifi_80211_tx` on the C6,
  and a P4+C6 esp-hosted "CustomRpc" path for raw frames has been demonstrated
  elsewhere — ref `github.com/r4d10n/esp32p4-c6-wifi-test`).
- **Cost to the design:** high, and against the grain. It introduces a raw-radio
  surface the architecture spent effort *not* having, with its own containment
  questions.
- **Capability ceiling:** C6 is **2.4 GHz only** — no 5 GHz, no WPA3-SAE
  overflow, limited injection vs. the C5. You get a weaker subset at the cost of
  a boundary-breaking service.

Refs:
- https://github.com/r4d10n/esp32p4-c6-wifi-test
- https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32c6/api-guides/wifi-driver/wifi-vendor-features.html

## Bottom line

- Neither interpretation is a pure userspace app **today** — both need a new
  TabOS kernel-side service first (serial transport for A, raw-radio for B).
- **Path A is the realistic one** and matches "custom app, not custom firmware":
  the missing piece (a typed serial service) is already a planned roadmap item,
  small, and non-invasive. Monster's own split (external C5 does the radio, host
  runs a thin UI) maps cleanly onto "TabOS app driving an external board."
- **Path B fights the architecture**: it requires breaking the deliberate
  socket-only boundary for a 2.4 GHz-only, weaker feature set. Reasonable only as
  a separate, later, explicitly-scoped "raw radio" research effort — not a port.

## Open questions (if this proceeds later)

- JanOS serial protocol spec: exact baud, framing, command/response grammar
  (not found in public search; need the C5Lab repo / JanOS docs or a serial
  capture).
- Which Tab5 M5-Bus pins carry the UART the Monster board uses.
- Whether a minimal read/write serial channel is acceptable, or the full Phase 7
  typed-serial service is the right vehicle.
