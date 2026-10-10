# Pool assets

`font5x7.inc` is the original TabOS Starfall HUD font, copied from
`apps/snake/assets/font5x7.inc`. The MIT notice is preserved in `../LICENSE`.
The compact 3×5 ball digits and all table/ball artwork are procedural drawing
in `src/render.c`. There are no runtime asset files or external recordings.

`sine_q12.inc` is a generated 1025-entry integer quarter-wave sine table, compiled
into the executable. Regenerate from the repository root with
`python3 apps/pool/tools/generate_angles.py`. The generator uses Python math only
at development time; application execution uses no floating-point operations.

`sound_pcm.inc` contains six original mono PCM16 effects at 44.1 kHz, embedded
in the executable. Regenerate with `python3 apps/pool/tools/generate_sound.py`
or verify with `python3 apps/pool/tools/generate_sound.py --check`. These replace
runtime synthesis while preserving the original effect samples. Python is needed
only for regeneration/checking; normal builds use the committed includes.
