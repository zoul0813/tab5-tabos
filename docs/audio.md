# Audio Service

TabOS exposes process-owned playback and capture streams through `<tabos/audio.h>`. The
wire format is signed 16-bit little-endian PCM. Supported sample rates are 8, 11.025, 12,
16, 22.05, 24, 32, 44.1, 48, 88.2, and 96 kHz. The default is 44.1 kHz. Playback accepts
mono or stereo. Capture accepts one channel through the channel count reported by
`tabos_audio_get_info()`. Its `sample_rates` mask and `default_sample_rate` field report
backend capabilities.

The `audio0` device reports the available playback, capture, route, and AEC features.
Tab5 uses ES8388 playback and ES7210 capture. The host uses SDL3 playback and recording;
headless host tests use deterministic callback buffers. AEC is absent unless a platform
backend explicitly reports working support.

Tab5 monitors the headphone-detect input on its PI4IOE5V6408 expander. Inserting headphones
automatically disables the main speaker amplifier while leaving headphone playback active.
Removing them restores the speaker only when the active playback route requests the speaker;
an explicitly selected headphone route keeps the speaker disabled.

## Stream API

Open a stream with `tabos_audio_open()` and a `tabos_audio_config_t` direction, channel
count, single route, and sample rate. A zero sample rate selects
`TABOS_AUDIO_DEFAULT_SAMPLE_RATE`. Playback routes are speaker or headphone. Capture uses
the microphone route. Up to eight streams may exist system-wide, and every stream owns a
bounded 32 KiB ring buffer.

ES8388 playback and ES7210 capture share physical clocks. All concurrently open streams
therefore use one sample rate. Opening the first stream configures the backend to its requested
rate. A later open at a different rate fails with `EBUSY`; TabOS does not silently resample it.
After every stream closes, the next open may select any supported rate.

`tabos_audio_write()` and `tabos_audio_read()` are nonblocking. They return a positive
byte count when progress is possible and fail with `EAGAIN` when the ring cannot accept or
provide one complete frame. Buffers and byte counts must contain complete PCM frames and
one call is limited to `TABOS_AUDIO_IO_MAX` bytes.

`tabos_audio_flush()` immediately discards PCM queued on a playback stream without closing
the stream. Use it when replacing or stopping time-sensitive audio. Capture streams and
foreign or stale handles fail with `EBADF`.

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

`info` reports every supported sample rate and the 44.1 kHz default. `tone` generates a
two-second 440 Hz square wave. `level` prints microphone peaks for two
seconds. `loopback` sends microphone capture to the chosen output at half volume for five
seconds. `route` verifies route selection, and `buffers` deliberately leaves streams idle
before reporting underrun and overrun counters.
