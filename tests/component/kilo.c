#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <kilo/editor.h>
#include <kilo/input.h>
#include <kilo/storage.h>
#include <kilo/render.h>
#include <tabos/internal/terminal.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "Kilo test failed: %s (errno %d)\n", message, errno);
        exit(1);
    }
}
static int allocations = -1;
static void* allocate(size_t size)
{
    if (allocations == 0) {
        return NULL;
    }
    if (allocations > 0) {
        --allocations;
    }
    return malloc(size);
}
static kilo_action_t key(kilo_editor_t* e, tabos_key_t code, uint8_t mods, bool repeat)
{
    const tabos_input_event_t event = {.type = TABOS_INPUT_KEY_DOWN, .key = code, .modifiers = mods, .repeat = repeat};
    return kilo_input(e, &event);
}
static void text(kilo_editor_t* e, const char* value)
{
    tabos_input_event_t event = {.type = TABOS_INPUT_TEXT};
    (void) snprintf(event.text, sizeof(event.text), "%s", value);
    (void) kilo_input(e, &event);
}
static void editing(void)
{
    kilo_editor_t e;
    check(kilo_init(&e, "fixture.c") && kilo_append_row(&e, "", 0U, 0U), "empty document");
    e.screenrows = 3U;
    e.screencols = 8U;
    e.allocate   = allocate;
    text(&e, "int\t\200");
    check(e.row[0].size == 4U && (unsigned char) e.row[0].chars[3] == 128U, "control text ignored, CP437 preserved");
    e.cx = 3U;
    (void) key(&e, TABOS_KEY_TAB, 0, false);
    text(&e, "\t");
    check(e.row[0].size == 5U && kilo_render_column(&e.row[0], 4U) == 4U, "tab inserted exactly once");
    check(e.row[0].hl[0] == HL_KEYWORD2, "upstream C keyword highlighting");
    allocations = 0;
    check(!kilo_insert(&e, 'X') && e.row[0].size == 5U, "failed insert preserves row");
    allocations = 2;
    check(!kilo_split(&e) && e.numrows == 1U && e.cx == 4U, "failed split preserves cursor and row");
    allocations = -1;
    (void) key(&e, TABOS_KEY_ENTER, 0, false);
    text(&e, "\n");
    check(e.numrows == 2U && e.row[1].size == 1U, "enter split exactly once");
    allocations = 0;
    check(!kilo_delete(&e, true) && e.numrows == 2U && e.cy == 1U, "failed join transactional");
    allocations = -1;
    check(kilo_delete(&e, true) && e.numrows == 1U && e.cx == 4U, "backspace joins");
    check(kilo_delete(&e, false) && e.row[0].size == 4U, "forward delete");
    (void) key(&e, TABOS_KEY_Q, TABOS_MODIFIER_CONTROL, false);
    check(e.confirming && !e.quit, "dirty quit prompt");
    (void) key(&e, TABOS_KEY_Y, 0U, true);
    check(!e.quit, "held Y cannot discard");
    (void) key(&e, TABOS_KEY_ESCAPE, 0U, false);
    text(&e, "n");
    check(e.row[0].size == 4U, "prompt transition text suppressed");
    (void) key(&e, TABOS_KEY_F, TABOS_MODIFIER_CONTROL, false);
    (void) key(&e, TABOS_KEY_I, 0U, false);
    text(&e, "int");
    check(e.searching && e.cx == 0U, "incremental find");
    (void) key(&e, TABOS_KEY_ESCAPE, 0U, false);
    check(e.cx == 4U, "cancel restores position");
    (void) key(&e, TABOS_KEY_LEFT, TABOS_MODIFIER_CONTROL, true);
    check(e.cx == 0U, "Ctrl-left reaches editor on repeat");
    check(key(&e, TABOS_KEY_S, TABOS_MODIFIER_CONTROL, true) == KILO_IDLE, "held save ignored");
    e.cx = 0U;
    check(kilo_split(&e), "split at start");
    check(kilo_insert(&e, '/') && kilo_insert(&e, '*'), "comment insert");
    check(kilo_append_row(&e, "continued */ int", 16U, 0U), "comment continuation");
    kilo_highlight(&e, 0U);
    check(e.row[2].hl[0] == HL_MLCOMMENT, "iterative multiline highlight");
    kilo_dispose(&e);
    check(kilo_init(&e, "plain.unknown"), "unfamiliar extension");
    char* line = malloc(KILO_LINE_MAX + 1U);
    check(line != NULL, "fixture allocation");
    memset(line, '\t', KILO_LINE_MAX);
    line[KILO_LINE_MAX] = 0;
    for (size_t i = 0U; i < 15U; ++i) {
        check(kilo_append_row(&e, line, KILO_LINE_MAX, 1U), "near-limit tab file");
    }
    check(kilo_render_column(&e.row[0], KILO_LINE_MAX) == KILO_LINE_MAX * 4U,
          "worst-case tab expansion without allocation");
    e.cx = KILO_LINE_MAX;
    check(!kilo_insert(&e, 'a'), "line ceiling");
    check(!kilo_append_row(&e, line, KILO_LINE_MAX, 1U), "document ceiling");
    free(line);
    kilo_dispose(&e);
}
static void boundaries(void)
{
    kilo_editor_t e;
    check(kilo_init(&e, "search.txt") && kilo_append_row(&e, "one one", 7U, 1U) && kilo_append_row(&e, "two", 3U, 0U),
          "search rows");
    e.screenrows = 1U;
    e.screencols = 3U;
    memcpy(e.query, "one", 4U);
    e.query_size = 3U;
    check(kilo_search(&e, 1, true) && e.cx == 4U, "find next in same row");
    check(kilo_search(&e, 1, true) && e.cx == 0U, "find wraps document");
    check(kilo_search(&e, -1, true) && e.cx == 4U, "find previous wraps");
    kilo_scroll(&e);
    check(e.coloff == 2U, "horizontal viewport follows byte cursor");
    (void) key(&e, TABOS_KEY_PAGE_DOWN, 0U, false);
    kilo_scroll(&e);
    check(e.cy == 1U && e.cx == 3U && e.rowoff == 1U, "page clips cursor at shorter line");
    (void) key(&e, TABOS_KEY_PAGE_UP, 0U, false);
    check(e.cy == 0U, "page up boundary");
    kilo_dispose(&e);
    check(kilo_init(&e, "rows.txt"), "row-limit init");
    for (size_t i = 0U; i < KILO_ROWS_MAX; ++i) {
        check(kilo_append_row(&e, "", 0U, 1U), "row ceiling fixture");
    }
    check(!kilo_split(&e) && e.numrows == KILO_ROWS_MAX && !e.dirty, "split at row ceiling preserves clean state");
    kilo_dispose(&e);
}

