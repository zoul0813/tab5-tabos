-- Run: lua T:/data/lua/snake.lua
-- Arrows/WASD move; Space pauses; Enter restarts; M toggles sound; Q quits.
-- Everything here, including artwork and score digits, is Lua source.
local tabos = require("tabos")
local screen <close> = assert(tabos.graphics.open(320, 180))
-- Exact melody, phase, envelope, and C truncation semantics from native Snake
-- (apps/snake/src/sound.c, commit 809b65f). Cache PCM once, never per game frame.
local melodies = {start = {440, 660, 880}, eat = {880, 1320},
    lose = {330, 220, 110}, win = {660, 880, 1320}}
local sounds = {}
local function trunc_div(value, divisor)
    if value < 0 then return -((-value) // divisor) end
    return value // divisor
end
for name, notes in pairs(melodies) do
    local samples = {}
    for _, frequency in ipairs(notes) do
        local phase, increment = 0, frequency * 65536 // 44100
        for i = 0, 2645 do
            phase = (phase + increment) & 65535
            local wave = phase
            if phase >= 32768 then wave = 65535 - phase end
            wave = trunc_div(wave - 16384, 4)
            local envelope = math.min(i, 220, 2645 - i)
            samples[#samples + 1] = string.pack("<i2", trunc_div(wave * envelope, 220))
        end
    end
    sounds[name] = table.concat(samples)
end
local sound_stream, muted
local function close_sound()
    if sound_stream then sound_stream:close(); sound_stream = nil end
end
-- This guard follows stream replacements and closes on scope exit/error.
local sound_guard <close> = setmetatable({}, {__close = close_sound})
local function stop_sound()
    if sound_stream and not sound_stream:flush() then close_sound() end
end
local function play_sound(name)
    if muted then return end
    if not sound_stream then sound_stream = tabos.audio.open(44100, 1, "speaker") end
    if not sound_stream then return end -- Audio failure never stops the game.
    stop_sound()
    if not sound_stream then return end
    local pcm, offset = sounds[name], 0
    while offset < #pcm do
        local count = sound_stream:write(pcm, offset)
        if not count or count <= 0 then close_sound(); return end
        offset = offset + count
    end
end
local colors = {
    background = tabos.rgb(12, 18, 28), border = tabos.rgb(50, 70, 90),
    snake = tabos.rgb(70, 200, 130), head = tabos.rgb(180, 255, 190),
    food = tabos.rgb(255, 100, 80), text = tabos.rgb(220, 235, 255),
}
local digits = {
    "111101101101111", "010110010010111", "111001111100111",
    "111001111001111", "101101111001001", "111100111001111",
    "111100111101111", "111001001001001", "111101111101111",
    "111101111001111",
}
local columns, rows, cell = 38, 18, 8
local body, dx, dy, next_dx, next_dy, food, score, paused, over, turned
local next_tick
math.randomseed(tabos.monotonic_ms())

local function occupied(x, y, count)
    for i = 1, count or #body do
        if body[i].x == x and body[i].y == y then return true end
    end
    return false
end

local function place_food()
    -- Choose from empty cells with bounded work, even on a nearly full board.
    local choice = math.random(columns * rows - #body)
    for y = 0, rows - 1 do
        for x = 0, columns - 1 do
            if not occupied(x, y) then
                choice = choice - 1
                if choice == 0 then return {x = x, y = y} end
            end
        end
    end
end

local function restart()
    body = {{x = 10, y = 9}, {x = 9, y = 9}, {x = 8, y = 9}}
    dx, dy, next_dx, next_dy = 1, 0, 1, 0
    score, paused, over, turned = 0, false, false, false
    food = place_food()
    play_sound("start")
    next_tick = tabos.monotonic_ms() + 120
end

local directions = {
    left = {-1, 0}, a = {-1, 0}, right = {1, 0}, d = {1, 0},
    up = {0, -1}, w = {0, -1}, down = {0, 1}, s = {0, 1},
}

local function draw_score()
    local text = tostring(score)
    for i = 1, #text do
        local pattern = digits[tonumber(text:sub(i, i)) + 1]
        for j = 1, 15 do
            if pattern:sub(j, j) == "1" then
                assert(screen:fill_rect(8 + (i - 1) * 8 + ((j - 1) % 3) * 2,
                    4 + ((j - 1) // 3) * 2, 2, 2, colors.text))
            end
        end
    end
end

restart()
while true do
    local quitting = false
    while true do
        local event = screen:poll()
        if not event then break end
        if event.type == "key_down" and not event["repeat"] then
            local direction = directions[event.key]
            if event.key == "q" then
                quitting = true
            elseif event.key == "m" then
                muted = not muted
                if muted then close_sound() end
            elseif event.key == "enter" then
                restart()
            elseif event.key == "space" and not over then
                paused = not paused
                stop_sound()
                next_tick = tabos.monotonic_ms() + 120
            elseif direction and not paused and not over and not turned
                and not (direction[1] == -dx and direction[2] == -dy) then
                next_dx, next_dy = direction[1], direction[2]
                turned = true
            end
        end
    end
    if quitting then break end

    local now = tabos.monotonic_ms()
    if not paused and not over and now >= next_tick then
        next_tick = now + 120
        dx, dy, turned = next_dx, next_dy, false
        local head = {x = body[1].x + dx, y = body[1].y + dy}
        local eating = head.x == food.x and head.y == food.y
        local collision_count = eating and #body or #body - 1
        if head.x < 0 or head.x >= columns or head.y < 0 or head.y >= rows
            or occupied(head.x, head.y, collision_count) then
            over = true
            play_sound("lose")
        else
            table.insert(body, 1, head)
            if eating then
                score = score + 1
                if #body == columns * rows then
                    over = true
                    food = nil
                    play_sound("win")
                else
                    food = place_food()
                    play_sound("eat")
                end
            else
                table.remove(body)
            end
        end
    end

    assert(screen:clear(colors.background))
    local border = colors.border
    if over then border = colors.food elseif paused then border = colors.text end
    assert(screen:rect(6, 18, columns * cell + 4, rows * cell + 4, border))
    draw_score()
    if food then
        assert(screen:fill_rect(8 + food.x * cell, 20 + food.y * cell,
            cell - 1, cell - 1, colors.food))
    end
    for i, segment in ipairs(body) do
        assert(screen:fill_rect(8 + segment.x * cell, 20 + segment.y * cell,
            cell - 1, cell - 1, i == 1 and colors.head or colors.snake))
    end
    assert(screen:present())
    assert(tabos.sleep_ms(10))
end
close_sound()
assert(screen:close())
print("Snake score: " .. score)
