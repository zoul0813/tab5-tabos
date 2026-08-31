# Audio Service

TabOS exposes process-owned playback and capture streams through `<tabos/audio.h>`. The
mandatory wire format is signed 16-bit little-endian PCM at 48 kHz. Playback accepts mono
or stereo. Capture accepts one channel through the channel count reported by
`tabos_audio_get_info()`.

The `audio0` device reports the available playback, capture, route, and AEC features.
Tab5 uses ES8388 playback and ES7210 capture. The host uses SDL3 playback and recording;
headless host tests use deterministic callback buffers. AEC is absent unless a platform
backend explicitly reports working support.

## Stream API

Open a stream with `tabos_audio_open()` and a `tabos_audio_config_t` direction, channel
count, and single route. Playback routes are speaker or headphone. Capture uses the
microphone route. Up to eight streams may exist system-wide, and every stream owns a
bounded 32 KiB ring buffer.

`tabos_audio_write()` and `tabos_audio_read()` are nonblocking. They return a positive
byte count when progress is possible and fail with `EAGAIN` when the ring cannot accept or
provide one complete frame. Buffers and byte counts must contain complete PCM frames and
one call is limited to `TABOS_AUDIO_IO_MAX` bytes.

Use `tabos_audio_wait_source()` with `tabos_wait()` to wait for `TABOS_WAIT_WRITABLE` on
playback or `TABOS_WAIT_READABLE` on capture. Request `TABOS_WAIT_ERROR` and
`TABOS_WAIT_HANGUP` as well when a stream must detect an asynchronous backend failure.
Wait readiness does not move PCM data.

`tabos_audio_set_volume()` controls playback stream gain from zero through
`TABOS_AUDIO_VOLUME_MAX`. The mixer combines at least four active streams and saturates
the signed 16-bit result. `tabos_audio_set_route()` selects the physical route; playback
route selection is shared hardware state, so a later route change affects physical output
for all mixed streams.

`tabos_audio_get_status()` reports buffered bytes, buffer capacity, playback underruns,
and capture overruns. Close streams explicitly with `tabos_audio_close()`. TabOS also
invalidates wait sources, stops access, and reclaims stream buffers when a process exits,
faults, or fails during launch.

## Audio Test Utility

Build and install `audiotest` with the other core utilities, then use:

```text
audiotest info
audiotest tone [speaker|headphone]
audiotest level
audiotest loopback [speaker|headphone]
audiotest route speaker|headphone
audiotest buffers
```

`tone` generates a two-second 440 Hz square wave. `level` prints microphone peaks for two
seconds. `loopback` sends microphone capture to the chosen output at half volume for five
seconds. `route` verifies route selection, and `buffers` deliberately leaves streams idle
before reporting underrun and overrun counters.
