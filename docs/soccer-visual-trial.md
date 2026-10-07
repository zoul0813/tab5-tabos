# Soccer player and ball visual rollout

Status: trial approved; team-wide rollout implemented.

Run `soccer` for the approved artwork across both teams and goalkeepers. It is the
only runtime visual style. Use `soccer --profile` for a Tab5 timing measurement.

## Inspection

The game draws procedural RGB565 rectangles/pixels, not loaded sprite textures.
The 320×240 logical canvas scales uniformly by an integer factor (3× on a
1280×720 display). Undrawn pixels are transparent; no alpha buffers or new assets
are loaded. The ground position anchors both sorting and sprite placement.
Original standing players are roughly 15×24 pixels, plus marker/shadow and extended
limbs. The original ball is approximately 5×6 pixels with a dominant 2×2 dark patch.

Eight-way facing is already in the rules. Original front/side poses mostly share
one body; rear art has limited variation. Running uses an eight-phase stride
selected from an intent-driven animation counter, which can advance against a
boundary. Kicks use a single extending boot, with `kick_ticks` starting at ball
release. Thus both artwork and animation selection contribute to stiffness.
Ball spin previously used `(x+y)%3`, which can freeze on a diagonal even while
moving. Ball height already raises its draw position above a separate ground
shadow; the trial reuses that system and its depth sorting.

## Approved scope

- Both outfield teams, both goalkeepers and the ball use the approved proportions
  and palette treatment by default.
- Five canonical upright views (front, rear, side and two diagonals), mirrored for
  eight directions. Side heads have profile noses; rear views omit faces.
- Four run frames use measured world displacement, with an octagonal distance
  approximation. Blocked/stationary players return to planted feet without cycling.
  The ground/shadow anchor is fixed; intentional torso bob is one logical pixel.
- Opposing arm/leg movement, connected hip/knee kick geometry, and a small rear
  3×4 number replace the dominant front number. No number is forced onto profiles.
- Held pass/shot charge supplies anticipation. Passes use a compact side-foot
  extension; shots use a stronger follow-through. On release the contact pose starts
  immediately and recovers over the existing kick timer, retaining its direction.
  Instant taps have no pre-release anticipation: inventing one would delay gameplay.
- Ball diameter is fixed at 5×5 logical pixels, with off-white leather, a fixed
  shaded lower edge and three asymmetric interior panel pixels. Four rolling frames
  follow ground displacement, including diagonals; stationary balls do not cycle.
  Airborne panels keep their last phase while height separates the ball and shadow.
- Headers lift the same directional body with raised arms. Slide tackles use a low,
  extended silhouette and settle into a braced final phase. Goalkeepers share the
  compact body proportions, with visible gloves, catches and directional dives.
- `visuals.c` observes const game state for all 12 actors in a fixed-size struct. It does not
  touch movement, collision, AI, ball physics, events, timing, camera or pitch.
  No per-frame allocations, textures, file loading or new game dependencies.

## Reproduce review material

Using the existing host compiler and ffmpeg (preview tools only):

```sh
python3 apps/soccer/tools/visual_review.py
```

Open `build/soccer-visual-review/review.html`. It contains native logical-size
and enlarged comparisons. `players-before/after.png` show all eight directions
and four run frames; `actions-before/after.png` show anticipation, contact and
recovery. `motion.gif` loops the directional poses. `ball-before/after.png` show
all panel frames on both grass stripe colours and white pitch markings;
`ball-motion.gif` shows stationary, rolling-panel and airborne states.
`pitch-before.png` and `pitch-trial.png` compare an identical in-game scene with
the trial player next to original players.

These are scripted snapshots rendered by production C code, not hardware captures.
`play-before.gif` and `play-trial.gif` render the same short movement/shot replay
using the actual rules, observer and camera, with initial possession protected
for the fixture. They show translation as well as animation.
The pose loop intentionally runs in place for side-by-side review; actual running
phase is displacement-driven. The airborne preview supplies existing height values
without adding physics. PPM source frames are retained beside the PNG/GIF outputs.

## Validation and remaining limits

Debug/Release rules, sound, visual observer and application-boundary checks pass.
Observer tests cover blocked/stopped motion, both-team travel, diagonal ball travel,
paused state, airborne phase, action-facing retention, restart resets and byte-for-byte unchanged
game state. The actual RV32 harness passes two approved-style launches, including
passes, lobs, pause, sound toggles,
charged shots, reset and exit. The existing full-match simulations also pass.

The game/physics/camera sources are unchanged. Sprite sheets and native
logical-size pitch images were visually inspected, including connected kicking
legs, fixed anchors and ball contrast. Host checks do not establish Tab5 frame rate,
input-to-display feel or subjective running naturalness. The offline review fixture
retains its historical before/after images; the game executable does not expose the old style.
