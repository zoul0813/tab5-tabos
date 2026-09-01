#include "doom_tabos_sfx.h"

#include <tabos/audio.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "deh_str.h"
#include "doomtype.h"
#include "i_sound.h"
#include "m_argv.h"
#include "w_wad.h"
#include "z_zone.h"

// RV32 rendering can leave more than one 35 Hz game tic between audio updates. Per-channel
// streams allow this lookahead without retaining stopped or replaced effects.
enum {
    DOOM_AUDIO_CHUNK_FRAMES  = 480,
    DOOM_AUDIO_TARGET_FRAMES = 5760,
};

typedef struct {
        tabos_audio_stream_t stream;
        int16_t samples[DOOM_AUDIO_CHUNK_FRAMES * 2U];
        uint32_t frames;
        uint32_t offset;
        boolean playing;
} doom_audio_output_t;

static doom_tabos_sfx_mixer_t mixer;
static doom_audio_output_t outputs[DOOM_TABOS_SFX_CHANNELS];
static uint32_t playback_route;
static boolean use_sfx_prefix;

static void sound_name(sfxinfo_t* sound, char name[9])
{
    if (sound->link != NULL) {
        sound = sound->link;
    }
    if (use_sfx_prefix) {
        (void) snprintf(name, 9U, "ds%.6s", DEH_String(sound->name));
    } else {
        (void) snprintf(name, 9U, "%.8s", DEH_String(sound->name));
    }
}

static int sound_lump(sfxinfo_t* sound)
{
    char name[9];
    sound_name(sound, name);
    return W_GetNumForName(name);
}

static void stop_all(void)
{
    doom_tabos_sfx_destroy(&mixer);
    for (unsigned int channel = 0U; channel < DOOM_TABOS_SFX_CHANNELS; ++channel) {
        if (outputs[channel].stream != TABOS_AUDIO_STREAM_INVALID) {
            (void) tabos_audio_close(outputs[channel].stream);
        }
        outputs[channel] = (doom_audio_output_t) {.stream = TABOS_AUDIO_STREAM_INVALID};
    }
}

static boolean sound_init(boolean prefix)
{
    tabos_audio_info_t info;
    use_sfx_prefix = prefix;
    doom_tabos_sfx_init(&mixer);
    for (unsigned int channel = 0U; channel < DOOM_TABOS_SFX_CHANNELS; ++channel) {
        outputs[channel].stream = TABOS_AUDIO_STREAM_INVALID;
    }
    playback_route = M_CheckParm("-headphone") > 0 ? TABOS_AUDIO_ROUTE_HEADPHONE : TABOS_AUDIO_ROUTE_SPEAKER;
    if (tabos_audio_get_info(&info) != 0 || (info.features & TABOS_AUDIO_FEATURE_PLAYBACK) == 0U ||
        (info.routes & playback_route) == 0U) {
        fprintf(stderr, "doom: sound effects unavailable: %s\n", strerror(errno));
        return false;
    }
    snd_samplerate = DOOM_TABOS_SFX_SAMPLE_RATE;
    return true;
}

static void sound_shutdown(void)
{
    stop_all();
}

static void close_output(unsigned int channel)
{
    if (outputs[channel].stream != TABOS_AUDIO_STREAM_INVALID) {
        (void) tabos_audio_close(outputs[channel].stream);
    }
    outputs[channel] = (doom_audio_output_t) {.stream = TABOS_AUDIO_STREAM_INVALID};
}

static boolean open_output(unsigned int channel)
{
    if (outputs[channel].stream != TABOS_AUDIO_STREAM_INVALID) {
        return true;
    }
    const tabos_audio_config_t config = {
        .direction = TABOS_AUDIO_PLAYBACK,
        .channels  = 2U,
        .route     = playback_route,
    };
    outputs[channel].stream = tabos_audio_open(&config);
    return outputs[channel].stream != TABOS_AUDIO_STREAM_INVALID;
}

static boolean discard_output(unsigned int channel)
{
    doom_audio_output_t* output = &outputs[channel];
    output->frames              = 0U;
    output->offset              = 0U;
    output->playing             = false;
    if (output->stream == TABOS_AUDIO_STREAM_INVALID) {
        return true;
    }
    if (tabos_audio_flush(output->stream) != 0) {
        close_output(channel);
        return false;
    }
    return true;
}

static boolean reset_output(unsigned int channel)
{
    return open_output(channel) && discard_output(channel);
}

