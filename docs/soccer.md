# Soccer

Soccer is an original arcade soccer game for TabOS. Each side has five outfield
players and an automatic goalkeeper. You control the blue team, attacking right,
against red opponents who tackle, intercept and shoot at your goal. Matches last
three minutes of live play on a large scrolling pitch. Launch it as `soccer`.

## Build and run

The ordinary `./apps/build.sh` build automatically includes soccer. With the RISC-V
toolchain activated, `make -C apps/soccer` builds and installs it individually.
The executable is `build/apps/soccer/soccer`, installed in `.local/rootfs/T/bin/`.
The individual install also copies the MIT notice to
`.local/rootfs/T/share/licenses/soccer/LICENSE`. No external runtime assets are needed.

Start the macOS emulator with:

```sh
./tools/tabos macos debug run
```

Then enter `soccer` in the TabOS shell and press Enter at the title screen.
On Linux use `./tools/tabos linux debug run`. On Tab5 copy the executable to the
microSD card's `bin/` directory, or use the usual `./apps/build.sh --msc` workflow.

## Controls and rules

| Key | Action |
| --- | --- |
| Enter | Start a match; play again after full time |
| Left/Right or A/D on title/results | Cycle Easy, Normal and Hard |
| Arrows or WASD | Move; hold two directions for diagonals |
| J | Tap for a ground pass; hold for a lob; inactive without possession |
| K | Tap to shoot; hold for power; without possession, tackle or header |
| P | Pause/resume |
| M | Toggle sound; the setting lasts until you exit the game |
| R | Start a new match, clearing the score and counters |
| Q or Escape | Return to the shell |

Choose a difficulty on the title or full-time screen with Left/Right or A/D.
Normal preserves the original pace; Easy slows the red outfield players and Hard
speeds them up, while keeping them slower than your controlled player. Red through
balls adjust their arrival time to the chosen pace. Your movement, keeper rules,
match length and ball physics are the same across levels. The current level appears
on the title and full-time screens. Enter rematches and R resets retain your selection for this session;
relaunching the game starts at Normal. Levels cannot change during live or paused play.

Walk near the ball to collect it, then turn and dribble into position. Kick toward
the right-hand goal. Releasing movement preserves your facing direction. Diagonal
movement has the same intended speed as straight movement. When aiming toward the
opponent's goal, straight input targets its centre and the two forward diagonals
target upper and lower lanes derived from the goalmouth. Sideways and backward
input retain raw eight-way shots for clearances. A no-direction release uses the
player's facing direction. With possession, tap K for a quick shot or hold to fill
the gold power bar, then release to shoot. A 15-tick charge gives 125% speed and
the automatic 30-tick release gives 150%. Charges through 15 ticks retain the
chosen lane. Above that, upper/lower shots gain a deterministic outward error
that grows linearly, rounded up to whole world units, from one unit at tick 16 to
six at full power; centre shots remain centred.
You can move and change aim while charging, but movement adds no separate trajectory
bias. Opponents can still steal the ball. Passing, switching players, losing
possession, pausing or restarting cancels the charge. Holding the button after
firing never repeats the shot.
Tackles, headers and corner kicks still act on the initial press. Shots count only
kicks made with possession.

Gameplay uses eight-way movement and two action buttons. There is no sprint,
separate through-pass button, or manual goalkeeper control. Normal running speed
is retained. Opponents can still use through balls as part of their AI.

Tap J for a ground pass on release. Holding J for about 0.3 seconds automatically
sends a lob; continuing to hold never repeats it. You can aim and move while
holding. Losing possession, shooting, pausing or restarting cancels a pending
pass. A defensive J press does nothing and cannot turn into a pass if you collect
the ball while still holding it.

