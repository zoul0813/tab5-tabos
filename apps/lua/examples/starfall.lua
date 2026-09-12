-- Run: lua T:/data/lua/starfall.lua
-- A/S move; hold K to fire/start/restart; P pauses; Q or Escape quits.
-- Port of apps/starfall: all gameplay, artwork and font live in this file.
-- MIT License
-- Copyright (c) 2026 TabOS contributors
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

local tabos = require("tabos")
local width, height = 640, 360
local score_path = "T:/data/lua/starfall-highscore.dat"
local score_temp = "T:/data/lua/starfall-highscore.tmp"

local function load_score()
    local file <close> = io.open(score_path, "rb")
    if not file then return 0 end
    local bytes = file:read(24)
    local value = bytes and bytes:match("^(%d+)\n?$")
    value = value and tonumber(value)
    return value and value <= 999999 and value or 0
end

local function save_score(score)
    -- Installation creates data/lua; unavailable or read-only storage is harmless.
    local file = io.open(score_temp, "wb")
    if not file then return false end
    local written = file:write(string.format("%d\n", math.min(score, 999999)))
    local closed = file:close()
    if not written or not closed then os.remove(score_temp); return false end
    -- Keep the previous score intact if the filesystem cannot replace it.
    if not os.rename(score_temp, score_path) then
        os.remove(score_temp)
        return false
    end
    return true
end

local game = {random_state = tabos.monotonic_ms() & 0xffffffff, high_score = load_score()}
local persisted_high_score = game.high_score
local function random_next()
    -- Match the native game's uint32_t xorshift on Lua's 64-bit integers.
    local value = game.random_state ~= 0 and game.random_state or 0x51a7f411
    value = (value ~ (value << 13)) & 0xffffffff
    value = value ~ (value >> 17)
    value = (value ~ (value << 5)) & 0xffffffff
    game.random_state = value
    return value
end

local function pool(count)
    local items = {}
    for i = 1, count do items[i] = {active = false} end
    return items
end

local function initialize()
    game.mode, game.score, game.tick, game.wave, game.lives = "title", 0, 0, 0, 0
    game.player_x, game.player_y = 0, 0
    game.next_fire_tick, game.next_spawn_tick = 0, 0
    game.invulnerable_until, game.damage_flash_until = 0, 0
    game.shots, game.enemies, game.particles = pool(32), pool(24), pool(64)
    game.stars = {}
    for i = 1, 96 do
        local x = random_next() % width
        local y = random_next() % height
        game.stars[i] = {x = x, y = y, speed = 1 + random_next() % 3}
    end
end

local function start()
    initialize()
    game.mode, game.player_x, game.player_y = "playing", 310, 318
    game.lives, game.wave, game.next_spawn_tick = 3, 1, 90
end

local function toggle_pause()
    if game.mode == "playing" then game.mode = "paused"
    elseif game.mode == "paused" then game.mode = "playing" end
end

local function overlaps(ax, ay, aw, ah, bx, by, bw, bh)
    return ax < bx + bw and ax + aw > bx and ay < by + bh and ay + ah > by
end

local function emit_particles(x, y)
    for _ = 1, 12 do
        for _, particle in ipairs(game.particles) do
            if not particle.active then
                particle.active, particle.x, particle.y = true, x, y
                particle.dx = random_next() % 5 - 2
                particle.dy = random_next() % 5 - 2
                particle.life = 12 + random_next() % 14
                break
            end
        end
    end
end

local function spawn_enemy()
    for _, enemy in ipairs(game.enemies) do
        if not enemy.active then
            enemy.active, enemy.kind = true, random_next() % 3
            enemy.x, enemy.y = 12 + random_next() % (width - 42), 28
            enemy.dx = random_next() & 1 ~= 0 and 1 or -1
            return
        end
    end
end

local function fire(held)
    if not held or game.tick < game.next_fire_tick then return end
    for _, shot in ipairs(game.shots) do
        if not shot.active then
            shot.active, shot.x, shot.y = true, game.player_x + 9, game.player_y - 8
            game.next_fire_tick = game.tick + 8
            return
        end
    end
end

local function damage_player()
    if game.tick < game.invulnerable_until then return end
    game.lives = math.max(0, game.lives - 1)
    game.damage_flash_until, game.invulnerable_until = game.tick + 10, game.tick + 90
    emit_particles(game.player_x + 10, game.player_y + 8)
    if game.lives == 0 then
        game.mode = "game_over"
        game.high_score = math.max(game.high_score, game.score)
    end
end

