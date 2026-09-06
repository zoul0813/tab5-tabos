# Persistent shell history

## Implementation status

Implemented on `feature/shell-history`. macOS Debug ASan/UBSan suite passes all 55
tests; the rebuilt RV32 shell also passes the explicit host-interpreter restart/recall
test. Physical Tab5 validation and Linux execution remain pending.

## Behavior

- Retain last 32 nonblank commands; skip consecutive exact duplicates.
- Up recalls newest command, then older entries. Stop at oldest; no wrapping.
- Down moves newer, then restores unfinished line saved before browsing.
- Recalled commands editable using existing typing/Backspace. Editing leaves history browsing and makes edited text current draft; stored entry unchanged.
- Add `history` built-in: print retained entries oldest first, numbered 1–32. Include `history` itself under normal recording rules.
- Preserve original command text before parser modifies it, including quotes and spacing. Record failed commands too.

## Implementation

- Add shell-local history module with fixed-capacity storage: 32 entries, each supporting existing 255-character command limit. Keep history storage off application stack.
- Extend input decoder to return text, Up, Down, or ignored-input actions. Handle escape sequences split across reads; continue ignoring unsupported sequences.
- Replace displayed input using existing destructive Backspace behavior, then print recalled text. Preserve prompt and handle wrapped commands. Ctrl+Arrow remains terminal scrollback.
- Load `T:/user/history.txt` before first prompt. Missing file means empty history.
- Save whenever history changes, before executing command, including `reboot` and `shutdown`. Create `T:/user` when needed.
- File contains one command per line, oldest first, LF endings. Accept CRLF and final line without newline when loading. Skip blank, overlong, or invalid nonprintable entries; retain newest 32 valid entries.
- Write complete snapshot to `T:/user/.history.txt.tmp`, check writes and close, then rename over history file. On failure, preserve previous history where possible and continue with in-memory history. No claim of sudden-power-loss durability.
- Report storage failures once per failure episode; retry on next changed submission. Clear warning suppression after successful save.
- Update shell documentation and help text with navigation, persistence, duplicate policy, and plaintext storage behavior. No public SDK or kernel API changes.

## Verification

- Unit tests: empty history, 32-entry eviction, consecutive duplicates, exact text preservation, navigation bounds, draft restoration, recalled-line editing.
- Input tests: split Up/Down sequences, repeated arrows, unsupported/control sequences, existing text filtering.
- Persistence tests using temporary host storage: missing directory/file, round trip, CRLF, malformed/oversized lines, excess entries, and write/rename failures.
- Integration checks: short/long replacements, wrapped input, prompt preservation, unchanged Ctrl+Arrow scrollback, recalled command execution, and saved text surviving parser mutation.
- Build shell RV32 executable; run relevant host tests. Verify persistence across host restart and physical Tab5 reboot when hardware available.

## Defaults and limits

- Fixed path and capacity; no configuration option.
- No `history -c`, history expansion, or general cursor editing.
- Shell remains usable when history storage unavailable.
