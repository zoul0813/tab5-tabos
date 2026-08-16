# TabOS Tester

Tester is an independently loaded C17 application that validates the public TabOS SDK
and application ABI. Tests are divided into modules under `src/tests/` so new subsystem
coverage can be added without growing one large source file.

Activate the application toolchain, build, and install into the host TF-card root:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/tester
```

The runnable image is `build/apps/tester/tester.bin` and installs as
`.local/rootfs/T/bin/tester.bin`. Copy it to `T:/bin/tester.bin` on Tab5, then run:

```text
> tester one "two words"
```

Tester prints one result per module and returns zero only when every assertion passes.
The filesystem module uses `T:/tabos-tester/` temporarily and removes it before return.
The input module briefly enables nonblocking stdin and consumes any input already queued
for the process.
