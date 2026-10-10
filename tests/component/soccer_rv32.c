#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/application.h>
#include <tabos/internal/input.h>
#include <tabos/internal/audio.h>
#include <tabos/internal/display.h>
#include <tabos/internal/console.h>
#include <tabos/internal/runtime.h>
#include <tabos/internal/application.h>
#include <tabos/platform/storage_backend.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// This optional executable takes the actual SDK-built shell as argv[1].
// Override only the host drive mapping; runtime, interpreter and SDK stay real.
static char storage_root[] = "/tmp/tabos-soccer-rv32-XXXXXX";

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "RV32 Soccer test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char* letter, char* root, size_t root_size, bool* removable, const char** name)
{
    if (index != 0U || strlen(storage_root) >= root_size) {
        return false;
    }
    strcpy(root, storage_root);
    *letter    = 'T';
    *removable = true;
    *name      = "Shell test";
    return true;
}

void storage_backend_unmount(char letter)
{
    (void) letter;
}

bool storage_backend_info(char letter, uint64_t* total_bytes, uint64_t* free_bytes)
{
    (void) letter;
    *total_bytes = 1024U * 1024U;
    *free_bytes  = 512U * 1024U;
    return true;
}

static void pump(void)
{
    for (unsigned int index = 0U; index < 10U; ++index) {
        // Keep batches short enough to inject controls before a full squad advances
        // through several frames on the slower instrumented interpreter.
        const platform_runtime_events_t events = platform_runtime_wait_until(platform_time_ms());
        kernel_runtime_update(events);
    }
}

static void key(tabos_key_t code, uint8_t modifiers)
{
    tabos_input_event_t event = {.type = TABOS_INPUT_KEY_DOWN, .key = code, .modifiers = modifiers};
    check(input_submit(&event), "key down");
    event.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&event), "key up");
    if (code == TABOS_KEY_ENTER) {
        const tabos_input_event_t newline = {.type = TABOS_INPUT_TEXT, .text = "\n"};
        check(input_submit(&newline), "normalized enter text");
    }
    pump();
}

static void text(const char* value)
{
    while (*value != '\0') {
        const tabos_input_event_t down = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
        check(input_submit(&down), "text physical key");
        const tabos_input_event_t event = {
            .type = TABOS_INPUT_TEXT, .text = {*value++, '\0'}
        };
        check(input_submit(&event), "text input");
        const tabos_input_event_t up = {.type = TABOS_INPUT_KEY_UP, .key = TABOS_KEY_A};
        check(input_submit(&up), "text key release");
        pump();
    }
}

static void boot(void)
{
    check(kernel_runtime_init() && platform_init(getenv("SOCCER_TEST_WINDOW") == NULL) && kernel_runtime_start(false),
          "runtime startup");
    check(tabos_app_launch_path("T:/shell") == TABOS_APP_RESULT_OK, "launch real RV32 shell");
    pump();
}

static void stop(void)
{
    kernel_runtime_shutdown();
    platform_shutdown();
}


static void copy(const char* src, const char* dst)
{
    FILE* source      = fopen(src, "rb");
    FILE* destination = fopen(dst, "wb");
    check(source != NULL && destination != NULL, "open artifact");
    char bytes[4096];
    size_t size;
    while ((size = fread(bytes, 1U, sizeof(bytes), source)) > 0U) {
        check(fwrite(bytes, 1U, size, destination) == size, "copy artifact");
    }
    check(!ferror(source) && fclose(source) == 0 && fclose(destination) == 0, "close artifact");
}
static void settle(void)
{
    for (unsigned int i = 0U; i < 200U; ++i) {
        pump();
        if (!kernel_application_system_runnable()) {
            return;
        }
    }
    check(false, "guest reaches input wait");
}

