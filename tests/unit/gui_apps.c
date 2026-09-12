#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <calculator/calculate.h>
#include <editor/document.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool fail_write, fail_rename, fail_read, fail_close;
static int rename_calls, fail_rename_call;
static ssize_t test_write(int fd, const void* bytes, size_t count)
{
    if (fail_write) {
        return -1;
    }
    return write(fd, bytes, count > 3U ? 3U : count);
}
static ssize_t test_read(int fd, void* bytes, size_t count)
{
    return fail_read ? -1 : read(fd, bytes, count);
}
static int test_rename(const char* old_path, const char* new_path)
{
    ++rename_calls;
    return fail_rename || rename_calls == fail_rename_call ? -1 : rename(old_path, new_path);
}
static int test_close(int fd)
{
    const int result = close(fd);
    return fail_close ? -1 : result;
}
#define write  test_write
#define read   test_read
#define rename test_rename
#define close  test_close
#include "../../apps/editor/src/document.c"
#undef write
#undef read
#undef rename
#undef close

static void contents(const char* path, const char* expected)
{
    char bytes[128] = {0};
    FILE* file      = fopen(path, "rb");
    assert(file != NULL);
    const size_t count = fread(bytes, 1U, sizeof(bytes) - 1U, file);
    assert(count == strlen(expected) && strcmp(bytes, expected) == 0);
    assert(fclose(file) == 0);
}

int main(void)
{
    char result[128];
    assert(calculator_evaluate("2+3*4-6/2", result, sizeof(result)) && strcmp(result, "11") == 0);
    assert(calculator_evaluate("-2*-3+0.5", result, sizeof(result)) && strcmp(result, "6.5") == 0);
    assert(!calculator_evaluate("1/0", result, sizeof(result)));
    assert(!calculator_evaluate("nan", result, sizeof(result)));
    assert(!calculator_evaluate("1e999", result, sizeof(result)));
    assert(!calculator_evaluate("2+", result, sizeof(result)));
    assert(!calculator_evaluate("(2)", result, sizeof(result)));
    char root[] = "/tmp/tabos-editor-XXXXXX";
    assert(mkdtemp(root) != NULL);
    char path[192];
    assert(snprintf(path, sizeof(path), "%s/note", root) > 0);
    editor_document_t document = {.text = "original", .dirty = true};
    assert(editor_document_save(&document, path) && !document.dirty);
    strcpy(document.text, "changed text");
    document.dirty = true;
    fail_write     = true;
    assert(!editor_document_save(&document, path) && document.dirty);
    contents(path, "original");
    fail_write  = false;
    fail_rename = true;
    assert(!editor_document_save(&document, path) && document.dirty);
    contents(path, "original");
    fail_rename      = false;
    rename_calls     = 0;
    fail_rename_call = 2;
    assert(!editor_document_save(&document, path) && document.dirty);
    contents(path, "original");
    assert(rename_calls == 3);
    fail_rename_call = 0;
    fail_close       = true;
    assert(!editor_document_save(&document, path) && document.dirty);
    contents(path, "original");
    fail_close = false;
    fail_read  = true;
    assert(!editor_document_open(&document, path) && document.dirty && strcmp(document.text, "changed text") == 0);
    fail_read = false;
    assert(editor_document_save(&document, path) && !document.dirty);
    contents(path, "changed text");
    strcpy(document.text, "unsaved");
    document.dirty = true;
    assert(editor_document_open(&document, path) && !document.dirty && strcmp(document.text, "changed text") == 0);
    FILE* file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite("binary\0data", 1U, 11U, file) == 11U && fclose(file) == 0);
    document.dirty = true;
    assert(!editor_document_open(&document, path) && document.dirty && strcmp(document.text, "changed text") == 0);
    file = fopen(path, "wb");
    assert(file != NULL);
    for (size_t index = 0U; index < EDITOR_TEXT_CAPACITY; ++index) {
        assert(fputc('a', file) != EOF);
    }
    assert(fclose(file) == 0);
    assert(!editor_document_open(&document, path) && document.dirty && strcmp(document.text, "changed text") == 0);
    assert(unlink(path) == 0 && rmdir(root) == 0);
    return 0;
}
