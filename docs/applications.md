# Application Lifecycle

The supported external application SDK and libc surface are documented in `docs/sdk.md`.

TabOS has portable application descriptors, built-in application registry, and single foreground application lifecycle. This foundation runs identically in host and Tab5 builds. Applications use public TabOS APIs and do not call SDL3, ESP-IDF, or FreeRTOS directly.

## Building Applications

Build and install every independently loaded application with:

```sh
./apps/build.sh
```

Script discovers maintained `apps/*/Makefile` projects, builds each default application,
and installs runnable ELF images under `.local/rootfs/T/bin/`. Optional DOOM stays
excluded unless `--with-doom` is present. Script activates project-local ESP-IDF
toolchain automatically when compiler is not already available. Arguments pass to each
application Makefile. Repository paths containing spaces are supported. For example,
this builds without installing:

```sh
./apps/build.sh build
```

Explicitly fetch, build, and install pinned optional DOOM application with:

```sh
./apps/build.sh --with-doom
```

To build, install, and copy the applications to the Tab5 MSC volume, then safely
eject it after copying:

```sh
./apps/build.sh --msc
```

The default mount point is `/Volumes/TAB5`. Use `--msc-mount=/path` or set
`TABOS_MSC_MOUNT` to override it. Runnable extensionless outputs are copied to the
volume's `bin/` directory. Intermediate build files and source assets are excluded.
Grouped utility outputs are flattened; for example, `build/apps/coreutils/ls/ls` is
copied to `bin/ls`.

DOOM is not copied from an earlier build unless explicitly selected. To build and copy
it with other applications, use:

```sh
./apps/build.sh build --msc --with-doom
```

When `build/apps/doom/doom` exists, `--msc --with-doom` copies that extensionless
executable to `T:/bin/doom`. Build and installation never copy or download WAD data.

Individual application commands such as `make -C apps/shell` remain available.

Core utilities are grouped under `apps/coreutils/`, but each utility remains a separate
program. Build one with `make -C apps/coreutils ls` or `make -C apps/coreutils mkdir`.
Sources live in `apps/coreutils/src/<name>/main.c`; each output installs directly under
`T:/bin/`.

`devices` lists registry entries using public copied metadata: unpadded decimal boot-local ID,
logical name, class, state, symbolic features, driver name, and nonzero last error. It does not
expose native platform handles or implementation details.

`audiotest` exercises `audio0` through the public SDK. It reports capabilities, generates
speaker or headphone tones, displays microphone levels, performs microphone loopback,
changes routes, and reports deliberate underrun/overrun counters. See `docs/audio.md`.

`touchtest` exercises `touch0` through the public pointer and wait APIs. It prints
contact lifecycle, logical coordinates, buttons, and optional pressure. See
`docs/pointer.md`.

Networking utilities are grouped under `apps/netutils/`. Build one with
`make -C apps/netutils netctl`, `make -C apps/netutils ping`, or
`make -C apps/netutils nettest`. Sources live in
`apps/netutils/src/<name>/main.c`; each output also installs directly under
`T:/bin/`.

Applications may include `<sys/reboot.h>` and call `reboot(RB_AUTOBOOT)` or
`reboot(RB_POWER_OFF)`. A successful request does not return. TabOS stops applications,
closes and unmounts storage, and shuts down platform services before applying the action.
The `reboot` and `shutdown` core utilities expose these actions directly.

## Descriptor

Built-in applications export `tabos_app_descriptor_t` from `<tabos/application.h>`:

```c
static bool hello_entry(tabos_app_context_t *context)
{
    const tabos_console_session_t *console = tabos_app_console(context);
    if (console == NULL) {
        return false;
    }
    tabos_console_write_line(console, "Hello from TabOS");
    tabos_app_request_exit(context, 0);
    return true;
}

const tabos_app_descriptor_t hello_app = {
    .abi_version = TABOS_APPLICATION_ABI_VERSION,
    .name = "hello",
    .version = "1.0.0",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry = hello_entry,
};
```

Descriptor contains ABI version, stable application name, application version, requested capabilities, entry callback, optional update callback, and optional cleanup callback. Current descriptor is runtime metadata, not ELF header or final on-disk package format.

## Lifecycle

Registry rejects invalid ABI versions, unsupported capabilities, duplicate names, and overflow. Runtime can launch one foreground application by registered name. Launch sequence is:

1. Resolve descriptor from registry.
2. Acquire requested services, currently foreground console.
3. Call entry callback.
4. Call update callback from normal runtime loop.
5. Honor `tabos_app_request_exit()`.
6. Call cleanup callback with exit status.
7. Release console ownership and return to idle runtime.

`tabos_app_active()`, `tabos_app_is_running()`, and `tabos_app_last_exit_status()` expose
lifecycle state. Built-in applications can call `tabos_app_exec(context, path)` to request
a filesystem-backed ELF child, then must return from current callback. Parent remains
loaded and blocked while child owns console and input; `tabos_app_take_child_status()`
receives status from a later callback after cleanup and parent resume.
Filesystem shell uses same process operation through ELF ABI call gate.