The yellow arrow identifies the controlled player; cyan arrows identify teammates.
J chooses a reachable teammate in front of you, favouring your facing direction
and penalizing passing lanes covered by an opponent, or sends a short ball in
your facing direction if no suitable receiver is available. The receiver moves
to meet the pass, and control transfers automatically on reception. Loose-ball
recoveries and successful tackles by blue players also select that player, so the
ball winner immediately responds to movement and passing. Defensive selection is automatic:
a clearly closer, ready outfield player must remain the best candidate for 0.2
seconds before control transfers. Switches have a one-second cooldown; tackles
and jumps protect your current selection. A player already near the ball stays
selected, and targeted blue passes keep control until reception. Goalkeepers
remain AI-controlled. J only passes; it never switches players.
Holding J with possession uses the same teammate selection but lofts the ball.
Assisted lobs use
less horizontal speed so they land near the receiver. Without a suitable receiver,
the ball is lofted in your facing direction. Its shadow stays on the grass while
the ball rises; high balls draw over the players. Outfield players must wait until
the ball is near ground level to control it. Uncollected balls bounce with decreasing
height before rolling to a stop.

When the ball is airborne and loose, K starts a jump. Use movement keys to
face the direction of the header. Contact is possible during the middle of the
jump when the ball is nearby and at a reachable height; the ball is directed
forward and downward. Each jump can contact the ball once, followed by a short
recovery before another jump. Jumping too early, too late or too far away misses.
The jumping player rises above their shadow, and a successful contact plays
a header sound. AI players can challenge opponents’ lobs; they leave
their own team’s lofted passes for the receiver. Ground control waits until landing.

The central title/pause banner shows `J PASS/LOB` and `K SHOT/ACTION`. Live play has no
contextual button hints, action messages or bottom controls bar over the pitch.
The shot-power meter remains visible while charging.

The live view shows only a yellow timer centred at the top, directly over the pitch
without a banner. Press P to see blue goals (B), red goals (R) and completed passes
(PASS) inside the pause banner.

Each team uses a 2–2–1: one striker, two midfielders and two defenders. Their
positions shift up and down the field with the ball. One nearby player pressures
or chases; the others offer passing options or recover toward their own goal.
After a targeted pass, a striker or midfielder offers a short forward, diagonal
return option for up to 2.5 seconds. The run starts when that player is AI-controlled,
keeps away from marked lanes and stops on a turnover or keeper possession.
Defenders retain their covering roles. Both teams use these support runs.
The attacking shape stays in place during targeted ground/lofted passes and keeper
possession. The striker and midfielders shift to an open lane beside their
normal position when a defender blocks the pass or marks the receiving space;
the two defenders retain their formation depth. When defending, the two defenders cover
distinct nearby attackers, moving to the goal side of their marks. The nearest
player still presses the ball. If an AI presser is recovering from a tackle, a
ready, grounded teammate within 120 world units can take over the pressure.
Your controlled player keeps responding only
to your movement input. Distant attackers do not draw defenders across the pitch;
unassigned defenders stay in their formation. Pass assistance also penalizes marked
receivers, including defenders just beyond the end of a pass.
Blue teammates can tackle and recover the ball automatically. Local avoidance
helps supporting players spread out. Opposing off-ball overlaps are gently separated
within pitch bounds. This never displaces the human-controlled player or separates
players challenging near the ball; physical body collisions remain simple.
Red ball carriers check a short route ahead and take an open diagonal dribble
when a defender blocks it. They reject routes too close to the touchline and keep
their normal approach when both alternatives are covered.
Red players pass to reachable outlets when pressured and shoot near goal. Before
shooting, they check the preferred corner for blocking outfield defenders, then
the other corner. If both lanes are blocked they can pass to an open outlet or
keep dribbling instead of automatically shooting. They
also play through balls to forwards or midfielders ahead of the carrier when
the passing route and receiving space are open. These passes travel more slowly
than blue through balls to match the red runners. Close to the goal line they
keep the normal pass/shot options instead of leading the ball toward the keeper. They
run more slowly than your controlled player so you have time to respond.
Chase them with the automatically selected player, face the carrier,
and press K to commit to a short feet-first sliding tackle. The slide lasts
0.3 seconds, travels up to 36 world units and slows from 3 to 1 world unit per
tick. It keeps the chosen direction, with a low body pose and a grass trail that shortens as the slide slows.
The selection arrow stays at its usual height, and extended poses remain visible
until they leave the viewport.
Contact within roughly 30 world units can win the ball at the start or during
the slide, once per attempt. Steering and pass/shot input resume when the slide
ends; automatic defensive selection waits for it too. A successful tackle
wins possession without immediately shooting. A new tackle is blocked for 0.7 seconds from the start of the attempt,
and a newly won ball is briefly protected against an immediate counter-tackle.
AI defenders turn toward the carrier before a close tackle, preventing repeated
wrong-way attempts after reaching the ball. Human tackles still use your facing
direction. Missed tackles consume the attempt too; holding K does not repeat it. J cannot
command a red player to pass. Red carriers advance toward your goal and aim shots
across the goalkeeper toward the far corner. Your unselected teammate still waits
for your input when holding the ball; control transfers to the ball winner.