local function update(left, right, firing)
    for _, star in ipairs(game.stars) do
        star.y = star.y + star.speed
        if star.y >= height then star.y, star.x = 24, random_next() % width end
    end
    if game.mode ~= "playing" then return end
    game.tick = game.tick + 1
    game.player_x = math.max(0, math.min(width - 20,
        game.player_x + (right and 4 or 0) - (left and 4 or 0)))
    fire(firing)
    if game.tick >= game.next_spawn_tick then
        spawn_enemy()
        game.next_spawn_tick = game.tick + (game.wave < 11 and 95 - game.wave * 5 or 40)
    end
    game.wave = 1 + game.score // 1000
    for _, shot in ipairs(game.shots) do
        if shot.active then
            shot.y = shot.y - 7
            if shot.y < 22 then shot.active = false end
        end
    end
    for _, enemy in ipairs(game.enemies) do
        if enemy.active then
            enemy.y = enemy.y + 1 + game.wave // 6
            if enemy.kind == 1 then
                enemy.x = enemy.x + enemy.dx * 2
                if enemy.x < 2 or enemy.x > width - 20 then enemy.dx = -enemy.dx end
            elseif enemy.kind == 2 and enemy.y > 100 then
                enemy.x = enemy.x + (game.player_x > enemy.x and 1 or -1)
            end
            if enemy.y >= height then
                enemy.active = false
            else
                for _, shot in ipairs(game.shots) do
                    if shot.active and overlaps(shot.x, shot.y, 3, 8, enemy.x, enemy.y, 18, 14) then
                        shot.active, enemy.active = false, false
                        game.score = game.score + 100 + enemy.kind * 50
                        game.high_score = math.max(game.high_score, game.score)
                        emit_particles(enemy.x + 9, enemy.y + 7)
                        break
                    end
                end
                if enemy.active and overlaps(game.player_x, game.player_y, 20, 16,
                    enemy.x, enemy.y, 18, 14) then
                    enemy.active = false
                    damage_player()
                end
            end
        end
    end
    for _, particle in ipairs(game.particles) do
        if particle.active then
            particle.x, particle.y = particle.x + particle.dx, particle.y + particle.dy
            particle.life = particle.life - 1
            if particle.life == 0 then particle.active = false end
        end
    end
end

local screen <close> = assert(tabos.graphics.open(width, height))
local colors = {
    space = tabos.rgb(2, 5, 18), cyan = tabos.rgb(48, 224, 255),
    white = tabos.rgb(240, 248, 255), yellow = tabos.rgb(255, 216, 48),
    red = tabos.rgb(255, 48, 72), magenta = tabos.rgb(224, 64, 255),
    green = tabos.rgb(64, 240, 128), divider = tabos.rgb(24, 72, 112),
    pause = tabos.rgb(0, 48, 128), damage = tabos.rgb(192, 0, 24),
}
local star_colors = {tabos.rgb(48, 64, 96), tabos.rgb(112, 144, 192), colors.white}
local enemy_colors = {colors.green, colors.yellow, colors.red}

-- Original Starfall five-bit rows from apps/starfall/assets/font5x7.inc.
local glyphs = {
    [" "]={0,0,0,0,0,0,0}, ["-"]={0,0,0,31,0,0,0}, [":"]={0,4,4,0,4,4,0},
    ["0"]={14,17,19,21,25,17,14}, ["1"]={4,12,4,4,4,4,14},
    ["2"]={14,17,1,2,4,8,31}, ["3"]={30,1,1,14,1,1,30},
    ["4"]={2,6,10,18,31,2,2}, ["5"]={31,16,16,30,1,1,30},
    ["6"]={14,16,16,30,17,17,14}, ["7"]={31,1,2,4,8,8,8},
    ["8"]={14,17,17,14,17,17,14}, ["9"]={14,17,17,15,1,1,14},
    A={14,17,17,31,17,17,17}, B={30,17,17,30,17,17,30},
    C={14,17,16,16,16,17,14}, D={30,17,17,17,17,17,30},
    E={31,16,16,30,16,16,31}, F={31,16,16,30,16,16,16},
    G={14,17,16,23,17,17,15}, H={17,17,17,31,17,17,17},
    I={14,4,4,4,4,4,14}, J={7,2,2,2,18,18,12},
    K={17,18,20,24,20,18,17}, L={16,16,16,16,16,16,31},
    M={17,27,21,21,17,17,17}, N={17,25,21,19,17,17,17},
    O={14,17,17,17,17,17,14}, P={30,17,17,30,16,16,16},
    Q={14,17,17,17,21,18,13}, R={30,17,17,30,20,18,17},
    S={15,16,16,14,1,1,30}, T={31,4,4,4,4,4,4},
    U={17,17,17,17,17,17,14}, V={17,17,17,17,17,10,4},
    W={17,17,17,21,21,21,10}, X={17,17,10,4,10,17,17},
    Y={17,17,10,4,4,4,4}, Z={31,1,2,4,8,16,31},
}

local function fill(x, y, w, h, color)
    assert(screen:fill_rect(x, y, w, h, color))
end

