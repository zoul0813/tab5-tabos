#ifndef EDITOR_DOCUMENT_H
#define EDITOR_DOCUMENT_H
#include <stdbool.h>
enum {
    EDITOR_TEXT_CAPACITY = 32768,
    EDITOR_PATH_CAPACITY = 192
};
typedef struct {
        char text[EDITOR_TEXT_CAPACITY];
        char path[EDITOR_PATH_CAPACITY];
        bool dirty;
        char message[160];
} editor_document_t;
/* On failure, document text/path/dirty remain unchanged. Saves replace through
 * a same-directory temporary file, never truncate the original in place. */
bool editor_document_open(editor_document_t* document, const char* path);
bool editor_document_save(editor_document_t* document, const char* path);
#endif
