# Application Lifecycle

TabOS has portable application descriptors, built-in application registry, and single foreground application lifecycle. This foundation runs identically in host and Tab5 builds. Applications use public TabOS APIs and do not call SDL3, ESP-IDF, or FreeRTOS directly.

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

`tabos_app_active()`, `tabos_app_is_running()`, and `tabos_app_last_exit_status()` expose lifecycle state. Future launcher or shell can inspect registry with `tabos_app_count()`, `tabos_app_at()`, and `tabos_app_find()`, then call `tabos_app_launch()`.

## Console Test Application

When `TABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP=ON`, built-in `console-test` application is registered and selected as persistent PID 0. It receives console ownership from process manager instead of acquiring console itself. Ctrl+Q reports diagnostic completion but leaves PID 0 active.

When `TABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP=ON`, built-in `filesystem-test`
application validates real mounted storage through public TabOS filesystem API,
prints each result, cleans its dedicated test directory after success, and exits.

## Current Limits

Current implementation is deliberately small:

- built-in applications are statically linked
- one cooperative foreground application runs at a time
- built-in root callbacks currently run in runtime loop rather than separate tasks
- console is only defined capability
- registry capacity is 16 built-in descriptors
- fixed-capacity process table supports nested foreground processes; blocked parents retain state and resume after child exit
- child launch is currently an internal built-in-test primitive; public synchronous execution API and RV32 call gate remain pending
- PID 0 exit enters kernel-panic state and retains its process record; memory isolation remains unavailable
- experimental filesystem-backed ELF loader exists, but no relocations, arguments,
  discovery, or general external application memory management yet

Descriptor and public application API avoid assumptions about executable container.
Current loader diagnostic reads stripped ELF through TabOS filesystem API and maps its
entry into this lifecycle without making ELF details part of normal application source
API. See [ELF Loader Experiment](elf-loader.md).
