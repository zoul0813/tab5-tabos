-- Touch or click anywhere to move the square, then drag to steer it.
-- Q / Escape / Ctrl-C / Ctrl-D exits. No image, font, or native game code needed.
local tabos = require("tabos")
print("Touch demo: touch or hold the mouse button to move. Q / Escape quits.")
local screen <close> = assert(tabos.graphics.open(320, 200))
assert(screen:pointer_open())

local width, height = screen:size()
local x, y, radius = width // 2, height // 2, 12
local active_device, active_contact
local background = tabos.rgb(12, 19, 32)
local grid = tabos.rgb(26, 39, 55)
local idle = tabos.rgb(67, 190, 230)
local held = tabos.rgb(255, 190, 65)
local white = tabos.rgb(240, 247, 255)
local dirty, running = true, true

local function move(event)
    x = math.max(radius, math.min(width - radius - 1, event.x))
    y = math.max(radius, math.min(height - radius - 1, event.y))
    dirty = true
end

while running do
    -- Bound each drain so continuous pointer traffic cannot starve keyboard input.
    for _ = 1, 64 do
        local event, message = screen:pointer_poll()
        if not event then
            assert(not message, message)
            break
        end
        if event.type == "down" and event.inside and active_contact == nil then
            active_device, active_contact = event.device_id, event.contact_id
            move(event)
        elseif event.device_id == active_device and event.contact_id == active_contact then
            if event.type == "move" or event.type == "up" then move(event) end
            if event.type == "up" or event.type == "cancel" then
                active_device, active_contact = nil, nil
                dirty = true
            end
        end
    end
    for _ = 1, 64 do
        local event = screen:poll()
        if not event then break end
        if event.type == "key_down" and (event.key == "q" or event.key == "escape") then
            running = false
        end
    end
    if dirty then
        assert(screen:clear(background))
        for column = 0, width - 1, 20 do assert(screen:line(column, 0, column, height - 1, grid)) end
        for row = 0, height - 1, 20 do assert(screen:line(0, row, width - 1, row, grid)) end
        assert(screen:rect(0, 0, width, height, idle))
        assert(screen:fill_rect(x - radius, y - radius, radius * 2 + 1, radius * 2 + 1,
            active_contact and held or idle))
        assert(screen:line(x - 5, y, x + 5, y, white))
        assert(screen:line(x, y - 5, x, y + 5, white))
        assert(screen:present())
        dirty = false
    end
    tabos.sleep_ms(16)
end
