# Soccer

Standalone TabOS arcade soccer application, currently 6-a-side timed matches with formations, ground/lofted/through passing, charged shots, headers, goalkeeper dives, tackling, scrolling, synthesized sound and throw-in/corner/goal-kick restarts.
See [the game guide](../../docs/soccer.md) for controls, builds and milestone scope.

`sound.c` plays generated original PCM through a public audio stream prepared
before play and reused between effects. `tools/generate_sound.py --check` verifies
the committed samples; normal builds use the assets directly.
`game.c` owns deterministic world state and physics; `render.c` owns the original
procedural art and tick-driven running/kicking/tackling/header poses; `camera.c` owns smooth world-to-view tracking, and `main.c` owns public SDK input,
timing and application lifetime. The expanded pitch scrolls in both directions.
No SDL, ESP-IDF, or kernel dependencies are used by the application.

Gameplay uses arrows/WASD plus J and K only: tap/hold J for ground/lofted passes,
automatic defensive selection, and tap/hold K for shots, tackles or
contextual headers. J does nothing without possession. Control follows receptions and recoveries automatically.
The camera follows the ball with bounded scrolling speed, independently of selection.

`make -C apps/soccer` installs the executable and its MIT notice at
`T:/share/licenses/soccer/LICENSE`. Sounds and glyphs are compiled into the game;
no separate runtime assets are required. Use `soccer --profile` to print timing
statistics after quitting. See [release acceptance](../../docs/soccer-acceptance.md)
for recorded evidence and the remaining endurance checks.

`make -C apps/soccer package` builds and verifies the local installation archive at
`build/packages/soccer-tabos.zip` using Python 3 and the activated RV32 toolchain.

The approved compact directional artwork covers both teams, goalkeepers, running,
passing, shooting, headers, slide recovery and keeper dives. It is the game's only
runtime visual style. See [the visual review](../../docs/soccer-visual-trial.md).
