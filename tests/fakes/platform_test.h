#ifndef TABOS_TEST_PLATFORM_TEST_H
#define TABOS_TEST_PLATFORM_TEST_H

#include <stdint.h>
#include <tabos/platform/platform.h>

void test_platform_set_time_ms(uint64_t time_ms);
void test_platform_advance_time_ms(uint64_t elapsed_ms);
uint64_t test_platform_time_ms(void);
uint8_t test_platform_brightness(void);
void test_platform_fail_brightness_once(void);
uint64_t test_platform_runtime_wait_deadline(void);
const char* test_storage_root(void);
const char* test_platform_last_log(void);
void test_platform_clear_log(void);
void test_platform_network_set_state(platform_network_state_t state, const char* failure);
unsigned int test_platform_network_connect_calls(void);
unsigned int test_platform_network_status_calls(void);
const char* test_platform_network_hostname(void);
void test_platform_keyboard_set_status(bool ready, int error);
unsigned int test_platform_keyboard_update_calls(void);
unsigned int test_platform_pointer_update_calls(void);
unsigned int test_platform_activity_reports(void);
unsigned int test_platform_display_present_calls(void);
void test_platform_display_compare_graphics_region(size_t x, size_t y, size_t width, size_t height);
bool test_platform_display_graphics_region_matches(void);
void test_platform_graphics_direct_begin(platform_pixel_t color);
void test_platform_graphics_direct_end(void);
void test_platform_graphics_direct_resume(void);
platform_pixel_t test_platform_graphics_pixel(size_t x, size_t y);
void test_platform_rtc_set_status(bool ready, int error);
void test_platform_battery_set_status(bool ready, int error);
void test_platform_audio_render(int16_t* stereo, size_t frames);
void test_platform_audio_capture(const int16_t* samples, size_t frames, uint32_t channels);
void test_platform_audio_error(int error);
uint32_t test_platform_audio_sample_rate(void);
bool test_platform_audio_active(void);
unsigned int test_platform_audio_start_calls(void);
unsigned int test_platform_audio_stop_calls(void);
void test_platform_audio_fail_start_once(void);
uint32_t test_platform_audio_route(void);
unsigned int test_platform_audio_route_calls(void);
void test_platform_camera_frame(const void* data, size_t size, uint32_t width, uint32_t height, uint32_t stride_bytes,
                                uint64_t timestamp_ms);
void test_platform_camera_encoded_frame(const void* data, size_t size, uint32_t width, uint32_t height, uint32_t format,
                                        uint64_t timestamp_ms);
void test_platform_camera_error(int error);

#endif