-- Compile horizontal glyph spans once; avoid one API call per lit font pixel.
local spans = {}
for character, rows in pairs(glyphs) do
    local runs = {}
    for row, bits in ipairs(rows) do
        local column = 0
        while column < 5 do
            if bits & (1 << (4 - column)) ~= 0 then
                local first = column
                repeat column = column + 1
                until column == 5 or bits & (1 << (4 - column)) == 0
                runs[#runs + 1] = {first, row - 1, column - first}
            else column = column + 1 end
        end
    end
    spans[character] = runs
end

local function draw_text(x, y, text, scale, color)
    for i = 1, #text do
        for _, run in ipairs(spans[text:sub(i, i)] or spans[" "]) do
            fill(x + run[1] * scale, y + run[2] * scale, run[3] * scale, scale, color)
        end
        x = x + 6 * scale
    end
end

local function centered(y, text, scale, color)
    draw_text((width - #text * 6 * scale) // 2, y, text, scale, color)
end

local function draw_player()
    if game.tick < game.invulnerable_until and (game.tick // 4) & 1 ~= 0 then return end
    local x, y = game.player_x, game.player_y
    fill(x + 8, y, 4, 16, colors.white)
    fill(x + 4, y + 6, 12, 8, colors.cyan)
    fill(x, y + 11, 20, 5, colors.magenta)
    fill(x + 7, y + 16, 3, 5, colors.yellow)
    fill(x + 11, y + 16, 3, 5, colors.red)
end

local function draw_enemy(enemy)
    local x, y, color = enemy.x, enemy.y, enemy_colors[enemy.kind + 1]
    fill(x + 4, y, 10, 4, color)
    fill(x, y + 4, 18, 6, color)
    fill(x + 3, y + 10, 4, 4, color)
    fill(x + 11, y + 10, 4, 4, color)
    fill(x + 6, y + 5, 2, 2, colors.space)
    fill(x + 11, y + 5, 2, 2, colors.space)
end

local function render()
    local border = game.mode == "paused" and colors.pause
        or game.tick < game.damage_flash_until and colors.damage or 0
    assert(screen:set_letterbox_color(border))
    assert(screen:clear(colors.space))
    for _, star in ipairs(game.stars) do
        fill(star.x, star.y, star.speed == 3 and 2 or 1, star.speed, star_colors[star.speed])
    end
    fill(0, 21, width, 1, colors.divider)
    draw_text(8, 6, string.format("SCORE %06d  HI %06d  LIVES %d  WAVE %d",
        game.score, game.high_score, game.lives, game.wave), 1, colors.cyan)
    for _, shot in ipairs(game.shots) do
        if shot.active then fill(shot.x, shot.y, 3, 8, colors.yellow) end
    end
    for _, enemy in ipairs(game.enemies) do
        if enemy.active then draw_enemy(enemy) end
    end
    for _, particle in ipairs(game.particles) do
        if particle.active then
            fill(particle.x, particle.y, 2, 2, particle.life & 1 ~= 0 and colors.yellow or colors.red)
        end
    end
    if game.mode == "playing" then draw_player() end
    if game.mode == "title" then
        centered(102, "STARFALL", 5, colors.cyan)
        centered(170, "A S MOVE  K FIRE", 2, colors.white)
        centered(210, "PRESS K", 2, colors.yellow)
        centered(244, "Q QUIT", 1, colors.green)
    elseif game.mode == "paused" then
        centered(145, "PAUSED", 4, colors.cyan)
        centered(190, "P RESUME", 2, colors.white)
    elseif game.mode == "game_over" then
        centered(130, "GAME OVER", 4, colors.red)
        centered(180, "K RESTART", 2, colors.white)
        centered(215, "Q QUIT", 1, colors.green)
    end
    if game.tick < game.damage_flash_until then
        assert(screen:rect(2, 23, width - 4, height - 25, colors.red))
    end
    assert(screen:present())
end

-- Fixed 60 Hz simulation, at most six catch-up steps after a slow frame.
initialize()
local previous, accumulator = tabos.monotonic_ms(), 0
local next_save_attempt = 0
while true do
    local quitting = false
    while true do
        local event = screen:poll()
        if not event then break end
        if event.type == "key_down" and not event["repeat"] then
            if event.key == "q" or event.key == "escape" then
                quitting = true
            elseif event.key == "k" and (game.mode == "title" or game.mode == "game_over") then
                start()
            elseif event.key == "p" then toggle_pause() end
        end
    end
    if quitting then break end
    local now = tabos.monotonic_ms()
    accumulator = accumulator + math.max(0, math.min(100, now - previous)) * 60
    previous = now
    -- Broker held state survives input queue overflow; repeats never toggle pause.
    local left, right, firing = screen:is_down("a"), screen:is_down("s"), screen:is_down("k")
    local catch_up = 0
    while accumulator >= 1000 and catch_up < 6 do
        update(left, right, firing)
        accumulator, catch_up = accumulator - 1000, catch_up + 1
    end
    if catch_up == 6 and accumulator >= 1000 then accumulator = 0 end
    render()
    if game.high_score > persisted_high_score and game.mode == "game_over" and now >= next_save_attempt then
        if save_score(game.high_score) then persisted_high_score = game.high_score end
        next_save_attempt = now + 5000 -- Do not hammer unavailable storage every frame.
    end
    local remaining = (1000 - accumulator + 59) // 60 - (tabos.monotonic_ms() - now)
    assert(tabos.sleep_ms(math.max(1, remaining)))
end
if game.high_score > persisted_high_score then save_score(game.high_score) end
assert(screen:close())
print("Starfall score: " .. game.score)