A goal counts once the whole ball crosses either goal line between the posts,
including own goals. Each goalmouth spans world Y 408–552. With the four-unit
ball and post radii, a ball centre from Y 416 through 544 can score, inclusive;
the whole ball crosses at four world units beyond the goal line. After a short
celebration, the team that conceded takes a centre kick-off. Players stay still
during the brief kick-off banner. The match
clock stops during celebrations, kick-offs, out-of-play restarts and pauses. At zero, play freezes and
the result appears; a goal scored on the final live tick still counts. Enter plays
again, and R starts a fresh match at any time.

The full-time panel compares both teams’ shots, on-target attempts, saves and
possession. Shots count shooting actions, headers and direct corner shots;
on-target attempts are those that score in the opponent’s goal or are stopped
by its keeper. A parry counts once, even if the keeper later gathers the rebound.
Collecting a pass or dribble does not count as a shot save. Possession measures
live ticks with an outfield player or keeper holding the ball, excluding loose-ball
time, pauses and restart setup. Match statistics survive goals and kick-offs and
reset when you start a new match.

Both automatic keepers track the ball within the goalmouth. They have limited lateral
speed and reach, so shots into space can beat them. A caught ball is held briefly,
then released upfield toward a teammate. Keeper possession cannot be tackled or
controlled manually. The blue keeper wears cyan, the red keeper yellow, with white
gloves. Keepers also collect an opponent’s dribble within reach. They currently
catch low airborne balls as well as ground balls. A high lob can clear their
reach, but it must also fall below the crossbar to score. Crossbar impacts rebound;
balls passing above the bar produce a corner or goal kick based on the last touch.
For incoming low shots, keepers observe for five simulation ticks, sample the
predicted intercept once, and then commit to a lateral dive without recalculating
or reversing. Their standing range is world Y 420–540. Dives cover at most 45 world
units, followed by half a second
of recovery during which the keeper cannot catch another ball. They cannot reverse
a dive to chase a deflection. Close-range far-corner shots and high lobs can still
beat them. Diving and recovery have an original prone pose; successful saves retain
the existing brief hold and upfield release for caught balls. Shots arriving at
10 world units per tick or faster are parried back into play, with a short sideways
deflection. The keeper needs 0.3 seconds before collecting
again, so either team can reach the loose ball and attackers can take a second
shot. Charged shots are more likely to spill at close range; ground drag can slow
a long-range strike enough for a normal catch. A parry counts as the keeper’s
last touch for corner/throw-in decisions.

The ball is out only when it has wholly crossed a boundary, whether kicked or
dribbled. The last team to touch it determines the restart: the other team takes a
throw-in at a touchline; an attacker’s miss over the goal line gives a goal kick,
and a defender’s last touch over their own goal line gives the attackers a corner.
Post impacts still rebound. A diagonal exit near a corner uses the first line crossed.

The camera moves to the restart and the banner identifies the team and type.
There is a 1.5-second setup with play and the clock stopped. For blue throw-ins,
press J when ready; for blue corners, tap J for a short pass, hold J to loft it, or use movement keys
to face a direction and K to kick. Blue takes an automatic short restart
after three seconds if no button is pressed. Red restarts and both teams’ goal
kicks release automatically after setup. P pauses a restart; R starts a new match.