static uint64_t frame_hash(void)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    uint64_t hash                       = 14695981039346656037ULL;
    for (uint32_t y = 0U; y < frame->height; ++y) {
        for (uint32_t x = 0U; x < frame->width; ++x) {
            hash ^= frame->pixels[y * frame->stride_pixels + x];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

static void run_for(uint32_t milliseconds)
{
    const uint64_t end = platform_time_ms() + milliseconds;
    while (platform_time_ms() < end) {
        pump();
    }
    /* Live play may stay runnable while rendering and synthesizing sound. Do not
       advance an unbounded number of match frames trying to find an idle slice. */
}

static void capture(const char* path)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    FILE* file                          = fopen(path, "wb");
    check(file != NULL, "open screenshot");
    fprintf(file, "P6\n%zu %zu\n255\n", frame->width, frame->height);
    for (uint32_t y = 0U; y < frame->height; ++y) {
        for (uint32_t x = 0U; x < frame->width; ++x) {
            const uint16_t pixel      = frame->pixels[y * frame->stride_pixels + x];
            const unsigned char rgb[] = {(unsigned char) (((pixel >> 11U) & 31U) * 255U / 31U),
                                         (unsigned char) (((pixel >> 5U) & 63U) * 255U / 63U),
                                         (unsigned char) ((pixel & 31U) * 255U / 31U)};
            check(fwrite(rgb, 1U, sizeof(rgb), file) == sizeof(rgb), "write screenshot");
        }
    }
    check(fclose(file) == 0, "close screenshot");
}

static void hold(tabos_key_t code, bool down)
{
    const tabos_input_event_t event = {.type = down ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP, .key = code};
    check(input_submit(&event), "held direction");
    pump();
}
static bool shot_charging(void)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    /* Logical power bar at (22,58), on the centered 3x canvas. */
    return frame->pixels[175U * frame->stride_pixels + 229U] == TABOS_RGB565(255, 212, 80);
}

