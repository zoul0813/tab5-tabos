#ifndef KILO_EDITOR_H
#define KILO_EDITOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    KILO_BYTES_MAX = 262144,
    KILO_ROWS_MAX  = 8192,
    KILO_LINE_MAX  = 16384,
    KILO_TAB_WIDTH = 4,
    KILO_QUERY_MAX = 256
};
enum {
    HL_NORMAL,
    HL_NONPRINT,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};
typedef struct {
        char* chars;
        unsigned char* hl;
        size_t size;
        uint8_t ending;
        bool open_comment;
} kilo_row_t;
typedef struct {
        kilo_row_t* row;
        size_t numrows, bytes, cx, cy, rowoff, coloff, screenrows, screencols;
        uint8_t newline;
        bool dirty, syntax, searching, confirming, quit;
        bool suppress_text;
        size_t saved_x, saved_y, saved_rowoff, saved_coloff;
        char query[KILO_QUERY_MAX];
        size_t query_size;
        char message[1200];
        char recovery[1200];
        const char* filename;
        void* (*allocate)(size_t);
} kilo_editor_t;
bool kilo_init(kilo_editor_t* editor, const char* filename);
void kilo_dispose(kilo_editor_t* editor);
bool kilo_append_row(kilo_editor_t* editor, const char* text, size_t size, uint8_t ending);
bool kilo_insert(kilo_editor_t* editor, unsigned char byte);
bool kilo_split(kilo_editor_t* editor);
bool kilo_delete(kilo_editor_t* editor, bool backward);
void kilo_highlight(kilo_editor_t* editor, size_t from);
size_t kilo_render_column(const kilo_row_t* row, size_t byte);
void kilo_scroll(kilo_editor_t* editor);
void kilo_move(kilo_editor_t* editor, int dx, int dy);
bool kilo_search(kilo_editor_t* editor, int direction, bool next);
void kilo_status(kilo_editor_t* editor, const char* text);
#endif