Restarts use simple arcade placement: the taker and a nearby outlet are repositioned,
with nearby players moved to make room. Throw-ins and corners start just inside the
line. Throw-ins and automatic restarts travel along the ground; holding J adds height to
a blue corner. There are no fouls, offsides, aerial throw animations
or half-time end changes yet. Dribbling across the goal line can score, including
an own goal; a keeper can intercept the dribble before it reaches the line.

The camera follows the ball independently of player selection, uses bounded ball-velocity
look-ahead, eases toward its target with a maximum of eight world units per axis per tick,
and ramps scrolling velocity by at most one world unit per tick (except stopping
at a pitch boundary). Reversals brake briefly before changing direction. The view
clamps at pitch boundaries. There is no radar overlay obscuring the pitch. New matches, kick-offs and out-of-play restarts immediately recentre it.
The pitch remains 1536 by 896 world units, with a 640 by 480 world-unit viewport.
It now accommodates 10 outfield players plus two keepers at the same sprite scale.
The formation spreads players across its width and limits team movement toward goal
so the whole side does not crowd the goal line. Only visible players submit drawing
commands. Pitch size and player density still need hands-on play testing on Tab5.
The smaller squads leave more room for passing and runs; the pitch has not been enlarged.

Passes, shots, headers, tackles, saves, posts, goals, kick-off and full time have distinct
original synthesized effects. M toggles sound, including from the title and pause
screens. Pausing, resetting or exiting stops queued audio. Sound is optional:
unavailable audio or a full output buffer does not stop gameplay. Effects are short,
quiet mono tones, generated offline without external recordings or game assets.
A playing cue finishes before an equal- or lower-priority cue can replace it;
higher-priority events interrupt immediately. This keeps a quick pass from
cutting off a save, post or goal sound. Suppressed effects are discarded, not
queued for delayed playback. Priority and timing reset on pause, mute or restart.
Audio opens before the match starts or resumes and stays open between effects,
including through the final whistle. Pause, mute, reset and exit release the stream;
start/resume/unmute prepare it again. This follows Pool’s hardware-tested lifecycle
and retains Snake’s 44.1 kHz mono speaker format. Device startup time is excluded
from match catch-up, and effects never open or close the device during simulation.

A playback failure disables effect writes until start/resume/unmute retries audio.
Queued samples are preserved on short-write failure rather than closing the device
immediately. On exit, any unresolved error is printed as `soccer: audio unavailable`.
If audio is silent, try M off/on, then check `audiotest tone speaker` and the error
printed after Q. The user confirmed working Soccer audio on the physical Tab5 on 2026-09-13.

The original waveforms are stored in `apps/soccer/assets/sound_pcm.inc`.
`python3 apps/soccer/tools/generate_sound.py --check` verifies reproducibility;
normal builds do not require Python to generate audio.

Action sounds identify shots, tackles, saves and post hits without text overlays. Fast balls leave
a short trail, and keepers raise their gloves while holding a standing save. Outfield
players have an eight-phase run cycle with directional foot movement, opposing arm
swing and a small body bob. Kicks extend and retract the striking foot, tackles use
a lower stance, and jumping players raise their arms for headers. These poses use
simulation ticks, so they freeze while paused without changing action timing or
collision reach. More detailed tactics remain for later stages. It uses original procedural player/ball/pitch
artwork. Goals use mirrored highlighted white frames with shaded edges, rear supports and
footings, a grass-visible sagging roof mesh, darker side netting and ground shadows.
The shared net bag extends 11 logical pixels (22 world units) behind either goal
line; its shallower depth does not affect the fixed goalmouth or collision geometry.
Off-screen net drawing is culled. The foreground posts draw over players; these are visual only and do not alter scoring
geometry or keeper reach. Its small text font comes from the MIT-licensed TabOS Starfall font.

After a held save, goalkeepers choose the nearest ready teammate ahead of them
within 300 world units, avoiding marked receivers and blocked passing lanes.
The receiver moves to meet the distribution, and blue reception transfers control
automatically. With no safe outlet, the keeper clears straight upfield.

During the kick-off countdown, directions aim the controlled kicker without moving
him. Held movement continues through the whistle, and the first pass uses that
aim. J/K still need a fresh press after the countdown.

