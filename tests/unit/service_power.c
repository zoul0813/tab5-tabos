#include <tabos/internal/audio.h>
#include <tabos/internal/camera.h>
#include <tabos/internal/display.h>
#include <tabos/internal/filesystem.h>
#include <tabos/internal/input.h>
#include <tabos/internal/network.h>
#include <tabos/internal/pointer.h>
#include <tabos/filesystem.h>
#include "platform_test.h"

#include <assert.h>
#include <string.h>

static int owner;

static void media(void)
{
    assert(audio_service_init());
    assert(camera_service_init());
    camera_service_set_device_id(42U);
    const tabos_audio_config_t audio = {
        .direction = TABOS_AUDIO_PLAYBACK, .channels = 1U, .route = TABOS_AUDIO_ROUTE_SPEAKER};
    const tabos_camera_config_t camera = {
        .device_id = 42U, .format = TABOS_CAMERA_FORMAT_RAW8, .width = 4U, .height = 2U, .fps = 10U};
    for (unsigned int cycle = 0U; cycle < 100U; ++cycle) {
        assert(audio_service_power_suspend() == 0);
        assert(audio_service_power_suspend() == 0);
        assert(camera_service_power_suspend() == 0);
        assert(camera_service_power_suspend() == 0);
        assert(audio_service_open(&owner, &audio) == -TABOS_EBUSY);
        assert(camera_service_open(&owner, &camera) == -TABOS_EBUSY);
        audio_service_power_resume();
        audio_service_power_resume();
        camera_service_power_resume();
        camera_service_power_resume();
        tabos_camera_info_t info;
        assert(camera_service_info(&info, NULL, NULL, NULL) && info.device_id == 42U);
        const tabos_audio_stream_t sound    = audio_service_open(&owner, &audio);
        const tabos_camera_stream_t capture = camera_service_open(&owner, &camera);
        assert(sound > 0 && capture > 0);
        const int16_t sample = 1234;
        assert(audio_service_write(&owner, sound, &sample, sizeof(sample)) == sizeof(sample));
        const uint8_t pixels[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        test_platform_camera_frame(pixels, sizeof(pixels), 4U, 2U, 4U, cycle);
        tabos_camera_frame_t frame;
        assert(camera_service_acquire(&owner, capture, &frame) == 0);
        assert(audio_service_power_suspend() == -TABOS_EBUSY);
        assert(camera_service_power_suspend() == -TABOS_EBUSY);
        tabos_audio_status_t status;
        assert(audio_service_get_status(&owner, sound, &status) == 0 && status.buffered_bytes == sizeof(sample));
        uint8_t copied[8];
        assert(camera_service_copy(&owner, capture, frame.lease, 0U, copied, sizeof(copied)) == sizeof(copied));
        assert(memcmp(copied, pixels, sizeof(copied)) == 0);
        assert(camera_service_release(&owner, capture, frame.lease) == 0);
        assert(camera_service_close(&owner, capture) == 0);
        assert(audio_service_close(&owner, sound) == 0);
    }
    assert(audio_service_power_suspend() == 0);
    assert(camera_service_power_suspend() == 0);
    camera_service_remove_device();
    assert(camera_service_info(NULL, NULL, NULL, NULL));
    camera_service_power_resume();
    assert(!camera_service_info(NULL, NULL, NULL, NULL));
    audio_service_shutdown();
    camera_service_shutdown();
    assert(audio_service_init() && camera_service_init());
    audio_service_shutdown();
    camera_service_shutdown();
}

static void input_display(void)
{
    assert(input_init() && pointer_service_init() && display_init());
    pointer_service_set_device_id(43U);
    pointer_service_set_foreground_owner(&owner);
    const tabos_pointer_stream_t pointer = pointer_service_open(&owner, 43U);
    assert(pointer > 0);
    platform_framebuffer_t* retained = display_framebuffer();
    retained->pixels[0]              = 0x1234U;
    assert(platform_power_set_brightness(37U));
    for (unsigned int cycle = 0U; cycle < 100U; ++cycle) {
        assert(input_power_suspend() == 0);
        assert(pointer_service_power_suspend() == 0);
        assert(display_power_suspend() == 0 && display_power_suspend() == 0);
        assert(!display_present() && !display_graphics_present());
        assert(test_platform_brightness() == 0U && !test_platform_panel_enabled());
        assert(pointer_service_open(&owner, 43U) == -TABOS_EBUSY);
        const tabos_input_event_t down = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
        const tabos_input_event_t up   = {.type = TABOS_INPUT_KEY_UP, .key = TABOS_KEY_A};
        assert(input_submit(&down) && input_submit(&up));
        tabos_input_event_t key;
        assert(!tabos_input_poll(&key));
        assert(input_next_deadline() == UINT64_MAX);
        const tabos_pointer_event_t touch_down = {.type = TABOS_POINTER_DOWN, .contact_id = 0U, .x = 10, .y = 20};
        tabos_pointer_event_t touch_up         = touch_down;
        touch_up.type                          = TABOS_POINTER_UP;
        pointer_service_submit(&touch_down);
        pointer_service_submit(&touch_up);
        tabos_pointer_event_t touch;
        assert(pointer_service_read(&owner, pointer, &touch) == -TABOS_EAGAIN);
        assert(display_power_resume() == 0 && display_power_resume() == 0);
        assert(display_framebuffer() == retained && retained->pixels[0] == 0x1234U);
        assert(test_platform_brightness() == 37U && test_platform_panel_enabled());
        input_power_resume();
        pointer_service_power_resume();
        assert(input_power_suspend() == -TABOS_EBUSY);
        assert(pointer_service_power_suspend() == -TABOS_EBUSY);
        assert(tabos_input_poll(&key) && key.type == TABOS_INPUT_KEY_DOWN);
        assert(tabos_input_poll(&key) && key.type == TABOS_INPUT_KEY_UP);
        assert(pointer_service_read(&owner, pointer, &touch) == 0 && touch.type == TABOS_POINTER_DOWN);
        assert(pointer_service_read(&owner, pointer, &touch) == 0 && touch.type == TABOS_POINTER_UP);
        bool held;
        assert(input_take_power_activity(&held) && !held);
        assert(pointer_service_take_power_activity(&held) && !held);
    }
    test_platform_fail_panel_once();
    assert(display_power_suspend() == -TABOS_EIO);
    assert(!display_present());
    test_platform_fail_brightness_once();
    assert(display_power_resume() == -TABOS_EIO && !display_present());
    assert(display_power_resume() == 0 && display_present());
    assert(test_platform_brightness() == 37U);
    assert(pointer_service_close(&owner, pointer) == 0);
    display_shutdown();
    pointer_service_shutdown();
    input_shutdown();
}

static void network(void)
{
    assert(filesystem_init() && network_service_init());
    network_status_t status;
    for (unsigned int cycle = 0U; cycle < 100U; ++cycle) {
        assert(network_service_connect("retained-ssid", "retained-password", false));
        assert(network_service_power_suspend() == -TABOS_EBUSY);
        test_platform_network_set_state(PLATFORM_NETWORK_ONLINE, NULL);
        network_service_update();
        test_platform_network_power_errors(-TABOS_ENOTSUP, 0);
        assert(network_service_power_suspend() == -TABOS_ENOTSUP);
        assert(network_service_status(&status) && status.state == NETWORK_STATE_ONLINE);
        test_platform_network_power_errors(0, 0);
        const unsigned int calls = test_platform_network_connect_calls();
        assert(network_service_power_suspend() == 0 && network_service_power_suspend() == 0);
        assert(!network_service_connect("replacement", "", true));
        assert(!network_service_disconnect());
        test_platform_advance_time_ms(5000U);
        network_service_update();
        assert(network_service_next_deadline() == UINT64_MAX);
        assert(test_platform_network_connect_calls() == calls);
        test_platform_network_power_errors(0, -TABOS_EIO);
        assert(network_service_power_resume() == -TABOS_EIO);
        assert(!network_service_connect("replacement", "", true));
        test_platform_network_power_errors(0, 0);
        assert(network_service_power_resume() == 0 && network_service_power_resume() == 0);
        assert(network_service_status(&status) && status.state == NETWORK_STATE_OFFLINE && status.ipv4[0] == '\0');
        assert(strcmp(status.ssid, "retained-ssid") == 0 && !status.auto_connect);
        test_platform_advance_time_ms(999U);
        network_service_update();
        assert(test_platform_network_connect_calls() == calls);
        test_platform_advance_time_ms(1U);
        network_service_update();
        assert(test_platform_network_connect_calls() == calls + 1U);
        /* No AP: ordinary bounded connection failure, not transport failure. */
        for (unsigned int attempt = 0; attempt < 3U; ++attempt) {
            test_platform_network_set_state(PLATFORM_NETWORK_FAILED, "AP absent");
            network_service_update();
            test_platform_advance_time_ms(1000U);
            network_service_update();
        }
        assert(test_platform_network_connect_calls() == calls + 3U);
        assert(network_service_next_deadline() == UINT64_MAX);
        assert(network_service_disconnect());
        assert(network_service_power_suspend() == 0 && network_service_power_resume() == 0);
        assert(network_service_next_deadline() == UINT64_MAX);
    }
    assert(network_service_power_suspend() == 0);
    network_service_shutdown();
    assert(network_service_init());
    network_service_shutdown();
    filesystem_shutdown();
}

int main(void)
{
    media();
    input_display();
    network();
    return 0;
}
