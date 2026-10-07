# Pool — final acceptance report

Stage 10 distribution checkpoint, 2026-09-13: `1b21165a64579e512fb1a4cd8286c24a6cfa0539`.
Ready for user/PR review, not yet merged or published. The user authorized this
stage after confirming smoother Pool gameplay and working speaker audio on Tab5.
The remaining instrumented Stage 9 checks are carried forward as limitations,
not marked passed. This report records the retained release evidence and known
limitations.

## Delivered game

- Standalone `pool` application using the public TabOS SDK; default computer
  opponent or shared-keyboard local two-player mode (`pool --two-player`).
- 640×360 top-down table, six pockets with finite jaws, 16 numbered balls,
  solids/stripes, cue and clipped aim guide, adjustable power and fine controls.
- Fixed-step movement, friction, cushion/ball collisions, pocket capture,
  scratches, ball-in-hand placement, turns, groups and 8-ball win/loss outcomes.
- Bounded direct-pot AI with controlled aim/power error and legal contact fallback;
  it uses the same shot commands, physics and rules as the human.
- Welcome/mode selection, help, pause, confirmed restart, result screens,
  potted-ball trays, cue/pocket animations and six optional speaker effects.
- Idle input waits, no per-tick application allocation, bounded catch-up and
  audio writes; prepared audio and embedded PCM avoid shot-time device startup,
  synthesis and shutdown. Pause/help/mute/restart/exit release audio.

## Controls

| Key | Action |
| --- | --- |
| Left/Right, A/D | Aim; select mode on welcome screen; horizontal placement. |
| Up/Down, W/S | Adjust power 1–100; vertical placement. |
| Aa/Shift held | Fine aim or slow placement. |
| Space/K | Shoot when the table is still and the turn permits it. |
| Enter | Start, confirm valid placement/restart, dismiss help, play again. |
| P / H / M | Pause / help / mute. |
| R | Request new rack; Enter confirms, R/Escape cancels. |
| Q / Escape | Exit; Escape closes help/confirmation first. |

Enter confirms placement; it does not shoot. Held triggers cannot automatically
fire on a new turn or after a transition. The [game guide](pool.md) describes
aliases, input gating and mode-specific behavior.

## Rules and deliberate simplifications

Player 1 breaks. Groups stay open after the break and are assigned by the first
recorded group pot on a later legal shot. Legal own-group pots continue the turn;
otherwise it passes. A valid first contact must be followed by an object pot or
rail/jaw contact. Scratches, no contact, wrong first contact and no later rail/pot
give the opponent ball in hand anywhere valid. Potted group balls stay down.

Clear the assigned group before the shot that legally pots the 8. An early or
foul 8 loses, including potting the last group ball and 8 together. An 8 on the
break reracks with the same breaker, including when scratched. No called pockets,
four-ball break-spread requirement, spin, jump shots, push-outs or respotted
object balls. Remaining group balls are the score display. The
[rule outcome table](pool-rules.md) specifies precedence, mixed pots and fouls.

## Physics and architecture

Separate table, game, input, clock, physics, rules, AI, rendering and sound modules
preserve SDK/platform boundaries. Q12 integer positions/velocities, 64-bit products
and an embedded angle table avoid runtime floating-point/trigonometry. Physics
runs at 60 ticks/second with four substeps, bounded sequential contact correction,
swept grazing checks and finite mouth/jaw capture. Rolling friction reaches exact
rest; cushion restitution is 0.85. Equal-mass contacts exchange normal motion.

The solver is deterministic for identical commands/ticks, but simultaneous
contacts are not globally chronological. Catch-up is capped at 100 ms: sustained
overload slows game time instead of increasing the physics step. It is an arcade
simulation without spin. Accepted trajectory and AI replay baselines are preserved.
The app requests a 1 MiB heap and 16 KiB stack; the logical framebuffer is 450 KiB.
The final executable reports 159,004 text, 496 data and 337 BSS bytes (159,837 static
bytes). These are not measurements of peak runtime memory or stack use.

## Distribution audit and changes in this stage

Pool follows the existing `apps/<name>/Makefile` and SDK build layout. Directory
discovery already includes Pool in `apps/build.sh`; CI packages the application
root filesystem into host and Tab5 artifacts. No central build-script entry or
OS/SDK/platform change is needed.

