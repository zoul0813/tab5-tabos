#ifndef KILO_STORAGE_H
#define KILO_STORAGE_H
#include <kilo/editor.h>
#include <sys/types.h>
#include <sys/stat.h>
typedef struct {
        int (*open)(const char*, int, int);
        ssize_t (*read)(int, void*, size_t);
        ssize_t (*write)(int, const void*, size_t);
        int (*close)(int);
        int (*stat)(const char*, struct stat*);
        int (*rename)(const char*, const char*);
        int (*unlink)(const char*);
} kilo_storage_t;
extern const kilo_storage_t kilo_storage_default;
bool kilo_load(kilo_editor_t* editor, const kilo_storage_t* storage);
bool kilo_save(kilo_editor_t* editor, const kilo_storage_t* storage);
#endif
