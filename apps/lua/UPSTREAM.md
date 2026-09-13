# Lua upstream provenance

PUC Lua 5.5.1, released 2026-07-24. Imported from the official source archive.

- Archive: https://www.lua.org/ftp/lua-5.5.1.tar.gz
- SHA-256: `1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce`
- Verified against https://www.lua.org/ftp/ before extraction.
- Import: complete archive contents under `vendor/lua/`, including source, manual, upstream Makefiles and notices. Only sources enumerated in `sources.mk` are compiled; upstream `lua.c`, `luac.c`, `linit.c`, `loslib.c` and desktop build recipes are not used.
- License: MIT, retained in `LICENSE`, upstream headers and manual.
- Bug review: https://www.lua.org/bugs.html listed no reported 5.5.1 bugs on 2026-09-12. No upstream bug fixes applied.

## TabOS configuration and patches

`include/lua_tabos/config.h` selects generic ISO C, standard 64-bit Lua integers and double precision, a 48-level C-call limit and 48-level pattern recursion limit. No POSIX, platform discovery, dynamic loading, readline or signal integration is enabled. Native tests compile the same profile. Application adapters live in `src/`; the application uses shared SDK build rules with trailing `-lm`.

Maintained vendor edits preserve surrounding upstream formatting:

- `ldo.c`: reject every binary chunk in the shared parser, including reader-function loads.
- `lauxlib.c`: reject console chunk loading; remove binary-file reopening, which is unnecessary for source-only loading and references unsupported newlib functionality.
- `lbaselib.c`: reject embedded NUL in `loadfile` and `dofile` filenames.
- `liolib.c`: reject embedded NUL in filenames/modes, route stdin reads and line iteration through the console broker, and replace `io.tmpfile` with an unsupported-operation error.
- `loadlib.c`: ignore environment paths, install TabOS source paths and empty cpath, omit native searchers, and validate C-string path/name arguments.
- `lstrlib.c`: omit `string.dump` registration.

Local replacements provide the UTC-only `os` library, explicit standard-library registration, CLI/REPL, checked allocator, console ownership and cancellation, and the small `tabos` module. No kernel or private ABI access exists in the application.

## Regression test provenance

- Archive: https://www.lua.org/tests/lua-5.5.1-tests.tar.gz
- SHA-256: `da07b543872dc0bb2ff12aabd0c248578d78df3eb6b67efdc537a46d455c7f31`
- Verified against https://www.lua.org/tests/ before extraction.
- Imported unchanged: `utf8.lua`, `strings.lua`, `math.lua` under `tests/upstream/`, covered by Lua's MIT notice.
- Run each with `_U=true; _soft=true; _port=true`. These flags skip platform-dependent, locale, native test-module and high-resource cases where the upstream tests provide those guards.
- The remaining upstream suite is not claimed to pass: it exercises debug/native test libraries, binary dumping/loading, desktop OS behavior, and deeper or larger resource profiles unavailable here. Local tests cover the documented profile, files/modules, allocator failure, console input and coroutine interruption separately.

Source and selected tests are checked in; normal builds and tests require no network download. See `docs/lua.md` at the repository root for usage and validation limits.
