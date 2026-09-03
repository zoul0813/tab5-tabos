# TabOS Tester

Tester is an independently loaded C17 application that validates the public TabOS SDK
and application ABI. Tests are divided into modules under `src/tests/` so new subsystem
coverage can be added without growing one large source file.

Activate the application toolchain, build, and install into the host TF-card root:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/tester
```

The runnable image is `build/apps/tester/tester` and installs as
`.local/rootfs/T/bin/tester`. Copy it to `T:/bin/tester` on Tab5, then run:

```text
> tester one "two words"
```

Tester prints one result per module and returns zero only when every assertion passes.
The filesystem module uses `T:/tabos-tester/` temporarily and removes it before return.
The input module briefly enables nonblocking stdin and consumes any input already queued
for the process.

The pointer module validates `touch0` discovery and stream/wait-source lifecycle when a
pointer backend is available. Failed-child cleanup also covers a deliberately leaked
pointer stream.

The networking module validates zero and finite waits; mixed socket/device sources;
stable item ordering; stale and foreign handles; and process cleanup. Backend component
tests validate cancellable infinite waits without risking an unrecoverable application hang. When Wi-Fi
is online, it briefly disconnects through a nested tester child, verifies the queued
`wifi0` lifecycle event, then starts a saved-config reconnect. Run tester with saved Wi-Fi
configuration when validating this path on Tab5.
