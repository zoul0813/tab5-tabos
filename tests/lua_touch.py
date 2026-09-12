"""Run the shipped touch demo with deterministic contacts and real SDK drawing."""
import pathlib
import subprocess
import sys
import tempfile

executable = pathlib.Path(sys.argv[1]).resolve()
source = pathlib.Path(sys.argv[2]).resolve()
wrapper = r'''
local t = require('tabos')
local open = t.graphics.open
local frame, consumed, closed = 0, false, false
local object
local expected = {
    [0]={148,88,false}, [2]={38,58,true}, [4]={295,0,true},
    [5]={295,0,false}, [7]={88,108,true}, [8]={98,118,false}
}
local events = {
    [1]={type='down',x=-1,y=40,inside=false,device_id=42,contact_id=0},
    [2]={type='down',x=50,y=70,inside=true,device_id=42,contact_id=0},
    [3]={type='down',x=200,y=100,inside=true,device_id=42,contact_id=1},
    [4]={type='move',x=500,y=-20,inside=false,device_id=42,contact_id=0},
    [5]={type='cancel',x=500,y=-20,inside=false,device_id=42,contact_id=0},
    [6]={type='move',x=200,y=100,inside=true,device_id=42,contact_id=1},
    [7]={type='down',x=100,y=120,inside=true,device_id=42,contact_id=2},
    [8]={type='up',x=110,y=130,inside=true,device_id=42,contact_id=2}
}
t.graphics.open = function(w,h)
    local real = assert(open(w,h))
    return setmetatable({}, {
        __close=function() closed=true; assert(real:close()) end,
        __index=function(_,name)
            if name=='pointer_open' then return function() return real:pointer_open() end end
            if name=='pointer_poll' then return function()
                if consumed then return nil end
                consumed=true
                return events[frame]
            end end
            if name=='poll' then return function()
                if frame==9 then return {type='key_down',key='q'} end
            end end
            if name=='fill_rect' then return function(_,x,y,w,h,c)
                object={x,y,c}
                return real:fill_rect(x,y,w,h,c)
            end end
            if name=='present' then return function()
                local e=assert(expected[frame], 'unexpected redraw at '..frame)
                assert(object[1]==e[1] and object[2]==e[2], 'position at '..frame)
                assert(object[3]==(e[3] and t.rgb(255,190,65) or t.rgb(67,190,230)))
                expected[frame]=nil
                return real:present()
            end end
            return function(_,...) return real[name](real,...) end
        end
    })
end
t.sleep_ms=function() frame=frame+1; consumed=false; assert(frame<=10) end
dofile(arg[1])
assert(closed and next(expected)==nil)
-- Closing the demo releases graphics and its pointer stream for another launch.
local s <close> = assert(open(320,200)); assert(s:pointer_open())
print('TOUCH_DEMO_OK')
'''
with tempfile.TemporaryDirectory(prefix="tabos-lua-touch-") as directory:
    path = pathlib.Path(directory) / "check.lua"
    path.write_text(wrapper)
    result = subprocess.run([str(executable), str(path), str(source)], cwd=directory,
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "TOUCH_DEMO_OK" in result.stdout
print("Touch demo movement, contact ownership, clamping, cancel, release and cleanup passed")
