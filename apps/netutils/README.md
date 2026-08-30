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
```

Use `build-netctl`, `build-ping`, `build-nettest`, `build-ntpdate`, or
`build-httpsget` to compile without installing.
Runnable binaries install to `.local/rootfs/T/bin/` without filename extensions.

`netctl` reports and controls the saved Wi-Fi connection. `ping` performs DNS and
bounded ICMP echo checks. `nettest` validates TCP and UDP interoperability with a
LAN peer; see `docs/networking.md` for its client and listener workflows. `ntpdate`
sets the UTC clock from an NTP server. `httpsget` retrieves a certificate-verified
HTTPS URL.
