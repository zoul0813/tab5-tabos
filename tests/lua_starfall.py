"""Exercise shipped Starfall logic, rendering and storage in the native Lua profile."""
import pathlib
import subprocess
import sys
import tempfile

executable = pathlib.Path(sys.argv[1]).resolve()
source = pathlib.Path(sys.argv[2]).read_text()
prefix, marker, _ = source.partition("-- Fixed 60 Hz simulation")
assert marker, "Starfall simulation boundary missing"
checks = r'''
-- Storage is a real isolated host file, using the installed drive-shaped path.
assert(load_score() == 0)
assert(save_score(12345) and load_score() == 12345)
assert(save_score(23456) and load_score() == 23456)
assert(save_score(1000000) and load_score() == 999999)
for _, bytes in ipairs({"garbage", "-1", "1000000", "123junk", "1\n2", ""}) do
    local f = assert(io.open(score_path, "wb")); assert(f:write(bytes)); assert(f:close())
    assert(load_score() == 0, bytes)
end
assert(os.remove(score_path))
local saved_path, saved_temp = score_path, score_temp
score_path, score_temp = "missing/score", "missing/temp"
assert(load_score() == 0 and not save_score(100))
score_path, score_temp = saved_path, saved_temp
-- A failed replacement keeps the last good score and removes the temp file.
assert(save_score(42))
local rename = os.rename
os.rename = function() return nil, "injected failure" end
assert(not save_score(99) and load_score() == 42)
assert(not io.open(score_temp, "rb"))
os.rename = rename

-- Known uint32 xorshift vector, independent of host word width.
game.random_state = 1234
assert(random_next() == 332584831)
assert(random_next() == 1855942593)
game.high_score = 900
initialize()
assert(game.mode == "title" and #game.stars == 96)
render()
start()
assert(game.mode == "playing" and game.lives == 3 and game.wave == 1)
assert(#game.shots == 32 and #game.enemies == 24 and #game.particles == 64)
update(false, true, false)
assert(game.player_x == 314)
for _ = 1, 9 do update(false, false, true) end
assert(game.shots[1].active and game.shots[2].active and not game.shots[3].active)
game.player_x = 0; update(true, false, false); assert(game.player_x == 0)
game.player_x = 620; update(false, true, false); assert(game.player_x == 620)
local tick, star_y = game.tick, game.stars[1].y
toggle_pause(); update(false, true, true)
assert(game.mode == "paused" and game.tick == tick and game.stars[1].y ~= star_y)
render(); toggle_pause()
-- A shot kills each enemy type and awards its native score; touching edges miss.
assert(not overlaps(0, 0, 3, 8, 3, 0, 18, 14))
for kind = 0, 2 do
    start()
    game.enemies[1] = {active=true, x=100, y=90, dx=1, kind=kind}
    game.shots[1] = {active=true, x=105, y=100}
    update(false, false, false)
    assert(not game.enemies[1].active and not game.shots[1].active)
    assert(game.score == 100 + 50 * kind)
    local count = 0
    for _, p in ipairs(game.particles) do if p.active then count = count + 1 end end
    assert(count == 12)
    render()
end
-- Drift bounces and seekers track; spawn cadence accelerates with waves.
start()
game.enemies[1] = {active=true, x=620, y=30, dx=1, kind=1}
game.enemies[2] = {active=true, x=100, y=100, dx=1, kind=2}
update(false, false, false)
assert(game.enemies[1].x == 622 and game.enemies[1].dx == -1)
assert(game.enemies[2].x == 101)
game.tick = 89; update(false, false, false)
assert(game.enemies[3].active and game.next_spawn_tick == 180)
game.score = 10000; update(false, false, false); assert(game.wave == 11)
game.tick = 179; update(false, false, false); assert(game.next_spawn_tick == 220)
-- Damage immunity, game over, frozen simulation, then restart preserves best.
start()
local function collide()
    game.enemies[1] = {active=true, x=game.player_x, y=game.player_y, dx=1, kind=0}
    update(false, false, false)
end
collide(); assert(game.lives == 2 and game.invulnerable_until == game.tick + 90)
collide(); assert(game.lives == 2)
game.score, game.lives, game.invulnerable_until = 1200, 1, 0
collide()
assert(game.mode == "game_over" and game.lives == 0 and game.high_score == 1200)
tick = game.tick; update(false, true, true); assert(game.tick == tick)
render(); start()
assert(game.high_score == 1200 and game.lives == 3 and game.score == 0)
-- Bounded pools recycle expired objects instead of growing.
for _, p in ipairs(game.particles) do p.active, p.x, p.y, p.dx, p.dy, p.life = true, 0, 0, 0, 0, 1 end
emit_particles(5, 5); assert(#game.particles == 64)
update(false, false, false)
for _, p in ipairs(game.particles) do assert(not p.active) end
assert(screen:close())
'''
# Run the complete unmodified script too, with scheduled input wrapping the real
# SDK canvas. Validate held movement/fire, repeat suppression, pause/resume and exit.
wrapper = r'''
local t = require("tabos")
local open = t.graphics.open
local frames, closed = 0, false
local events = {[1]="k", [4]="p", [5]="p", [6]="p", [9]="q"}
local last_event = -1
local player_x, shots = {}, 0
t.graphics.open = function(w, h)
    local real = assert(open(w, h))
    local proxy = {}
    function proxy:poll()
        if last_event == frames then return nil end
        last_event = frames
        if events[frames] then
            return {type="key_down", key=events[frames], ["repeat"]=frames==5}
        end
    end
    function proxy:is_down(key) return key == "k" or key == "s" end
    function proxy:fill_rect(x, y, w, h, c)
        if w == 4 and h == 16 then player_x[frames] = x - 8 end
        if w == 3 and h == 8 then shots = shots + 1 end
        return real:fill_rect(x, y, w, h, c)
    end
    function proxy:present() frames = frames + 1; return real:present() end
    function proxy:close() closed = true; return real:close() end
    return setmetatable(proxy, {__close=function() proxy:close() end,
        __index=function(_, name) return function(_, ...) return real[name](real, ...) end end})
end
assert(loadfile(arg[1]))()
assert(closed and frames == 9 and shots > 0)
assert(player_x[3] > player_x[1])
assert(player_x[4] == nil and player_x[5] == nil) -- Repeat did not unpause.
assert(player_x[6] > player_x[3])
'''
with tempfile.TemporaryDirectory(prefix="tabos-lua-starfall-") as root:
    root = pathlib.Path(root)
    (root / "T:/data/lua").mkdir(parents=True)
    script = root / "check.lua"
    script.write_text(prefix + checks)
    subprocess.run([str(executable), str(script)], cwd=root, check=True, timeout=30)
    script.write_text(wrapper)
    game = root / "starfall.lua"
    game.write_text(source)
    subprocess.run([str(executable), str(script), str(game)], cwd=root, check=True, timeout=30)
print("Starfall gameplay, rendering, storage, scheduled keyboard input and cleanup passed")
