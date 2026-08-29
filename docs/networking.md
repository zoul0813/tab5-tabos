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

Interactive credential entry, scanning, forgetting credentials, DNS, sockets,
and ping are not implemented yet.

Applications can include `<tabos/network.h>` and call
`tabos_network_get_status()`, `tabos_network_connect_saved()`, and
`tabos_network_disconnect()`.
