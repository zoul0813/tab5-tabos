"""Compare Lua Snake PCM against the native Snake integer synthesis contract."""
import pathlib
import struct
import subprocess
import sys
import tempfile


def truncate(value, divisor):
    return value // divisor if value >= 0 else -((-value) // divisor)


def reference(notes):
    output = bytearray()
    for frequency in notes:
        phase = 0
        for frame in range(2646):
            phase = (phase + frequency * 65536 // 44100) & 65535
            triangle = min(phase, 65535 - phase)
            wave = truncate(triangle - 16384, 4)
            envelope = min(frame, 220, 2645 - frame)
            output.extend(struct.pack("<h", truncate(wave * envelope, 220)))
    return bytes(output)


executable = pathlib.Path(sys.argv[1]).resolve()
source = pathlib.Path(sys.argv[2]).read_text()
# Execute the actual game sound code without its interactive game loop.
prefix, marker, _ = source.partition("local colors =")
assert marker, "Snake sound prelude boundary missing"
with tempfile.TemporaryDirectory(prefix="tabos-lua-snake-audio-") as root:
    script = pathlib.Path(root, "check.lua")
    script.write_text(prefix + "\nfor name,pcm in pairs(sounds) do "
                      "local f=assert(io.open(name..'.pcm','wb')); "
                      "assert(f:write(pcm)); assert(f:close()) end\n"
                      "play_sound('start'); assert(sound_stream:status().buffered_bytes==15876); "
                      "play_sound('eat'); assert(sound_stream:status().buffered_bytes==10584); "
                      "stop_sound(); assert(sound_stream:status().buffered_bytes==0); "
                      "muted=true; close_sound(); play_sound('win'); assert(sound_stream==nil)\n")
    subprocess.run([str(executable), str(script)], cwd=root, check=True, timeout=30)
    for name, notes in {"start": [440, 660, 880], "eat": [880, 1320],
                        "lose": [330, 220, 110], "win": [660, 880, 1320]}.items():
        actual = pathlib.Path(root, name + ".pcm").read_bytes()
        assert actual == reference(notes), name + " PCM differs from native Snake"
print("All four Lua Snake effects match native PCM; replacement, stop and mute pass")