static int rename_calls, fail_rename, fail_rollback, close_calls, fail_close;
static bool fail_write, short_io, cleanup_fail, zero_write, fail_read;
static ssize_t read_injected(int fd, void* data, size_t size)
{
    if (fail_read) {
        errno = EIO;
        return -1;
    }
    return read(fd, data, short_io && size > 1U ? 1U : size);
}
static ssize_t write_injected(int fd, const void* data, size_t size)
{
    if (zero_write) {
        return 0;
    }
    if (fail_write) {
        errno = ENOSPC;
        return -1;
    }
    return write(fd, data, short_io && size > 1U ? 1U : size);
}
static int close_injected(int fd)
{
    const int result = close(fd);
    ++close_calls;
    if (close_calls == fail_close) {
        errno = EIO;
        return -1;
    }
    return result;
}
static int rename_injected(const char* src, const char* dst)
{
    ++rename_calls;
    if (rename_calls == fail_rename || rename_calls == fail_rollback) {
        errno = EIO;
        return -1;
    }
    // Model FatFs refusal to replace an existing destination.
    struct stat info;
    if (stat(dst, &info) == 0) {
        errno = EEXIST;
        return -1;
    }
    return rename(src, dst);
}
static int unlink_injected(const char* path)
{
    if (cleanup_fail && rename_calls >= 2 && strstr(path, ".bak") != NULL) {
        errno = EIO;
        return -1;
    }
    return unlink(path);
}
static void fixture(const char* path, const void* bytes, size_t size)
{
    FILE* file = fopen(path, "wb");
    check(file != NULL, "create fixture");
    check(fwrite(bytes, 1U, size, file) == size && fclose(file) == 0, "write fixture");
}
static bool contents(const char* path, const char* expected, size_t length)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    char bytes[256];
    size_t n = fread(bytes, 1U, sizeof(bytes), file);
    (void) fclose(file);
    return n == length && memcmp(bytes, expected, length) == 0;
}
static void storage(void)
{
    char root[] = "/tmp/tabos-kilo-XXXXXX";
    check(mkdtemp(root) != NULL, "temporary root");
    char path[512], tmp[512], bak[512];
    (void) snprintf(path, sizeof(path), "%s/a space.c", root);
    (void) snprintf(tmp, sizeof(tmp), "%s/.kilo-0.tmp", root);
    (void) snprintf(bak, sizeof(bak), "%s/.kilo-0.bak", root);
    const char original[] = "a\r\nb\nc\r\n\r\n\200\033[2J";
    fixture(path, original, sizeof(original) - 1U);
    kilo_storage_t io = kilo_storage_default;
    io.read           = read_injected;
    io.write          = write_injected;
    io.close          = close_injected;
    io.rename         = rename_injected;
    io.unlink         = unlink_injected;
    kilo_editor_t e;
    check(kilo_init(&e, path), "init storage");
    short_io = true;
    check(kilo_load(&e, &io) && e.newline == 2U && e.numrows == 5U, "short reads and mixed line endings");
    check(kilo_save(&e, &io) && contents(path, original, sizeof(original) - 1U),
          "byte-identical save with short writes and FAT rename");
    const size_t initial_bytes = e.bytes;
    fail_read                  = true;
    check(!kilo_load(&e, &io) && e.bytes == initial_bytes, "read error never treated as a new file");
    fail_read = false;
    check(kilo_insert(&e, 'X'), "modify document");
    zero_write = true;
    check(!kilo_save(&e, &io) && e.dirty && contents(path, original, sizeof(original) - 1U),
          "zero-progress write preserves original");
    zero_write = false;
    fail_write = true;
    check(!kilo_save(&e, &io) && e.dirty && contents(path, original, sizeof(original) - 1U),
          "full storage leaves original and dirty buffer");
    fail_write  = false;
    close_calls = 0;
    fail_close  = 1;
    check(!kilo_save(&e, &io) && contents(path, original, sizeof(original) - 1U), "staging close failure");
    fail_close   = 0;
    rename_calls = 0;
    fail_rename  = 2;
    check(!kilo_save(&e, &io) && e.dirty && contents(path, original, sizeof(original) - 1U),
          "install failure rolls back original");
    check(access(tmp, F_OK) == 0 && strstr(e.message, tmp) != NULL, "failed install retains named edited copy");
    check(unlink(tmp) == 0, "remove recovered staging");
    rename_calls  = 0;
    fail_rollback = 3;
    check(!kilo_save(&e, &io) && contents(bak, original, sizeof(original) - 1U) && access(tmp, F_OK) == 0,
          "rollback failure retains both copies");
    check(strstr(e.message, bak) != NULL && strstr(e.message, tmp) != NULL, "exact recovery paths");
    check(rename(bak, path) == 0 && unlink(tmp) == 0, "recover fixture");
    fail_rename   = 0;
    fail_rollback = 0;
    rename_calls  = 0;
    cleanup_fail  = true;
    check(kilo_save(&e, &io) && !e.dirty && strstr(e.message, bak) != NULL,
          "backup cleanup warning after successful save");
    cleanup_fail = false;
    check(unlink(bak) == 0, "remove leftover backup");
    fixture(tmp, "occupied", 8U);
    rename_calls = 0;
    check(kilo_save(&e, &io) && contents(tmp, "occupied", 8U), "exclusive temp collision preserves foreign file");
    check(unlink(tmp) == 0, "remove collision fixture");
    fixture(path, "a\0b", 3U);
    const size_t before = e.bytes;
    check(!kilo_load(&e, &io) && e.bytes == before, "binary reject leaves current document intact");
    check(unlink(path) == 0, "remove file");
    check(kilo_load(&e, &io) && e.bytes == 0U && access(path, F_OK) != 0, "missing file not created by open");
    check(kilo_save(&e, &io) && contents(path, "", 0U), "save creates empty file");
    check(unlink(path) == 0, "remove empty file");
    kilo_dispose(&e);
    check(kilo_init(&e, root) && !kilo_load(&e, &io), "reject directory");
    kilo_dispose(&e);
    check(kilo_init(&e, tmp) && kilo_load(&e, &io) && kilo_insert(&e, 'x') && kilo_save(&e, &io),
          "destination named like staging file");
    check(contents(tmp, "x", 1U), "temporary-style destination installed correctly");
    kilo_dispose(&e);
    check(unlink(tmp) == 0, "remove temp-style destination");
    check(rmdir(root) == 0, "clean storage");
}
static terminal_t terminal;
static ssize_t output(int fd, const void* data, size_t size)
{
    (void) fd;
    const size_t n = size > 7U ? 7U : size;
    char chunk[8];
    memcpy(chunk, data, n);
    chunk[n] = 0;
    terminal_write(&terminal, chunk);
    return (ssize_t) n;
}
static void rendering(void)
{
    enum {
        WIDTH  = 1280,
        HEIGHT = 720
    };
    platform_pixel_t* pixels           = calloc(WIDTH * HEIGHT, sizeof(*pixels));
    platform_framebuffer_t framebuffer = {.pixels = pixels, .width = WIDTH, .height = HEIGHT, .stride_pixels = WIDTH};
    check(terminal_init(&terminal, &framebuffer, 2U), "80x24 terminal");
    for (size_t i = 0U; i < 400U; ++i) {
        terminal_write(&terminal, "shell\n");
    }
    const uint64_t top = terminal.viewport_top;
    kilo_editor_t e;
    check(kilo_init(&e, "bad\033[2J.c"), "safe filename");
    check(kilo_append_row(&e, "A\033[2J\t\200", 8U, 0U), "unsafe text fixture");
    e.screenrows = 22U;
    e.screencols = 79U;
    check(kilo_render(&e, output), "split writes paint editor");
    check(terminal.viewport_top == top && terminal.cursor_visible, "paint does not scroll; cursor visible");
    check(terminal.cells[(top % terminal.line_capacity) * terminal.columns + 1U].character == '?',
          "control bytes inert");
    e.allocate  = allocate;
    allocations = 0;
    check(!kilo_render(&e, output) && e.numrows == 1U, "render allocation failure preserves document");
    allocations = -1;
    kilo_dispose(&e);
    terminal_shutdown(&terminal);
    free(pixels);
}
int main(void)
{
    editing();
    boundaries();
    storage();
    rendering();
    puts("Kilo editing, storage recovery, and terminal rendering passed");
    return 0;
}
