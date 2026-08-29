# TabOS Network Interoperability Tester

`nettest` is a standalone diagnostic application for end-to-end TCP and UDP
testing between TabOS and another machine on the local network.

Build and install it into the host root filesystem:

```sh
make -C apps/nettest
```

Run `tools/network_test_server.py` or `tools/network_test_server.mjs` on the peer
machine, then pass that machine's LAN IPv4 address or resolvable hostname to
`nettest`. See `docs/networking.md` for the complete client and listener workflows.