## Local release bundle

With the TabOS RISC-V toolchain activated, run:

```sh
make -C apps/soccer package
```

This builds the current application, checks generated PCM and writes
`build/packages/soccer-tabos.zip`. The bundle contains the executable, MIT notice,
installation instructions, game guide, acceptance record and `SHA256SUMS`.
Packaging verifies every archived byte before replacing the previous bundle.
Fixed entry timestamps and permissions produce identical archives for identical
inputs with the same Python/compression toolchain. `PACKAGE_PATH=/path/file.zip`
overrides the output. This command creates a local bundle; it does not upload or
install anything on the Tab5. Copy the contents of its `T/` directory to the
matching directories on the TabOS drive when ready.

## Performance and endurance checks

Launch `soccer --profile` for an optional measurement session. Play normally and
press Q to return to the shell and see the report. It adds no gameplay overlay.
Counters accumulate across rematches and exclude title/pause/full-time waiting
and audio startup at UI transitions. Active kickoffs and restarts are included.
The report includes elapsed milliseconds, presented frames, simulation steps,
total/worst update + audio + presentation time, frames taking at least 17 ms,
and time discarded by the existing 100 ms simulation catch-up limit.
Presentation time can include VSYNC waits; these are wall-clock measurements,
not CPU utilization. Millisecond rounding and optional timing calls affect precision.
Compute average frame rate as `frames * 1000 / elapsed_ms`, and average work per
frame as `total_work_ms / frames`, when the denominators are nonzero.

For physical Tab5 acceptance:

1. Run one profiled match with sound on and one with sound muted, at the same
   difficulty. Record both reports and the firmware/application revision.
2. Play five consecutive matches in one launch, using Enter for each rematch.
   Include scrolling toward both goals, lobs, tackles, saves and goals.
3. During the session, pause/resume and mute/unmute several times. Check for
   stuck controls, sound loss, visual corruption or worsening frame pacing.
4. Quit to the shell and launch again three times. Confirm clean return and
   working controls/audio after each launch.
5. Record the report and any visible stalls. The intended simulation rate is
   60 steps per second; a nonzero discarded-time counter warrants investigation.
   Compare sound-on/off runs and early/late sessions before changing performance
   settings. A user-supplied one-minute report is recorded in
   [release acceptance](soccer-acceptance.md); the five-match run remains pending.

The host RV32 check supports `SOCCER_TEST_ROUNDS=6` (2–20) for repeated launch,
play, pause, reset and exit cycles. Its first launch also exercises `--profile`.
These short cycles complement the 12 full rules simulations; they do not run six
complete matches or establish Tab5 timing, memory endurance or speaker behavior.

## Implementation and validation

The rules suite also plays 12 complete matches using the two-button input path
across all three difficulties. It checks blue possession/control consistency on
every live tick, camera step limits, pause freezing and arrival at full time, with
tackles, passes, shots and restarts exercised. This is simulation validation, not
a measurement of hardware performance or a substitute for hands-on play testing.

A 320x240 RGB565 canvas scales uniformly to the display. World coordinates are
independent of rendering. The game uses fixed-point positions and a fixed 60 Hz
simulation, with bounded catch-up after slow frames. Rendering follows simulation
changes and presentation pacing; actual frame rate depends on the platform. Titles, full-time results
and pause screens wait for keyboard input. The application requests a 512 KiB heap
and 16 KiB stack. Simulation allocates no memory during gameplay; sound uses a
read-only PCM assets and a stream prepared at UI transitions. Each effect makes
at most eight nonblocking writes; errors defer cleanup to a UI transition. At most one effect
is submitted per rendered frame, with major events taking priority during catch-up.

`unit.soccer_game` tests movement, pause, possession, shot release, friction,
post rebounds, goal-line crossing, counters and automatic reset, support movement, assisted passes, receptions,
legacy internal switching/call-for-ball rules (not keyboard bindings), defensive
pressure, interceptions, tackles, recovery,
protection after possession changes and opponent ball carrying. Match tests cover
both goal lines, keeper saves and releases, beatable keeper reach, AI shooting,
kick-off ownership, timer boundaries, final-tick goals, full-time freezing and a
complete unattended match. 6-a-side tests check role spacing, cycling all five
outfield players, nearest-player defensive switching, marked pass lanes, blue
recoveries and red pass reception without changing human control. The optional
actual-RV32 harness exercises the installed SDK binary through the headless runtime:

```sh
ctest --test-dir build/macos-debug -R soccer --output-on-failure
build/macos-debug/tests/tabos_soccer_rv32 build/apps/shell/shell build/apps/soccer/soccer
```

`unit.soccer_sound` verifies distinct bounded waveforms, silent whistle gaps,
short writes, unavailable audio, nonblocking back-pressure, mute and repeated close.
Rules tests verify action events and priority when a shot immediately hits a post.
Restart tests cover both touchlines and ends, last-touch awards, diagonal exit order,
setup/input/automatic release, paused clocks, dribbled exits and goals, keeper
interceptions, final-tick expiry and reset. Aerial tests cover height-gated
collection, assisted landing/reception, bounce decay, freeze/reset, keeper reach,
low goals, high misses and crossbar rebounds at both ends. Header tests check
contact timing, height/range misses, direction, one contact per jump, cooldown,
pause/reset and red challenges without changing human control.
The RV32 harness also toggles mute during the title and a paused match, then
holds J and checks that a lofted pass is received.

An optional final filename saves a PPM screenshot. The harness uses temporary storage.
The supplied short profile is recorded in [release acceptance](soccer-acceptance.md).
Sustained Tab5 performance and diagonal-plus-action combinations still need
acceptance checks; a host test does not establish hardware performance.

## Planned progression

1. Solo movement and shooting drill (complete).
2. Passing, receiving, a supporting teammate, and player switching (complete).
3. Opponents, stealing and tackling in a small-sided drill (complete).
4. Larger pitch and scrolling camera (brought forward and implemented).
5. Goalkeepers, scoring at both ends, kick-offs and timed matches (complete).
6. 6-a-side formations, defensive recovery and multi-player passing (this build).
7. Sound, action feedback and practical throw-in/corner/goal-kick restarts
   (implemented), plus lofted passes, ball height and bounces. Player
   animations, timed headers and goalkeeper dives are implemented.

The optional `SOCCER_TEST_WINDOW=1` environment setting runs `tabos_soccer_rv32`
with the real host window/audio backend to exercise device startup and teardown.
The default headless harness cannot validate speaker output or hardware latency.

The audio correction passed Debug/Release sound tests and actual RV32 gameplay
with both headless and real macOS SDL backends. These checks validate the host
path. The user subsequently confirmed working physical Tab5 audio on 2026-09-13.

## Release completion stages

1. Gameplay stability: automated checks pass, including 48 stationary ball contests,
   possession handoffs, off-ball overlap, restarts and 12 complete matches. Confirm
   the reported sticking is resolved during physical Tab5 play.
2. Controls and camera tuning: camera acceleration/braking is implemented and
   covered by host tests; Tab5 play-feel validation remains.
3. AI and match balance: blocked-shot decisions are implemented and tested across
   difficulties, along with ready-teammate pressure during tackle recovery; overall
   difficulty/scoring balance still needs play testing. Short dribble avoidance is
   also implemented and tested across difficulties.
4. Presentation and sound timing: short effect priority protection is implemented
   and tested; animation poses already follow simulation ticks and freeze on pause.
   Confirm the combined presentation and audio timing on Tab5.
5. Performance/endurance: optional profiling and repeated host launch checks are
   available; a user-supplied short report averages about 60 FPS with no discarded
   catch-up. Five consecutive Tab5 matches remain.
6. Release audit: documentation and individual license installation corrected;
   see [release acceptance](soccer-acceptance.md) for validation and remaining gates.

## Player and ball artwork

The approved compact directional artwork is standard for both teams, the ball and
goalkeepers. It includes displacement-driven running, distinct pass/shot follow-through,
headers, slide recovery, catches and dives. It is the only runtime visual style.
See [the visual review](soccer-visual-trial.md) for previews, checks and limits.