Added a Pool-local license-install target, following Kilo's pattern. Normal
`make -C apps/pool` / `./apps/build.sh` now includes
`T:/share/licenses/pool/LICENSE` in the root filesystem used by release packaging.
The existing MSC script already copies source notices. Separate binary transfers
must preserve the MIT notice too. Original procedural assets and PCM are MIT;
the Starfall font notice is retained unchanged. There are no runtime asset files.

Updated the app/asset README, application catalogue and game guide; synchronized
hardware evidence and stage authorization in the context/checkpoint documents.
Generated includes reproduce exactly and are compiler-tracked dependencies.
Python is only a development-time generator dependency. No debug game modes,
temporary binaries, user recordings or unrelated cleanup were added.

This stage changes `apps/pool/Makefile`, `apps/pool/README.md`,
`apps/pool/assets/README.md`, `docs/README.md`, `docs/applications.md`,
`docs/pool.md`, `docs/pool-progress.md`, `agents/roadmap.md`, `agents/testing.md`,
and adds this report. Application C, physics, rules, AI and test behavior are
unchanged. Existing untracked `apps/snake.zip` remains excluded.

## Reproduction commands

From the repository root:

```sh
./apps/build.sh
./tools/tabos macos debug build
./tools/tabos macos release build
ctest --test-dir build/macos-debug --output-on-failure
ctest --test-dir build/macos-release --output-on-failure
./tools/tabos tab5 debug build
./tools/tabos tab5 release build
```

With the RISC-V toolchain active (`eval "$(./tools/tabos activate-idf)"`):

```sh
make -C apps/pool
make -C apps/pool build BUILD_DIR=/tmp/pool-review-clean
python3 apps/pool/tools/generate_angles.py
python3 apps/pool/tools/generate_sound.py --check
git diff --exit-code -- apps/pool/assets/sine_q12.inc apps/pool/assets/sound_pcm.inc
riscv32-esp-elf-gcc -march=rv32i_zicsr_zifencei -mabi=ilp32 -std=c17 \
  -DTABOS_APPLICATION=1 -Isdk/include -Iapps/pool/include \
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
  -fsyntax-only apps/pool/src/*.c
clang-format --dry-run --Werror apps/pool/src/*.c apps/pool/include/pool/*.h \
  tests/unit/pool_*.c tests/component/pool_rv32.c
build/macos-debug/tests/tabos_pool_rv32 build/apps/shell/shell build/apps/pool/pool
POOL_TEST_WINDOW=1 build/macos-release/tests/tabos_pool_rv32 \
  build/apps/shell/shell build/apps/pool/pool
git diff --check
```

