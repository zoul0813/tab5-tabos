# Networking

TabOS uses the Tab5 ESP32-C6 companion over ESP-Hosted SDIO for Wi-Fi. Host
builds simulate Wi-Fi state without changing the workstation connection.

Saved credentials use `T:/etc/wifi.conf`. A documented, fully commented template
is provided in the repository at [`etc/wifi.conf`](../etc/wifi.conf). Copy it to
`etc/wifi.conf` on the microSD card, uncomment the version, section, and desired
settings, then replace the examples. For the host simulator, use `T/etc/wifi.conf`
below the configured host root. The template alone configures no network.
Credentials are plaintext; keep real passwords out of the repository.

Example active configuration:

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
sequence during boot and makes at most three attempts. Each attempt is bounded to
20 seconds, so a missing or rejecting network ends in a failure instead of a
connection that never completes.

A `[wifi]` section with neither `ssid` nor `password` is valid and means no saved
network; `name` and `auto_connect` still apply. `netctl forget` writes this form.
A `password` without an `ssid` is rejected as a damaged file.

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
netctl scan
netctl connect
netctl connect --prompt [--save]
netctl connect --ssid NAME [--save]
netctl disconnect
netctl save
netctl forget
netctl autoconnect on|off
```

Bare `netctl connect` joins the saved network.

`connect --prompt` scans, lists each named network once (strongest access point
first), and asks for a number or, for a hidden network, its name. `--ssid NAME`
skips the scan. The password is read without echo; each character shows as `*`.
Press Enter at the network prompt to cancel, or at the password prompt for an open
network. netctl waits for the connection, reporting a readable reason on failure
such as `network not found` or `wrong password or security mismatch`. After a
failure the saved network is unchanged and netctl rejoins it. After success it asks
whether to save the network as the default; `--save` saves without asking.
Credentials never appear in arguments, history, logs, or status.

Saving happens only after the connection succeeds, and the kernel saves the
credentials it is already using; no password is passed back from an application.
`save` stores the current connection, for example after a connection you chose not
to save. `forget` removes the saved network but keeps the hostname, autoconnect
policy, comments, and unknown settings; the current connection stays up until
disconnect or restart. `autoconnect on|off` sets whether the saved network is joined
at boot. These commands report a missing `T:` card or a damaged `wifi.conf`, which
they never overwrite, and serialize their changes to the file.

`status` reports state, saved network, hostname, autoconnect policy, attempt count,
current SSID, IPv4 address, signal strength, and last failure when those fields are
available. The current SSID differs from the saved network while trying another
network. It never reports the password.

`scan` lists nearby access points, strongest first, with signal strength,
channel, security, and SSID. Hidden networks appear as `<hidden>`. Scanning works
whether or not TabOS is connected and does not change the current connection. It
reports a busy radio while the ESP32-C6 is starting or associating, including a scan
interrupted by autoconnect; retry after the connection settles (shortly after boot,
`netctl status` shows `connecting`). Use `wifiscan` for filtering, repeated scans, and a channel
survey.

Failed autoconnect attempts use an exact monotonic retry deadline. Disconnecting or
starting an explicit connection cancels pending retry immediately; retry never fires
before its configured delay.

Status, connect, disconnect, and retry-deadline operations are safe when runtime and
native application tasks call them concurrently. Status always returns one complete
snapshot; backend connection work does not hold the portable state lock.

Only one network is saved at a time.

## Application API

Applications can include `<tabos/network.h>` and call
`tabos_network_get_status()`, `tabos_network_connect_saved()`, and
`tabos_network_disconnect()`. The same header exposes bounded networking
operations for independently loaded applications:

- `tabos_network_resolve()` resolves a hostname to one numeric IPv4 or IPv6
  address.

- `tabos_network_echo()` sends one bounded ICMP echo request to a resolved
  address.

- `tabos_network_connect()` starts a connection to an SSID and password without
  saving it; poll `tabos_network_get_status()` for `TABOS_NETWORK_ONLINE`, or
  `TABOS_NETWORK_FAILED` with three attempts. `tabos_network_save_profile()`
  saves the current connection while online (`ENETDOWN` otherwise).
  `tabos_network_forget_profile()` and `tabos_network_set_auto_connect()` edit
  the saved profile. Profile calls report `ENODEV` without `T:` and `EINVAL` for
  a damaged `wifi.conf`. Status `saved_ssid` names the saved network.

- `tabos_network_scan()` scans nearby 2.4 GHz access points and stores up to
  `TABOS_WIFI_SCAN_MAX` (32) `tabos_wifi_access_point_t` records, strongest
  signal first. Each record carries SSID (empty for hidden networks), BSSID,
  primary channel, signal in dBm, a `tabos_wifi_security_t` value, and
  `TABOS_WIFI_PHY_11B`/`11G`/`11N`/`11AX` flags. It blocks for the scan, usually a
  few seconds, returns the stored count, and fails with `ENETDOWN` when the radio is
  unavailable, `EBUSY` while it is starting, associating, or already scanning,
  `ETIMEDOUT`, or `EIO`. `tabos_wifi_security_name()` returns a short display name.
  Scanning is station-mode only: TabOS does not expose monitor mode or raw
  802.11 frames. The host build returns a fixed simulated set and never touches
  workstation Wi-Fi.

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
through `errno`. Host networking suppresses the native SIGPIPE signal for plain
sockets and TLS, so sending after a peer disconnect returns an application error
without terminating TabOS.

Sockets belong to the loaded application that opened or accepted them. TabOS
closes remaining sockets during normal exit and fault cleanup. Public headers do
not expose POSIX, lwIP, ESP-IDF, or native socket structures. Handles carry a
generation tag, so a handle retained after close cannot operate on a later
socket that reuses the same per-process slot.

Process cleanup interrupts blocked socket operations before it destroys the
application execution context. The platform then prevents new socket operations,
waits for the active backend operation to return, and disposes every owned socket.
On Tab5, socket waits/accept/connect/transfers use cancellable nonblocking worker
attempts; generic waits and ICMP receive waits check cancellation between short polls.
TLS uses nonblocking handshake and transfer attempts and preserves the ten-second
operation timeout. The underlying ESP-TLS TCP-connect readiness call may still wait up
to its configured ten seconds; an already-entered DNS lookup must finish or time out in
lwIP. Teardown retains the task and service state while these calls finish. The calling
gate consumes its worker reply and releases the worker mutex before task deletion, so
no stale response reaches the next application. Handle-close operations remain allowed
during cancellation so error-path cleanup can release newly created resources.

On macOS and Linux, a blocking RV32 socket or TLS call suspends only the guest.
The runtime continues servicing input, display deadlines, and window shutdown.
Explicitly nonblocking sockets still return `EAGAIN` directly to the application.
DNS, ICMP echo, and TLS connection setup use workers that own copies of their inputs;
process termination discards their replies without waiting for a resolver or peer.
At most 16 unfinished worker operations may exist across active and cancelled processes;
further setup requests report an I/O error until capacity is released. TLS connection
setup allows ten seconds for connection/handshake progress, excluding any uninterruptible
system resolver delay. Established TLS reads and writes retain blocking application
semantics while allowing the host runtime to progress.

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
its parent resources. Finite waits calculate one absolute monotonic deadline and
recompute remaining blocking time after intermediate wakes, so an early empty wake does
not become an early timeout. Zero-time polls inspect every source, including sockets.
On host, pending guest operations publish a retry deadline up to 10 ms away, shortened
to the original finite-wait deadline when applicable. No retry deadline remains after
completion or cancellation.

`tester` exercises socket-only and mixed socket/device waits. On an online configured
system it also disconnects Wi-Fi, confirms `wifi0` lifecycle readiness, and starts a saved
reconnect. This may briefly interrupt external networking while tester runs; loopback
socket coverage remains independent of Wi-Fi state.

### TLS client connections

`<tabos/tls.h>` provides certificate-verified client TLS connections. A TLS
connection is process-owned, uses the system CA store on host builds and the
ESP-IDF certificate bundle on Tab5, verifies both the certificate chain and the
requested hostname, and is cleaned up when its application exits. It supports
up to four bounded connections. Host trust-store setup is fail-closed and retried
with a fresh TLS context on every connection attempt after a setup failure.

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

Responses must use HTTP/1.0 or HTTP/1.1 and a successful 2xx status. `fetch`
supports a decimal `Content-Length` or a body delimited by a clean TLS close;
it rejects transfer codings, malformed or oversized headers, HTTP errors, and
truncated length-delimited bodies. The destination is replaced only after the
complete response has been written and closed successfully. Failed downloads
remove the `.part` file and preserve an existing destination.

## Wi-Fi Scanner

`wifiscan` is a site-survey tool built on `tabos_network_scan()`:

```sh
make -C apps/netutils wifiscan
wifiscan
wifiscan -s lab -c 6
wifiscan -r 10 -i 3
wifiscan --channels
```

The default view lists signal (dBm and a four-step bar), channel, security, PHY
modes, BSSID, and SSID. `-s` keeps SSIDs containing text, ignoring case; `-c` keeps
one channel. `-r` repeats the scan up to 1000 times with an `-i` second delay
(default 5) for walking a site. `--channels` replaces the list with per-channel
access-point counts, the strongest signal, and an overlap count of access points
within four channels, then names the least congested of channels 1, 6, and 11.

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
