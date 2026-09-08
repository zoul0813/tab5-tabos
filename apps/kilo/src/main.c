#include <kilo/editor.h>
#include <kilo/input.h>
#include <kilo/render.h>
#include <kilo/storage.h>
#include <tabos/tty.h>
#include <tabos/wait.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool geometry(kilo_editor_t* e)
{
    tabos_tty_size_t size;
    if (ioctl(0, TABOS_TTY_GET_SIZE, &size) != 0 || size.rows < 4U || size.columns < 20U || size.rows > 258U ||
        size.columns > 513U) {
        kilo_status(e, "Terminal geometry unavailable or too small (minimum 20x4)");
        return false;
    }
    e->screenrows = size.rows - 2U;
    // TabOS wraps immediately: the final column remains blank on every row.
    e->screencols = size.columns - 1U;
    return true;
}
static void report(const char* text)
{
    while (*text != '\0') {
        const unsigned char c = (unsigned char) *text++;
        (void) fputc(c < 32U || c == 127U ? '?' : c, stderr);
    }
    (void) fputc('\n', stderr);
}
int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("Usage: kilo <path>\nCtrl-S save; Ctrl-F find; Ctrl-L redraw; Ctrl-Q quit.\nDirty quit requires fresh Y; "
             "N/Escape cancels.\n256 KiB, 8192 lines, 16 KiB/line; CP437 bytes; 4-column tabs.");
        return 0;
    }
    if (argc != 2 || argv[1][0] == '\0' || argv[1][0] == '-') {
        fputs("Usage: kilo <path>\n", stderr);
        return 1;
    }
    kilo_editor_t e;
    if (!kilo_init(&e, argv[1])) {
        fputs("kilo: out of memory\n", stderr);
        return 1;
    }
    uint32_t inherited = 0U;
    bool mode_changed  = false;
    int status         = 1;
    if (!kilo_load(&e, &kilo_storage_default) || !geometry(&e)) {
        goto cleanup;
    }
    if (ioctl(0, TABOS_TTY_GET_MODE, &inherited) != 0 ||
        ioctl(0, TABOS_TTY_SET_MODE, inherited & ~(uint32_t) (TABOS_TTY_MODE_SCROLL_KEYS | TABOS_TTY_MODE_RAW_INPUT)) !=
            0) {
        kilo_status(&e, "Cannot set terminal input mode");
        goto cleanup;
    }
    mode_changed                     = true;
    const tabos_wait_source_t source = tabos_input_wait_source();
    if (source == TABOS_WAIT_SOURCE_INVALID) {
        kilo_status(&e, "Keyboard wait unavailable");
        goto cleanup;
    }
    bool redraw = true;
    while (!e.quit) {
        tabos_input_event_t event;
        size_t count = 0U;
        while (count < 32U && !e.quit && tabos_input_poll(&event)) {
            ++count;
            const kilo_action_t action = kilo_input(&e, &event);
            if (action == KILO_SAVE) {
                (void) kilo_save(&e, &kilo_storage_default);
            }
            if (action == KILO_RESIZE && !geometry(&e)) {
                goto cleanup;
            }
            redraw = redraw || action != KILO_IDLE;
        }
        if (e.quit) {
            break;
        }
        if (redraw && !kilo_render(&e, write)) {
            kilo_status(&e, "Terminal render failed");
            goto cleanup;
        }
        redraw = false;
        if (count == 32U) {
            continue;
        }
        tabos_wait_item_t item = {.source = source, .events = TABOS_WAIT_READABLE};
        if (tabos_wait(&item, 1U, TABOS_WAIT_TIMEOUT_INFINITE) < 0) {
            kilo_status(&e, "Keyboard wait failed");
            goto cleanup;
        }
    }
    status = 0;
cleanup:
    if (mode_changed) {
        const char reset[] = "\033[0m\033[?25h\033[2J\033[H";
        size_t offset      = 0U;
        while (offset < sizeof(reset) - 1U) {
            const ssize_t written = write(1, reset + offset, sizeof(reset) - 1U - offset);
            if (written <= 0) {
                status = 1;
                break;
            }
            offset += (size_t) written;
        }
        if (ioctl(0, TABOS_TTY_SET_MODE, inherited) != 0) {
            status = 1;
        }
    }
    if (e.recovery[0] != '\0') {
        report(e.recovery);
    }
    if (status != 0 && e.message[0] != '\0' && strcmp(e.message, e.recovery) != 0) {
        report(e.message);
    }
    kilo_dispose(&e);
    return status;
}