Use a fresh temporary directory for an independent clean build. The optional
harness exercises actual RV32 binaries against temporary storage; window mode
also exercises real SDL audio/video. Launch manually with
`./tools/tabos macos debug run`, then `pool` at the TabOS shell. For device
installation, follow [build and run](pool.md#build-and-run).

## Validation evidence

| Evidence | Result |
| --- | --- |
| All-app discovery build and normal installation | Passed; Pool and its MIT notice installed. |
| Independent clean Pool build/install | Passed; temporary executable and notice compare byte-for-byte with normal outputs. |
| macOS Debug / Release build | Both passed. |
| Full Debug CTest | 82/82 passed, 37.82 s; configured ASan/UBSan coverage. |
| Full Release CTest | 82/82 passed, 11.16 s. |
| Actual RV32 Release, real SDL window/audio | Passed local cycles and computer lifecycle. |
| Actual RV32 Debug, headless | Passed local cycles and computer lifecycle. |
| Tab5 Debug / Release cross-build | Both passed; no flash performed. |
| Strict Pool C warnings / formatting | Passed. |
| Generated sine/PCM includes | Reproduce exactly; all six PCM golden hashes pass. |
| Metadata, output discovery and asset dependencies | Reviewed; one extensionless executable, embedded assets. |
| Physical Tab5 | User confirms smoother gameplay and working speaker audio after fix. MSC SHA-256 matches the fixed app. |
| Linux execution | Not performed on this Mac. |

Existing build warnings remain: host duplicate `libtabos_core.a` linkage, SDK RV32
RWX LOAD segment, and Tab5 Debug application partition approximately 1% free.
These predate Pool; its separate SD executable does not enlarge the firmware
partition. No warning cleanup outside Pool was undertaken.

Final normal/clean/Mac-installed executable SHA-256 (also verified by readback
from `/Volumes/TAB5/bin/pool` before this packaging-only stage):

```text
e25d7dddfdfc53dea164c0521820ff007be5056f75900d57f006ec1898a85b62
```

Temporary validation logs use `/tmp/pool-stage10-*.log`; this report preserves the
results when those logs disappear. Earlier controlled host observations showed
the largest sampled moving-frame gap fall from 135 to 44 ms after the audio fix.
Those single-session measurements are not Tab5 frame-rate claims. Stage 10's
concurrent build/test runs are functional checks, not fresh timing benchmarks.

## Remaining limitations and recommendation

The physical app is user-tested, but firmware/display identity, measured frame
rate/input latency, break-shot load, CPU use, peak memory/stack and the specified
long-session/lifecycle protocol remain unverified. The compiler stack audit is
not proof of runtime margin. Headphones and every individual effect are not
physically validated. The audio stream stays open while unpaused/unmuted, even
between shots and at results; mute or pause releases it.

AI evaluates direct pots and contact fallbacks, without bank/kick planning,
lookahead or selectable difficulty. The sequential solver, small rack gaps and
house-rule simplifications remain deliberate. Soccer's separately reported
speaker silence is unresolved and outside this Pool delivery.

Review this checkpoint and proposed PR. Remaining hardware measurements can be
completed using the existing checklist; no additional feature stage is proposed.
No PR was opened, pushed, merged or published by this stage.

## Suggested PR

Title: **Add standalone Pool with house 8-ball rules and computer opponent**

Description:

> Add `pool`, a standalone keyboard-controlled 8-ball game for TabOS, with a
> computer opponent or local two-player mode. It includes bounded fixed-point
> physics, six pockets, turns/groups/fouls, ball-in-hand placement, title/help/result
> screens and optional speaker effects. Space/K shoots; Enter starts or confirms
> placement. Normal application builds discover Pool and install its MIT notice.
>
> macOS Debug and Release pass all 82 tests, actual RV32 gameplay harnesses pass,
> and both Tab5 configurations cross-build. The user confirms smoother physical
> Tab5 play and working speaker output; the installed app checksum was verified
> over MSC. Detailed hardware timing/resource/long-session checks and Linux
> execution remain outstanding. See `docs/pool-acceptance.md` and
> `docs/pool-hardware.md` for evidence and limitations.

PR scope is the Pool series relative to `b02595ab173220fd94f9ec7d2a1b38345a3411d1`.
The current `feature/arcade-games` branch also contains earlier Soccer work; use
an appropriate base or isolate the Pool commits before submitting to upstream.
Do not include the unrelated Snake archive. The exact series file list follows.

## Pool series changed files

```text
agents/roadmap.md
agents/testing.md
apps/pool/LICENSE
apps/pool/Makefile
apps/pool/README.md
apps/pool/assets/README.md
apps/pool/assets/font5x7.inc
apps/pool/assets/sine_q12.inc
apps/pool/assets/sound_pcm.inc
apps/pool/include/pool/ai.h
apps/pool/include/pool/clock.h
apps/pool/include/pool/game.h
apps/pool/include/pool/input.h
apps/pool/include/pool/physics.h
apps/pool/include/pool/render.h
apps/pool/include/pool/rules.h
apps/pool/include/pool/sound.h
apps/pool/include/pool/table.h
apps/pool/src/ai.c
apps/pool/src/clock.c
apps/pool/src/game.c
apps/pool/src/input.c
apps/pool/src/main.c
apps/pool/src/physics.c
apps/pool/src/render.c
apps/pool/src/rules.c
apps/pool/src/sound.c
apps/pool/src/table.c
apps/pool/tools/generate_angles.py
apps/pool/tools/generate_sound.py
docs/README.md
docs/applications.md
docs/images/pool-stage1.png
docs/images/pool-stage2.png
docs/images/pool-stage3.png
docs/images/pool-stage4.png
docs/images/pool-stage5.png
docs/images/pool-stage6.png
docs/images/pool-stage7.png
docs/images/pool-stage8-help.png
docs/images/pool-stage8-result.png
docs/images/pool-stage8-table.png
docs/images/pool-stage8-title.png
docs/pool-acceptance.md
docs/pool-hardware.md
docs/pool-progress.md
docs/pool-rules.md
docs/pool-stage0.md
docs/pool.md
tests/CMakeLists.txt
tests/check_application_api_boundary.cmake
tests/component/pool_rv32.c
tests/unit/pool_ai.c
tests/unit/pool_contacts.c
tests/unit/pool_game.c
tests/unit/pool_physics.c
tests/unit/pool_pockets.c
tests/unit/pool_render.c
tests/unit/pool_rules.c
tests/unit/pool_sound.c
tests/unit/pool_table.c
```
