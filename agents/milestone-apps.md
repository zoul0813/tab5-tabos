# Application Port Candidates

Status: candidate assessment as of 2026-09-07, not committed milestones or architectural decisions.

The strongest next candidates are Simon Tatham's Puzzles, Kilo, Lua, and Peanut-GB. Each fits TabOS's small-computer direction and adds a useful capability.

## Current TabOS Baseline

TabOS currently provides native RV32 C/newlib applications, filesystem access, keyboard and touch input, scaled RGB565 graphics, PCM audio, and sockets/TLS. DOOM is already an optional work-in-progress port with sound effects; music remains deferred.

The main constraints are partial POSIX support, shared RAM, foreground-only application execution, and no public SDL interface. Ports should use public TabOS APIs rather than directly depending on SDL, ESP-IDF, or FreeRTOS.

See [Application SDK](../docs/sdk.md) and [DOOM status](../docs/doom.md).

Effort estimates below reflect current APIs and upstream structure. None of these new candidates has been compiled or benchmarked on TabOS as part of this assessment.

## Games

| Candidate | Fit for TabOS | Main porting work | Estimated effort |
| --- | --- | --- | --- |
| **[Simon Tatham's Puzzles](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/index.html)** — MIT-style license | Strongest overall fit. Sudoku, Mines, Net, Bridges, and more. C backends are already separated from the graphical frontend. | Build a shared TabOS frontend for drawing, fonts, keyboard/touch, menus, and saves. Start with a few puzzles and expand through the same adapter. | Medium; excellent payoff |
| **[Frotz](https://gitlab.com/DavidGriffith/frotz)** — GPL-2.0 | Text adventures suit a keyboard-first machine. The existing dumb-terminal variant provides a small starting point. | Adapt console input, save files, and timing; add richer status/window handling afterward. Story files are separately licensed. | Low–medium |
| **[Peanut-GB](https://github.com/deltabeard/Peanut-GB)** — MIT | Portable C99 Game Boy emulator. Its 160×144 image scales exactly 5× to 800×720. | Add ROM loading, keys, RGB565 output, battery saves, and frame pacing. Audio requires separate APU integration. Start with homebrew ROMs. | Medium |
| **[TIC-80](https://github.com/nesbox/TIC-80)** — MIT core | Cartridge games plus eventual on-device game creation. Its 240×136 screen scales 5× to 1200×680. | Port the runtime first, select one scripting language, and connect graphics, audio, input, and files. Defer editors and the online browser. Cartridge licenses vary. | Medium–high |

## Useful Applications

| Candidate | Fit for TabOS | Main porting work | Estimated effort |
| --- | --- | --- | --- |
| **[Kilo](https://github.com/antirez/kilo)** — BSD-2-Clause | Small C editor with search and syntax highlighting, without a curses dependency. Makes configuration and source editing practical. | Replace `termios` and terminal probing with TabOS input and size APIs; check required escape sequences. Upstream explicitly describes the editor as experimental. | Low–medium |
| **[Lua](https://www.lua.org/about.html)** — MIT | Portable C scripting language and a foundation for writing small programs directly on Tab5. | Build the interpreter and adapt unavailable OS functions and module loading. Add TabOS graphics, input, and audio bindings later. | Low–medium for CLI; medium with bindings |
| **Music player using [dr_mp3 / dr_wav](https://github.com/mackron/dr_libs)** — public domain or MIT-0 | Existing filesystem and PCM output APIs cover most platform needs. | Write a small player frontend around the decoder libraries; stream bounded chunks and handle audio backpressure and controls. | Low–medium |
| **Tracker player using [libxmp](https://github.com/libxmp/libxmp)** | MOD/XM-style playback fits the retro-computer direction. The library already renders modules to PCM. | Write a small file-browser/player frontend, queue PCM, and bound sample memory. Evaluate the lite build. | Medium |
| **MIDI player / keyboard synth using [TinySoundFont](https://github.com/schellingb/TinySoundFont)** — MIT | Portable C synthesizer that could also help fill DOOM's missing music support. | Bound voices and SoundFont size, schedule notes, and stream PCM. DOOM additionally needs MUS event handling. | Medium |

The player and synthesizer entries mean porting a library and writing a small TabOS application, rather than porting an existing complete UI. Music, SoundFonts, and other data have their own licensing terms.

## Stretch Candidate

**[Quake](https://github.com/id-Software/Quake)** is worth a low-resolution software-renderer experiment, but memory use and frame rate require early hardware measurements. It is more uncertain than the candidates above. The engine source is available under the GPL; original game data is separately licensed.

## Suggested Order

1. **Kilo** — immediate everyday usefulness.
2. **Simon Tatham's Puzzles** — one frontend unlocks many complete games.
3. **Peanut-GB** — a compelling graphics and audio showcase.
4. **Lua** — lets users make their own TabOS programs.
5. **TIC-80 runtime** — a larger investment with broad creative potential.
