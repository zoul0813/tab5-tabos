# Soccer release acceptance

## Supplied performance report

On 2026-09-14 the user supplied a `soccer --profile` screenshot with:

| Counter | Result |
| --- | ---: |
| Active elapsed time | 61,367 ms |
| Presented frames | 3,683 |
| Simulation steps | 3,682 |
| Total update/audio/present time | 49,754 ms |
| Worst update/audio/present time | 21 ms |
| Frames taking at least 17 ms | 26 |
| Discarded catch-up time | 0 ms |

Derived averages: 60.02 presented frames/s, 60.00 simulation steps/s and
13.51 ms update/audio/present time per frame. About 0.71% of frames reached
17 ms. Presentation includes display waits; the work counter is not CPU usage.
This is encouraging short-session evidence, not a five-match endurance result.
The screenshot does not identify target, firmware revision, difficulty or mute
state; those details were not independently verified. The profiling feature was
introduced in local commit `d1baa6a`.

## Release audit

- Original procedural graphics and generated PCM are maintained in the repo.
- The inherited Starfall/Snake glyph notice matches Soccer's MIT notice.
- Individual installs now include `T:/share/licenses/soccer/LICENSE`, following
  Pool's convention. Bulk MSC copying already includes application notices.
- The executable embeds all glyphs and PCM; no runtime asset download is needed.
- J remains pass-only; the public keyboard scheme remains movement plus J/K.
  Some legacy rule-level input fields remain covered by tests and are not exposed
  by the keyboard adapter. Removing them is a separate internal refactor.
- Current validation commands and hardware protocol are in [the guide](soccer.md).

## Validation on 2026-09-14

- Debug (sanitizers) and Release Soccer rules, sound and application-boundary
  checks passed, including the 12 complete rules simulations.
- A fresh RV32 build in an isolated directory installed the binary and MIT notice;
  both installed files matched their sources byte for byte.
- PCM regeneration check passed. No generated asset changes were necessary.
- The earlier six-cycle host launch check passed; the fresh install also
  passed both actual RV32 gameplay rounds before packaging.
- Local bundle: `build/packages/soccer-tabos.zip`, containing `T/bin/soccer`,
  `T/share/licenses/soccer/LICENSE`, the guide, this acceptance record and SHA-256
  checksums. Copy the contents of `T/` to the matching directories on the TabOS
  drive. This is a local pre-release application bundle, not firmware; use it
  with the matching current TabOS SDK/runtime. The application ABI is not frozen.

## Subsequent gameplay change

Sliding tackles now add 18 ticks of committed, decelerating movement and a low
feet-first pose with grass trails. Repeat the gameplay/performance acceptance
with this version: the earlier one-minute profile predates the slide animation.

## Final slide regression on 2026-09-15

Both AI teams now have exact slide-distance checks in all eight directions. The
new test reproduced an extra normal movement on the last slide tick; the update
loop now allows only the slide movement on that tick, including control handoffs.
This does not change the intended 18-tick duration or 36-unit maximum distance.

## Repeatable packaging

`make -C apps/soccer package` rebuilds as needed, checks PCM reproducibility and
creates the local bundle with installation instructions and SHA-256 checksums.
The packager verifies ELF architecture and archive contents before atomically
replacing an existing archive. Fixed entry metadata makes identical inputs
reproducible within the same Python/compression toolchain.

## Visual polish on 2026-09-15

Player culling now includes extended slide/dive poses at viewport edges. Sliding
selection arrows keep the standing arrow height, and grass trails shorten in
three phases as the slide slows. Gameplay timing and tackle reach are unchanged.

## Approved art rollout on 2026-09-23

The approved player/ball style is standard across both teams and goalkeepers and
is the only runtime visual style. See [the visual review](soccer-visual-trial.md).

## Stage C goal and shooting implementation on 2026-09-28

The goalmouth is now world Y 420–540, with legal ball-centre scoring positions
Y 428–532 inclusive and keeper standing positions Y 432–528. Forward human shots
use derived upper, centre and lower goal lanes; side/back shots retain raw eight-way
direction. The existing 100/125/150% charge speeds remain. Above 15 ticks, an
upper or lower target receives a deterministic outward error that grows to six
world units at full charge, using upward whole-unit rounding from tick 16; centre
shots have no placement penalty.

Keepers retain their prior tracking, catch, parry, committed dive and recovery
values. They now observe an incoming shot for five simulation ticks, sample its
intercept once, and cannot reverse after committing. AI upper/lower shot targets
use the same derived goal geometry. Automated validation covers exact scoring
limits at both ends, post/crossbar behavior, high-speed swept crossings, mirrored
shot intent, exact charge speeds, deterministic placement, keeper reaction and
the existing gameplay regressions. All 91 Debug tests and all 91 Release tests
passed, as did two actual RV32 gameplay rounds in each host build and the PCM
reproducibility check. The RV32 linker retains its existing executable-RWX warning.
Goal size, keeper reaction and shot placement still require emulator and Tab5
play testing before further tuning.

## Stage D goal-size visual tuning on 2026-09-28

The goalmouth alone increased to world Y 408–552, a 144-world-unit or
72-logical-pixel opening. Existing formulas now derive the inclusive scoring
range as Y 416–544, the keeper standing range as Y 420–540, and the shot lanes
as upper 432, centre 480 and lower 528. Both renderers and both ends consume
the shared bounds. Shooting, keeper timing and reach, AI decisions, controls,
pitch and penalty-area geometry are unchanged. All 91 Debug tests, all 91 Release
tests, and two actual RV32 gameplay rounds passed.

## Stage E goal artwork proportion tuning on 2026-09-28

The mirrored renderer's net depth decreased from 14 to 11 logical pixels, or
from 28 to 22 world units, a 21.4% reduction. Its existing mesh span and tie
points were shortened proportionally through one shared rendering path. The
front posts and the accepted 408–552 gameplay opening remain fixed; no rules,
shooting, keeper, AI, controls, pitch or penalty-area code changed. All 91 Debug
tests, all 91 Release tests and two actual RV32 gameplay rounds passed.

## Remaining acceptance

The user explicitly deferred the physical endurance session until next time.


Record five consecutive complete Tab5 matches, sound-on/off timing comparison,
pause/mute recovery, repeated exits/relaunches and diagonal-plus-action input.
Retain the exact firmware/game revision and target with the reports. Confirm
camera feel, absence of sticking, match balance and the latest sound priorities.
The existing physical audio confirmation remains valid for the earlier lifecycle
fix; host checks alone do not validate these later presentation changes.

No remote branch, release or download has been published by this audit.