Filesystem applications receive `argc` and `argv` at entry. Process/loader copies at
most 16 arguments and 512 total argument bytes into child-owned storage before launch;
the copy remains valid for process lifetime. `tabos_app_exec_args()` is token-based and
does not parse quoting or escaping. Shell owns command-line syntax and passes finalized
strings.

Independently loaded C17 applications expose `main(argc, argv)`, not a kernel descriptor
or raw ELF entry. SDK `crt0` and newlib stubs translate standard C/POSIX calls to the
versioned TabOS ABI. Standard streams are console-backed: stdin is unbuffered, stdout is
line-buffered, and stderr is unbuffered. Each process owns descriptors, errno, current
working directory, and a bounded heap; children inherit a copy of the parent's working
directory. TTY input mode is also inherited by value. Applications can include
`<sys/ioctl.h>` and `<tabos/tty.h>` to query or replace that mode. For example, a game
can preserve current mode, clear `TABOS_TTY_MODE_SCROLL_KEYS`, and set
`TABOS_TTY_MODE_RAW_INPUT` to receive physical key events without cooked text events;
the blocked parent's mode remains unchanged and is restored with that parent.

`<tabos/process.h>` provides synchronous `tabos_exec(path, argc, argv)`. Parent remains
loaded but blocked while child owns foreground console/input. Function returns child's
exit status after cleanup and parent restoration. Arguments are already-tokenized strings;
API performs no shell quoting or parsing.

`tabos_app_launch_path()` starts filesystem-backed ELF directly as process 0. Runtime uses
this for shell startup. Each loaded ELF owns path, descriptor, executable mapping, and
execution context until process cleanup.

Process 0 is required and cannot exit. Exit request, executable return, execution fault,
or forced termination moves it into kernel-panic state without unloading or restarting
it. `tabos_process_panic_info()` reports retained cause and status. Panic message is sent
to serial logging and directly to framebuffer console even if ordinary console ownership
is unavailable.

## Console Test Application

When `TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP=ON`, built-in `console-test` application is registered and selected as persistent PID 0. It receives console ownership from process manager instead of acquiring console itself. Ctrl+Q reports diagnostic completion but leaves PID 0 active.

When `TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP=ON`, built-in `filesystem-test`
application validates real mounted storage through public TabOS filesystem API,
prints each result, cleans its dedicated test directory after success, and exits.

When `TABOS_ENABLE_ELF_LOADER_EXPERIMENT=ON`, persistent `elf-hello` launcher runs
configured filesystem ELF twice as child processes. It verifies loading, process-owned
execution state, console transfer, exit status, cleanup, and parent restoration without
exiting process 0.

## Current Limits

Current implementation is deliberately small:

- built-in applications are statically linked
- one cooperative foreground application runs at a time
- built-in root callbacks currently run in runtime loop rather than separate tasks
- console is only defined capability
- registry capacity is 16 built-in descriptors
- fixed-capacity process table supports nested foreground processes; blocked parents retain state and resume after child exit
- C application API supports cooperative filesystem-backed child requests and status
  collection; ELF API exposes blocking-style child execution through cooperative pending
  status while process manager blocks parent
- PID 0 exit enters kernel-panic state and retains its process record; memory isolation remains unavailable
- filesystem-backed ELF loader supports argument vectors, static load relocation, and a
  first C17/newlib runtime, but no dynamic linking, discovery, or executable metadata yet
- Tab5 runs each native ELF entry in managed FreeRTOS application task so persistent
  applications do not block runtime/service loop
- shared console state uses platform mutexes: SDL mutex on host and a FreeRTOS mutex with
  priority inheritance on Tab5

Filesystem ELF processes own their loaded image, executable mapping, execution
context, heap arena, open descriptors, graphics session, arguments, working directory,
and terminal policy. TabOS releases these resources through one idempotent teardown path
after normal return, requested exit, launch failure, or a fault reported by the execution
backend. A child failure returns a nonzero status after teardown and restores its parent;
a PID 0 failure enters kernel panic.

The host RV32 interpreter bounds-checks guest memory and turns invalid guest accesses
into child-process faults. Tab5 currently executes ELF code natively in a FreeRTOS task
with kernel privilege. The task provides independent scheduling and stack allocation, but
not memory isolation: an arbitrary native memory or instruction fault can still invoke
the ESP-IDF panic handler and reboot the device. True Tab5 crash containment requires a
future user-mode/PMP execution boundary and recoverable trap handler.

Descriptor and public application API avoid assumptions about executable container.
Loader reads ELF with debug sections removed and static relocations retained through
TabOS filesystem API and maps its entry into this
lifecycle without making ELF details part of normal application source API. See
[ELF Loader Experiment](elf-loader.md).
