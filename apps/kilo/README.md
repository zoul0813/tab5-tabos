# Kilo for TabOS

Kilo is an independently built CP437 terminal editor using the public TabOS SDK.
See [the user guide](../../docs/kilo.md) for controls, limits, saving, and recovery.

```sh
./apps/build.sh
```

From the repository root with the SDK toolchain activated:

```sh
make -C apps/kilo
make -C apps/kilo metadata
```

The runnable image is `build/apps/kilo/kilo`; `kilo.elf` retains debug information.
The default install includes `T:/share/licenses/kilo/`. Source provenance and the
unchanged import are recorded in [UPSTREAM.md](UPSTREAM.md).
