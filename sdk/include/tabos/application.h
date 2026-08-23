#ifndef TABOS_APPLICATION_H
#define TABOS_APPLICATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tabos/console.h>

#define TABOS_APPLICATION_ABI_VERSION 1U

typedef uint32_t tabos_app_capabilities_t;

enum {
    TABOS_APP_CAPABILITY_NONE    = 0U,
    TABOS_APP_CAPABILITY_CONSOLE = 1U << 0,
};

typedef struct tabos_app_context tabos_app_context_t;
typedef uint32_t tabos_process_id_t;

#define TABOS_PROCESS_ID_INVALID UINT32_MAX

typedef enum {
    TABOS_PROCESS_RUNNING = 0,
    TABOS_PROCESS_BLOCKED,
    TABOS_PROCESS_PANICKED,
} tabos_process_state_t;

typedef enum {
    TABOS_PROCESS_TERMINATION_NONE = 0,
    TABOS_PROCESS_TERMINATION_EXIT_REQUEST,
    TABOS_PROCESS_TERMINATION_RETURN,
    TABOS_PROCESS_TERMINATION_FAULT,
    TABOS_PROCESS_TERMINATION_FORCED,
} tabos_process_termination_t;

typedef struct {
        tabos_process_id_t id;
        tabos_process_id_t parent_id;
        tabos_process_state_t state;
        const char* name;
} tabos_process_info_t;

typedef bool (*tabos_app_entry_fn)(tabos_app_context_t* context);
typedef void (*tabos_app_update_fn)(tabos_app_context_t* context);
typedef void (*tabos_app_cleanup_fn)(tabos_app_context_t* context, int exit_status);

typedef struct {
        uint32_t abi_version;
        const char* name;
        const char* version;
        tabos_app_capabilities_t capabilities;
        tabos_app_entry_fn entry;
        tabos_app_update_fn update;
        tabos_app_cleanup_fn cleanup;
} tabos_app_descriptor_t;

typedef enum {
    TABOS_APP_RESULT_OK = 0,
    TABOS_APP_RESULT_NOT_FOUND,
    TABOS_APP_RESULT_BUSY,
    TABOS_APP_RESULT_INVALID,
    TABOS_APP_RESULT_START_FAILED,
} tabos_app_result_t;

/* Inspect applications registered with the running system. */
size_t tabos_app_count(void);
const tabos_app_descriptor_t* tabos_app_at(size_t index);
const tabos_app_descriptor_t* tabos_app_find(const char* name);

/* Launch one registered foreground application. */
tabos_app_result_t tabos_app_launch(const char* name);
tabos_app_result_t tabos_app_launch_path(const char* path);
tabos_app_result_t tabos_app_launch_path_args(const char* path, size_t argc, const char* const* argv);
/* Start file-backed child. Caller must return control after a successful request. */
tabos_app_result_t tabos_app_exec(tabos_app_context_t* context, const char* path);
tabos_app_result_t tabos_app_exec_args(tabos_app_context_t* context, const char* path, size_t argc,
                                       const char* const* argv);
bool tabos_app_take_child_status(tabos_app_context_t* context, int* status);
bool tabos_app_is_running(void);
const tabos_app_descriptor_t* tabos_app_active(void);
/* Request clean exit. Cleanup occurs when control returns to lifecycle manager. */
void tabos_app_request_exit(tabos_app_context_t* context, int exit_status);

/* Access services granted from descriptor capabilities. */
const tabos_console_session_t* tabos_app_console(const tabos_app_context_t* context);
tabos_process_id_t tabos_app_process_id(const tabos_app_context_t* context);

/* Inspect currently loaded processes. */
size_t tabos_process_count(void);
bool tabos_process_info(tabos_process_id_t id, tabos_process_info_t* info);
bool tabos_process_system_panicked(void);
bool tabos_process_panic_info(tabos_process_termination_t* cause, int* exit_status);

/* Read most recently completed application's status. */
bool tabos_app_last_exit_status(int* exit_status);

#endif
