/* Kilo row/viewport rendering, adapted for the TabOS immediate-wrap terminal. */
#include <kilo/render.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        char* data;
        size_t used, capacity;
        bool failed;
} frame_t;
static void append(frame_t* f, const char* text, size_t size)
{
    if (size > f->capacity - f->used) {
        f->failed = true;
        return;
    }
    memcpy(f->data + f->used, text, size);
    f->used += size;
}
static void literal(frame_t* f, const char* text)
{
    append(f, text, strlen(text));
}
static void position(frame_t* f, size_t row, size_t column)
{
    char sequence[48];
    (void) snprintf(sequence, sizeof(sequence), "\033[%u;%uH", (unsigned) row + 1U, (unsigned) column + 1U);
    literal(f, sequence);
}
static char inert(unsigned char c)
{
    return c < 32U || c == 127U ? '?' : (char) c;
}
static void safe_text(frame_t* f, const char* text, size_t width)
{
    for (size_t i = 0U; i < width && text[i] != '\0'; ++i) {
        const char c = inert((unsigned char) text[i]);
        append(f, &c, 1U);
    }
}
static unsigned color(unsigned char highlight)
{
    static const unsigned colors[] = {39, 39, 36, 36, 33, 32, 35, 31, 34};
    return highlight <= HL_MATCH ? colors[highlight] : 39U;
}
bool kilo_render(kilo_editor_t* e, kilo_output_fn output)
{
    if (e->screenrows == 0U || e->screencols == 0U || e->screenrows > 256U || e->screencols > 512U) {
        return false;
    }
    const size_t capacity = (e->screenrows + 2U) * (e->screencols * 8U + 64U) + 128U;
    frame_t f             = {.data = e->allocate(capacity), .capacity = capacity};
    if (f.data == NULL) {
        kilo_status(e, "Cannot allocate terminal frame");
        return false;
    }
    kilo_scroll(e);
    literal(&f, "\033[?25l\033[0m");
    for (size_t y = 0U; y < e->screenrows; ++y) {
        position(&f, y, 0U);
        literal(&f, "\033[0m\033[2K");
        const size_t r = y + e->rowoff;
        if (r >= e->numrows) {
            literal(&f, "~");
            continue;
        }
        const kilo_row_t* row   = &e->row[r];
        size_t column           = 0U;
        unsigned previous_color = 39U;
        for (size_t i = 0U; i < row->size && column < e->coloff + e->screencols; ++i) {
            unsigned selected = color(row->hl[i]);
            if (e->searching && e->query_size != 0U && r == e->cy && i >= e->cx && i - e->cx < e->query_size) {
                selected = 34U;
            }
            const unsigned char c = (unsigned char) row->chars[i];
            const size_t count    = c == '\t' ? KILO_TAB_WIDTH - column % KILO_TAB_WIDTH : 1U;
            for (size_t j = 0U; j < count; ++j, ++column) {
                if (column < e->coloff || column >= e->coloff + e->screencols) {
                    continue;
                }
                if (selected != previous_color) {
                    char sequence[16];
                    (void) snprintf(sequence, sizeof(sequence), "\033[%um", selected);
                    literal(&f, sequence);
                    previous_color = selected;
                }
                const char shown = c == '\t' ? ' ' : inert(c);
                append(&f, &shown, 1U);
            }
        }
    }
    position(&f, e->screenrows, 0U);
    literal(&f, "\033[0m\033[2K\033[7m");
    char status[768];
    (void) snprintf(status, sizeof(status), "%s%s | %u:%u | %u lines", e->dirty ? "* " : "", e->filename,
                    (unsigned) e->cy + 1U, (unsigned) e->cx + 1U, (unsigned) e->numrows);
    safe_text(&f, status, e->screencols);
    char location[64];
    (void) snprintf(location, sizeof(location), " %u:%u %uL", (unsigned) e->cy + 1U, (unsigned) e->cx + 1U,
                    (unsigned) e->numrows);
    const size_t location_length = strlen(location);
    if (location_length <= e->screencols) {
        position(&f, e->screenrows, e->screencols - location_length);
        safe_text(&f, location, location_length);
    }
    literal(&f, "\033[0m");
    position(&f, e->screenrows + 1U, 0U);
    literal(&f, "\033[2K");
    if (e->confirming) {
        safe_text(&f, "Discard unsaved edits? Y = discard, N/Escape = cancel", e->screencols);
    } else if (e->searching) {
        (void) snprintf(status, sizeof(status), "Find: %s (arrows next/prev, Enter accept, Esc cancel)", e->query);
        safe_text(&f, status, e->screencols);
    } else {
        safe_text(&f, e->message[0] != '\0' ? e->message : "Ctrl-S save | Ctrl-F find | Ctrl-L redraw | Ctrl-Q quit",
                  e->screencols);
    }
    position(&f, e->cy - e->rowoff, kilo_render_column(&e->row[e->cy], e->cx) - e->coloff);
    literal(&f, "\033[?25h");
    bool ok     = !f.failed;
    size_t sent = 0U;
    while (ok && sent < f.used) {
        const ssize_t n = output(1, f.data + sent, f.used - sent);
        if (n <= 0 || (size_t) n > f.used - sent) {
            ok = false;
        } else {
            sent += (size_t) n;
        }
    }
    free(f.data);
    return ok;
}
