# Pool house 8-ball rules

These house rules were introduced in Stage 6. Stage 7 uses them unchanged for
human Player 1 versus computer Player 2 (`pool`) or two people sharing one
keyboard (`pool --two-player`). Stage 8 adds a welcome-screen mode selector;
Enter starts play. Player 1 breaks a new rack. This is a house
variant, not tournament compliance. Physics is unchanged.

The table stays open after the break. On a later legal shot, the first recorded
solid or stripe pot assigns that group to the shooter and the other group to the
opponent. Mixed pots use the solver's deterministic capture order. Until groups
are assigned, either group may be hit first, but not the 8. Once assigned, hit
one of your remaining balls first. After your group was completely cleared on
a previous shot, hit and pot the 8 to win. No pocket must be called.

A legal contact must be followed by any object pot or a cushion/jaw contact by
any ball. A rail reached only before cue contact does not count. The break uses
this same rule; there is no four-ball rail-spread requirement.

Shots resolve only when the remaining balls stop. This truth table defines
precedence, from top to bottom:

| Condition | Result |
| --- | --- |
| 8 potted on the break, including a scratch | Rerack automatically with the same breaker; no groups or winner. |
| 8 potted later, with any foul or your group not clear at shot start | Opponent wins. Potting the last group ball and 8 together loses. |
| 8 potted later, group already clear, 8 contacted first, no scratch | Shooter wins. |
| Cue scratched | Opponent takes the turn with ball in hand anywhere valid. |
| No cue-to-object contact | Opponent takes the turn with ball in hand. |
| Wrong first object, including 8 on an open table | Opponent takes the turn with ball in hand. |
| Correct first contact but no later rail/jaw or object pot | Opponent takes the turn with ball in hand. |
| Legal break with group pots | Shooter continues; groups stay open. |
| Legal later open-table group pot | Assign groups by first recorded pot; shooter continues. |
| Legal own-group pot, with or without opponent pots | Shooter continues. Opponent pots stay down. |
| Legal shot with only opponent pots or no pots | Opponent takes the turn; cue stays where it stopped. |

Foul messages use scratch, no contact, wrong first contact, then no rail/pot
precedence. Group pots on a foul stay down but never assign an open table.
There is no numeric points system: each player's remaining group count is shown.
Missing or scratching while legally targeting the 8 is a normal foul unless the
8 is also potted. All result states prevent further shots.

Ball in hand uses arrows/WASD and Shift for fine placement; Enter confirms only
a valid spot. It does not shoot. R opens a new-rack confirmation: Enter starts
a fresh Player 1 break; R or Escape cancels. The overlay freezes motion and
preserves the previous pause state. Q always exits. At a result, Enter starts
a new rack. H shows help without advancing play; H, Enter or Escape closes it.
M mutes/unmutes optional effects. Both new-rack controls require a fresh press; held keys do not auto-confirm.

No called pockets, spin, jump shots, push-outs, tournament break-spread,
respotted object balls or special tournament foul exceptions are implemented.
Contact and capture order follows the accepted bounded sequential solver;
nearly simultaneous events are not claimed globally chronological.
