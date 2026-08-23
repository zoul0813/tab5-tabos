#ifndef TABOS_PLATFORM_STORAGE_H
#define TABOS_PLATFORM_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/filesystem.h>

typedef uintptr_t platform_file_t;
typedef uintptr_t platform_dir_t;

typedef struct {
        uint64_t total_bytes;
        uint64_t free_bytes;
        bool mounted;
        bool removable;
        const char* name;
        char letter;
} platform_storage_info_t;

bool platform_storage_init(void);
void platform_storage_shutdown(void);
size_t platform_storage_drive_count(void);
bool platform_storage_info(size_t index, platform_storage_info_t* info);
bool platform_storage_has_drive(char letter);
char platform_storage_default_drive(void);

int platform_storage_open(char drive, const char* path, int flags, uint32_t mode, platform_file_t* file);
int platform_storage_close(platform_file_t file);
int platform_storage_read(platform_file_t file, void* buffer, size_t count, size_t* bytes_read);
int platform_storage_write(platform_file_t file, const void* buffer, size_t count, size_t* bytes_written);
int platform_storage_seek(platform_file_t file, tabos_off_t offset, int whence, tabos_off_t* position);
int platform_storage_stat(char drive, const char* path, tabos_stat_t* status);
int platform_storage_fstat(platform_file_t file, tabos_stat_t* status);
int platform_storage_unlink(char drive, const char* path);
int platform_storage_rename(char drive, const char* old_path, const char* new_path);
int platform_storage_mkdir(char drive, const char* path, uint32_t mode);
int platform_storage_rmdir(char drive, const char* path);
int platform_storage_opendir(char drive, const char* path, platform_dir_t* directory);
int platform_storage_readdir(platform_dir_t directory, tabos_dirent_t* entry, bool* end);
int platform_storage_closedir(platform_dir_t directory);

#endif
