#!/usr/bin/env python3
"""Render the original/trial review sheets and loops using the production C artwork.
Requires an existing host C compiler and ffmpeg; neither is a game dependency.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'build/soccer-visual-review'
OUT.mkdir(parents=True, exist_ok=True)

def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)

run('cc', '-std=c17', '-Iapps/soccer/include', '-Isdk/include',
    'apps/soccer/tools/visual_preview.c', 'apps/soccer/src/game.c',
    'apps/soccer/src/visuals.c', 'apps/soccer/src/camera.c', '-o', OUT / 'preview')
run(OUT / 'preview', OUT)
for name in ('players-before', 'players-after', 'actions-before', 'actions-after',
             'ball-before', 'ball-after', 'pitch-before', 'pitch-trial', 'rollout-actions'):
    run('ffmpeg', '-v', 'error', '-y', '-i', OUT / (name + '.ppm'),
        '-vf', 'scale=1280:960:flags=neighbor', OUT / (name + '.png'))
run('ffmpeg', '-v', 'error', '-y', '-framerate', '15', '-i', OUT / 'motion-%03d.ppm',
    '-filter_complex', 'scale=960:720:flags=neighbor,split[a][b];[a]palettegen=max_colors=64[p];[b][p]paletteuse=dither=none', '-loop', '0', OUT / 'motion.gif')
run('ffmpeg', '-v', 'error', '-y', '-framerate', '15', '-i', OUT / 'motion-%03d.ppm',
    '-filter_complex', 'crop=320:90:0:150,scale=960:270:flags=neighbor,split[a][b];[a]palettegen=max_colors=64[p];[b][p]paletteuse=dither=none', '-loop', '0', OUT / 'ball-motion.gif')
for variant in ('before', 'trial'):
    run('ffmpeg', '-v', 'error', '-y', '-framerate', '15', '-i', OUT / ('play-' + variant + '-%03d.ppm'),
        '-filter_complex', 'scale=640:480:flags=neighbor,split[a][b];[a]palettegen=max_colors=96[p];[b][p]paletteuse=dither=none',
        '-loop', '0', OUT / ('play-' + variant + '.gif'))
(OUT / 'review.html').write_text('''<!doctype html><meta charset="utf-8">
<title>Soccer visual rollout review</title>
<style>body{background:#101e27;color:#eef0dd;font:16px system-ui;max-width:1400px;margin:32px auto;padding:0 20px}
img{image-rendering:pixelated;max-width:100%;height:auto}.pair{display:flex;gap:20px;flex-wrap:wrap}
figure{margin:10px 0}figcaption{margin-bottom:8px}a{color:#ffd450}.sheet{width:640px}.native{width:320px}</style>
<h1>Soccer: approved player and ball rollout</h1>
<p><code>soccer</code> uses the approved art across both teams and goalkeepers.
The old style below is retained only in this offline historical comparison fixture.
Gameplay and physics are unchanged.</p>
<p>These images use the production renderer with scripted review snapshots, not Tab5 captures.
The rollout adds pass/shot, header, slide/recovery, catch and keeper-dive poses.</p>
<h2>Pitch at native logical resolution</h2><div class="pair">
<figure><figcaption>Original</figcaption><img class="native" src="pitch-before.png"></figure>
<figure><figcaption>Approved style across both teams</figcaption><img class="native" src="pitch-trial.png"></figure></div>
<h2>Team rollout: active slide, recovery slide and goalkeeper dives</h2>
<img class="native" src="rollout-actions.png"> <img width="640" src="rollout-actions.png">
<h2>Simulation-driven running and shot (same replay, two renderers)</h2>
<p>The fixture keeps initial possession protected, then applies NE/E/SE movement and a tap shot
through the actual game, visual observer and camera updates. This is a host fixture, not hardware video.</p>
<div class="pair"><figure><figcaption>Original</figcaption><img width="640" src="play-before.gif"></figure>
<figure><figcaption>One trial player + ball</figcaption><img width="640" src="play-trial.gif"></figure></div>
<h2>Directional running and kick sequence</h2>
<p>Four run poses → held-charge anticipation → release/contact → recovery. Tap passes start at contact.
The poses loop in place for comparison; in the game run phase follows measured displacement.</p>
<img class="native" src="motion.gif"> <img width="640" src="motion.gif">
<h2>Run sheets: N, NE, E, SE, S, SW, W, NW; four frames top to bottom</h2>
<div class="pair"><img class="sheet" src="players-before.png"><img class="sheet" src="players-after.png"></div>
<h2>Action sheets: anticipation, contact, recovery</h2>
<div class="pair"><img class="sheet" src="actions-before.png"><img class="sheet" src="actions-after.png"></div>
<h2>Ball: still, rolling panels, airborne with ground shadow</h2><img width="960" src="ball-motion.gif">
<p>Four panel frames on both grass colours and the pitch marking colour. The outer silhouette is fixed.</p>
<div class="pair"><img class="sheet" src="ball-before.png"><img class="sheet" src="ball-after.png"></div>
<p>Click image filenames in this folder for full 4× sheets. Hardware feel and performance still require device play.</p>
''')
print(OUT / 'review.html')
