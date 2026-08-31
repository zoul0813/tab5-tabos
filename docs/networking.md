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
make -C apps/netutils netctl
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
close, and nonblocking mode. Each process may own up to `TABOS_SOCKET_MAX`
sockets. Opening or accepting another socket reports `EMFILE`; a failed accept
closes the native accepted socket before returning.

Socket payload calls are bounded to `TABOS_NETWORK_IO_MAX` bytes. Applications
must loop when transferring larger streams. A zero return from receive means the
peer performed an orderly shutdown. Nonblocking operations report `EAGAIN`
through `errno`.

Sockets belong to the loaded application that opened or accepted them. TabOS
closes remaining sockets during normal exit and fault cleanup. Public headers do
not expose POSIX, lwIP, ESP-IDF, or native socket structures. Handles carry a
generation tag, so a handle retained after close cannot operate on a later
socket that reuses the same per-process slot.

Process cleanup interrupts blocked socket operations before it destroys the
application execution context. The platform then prevents new socket operations,
waits for the active backend operation to return, and disposes every owned socket.
This keeps a terminated process from leaving a blocked worker or response for the
next application.

### Waiting for readiness

`<tabos/wait.h>` provides generic, opaque `tabos_wait_source_t` handles. Convert a
socket with `tabos_socket_wait_source()`, place the resulting source in a
`tabos_wait_item_t`, and call `tabos_wait()` with as many as `TABOS_WAIT_MAX`
items. Wait sources are process-owned and generation-tagged. Closing a socket
invalidates its source; stale or foreign sources fail with `EBADF`.

Device lifecycle sources returned by `tabos_device_subscription_wait_source()` may share
the same wait call, allowing one application loop to wait for network and device activity.

Wait items may request readable, writable, state-changed, error, or hangup events.
Sockets support readable, writable, error, and hangup. Device lifecycle sources support
readable and state-changed. A zero timeout
polls immediately, a finite timeout uses monotonic milliseconds, and
`TABOS_WAIT_TIMEOUT_INFINITE` waits until an event or process cleanup interrupts
the operation. The return value is the number of ready items, zero for timeout,
or `-1` with `errno` set. Process teardown cancels an active wait before closing
its parent resources.

`tester` exercises socket-only and mixed socket/device waits. On an online configured
system it also disconnects Wi-Fi, confirms `wifi0` lifecycle readiness, and starts a saved
reconnect. This may briefly interrupt external networking while tester runs; loopback
socket coverage remains independent of Wi-Fi state.

### TLS client connections

`<tabos/tls.h>` provides certificate-verified client TLS connections. A TLS
connection is process-owned, uses the system CA store on host builds and the
ESP-IDF certificate bundle on Tab5, verifies both the certificate chain and the
requested hostname, and is cleaned up when its application exits. It supports
up to four bounded connections.

`tabos_tls_connect()`, `tabos_tls_send()`, `tabos_tls_receive()`, and
`tabos_tls_close()` use the same `errno` convention as sockets. Send and receive
calls are limited to 1024 bytes; applications must loop for larger transfers.
There is deliberately no insecure or certificate-bypass mode.

## HTTPS retrieval

`httpsget` is the first TLS client utility. It performs an HTTP/1.1 GET over a
certificate-verified TLS connection and writes the complete HTTP response to
standard output:

```sh
make -C apps/netutils httpsget
httpsget https://example.com/
httpsget -c 8 https://example.com/
```

Only `https://host[/path]` URLs are currently accepted. Custom ports, user
credentials, redirects, chunk decoding, proxies, and IPv6 URL literals are
deferred until a broader HTTP client interface is needed.

Use `-c 1` through `-c 16` to repeat a request and verify TLS connection cleanup.
For physical TLS validation, a known-good URL must succeed, while an expired or
hostname-mismatched certificate (for example `https://expired.badssl.com/`) must
fail. Disconnecting Wi-Fi must also return an error without resetting TabOS.

`fetch` downloads an HTTPS response body to a file. Its destination is optional;
the last URL path component is used in the current directory, or `download` when
the URL does not provide a usable filename:

```sh
fetch https://example.com/files/readme.txt
fetch https://example.com/ T:/example.html
```

## Ping

The `ping` core utility uses only these public APIs. Build and install it with:

```sh
make -C apps/netutils ping
```

It accepts `-4` or `-6`, `-c count`, and `-W timeout-ms`; without options it
sends four requests with a one-second timeout.

## NTP clock synchronization

`ntpdate` is a network utility that resolves an NTP server, sends one bounded UDP
NTP request, validates the server response, and sets the TabOS UTC clock. It defaults
to `pool.ntp.org`; supply a server name or numeric address to select another server:

```sh
make -C apps/netutils ntpdate
ntpdate
ntpdate time.cloudflare.com
ntpdate -6 time.cloudflare.com
```

The utility accepts synchronized NTPv4 server responses. IPv4 is the default; `-6`
selects IPv6. It sets whole Unix seconds; sub-second clock discipline and NTP
authentication are outside its scope.

## Host-to-Tab5 Socket Test

The ordinary tester uses loopback sockets. To prove that TCP and UDP packets traverse
the ESP32-C6 and the local Wi-Fi network, build and install the `nettest`
diagnostic utility:

```sh
make -C apps/netutils nettest
```

Run the companion echo service on another machine on the same LAN:

```sh
python3 tools/network_test_server.py
```

On a managed macOS host where the application firewall blocks the active Python
runtime, use the equivalent Node.js server when Node is approved:

```sh
node tools/network_test_server.mjs
```

Find that machine's LAN IPv4 address, then run on Tab5:

```sh
nettest 192.168.1.20
```

The combined test stops after a TCP failure so an unavailable UDP reply cannot trap
the shell. Run either protocol independently when diagnosing a failure:

```sh
nettest --tcp 192.168.1.20
nettest --udp 192.168.1.20
```

The default TCP and UDP ports are 39001 and 39002. Override them on both sides with
`--tcp-port` and `--udp-port` on the Python server and positional port arguments on
`nettest`. The test resolves the supplied host, verifies a server-first handshake,
validates a fixed-length 1537-byte TCP stream across multiple bounded API calls,
performs an orderly TCP write-half shutdown before receiving the echoed stream,
and validates a 257-byte UDP datagram plus its reply endpoint. A TCP pass proves
the host received every queued byte and the Tab5 kept its read half open after
closing its write half.

For reverse-direction manual testing, make Tab5 listen for one exchange:

```sh
nettest --listen tcp 39001
nettest --listen udp 39002
```

Connect from the host with `nc`:

```sh
nc 192.168.1.50 39001
nc -u 192.168.1.50 39002
```

Use the addresses reported by `netctl status` and the host operating system. Host
firewall rules must permit inbound Python traffic, and the access point must not use
wireless client isolation.
