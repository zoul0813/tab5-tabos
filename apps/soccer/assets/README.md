# Assets

Pitch, net, ball, player poses, selection marker and HUD layout are original
procedural artwork in `src/render.c`. No commercial-game graphics, sounds or code
are used. `font5x7.inc` is copied from the TabOS Snake/Starfall MIT font; its
copyright and permission notice are preserved in the application `LICENSE`.

Sound effects are original, generated offline by `tools/generate_sound.py` into
`assets/sound_pcm.inc` and played by `src/sound.c`. No recordings or external game
audio are included. Run the generator with `--check` to verify the committed PCM.
