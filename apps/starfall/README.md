# Starfall

Starfall is a standalone 640x360 arcade shooter demonstrating TabOS scaled RGB565
graphics, raw keyboard input, fixed-step timing, and persistent filesystem data.

## Controls

- A/S: move left/right
- K: start, fire, or restart
- P: pause
- Q or Escape: exit

Build and install it into the host root filesystem with:

```sh
make -C apps/starfall
```

Run `starfall` from the TabOS shell. High score is stored at
`T:/data/starfall/highscore.dat`. Storage failure is nonfatal. Starfall currently has
no audio because TabOS does not yet expose a public audio API.
