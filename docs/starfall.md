# Starfall

Starfall is TabOS's first complete demonstration game. It uses a 640x360 RGB565
canvas scaled exactly 2x to the 1280x720 display, raw keyboard events, fixed-step
game timing, and persistent storage.

Build all applications and install them into the host root filesystem:

```sh
./apps/build.sh
```

Then launch `starfall` from the TabOS shell.

## Controls

- A/S: move left/right
- K: start, fire, or restart
- P: pause or resume
- Q or Escape: exit

The game stores its high score in `T:/data/starfall/highscore.dat`. Failure to read
or write the score does not prevent play.

All current artwork and bitmap glyphs were created specifically for Starfall. No
third-party or commercial-game graphics are included.