static bool pass_received(void)
{
    /* Scores are intentionally only shown in the central pause banner. */
    key(TABOS_KEY_P, 0U);
    settle();
    const platform_framebuffer_t* frame = display_framebuffer();
    const bool received = frame->pixels[348U * frame->stride_pixels + 751U] == TABOS_RGB565(14, 28, 40) &&
                          frame->pixels[348U * frame->stride_pixels + 754U] == TABOS_RGB565(224, 240, 212);
    key(TABOS_KEY_P, 0U);
    run_for(100U);
    return received;
}
int main(int argc, char** argv)
{
    check(argc == 3 || argc == 4, "pass shell, soccer, optional PPM screenshot");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char shell_path[512], soccer_path[512], history_path[512], user_path[512];
    (void) snprintf(shell_path, sizeof(shell_path), "%s/shell", storage_root);
    (void) snprintf(soccer_path, sizeof(soccer_path), "%s/soccer", storage_root);
    (void) snprintf(history_path, sizeof(history_path), "%s/user/history.txt", storage_root);
    (void) snprintf(user_path, sizeof(user_path), "%s/user", storage_root);
    copy(argv[1], shell_path);
    copy(argv[2], soccer_path);
    if (getenv("SOCCER_TEST_WINDOW") == NULL) {
        check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0 && setenv("SDL_AUDIODRIVER", "dummy", 1) == 0, "headless");
    }
    boot();
    unsigned long rounds    = 2U;
    const char* rounds_text = getenv("SOCCER_TEST_ROUNDS");
    if (rounds_text != NULL) {
        char* end = NULL;
        rounds    = strtoul(rounds_text, &end, 10);
        check(rounds_text[0] != '\0' && *end == '\0' && rounds >= 2U && rounds <= 20U,
              "SOCCER_TEST_ROUNDS must be 2 through 20");
    }
    for (unsigned int round = 0U; round < rounds; ++round) {
        fprintf(stderr, "Soccer RV32 round %u\n", round + 1U);
        text(round == 0U ? "./soccer --profile" : "./soccer");
        key(TABOS_KEY_ENTER, 0U);
        settle();
        check(tabos_process_count() == 2U && console_next_deadline() == UINT64_MAX, "fullscreen child");
        const uint64_t title = frame_hash();
        key(TABOS_KEY_LEFT, 0U);
        settle();
        check(frame_hash() != title, "title selects Easy difficulty");
        const uint64_t easy_title = frame_hash();
        key(TABOS_KEY_LEFT, 0U);
        settle();
        check(frame_hash() != title && frame_hash() != easy_title, "difficulty wraps to Hard");
        key(TABOS_KEY_A, 0U);
        settle();
        check(frame_hash() == title, "A selects Normal without moving a player");
        key(TABOS_KEY_D, 0U);
        key(TABOS_KEY_RIGHT, 0U);
        key(TABOS_KEY_RIGHT, 0U);
        settle();
        check(frame_hash() == title, "right selection cycles all three levels");
        run_for(100U);
        check(frame_hash() == title, "title idle");
        key(TABOS_KEY_M, 0U);
        settle();
        check(frame_hash() != title, "mute indicator updates on idle title");
        key(TABOS_KEY_M, 0U);
        settle();
        check(frame_hash() == title, "sound can be re-enabled without starting match");
        key(TABOS_KEY_ENTER, 0U);
        run_for(1800U);
        check(frame_hash() != title, "kick-off starts match");
        check(audio_service_power_inhibited(), "match prepares and retains its audio stream");
        if (argc == 4 && round == 0U) {
            capture(argv[3]);
        }
        if (round == 0U) {
            hold(TABOS_KEY_W, true);
            run_for(50U);
            key(TABOS_KEY_J, 0U);
            hold(TABOS_KEY_W, false);
            run_for(100U); /* Let the tap reach a simulation tick before pausing to inspect it. */
            const uint64_t pass_deadline = platform_time_ms() + 3000U;
            while (!pass_received() && platform_time_ms() < pass_deadline) {
                pump();
            }
            check(pass_received(), "J pass reaches teammate and increments completions");
            key(TABOS_KEY_J, 0U);
            run_for(700U);
            key(TABOS_KEY_R, 0U);
            run_for(1800U);
            hold(TABOS_KEY_W, true);
            run_for(50U);
            hold(TABOS_KEY_W, false);
            hold(TABOS_KEY_J, true);
            run_for(350U);
            hold(TABOS_KEY_J, false);
            run_for(180U);
            if (argc == 4) {
                capture(argv[3]);
            }
            const uint64_t lob_deadline = platform_time_ms() + 4000U;
            while (!pass_received() && platform_time_ms() < lob_deadline) {
                pump();
            }
            check(pass_received(), "Held J lofted pass lands and reaches teammate");
            key(TABOS_KEY_R, 0U);
            run_for(1800U);
        }
        const uint64_t initial = frame_hash();
        hold(TABOS_KEY_UP, true);
        hold(TABOS_KEY_A, true);
        run_for(150U);
        hold(TABOS_KEY_UP, false);
        hold(TABOS_KEY_A, false);
        check(frame_hash() != initial, "diagonal input moves player");
        key(TABOS_KEY_P, 0U);
        settle();
        check(!audio_service_power_inhibited(), "pause releases the audio device");
        const uint64_t paused = frame_hash();
        run_for(100U);
        check(frame_hash() == paused, "pause freezes scene");
        key(TABOS_KEY_M, 0U);
        key(TABOS_KEY_M, 0U);
        settle();
        check(frame_hash() == paused, "mute toggles preserve paused match");
        key(TABOS_KEY_P, 0U);
        key(TABOS_KEY_R, 0U);
        run_for(1800U);
        check(!pass_received(), "new match clears completed passes");
        const uint64_t before_shot = frame_hash();
        key(TABOS_KEY_K, 0U);
        run_for(300U);
        check(frame_hash() != before_shot, "shot and scrolling update match scene");
        key(TABOS_KEY_R, 0U);
        run_for(1800U);
        hold(TABOS_KEY_K, true);
        run_for(120U);
        check(shot_charging(), "holding shot displays power meter");
        if (round == 0U) {
            hold(TABOS_KEY_K, false);
            run_for(150U);
            check(!shot_charging(), "releasing shot clears power meter");
        } else {
            key(TABOS_KEY_P, 0U);
            settle();
            key(TABOS_KEY_P, 0U);
            hold(TABOS_KEY_K, false);
            run_for(100U);
            check(!shot_charging(), "pause cancels pending shot charge");
        }
        key(TABOS_KEY_R, 0U);
        run_for(100U);
        key(TABOS_KEY_P, 0U);
        const uint64_t kickoff_paused = frame_hash();
        run_for(200U);
        check(frame_hash() == kickoff_paused, "kick-off can pause without advancing");
        key(TABOS_KEY_P, 0U);
        key(round == 0U ? TABOS_KEY_Q : TABOS_KEY_ESCAPE, 0U);
        for (unsigned int i = 0U; i < 100U && tabos_process_count() != 1U; ++i) {
            pump();
        }
        int status = -1;
        check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "shell restored");
        check(tabos_app_last_exit_status(&status) && status == 0, "zero exit status");
        check(console_next_deadline() != UINT64_MAX, "terminal restored");
    }
    stop();
    (void) unlink(history_path);
    (void) rmdir(user_path);
    check(unlink(shell_path) == 0 && unlink(soccer_path) == 0 && rmdir(storage_root) == 0, "remove fixtures");
    puts("Soccer RV32 gameplay passed");
    return 0;
}
