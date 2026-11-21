# TabOS Network Utilities

Network utilities are small, independently loaded TabOS programs. Each utility has
its own source directory and produces its own ELF binary.

Build every network utility:

```sh
make -C apps/netutils
```

Build and install one utility:

```sh
make -C apps/netutils netctl
make -C apps/netutils ping
make -C apps/netutils nettest
make -C apps/netutils ntpdate
make -C apps/netutils httpsget
make -C apps/netutils fetch
make -C apps/netutils irc
make -C apps/netutils wifiscan
```

Use `build-netctl`, `build-ping`, `build-nettest`, `build-ntpdate`,
`build-httpsget`, `build-fetch`, `build-irc`, or `build-wifiscan` to compile without
installing.
Runnable binaries install to `.local/rootfs/T/bin/` without filename extensions.

`netctl` reports and controls Wi-Fi: it lists nearby access points with
`netctl scan`, joins a scanned or hidden network with `netctl connect --prompt`
(hidden password, saved only after success), and edits the saved profile with
`save`, `forget`, and `autoconnect on|off`. `wifiscan` filters, repeats, and surveys channel
congestion from the same scan API. `ping` performs DNS and
bounded ICMP echo checks. `nettest` validates TCP and UDP interoperability with a
LAN peer; see `docs/networking.md` for its client and listener workflows. `ntpdate`
sets the UTC clock from an NTP server. `httpsget` retrieves a certificate-verified
HTTPS URL. `fetch` saves a successful, complete HTTPS response body through a
temporary `.part` file.

`irc` keeps a bounded CRLF-delimited receive stream so fragmented and coalesced
server messages are handled without disturbing text currently being edited.