static void sound_update_channel(unsigned int channel)
{
    doom_audio_output_t* output = &outputs[channel];
    if (output->stream == TABOS_AUDIO_STREAM_INVALID || !output->playing) {
        return;
    }
    tabos_audio_status_t status;
    if (tabos_audio_get_status(output->stream, &status) != 0) {
        doom_tabos_sfx_stop(&mixer, channel);
        close_output(channel);
        return;
    }
    uint32_t buffered_frames = status.buffered_bytes / (2U * sizeof(int16_t));
    while ((doom_tabos_sfx_playing(&mixer, channel) || output->offset < output->frames) &&
           buffered_frames < DOOM_AUDIO_TARGET_FRAMES) {
        if (output->offset == output->frames) {
            doom_tabos_sfx_render_channel(&mixer, channel, output->samples, DOOM_AUDIO_CHUNK_FRAMES);
            output->frames = DOOM_AUDIO_CHUNK_FRAMES;
            output->offset = 0U;
        }
        const uint32_t bytes = (output->frames - output->offset) * 2U * sizeof(int16_t);
        const int written    = tabos_audio_write(output->stream, &output->samples[output->offset * 2U], bytes);
        if (written < 0) {
            if (errno != EAGAIN) {
                doom_tabos_sfx_stop(&mixer, channel);
                close_output(channel);
            }
            return;
        }
        if (written == 0) {
            return;
        }
        const uint32_t frames  = (uint32_t) written / (2U * sizeof(int16_t));
        output->offset        += frames;
        buffered_frames       += frames;
    }
    output->playing =
        doom_tabos_sfx_playing(&mixer, channel) || output->offset < output->frames || buffered_frames > 0U;
}

static void sound_update(void)
{
    for (unsigned int channel = 0U; channel < DOOM_TABOS_SFX_CHANNELS; ++channel) {
        sound_update_channel(channel);
    }
}

static void sound_set_params(int channel, int volume, int separation)
{
    if (channel >= 0) {
        doom_tabos_sfx_set_params(&mixer, (unsigned int) channel, volume, separation);
    }
}

static int sound_start(sfxinfo_t* sound, int channel, int volume, int separation)
{
    if (channel < 0 || channel >= DOOM_TABOS_SFX_CHANNELS) {
        return -1;
    }
    doom_tabos_sfx_stop(&mixer, (unsigned int) channel);
    if (!reset_output((unsigned int) channel)) {
        return -1;
    }
    const int lump        = sound->lumpnum;
    const int lump_length = W_LumpLength((unsigned int) lump);
    uint8_t* data         = W_CacheLumpNum(lump, PU_STATIC);
    const uint8_t* samples;
    size_t sample_count;
    uint32_t sample_rate;
    if (!doom_tabos_sfx_decode_dmx(data, (size_t) lump_length, &samples, &sample_count, &sample_rate)) {
        W_ReleaseLumpNum(lump);
        return -1;
    }
    const bool started =
        doom_tabos_sfx_start(&mixer, (unsigned int) channel, samples, sample_count, sample_rate, volume, separation);
    W_ReleaseLumpNum(lump);
    if (!started) {
        doom_tabos_sfx_stop(&mixer, (unsigned int) channel);
        return -1;
    }
    outputs[channel].playing = true;
    sound_update_channel((unsigned int) channel);
    return channel;
}

static void sound_stop(int channel)
{
    if (channel >= 0 && channel < DOOM_TABOS_SFX_CHANNELS) {
        doom_tabos_sfx_stop(&mixer, (unsigned int) channel);
        (void) discard_output((unsigned int) channel);
    }
}

static boolean sound_playing(int channel)
{
    return channel >= 0 && channel < DOOM_TABOS_SFX_CHANNELS && outputs[channel].playing;
}

static void sound_precache(sfxinfo_t* sounds, int count)
{
    (void) sounds;
    (void) count;
}

static snddevice_t sound_devices[] = {
    SNDDEVICE_SB, SNDDEVICE_PAS, SNDDEVICE_GUS, SNDDEVICE_WAVEBLASTER, SNDDEVICE_SOUNDCANVAS, SNDDEVICE_AWE32,
};

sound_module_t DG_sound_module = {
    sound_devices,    (int) (sizeof(sound_devices) / sizeof(sound_devices[0])),
    sound_init,       sound_shutdown,
    sound_lump,       sound_update,
    sound_set_params, sound_start,
    sound_stop,       sound_playing,
    sound_precache,
};
