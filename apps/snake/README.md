# Snake

Snake is a classic Snake game for TabOS.

## Running

From the TabOS shell:

    snake

## Controls

- Arrow keys: move the snake
- Q or Escape: quit to the shell

## Building

From the TabOS repository root:

    make -C apps/snake

`src/game.c` contains deterministic game rules, `src/render.c` draws the logical
canvas, `src/sound.c` synthesizes brief PCM effects, and `src/main.c` owns public SDK input, graphics, and deadline waits.
The bundled 5x7 font is copied from Starfall under its MIT license, preserved in
`LICENSE`. All other visuals are drawn with graphics primitives.
