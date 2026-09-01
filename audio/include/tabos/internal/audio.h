#ifndef TABOS_INTERNAL_AUDIO_H
#define TABOS_INTERNAL_AUDIO_H

#include <tabos/audio.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool audio_service_init(void);
void audio_service_shutdown(void);
bool audio_service_info(tabos_audio_info_t* info, const char** driver, int* error);
tabos_audio_stream_t audio_service_open(const void* owner, const tabos_audio_config_t* config);
int audio_service_close(const void* owner, tabos_audio_stream_t stream);
int audio_service_flush(const void* owner, tabos_audio_stream_t stream);
void audio_service_close_owner(const void* owner);
int audio_service_write(const void* owner, tabos_audio_stream_t stream, const void* pcm, uint32_t bytes);
int audio_service_read(const void* owner, tabos_audio_stream_t stream, void* pcm, uint32_t capacity);
int audio_service_set_volume(const void* owner, tabos_audio_stream_t stream, uint32_t volume);
int audio_service_set_route(const void* owner, tabos_audio_stream_t stream, uint32_t route);
int audio_service_get_status(const void* owner, tabos_audio_stream_t stream, tabos_audio_status_t* status);
int audio_service_poll(const void* owner, tabos_audio_stream_t stream, uint32_t requested_events,
                       uint32_t* returned_events);

/* Platform callbacks and deterministic component-test entry points. */
void audio_service_render(int16_t* stereo, size_t frames);
void audio_service_capture(const int16_t* samples, size_t frames, uint32_t channels);
void audio_service_error(int error);

#endif
