/* Adapted from Kilo, Copyright (C) 2016 Salvatore Sanfilippo. See ../LICENSE. */
#include <kilo/editor.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* const keywords[] = {
    /* C Keywords */
    "auto", "break", "case", "continue", "default", "do", "else", "enum", "extern", "for", "goto", "if", "register",
    "return", "sizeof", "static", "struct", "switch", "typedef", "union", "volatile", "while", "NULL",

    /* C++ Keywords */
    "alignas", "alignof", "and", "and_eq", "asm", "bitand", "bitor", "class", "compl", "constexpr", "const_cast",
    "deltype", "delete", "dynamic_cast", "explicit", "export", "false", "friend", "inline", "mutable", "namespace",
    "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
    "reinterpret_cast", "static_assert", "static_cast", "template", "this", "thread_local", "throw", "true", "try",
    "typeid", "typename", "virtual", "xor", "xor_eq",

    /* C types */
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", "short|", "auto|", "const|",
    "bool|", NULL};
void kilo_status(kilo_editor_t* e, const char* text)
{
    (void) snprintf(e->message, sizeof(e->message), "%s", text);
}
static void free_row(kilo_row_t* row)
{
    free(row->chars);
    free(row->hl);
    *row = (kilo_row_t) {0};
}
static bool make_row(kilo_editor_t* e, kilo_row_t* row, const char* text, size_t size, uint8_t ending)
{
    if (size > KILO_LINE_MAX) {
        kilo_status(e, "Line limit reached (16 KiB)");
        return false;
    }
    char* chars       = e->allocate(size + 1U);
    unsigned char* hl = e->allocate(size + 1U);
    if (chars == NULL || hl == NULL) {
        free(chars);
        free(hl);
        kilo_status(e, "Out of memory; document unchanged");
        return false;
    }
    if (size != 0U) {
        memcpy(chars, text, size);
    }
    chars[size] = '\0';
    memset(hl, HL_NORMAL, size + 1U);
    *row = (kilo_row_t) {.chars = chars, .hl = hl, .size = size, .ending = ending};
    return true;
}
bool kilo_init(kilo_editor_t* e, const char* filename)
{
    *e              = (kilo_editor_t) {.filename = filename, .newline = 1U, .allocate = malloc};
    e->row          = calloc(KILO_ROWS_MAX, sizeof(*e->row));
    const char* ext = strrchr(filename, '.');
    e->syntax       = ext != NULL && (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 || strcmp(ext, ".cpp") == 0 ||
                                strcmp(ext, ".hpp") == 0 || strcmp(ext, ".cc") == 0);
    return e->row != NULL;
}
void kilo_dispose(kilo_editor_t* e)
{
    for (size_t i = 0U; i < e->numrows; ++i) {
        free_row(&e->row[i]);
    }
    free(e->row);
    *e = (kilo_editor_t) {0};
}
bool kilo_append_row(kilo_editor_t* e, const char* text, size_t size, uint8_t ending)
{
    if (e->numrows >= KILO_ROWS_MAX || size > KILO_BYTES_MAX || size + ending > KILO_BYTES_MAX - e->bytes) {
        kilo_status(e, "Document limit reached (256 KiB / 8192 lines)");
        return false;
    }
    if (!make_row(e, &e->row[e->numrows], text, size, ending)) {
        return false;
    }
    ++e->numrows;
    e->bytes += size + ending;
    return true;
}
static bool separator(unsigned char c)
{
    return c == 0U || c == ' ' || c == '\t' || c == '\r' || strchr(",.()+-/*=~%[];{}:!&|<>?", c) != NULL;
}
void kilo_highlight(kilo_editor_t* e, size_t from)
{
    bool comment = from > 0U && e->row[from - 1U].open_comment;
    for (size_t r = from; r < e->numrows; ++r) {
        kilo_row_t* row = &e->row[r];
        memset(row->hl, HL_NORMAL, row->size);
        bool prev_sep       = true;
        unsigned char quote = 0U;
        size_t i            = 0U;
        while (e->syntax && i < row->size) {
            const unsigned char c    = (unsigned char) row->chars[i];
            const unsigned char next = (unsigned char) row->chars[i + 1U];
            if (comment) {
                row->hl[i++] = HL_MLCOMMENT;
                if (c == '*' && next == '/') {
                    row->hl[i++] = HL_MLCOMMENT;
                    comment      = false;
                    prev_sep     = true;
                }
                continue;
            }
            if (quote != 0U) {
                row->hl[i++] = HL_STRING;
                if (c == '\\' && i < row->size) {
                    row->hl[i++] = HL_STRING;
                } else if (c == quote) {
                    quote = 0U;
                }
                continue;
            }
            if (c == '/' && next == '/') {
                memset(row->hl + i, HL_COMMENT, row->size - i);
                break;
            }
            if (c == '/' && next == '*') {
                row->hl[i++] = HL_MLCOMMENT;
                row->hl[i++] = HL_MLCOMMENT;
                comment      = true;
                continue;
            }
            if (c == '"' || c == '\'') {
                quote        = c;
                row->hl[i++] = HL_STRING;
                prev_sep     = false;
                continue;
            }
            if ((c >= '0' && c <= '9' && (prev_sep || (i > 0U && row->hl[i - 1U] == HL_NUMBER))) ||
                (c == '.' && i > 0U && row->hl[i - 1U] == HL_NUMBER)) {
                row->hl[i++] = HL_NUMBER;
                prev_sep     = false;
                continue;
            }
            bool matched = false;
            for (size_t k = 0U; prev_sep && keywords[k] != NULL; ++k) {
                size_t length   = strlen(keywords[k]);
                const bool type = keywords[k][length - 1U] == '|';
                if (type) {
                    --length;
                }
                if (length <= row->size - i && memcmp(row->chars + i, keywords[k], length) == 0 &&
                    separator((unsigned char) row->chars[i + length])) {
                    memset(row->hl + i, type ? HL_KEYWORD2 : HL_KEYWORD1, length);
                    i        += length;
                    matched   = true;
                    prev_sep  = false;
                    break;
                }
            }
            if (!matched) {
                prev_sep = separator(c);
                ++i;
            }
        }
        row->open_comment = comment;
    }
}
static bool room(kilo_editor_t* e, size_t bytes, bool line)
{
    if (bytes > KILO_BYTES_MAX - e->bytes || (line && e->numrows == KILO_ROWS_MAX)) {
        kilo_status(e, "Document limit reached (256 KiB / 8192 lines)");
        return false;
    }
    return true;
}
bool kilo_insert(kilo_editor_t* e, unsigned char byte)
{
    if (!room(e, 1U, false) || byte == 0U) {
        return false;
    }
    kilo_row_t* row = &e->row[e->cy];
    if (row->size == KILO_LINE_MAX) {
        kilo_status(e, "Line limit reached (16 KiB)");
        return false;
    }
    kilo_row_t replacement;
    char* chars       = e->allocate(row->size + 2U);
    unsigned char* hl = e->allocate(row->size + 2U);
    if (chars == NULL || hl == NULL) {
        free(chars);
        free(hl);
        kilo_status(e, "Out of memory; document unchanged");
        return false;
    }
    memcpy(chars, row->chars, e->cx);
    chars[e->cx] = (char) byte;
    memcpy(chars + e->cx + 1U, row->chars + e->cx, row->size - e->cx + 1U);
    replacement = (kilo_row_t) {.chars = chars, .hl = hl, .size = row->size + 1U, .ending = row->ending};
    free_row(row);
    *row = replacement;
    ++e->cx;
    ++e->bytes;
    e->dirty = true;
    kilo_highlight(e, e->cy);
    return true;
}
bool kilo_split(kilo_editor_t* e)
{
    if (!room(e, e->newline, true)) {
        return false;
    }
    kilo_row_t* row = &e->row[e->cy];
    kilo_row_t left, right;
    if (!make_row(e, &left, row->chars, e->cx, e->newline)) {
        return false;
    }
    if (!make_row(e, &right, row->chars + e->cx, row->size - e->cx, row->ending)) {
        free_row(&left);
        return false;
    }
    free_row(row);
    memmove(row + 2U, row + 1U, (e->numrows - e->cy - 1U) * sizeof(*row));
    row[0] = left;
    row[1] = right;
    ++e->numrows;
    e->bytes += e->newline;
    e->dirty  = true;
    kilo_highlight(e, e->cy);
    ++e->cy;
    e->cx = 0U;
    return true;
}
bool kilo_delete(kilo_editor_t* e, bool backward)
{
    size_t y = e->cy, x = e->cx;
    if (backward) {
        if (x > 0U) {
            --x;
        } else if (y > 0U) {
            --y;
            x = e->row[y].size;
        } else {
            return false;
        }
    }
    kilo_row_t* row = &e->row[y];
    if (x < row->size) {
        memmove(row->chars + x, row->chars + x + 1U, row->size - x);
        --row->size;
        --e->bytes;
    } else {
        if (y + 1U == e->numrows) {
            return false;
        }
        kilo_row_t* next = row + 1U;
        if (next->size > KILO_LINE_MAX - row->size) {
            kilo_status(e, "Joined line exceeds 16 KiB");
            return false;
        }
        kilo_row_t joined;
        const size_t size = row->size + next->size;
        char* chars       = e->allocate(size + 1U);
        unsigned char* hl = e->allocate(size + 1U);
        if (chars == NULL || hl == NULL) {
            free(chars);
            free(hl);
            kilo_status(e, "Out of memory; document unchanged");
            return false;
        }
        memcpy(chars, row->chars, row->size);
        memcpy(chars + row->size, next->chars, next->size + 1U);
        joined    = (kilo_row_t) {.chars = chars, .hl = hl, .size = size, .ending = next->ending};
        e->bytes -= row->ending;
        free_row(row);
        free_row(next);
        *row = joined;
        memmove(row + 1U, row + 2U, (e->numrows - y - 2U) * sizeof(*row));
        --e->numrows;
    }
    e->cx    = x;
    e->cy    = y;
    e->dirty = true;
    kilo_highlight(e, y);
    return true;
}
size_t kilo_render_column(const kilo_row_t* row, size_t byte)
{
    size_t column = 0U;
    for (size_t i = 0U; i < byte && i < row->size; ++i) {
        column += row->chars[i] == '\t' ? KILO_TAB_WIDTH - column % KILO_TAB_WIDTH : 1U;
    }
    return column;
}
void kilo_scroll(kilo_editor_t* e)
{
    if (e->cy < e->rowoff) {
        e->rowoff = e->cy;
    }
    if (e->cy >= e->rowoff + e->screenrows) {
        e->rowoff = e->cy - e->screenrows + 1U;
    }
    const size_t column = kilo_render_column(&e->row[e->cy], e->cx);
    if (column < e->coloff) {
        e->coloff = column;
    }
    if (column >= e->coloff + e->screencols) {
        e->coloff = column - e->screencols + 1U;
    }
}
void kilo_move(kilo_editor_t* e, int dx, int dy)
{
    if (dy < 0) {
        const size_t n  = (size_t) -dy;
        e->cy          -= n < e->cy ? n : e->cy;
    }
    if (dy > 0) {
        const size_t n          = (size_t) dy;
        const size_t available  = e->numrows - e->cy - 1U;
        e->cy                  += n < available ? n : available;
    }
    if (dx < 0) {
        if (e->cx > 0U) {
            --e->cx;
        } else if (e->cy > 0U) {
            --e->cy;
            e->cx = e->row[e->cy].size;
        }
    }
    if (dx > 0) {
        if (e->cx < e->row[e->cy].size) {
            ++e->cx;
        } else if (e->cy + 1U < e->numrows) {
            ++e->cy;
            e->cx = 0U;
        }
    }
    if (e->cx > e->row[e->cy].size) {
        e->cx = e->row[e->cy].size;
    }
}
bool kilo_search(kilo_editor_t* e, int direction, bool next)
{
    if (e->query_size == 0U) {
        return false;
    }
    size_t y = e->cy;
    for (size_t visited = 0U; visited <= e->numrows; ++visited) {
        const kilo_row_t* row = &e->row[y];
        if (row->size >= e->query_size) {
            if (direction > 0) {
                size_t start = 0U;
                if (visited == 0U) {
                    start = e->cx;
                    if (next) {
                        ++start;
                    }
                }
                for (size_t x = start; x <= row->size - e->query_size; ++x) {
                    if (memcmp(row->chars + x, e->query, e->query_size) == 0) {
                        e->cy = y;
                        e->cx = x;
                        return true;
                    }
                }
            } else {
                size_t end = row->size - e->query_size + 1U;
                if (visited == 0U && e->cx + (next ? 0U : 1U) < end) {
                    end = e->cx + (next ? 0U : 1U);
                }
                while (end > 0U) {
                    --end;
                    if (memcmp(row->chars + end, e->query, e->query_size) == 0) {
                        e->cy = y;
                        e->cx = end;
                        return true;
                    }
                }
            }
        }
        y = direction > 0 ? (y + 1U) % e->numrows : (y + e->numrows - 1U) % e->numrows;
    }
    return false;
}
