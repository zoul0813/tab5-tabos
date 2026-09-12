#ifndef TABOS_INTERNAL_POWER_H
#define TABOS_INTERNAL_POWER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tabos/platform/platform.h>

enum {
    POWER_PARTICIPANT_CAPACITY = 16,
    POWER_NAME_CAPACITY        = 32,
    POWER_DEPENDENCY_CAPACITY  = 8,
    POWER_TRACE_CAPACITY       = 64,
    POWER_INHIBITOR_KEYBOARD   = 1U << 0U,
    POWER_INHIBITOR_POINTER    = 1U << 1U,
    POWER_INHIBITOR_FULLSCREEN = 1U << 2U,
    POWER_INHIBITOR_MEDIA      = 1U << 3U,
    POWER_INHIBITOR_PANIC      = 1U << 4U
};

typedef enum {
    POWER_STATE_ACTIVE,
    POWER_STATE_IDLE,
    POWER_STATE_SUSPENDING,
    POWER_STATE_SUSPENDED,
    POWER_STATE_RESUMING,
    POWER_STATE_SHUTTING_DOWN
} power_state_t;
typedef enum {
    POWER_REASON_STARTUP,
    POWER_REASON_INACTIVITY,
    POWER_REASON_REQUEST,
    POWER_REASON_ACTIVITY,
    POWER_REASON_WAKE,
    POWER_REASON_ROLLBACK,
    POWER_REASON_SYSTEM_ACTION
} power_reason_t;
typedef enum {
    POWER_FAILURE_NONE,
    POWER_FAILURE_ARGUMENT,
    POWER_FAILURE_CAPACITY,
    POWER_FAILURE_DUPLICATE,
    POWER_FAILURE_MISSING_DEPENDENCY,
    POWER_FAILURE_DEPENDENCY_CYCLE,
    POWER_FAILURE_INVALID_STATE,
    POWER_FAILURE_REGISTRATION,
    POWER_FAILURE_BLOCKED,
    POWER_FAILURE_CALLBACK,
    POWER_FAILURE_PLATFORM_PREPARE,
    POWER_FAILURE_PLATFORM_SLEEP,
    POWER_FAILURE_PLATFORM_RESTORE,
    POWER_FAILURE_BRIGHTNESS
} power_failure_code_t;
typedef enum {
    POWER_CALLBACK_SUCCESS,
    POWER_CALLBACK_PENDING,
    POWER_CALLBACK_FAILURE
} power_callback_result_t;
typedef enum {
    POWER_OPERATION_SUSPEND,
    POWER_OPERATION_RESUME
} power_operation_t;

typedef struct {
        uint64_t generation;
        uint16_t participant;
        power_operation_t operation;
} power_completion_token_t;
typedef bool (*power_blocker_fn)(void* context, char* reason, size_t reason_capacity);
typedef power_callback_result_t (*power_callback_fn)(void* context, power_completion_token_t token);
typedef struct {
        const char* name;
        const char* const* dependencies;
        size_t dependency_count;
        power_blocker_fn blocked;
        power_callback_fn suspend;
        power_callback_fn resume;
        void* context;
} power_participant_registration_t;
typedef struct {
        uint64_t idle_ms;
        uint64_t screen_off_ms; /* Total inactivity; zero disables screen-off. */
        uint64_t suspend_ms;
        uint8_t active_brightness;
        uint8_t idle_brightness;
        bool automatic_suspend;
} power_policy_t;
typedef struct {
        power_failure_code_t code;
        uint64_t generation;
        char participant[POWER_NAME_CAPACITY];
} power_failure_t;
typedef struct {
        power_state_t state;
        power_reason_t reason;
        power_policy_t policy;
        uint64_t generation;
        uint64_t last_activity_ms;
        uint64_t state_changed_ms;
        uint64_t suspend_started_ms;
        uint64_t suspend_duration_ms;
        uint64_t resume_started_ms;
        uint64_t resume_duration_ms;
        platform_power_wake_cause_t wake_causes;
        power_failure_t failure;
        size_t blocker_count;
        char blockers[POWER_PARTICIPANT_CAPACITY][POWER_NAME_CAPACITY];
        uint32_t dim_inhibitors;
        uint8_t desired_brightness;
        uint8_t effective_brightness;
        bool brightness_valid;
        bool screen_off_requested;
        bool suspend_available;
} power_status_t;
typedef struct {
        char name[POWER_NAME_CAPACITY];
        char dependencies[POWER_DEPENDENCY_CAPACITY][POWER_NAME_CAPACITY];
        size_t dependency_count;
        power_blocker_fn blocked;
        power_callback_fn suspend;
        power_callback_fn resume;
        void* context;
} power_participant_t;
typedef enum {
    POWER_PHASE_NONE,
    POWER_PHASE_SUSPEND,
    POWER_PHASE_WAIT_SUSPEND,
    POWER_PHASE_SLEEP,
    POWER_PHASE_RESUME,
    POWER_PHASE_WAIT_RESUME
} power_phase_t;
typedef struct {
        power_status_t status;
        power_participant_t participants[POWER_PARTICIPANT_CAPACITY];
        size_t participant_count;
        size_t dependency_order[POWER_PARTICIPANT_CAPACITY];
        size_t cursor;
        size_t suspended_count;
        size_t suspended[POWER_PARTICIPANT_CAPACITY];
        power_phase_t phase;
        power_completion_token_t expected;
        platform_mutex_t* mutex;
        bool finalized;
        bool suspend_requested;
        bool activity_requested;
        uint64_t activity_ms;
        bool completion_ready;
        power_callback_result_t completion_result;
        bool platform_prepared;
        char trace[POWER_TRACE_CAPACITY][POWER_NAME_CAPACITY + 8];
        size_t trace_count;
} power_manager_t;

bool power_manager_init(power_manager_t* manager, power_policy_t policy, uint64_t now_ms);
bool power_manager_register(power_manager_t* manager, const power_participant_registration_t* registration);
bool power_manager_finalize(power_manager_t* manager);
void power_manager_shutdown(power_manager_t* manager);
bool power_manager_request_suspend(power_manager_t* manager);
void power_manager_request_activity(power_manager_t* manager, uint64_t now_ms);
bool power_manager_set_policy(power_manager_t* manager, power_policy_t policy, uint64_t now_ms);
void power_manager_set_dim_inhibitors(power_manager_t* manager, uint32_t inhibitors, uint64_t now_ms);
void power_manager_complete(power_manager_t* manager, power_completion_token_t token, power_callback_result_t result);
void power_manager_update(power_manager_t* manager, platform_runtime_events_t events, uint64_t now_ms);
uint64_t power_manager_next_deadline(const power_manager_t* manager);
void power_manager_begin_shutdown(power_manager_t* manager, uint64_t now_ms);
const power_status_t* power_manager_status(const power_manager_t* manager);
const char* power_manager_ordered_name(const power_manager_t* manager, size_t index);
size_t power_manager_trace_count(const power_manager_t* manager);
const char* power_manager_trace_entry(const power_manager_t* manager, size_t index);

#endif
