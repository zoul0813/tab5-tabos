#ifndef TABOS_INTERNAL_FILESYSTEM_H
#define TABOS_INTERNAL_FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FILESYSTEM_POWER_ACTIVE,
    FILESYSTEM_POWER_DRAINING,
    FILESYSTEM_POWER_SYNCING,
    FILESYSTEM_POWER_ABORTING,
    FILESYSTEM_POWER_READY,
    FILESYSTEM_POWER_FAILED
} filesystem_power_state_t;

typedef struct {
        filesystem_power_state_t state;
        unsigned int operations;
        unsigned int mutations;
        uint64_t deadline_ms;
        int error;
} filesystem_power_status_t;

/* Runtime-owned asynchronous storage barrier, independent of CPU sleep. */
bool filesystem_power_begin(uint64_t now_ms);
void filesystem_power_update(uint64_t now_ms);
void filesystem_power_abort(void);
/* Teardown only: join borrowed sync work and reopen storage before app cleanup. */
void filesystem_power_finish_for_shutdown(void);
filesystem_power_status_t filesystem_power_status(void);
uint64_t filesystem_power_next_deadline(void);
bool filesystem_power_is_frozen(void);

bool filesystem_init(void);
void filesystem_shutdown(void);
bool filesystem_is_mounted(void);
bool filesystem_normalize_path(const char* path, const char* working_directory, char* output, size_t output_size);

#endif
