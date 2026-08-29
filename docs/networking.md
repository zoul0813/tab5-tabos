# Networking

TabOS uses the Tab5 ESP32-C6 companion over ESP-Hosted SDIO for Wi-Fi. Host
builds simulate Wi-Fi state without changing the workstation connection.

Saved credentials use `T:/etc/wifi.conf`:

```ini
version=1

[wifi]
ssid="network name"
password="secret"
name="TabOS"
auto_connect=true
```

The `ssid` and `password` values must be quoted. An empty password selects an
open network. With `auto_connect=true`, TabOS starts one asynchronous connection
sequence during boot and makes at most three attempts.

The optional quoted `name` is sent as the DHCP hostname. It supports one through
32 ASCII letters, digits, or hyphens; a hyphen cannot be first or last. It
defaults to `TabOS` when omitted.

## Network Control

Build and install `netctl` with:

```sh
make -C apps/coreutils netctl
```

Supported commands are:

```sh
netctl status
netctl connect
netctl connect --prompt
netctl disconnect
```

Bare `netctl connect` uses the saved profile. `--prompt` explicitly selects
interactive credential entry; that path is reserved for the next networking
slice and currently reports an unsupported operation.

`status` reports state, hostname, saved configuration availability, autoconnect
policy, attempt count, SSID, IPv4 address, signal strength, and last failure
when those fields are available. It never reports the password.

Interactive credential entry, scanning, and forgetting credentials are not
implemented yet.

## Application API

Applications can include `<tabos/network.h>` and call
`tabos_network_get_status()`, `tabos_network_connect_saved()`, and
`tabos_network_disconnect()`. The same header exposes bounded networking
operations for independently loaded applications:

- `tabos_network_resolve()` resolves a hostname to one numeric IPv4 or IPv6
  address.

- `tabos_network_echo()` sends one bounded ICMP echo request to a resolved
  address.

### TCP and UDP sockets

Applications use opaque `tabos_socket_t` handles and portable textual IPv4 or
IPv6 endpoints. The API supports TCP and UDP sockets, bind, ephemeral local
ports, listen, accept, connect, send, receive, datagram send/receive, shutdown,
close, and nonblocking mode.

Socket payload calls are bounded to `TABOS_NETWORK_IO_MAX` bytes. Applications
must loop when transferring larger streams. A zero return from receive means the
peer performed an orderly shutdown. Nonblocking operations report `EAGAIN`
through `errno`.

Sockets belong to the loaded application that opened or accepted them. TabOS
closes remaining sockets during normal exit and fault cleanup. Public headers do
not expose POSIX, lwIP, ESP-IDF, or native socket structures.

## Ping

The `ping` core utility uses only these public APIs. Build and install it with:

```sh
make -C apps/coreutils ping
```

It accepts `-4` or `-6`, `-c count`, and `-W timeout-ms`; without options it
sends four requests with a one-second timeout.
