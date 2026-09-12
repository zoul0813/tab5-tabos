#include <tabos/internal/power.h>
#include <stdio.h>
#include <string.h>

static uint64_t add(uint64_t a, uint64_t b)
{
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}
static bool name_ok(const char* name)
{
    return name != NULL && name[0] != '\0' && strlen(name) < POWER_NAME_CAPACITY;
}
static size_t find(const power_manager_t* manager, const char* name)
{
    for (size_t i = 0U; i < manager->participant_count; ++i) {
        if (strcmp(manager->participants[i].name, name) == 0) {
            return i;
        }
    }
    return POWER_PARTICIPANT_CAPACITY;
}
static void failure(power_manager_t* manager, power_failure_code_t code, const char* name)
{
    manager->status.failure.code       = code;
    manager->status.failure.generation = manager->status.generation;
    (void) snprintf(manager->status.failure.participant, POWER_NAME_CAPACITY, "%s", name != NULL ? name : "");
}

static bool policy_ok(power_policy_t policy)
{
    return policy.active_brightness <= 100U && policy.idle_brightness <= 100U &&
           (policy.screen_off_ms == 0U || policy.screen_off_ms >= policy.idle_ms) &&
           (policy.panel_off_ms == 0U || (policy.screen_off_ms != 0U && policy.panel_off_ms >= policy.screen_off_ms));
}

static uint8_t idle_brightness(power_policy_t policy)
{
    return policy.idle_brightness < policy.active_brightness ? policy.idle_brightness : policy.active_brightness;
}

static const char* inhibitor_name(uint32_t inhibitors)
{
    if ((inhibitors & POWER_INHIBITOR_KEYBOARD) != 0U) {
        return "keyboard";
    }
    if ((inhibitors & POWER_INHIBITOR_POINTER) != 0U) {
        return "pointer";
    }
    if ((inhibitors & POWER_INHIBITOR_FULLSCREEN) != 0U) {
        return "fullscreen";
    }
    if ((inhibitors & POWER_INHIBITOR_PANIC) != 0U) {
        return "panic";
    }
    return "media";
}

static bool set_brightness(power_manager_t* manager, uint8_t brightness)
{
    manager->status.desired_brightness = brightness;
    if (manager->status.brightness_valid && manager->status.effective_brightness == brightness) {
        return true;
    }
    if (!platform_power_set_brightness(brightness)) {
        manager->status.brightness_valid = false;
        failure(manager, POWER_FAILURE_BRIGHTNESS, NULL);
        return false;
    }
    manager->status.effective_brightness = brightness;
    manager->status.brightness_valid     = true;
    return true;
}

static bool set_panel(power_manager_t* manager, bool enabled)
{
    if (manager->status.panel_valid && manager->status.panel_enabled == enabled) {
        return true;
    }
    if (!platform_power_set_panel_enabled(enabled)) {
        manager->status.panel_valid = false;
        failure(manager, POWER_FAILURE_PANEL, NULL);
        return false;
    }
    manager->status.panel_enabled = enabled;
    manager->status.panel_valid   = true;
    return true;
}

static void set_display(power_manager_t* manager, uint8_t brightness, bool panel_enabled)
{
    manager->status.desired_brightness = brightness;
    if (!panel_enabled) {
        /* Never disable the panel while its backlight is still on. */
        if (set_brightness(manager, 0U)) {
            (void) set_panel(manager, false);
        }
    } else if (set_panel(manager, true)) {
        /* Restore panel output before illuminating the retained frame. */
        (void) set_brightness(manager, brightness);
    }
}

static void restore_display(power_manager_t* manager)
{
    manager->status.screen_off_requested = false;
    manager->status.panel_off_requested  = false;
    set_display(manager, manager->status.policy.active_brightness, true);
}

static void apply_idle_display(power_manager_t* manager, uint64_t now_ms)
{
    const power_policy_t policy = manager->status.policy;
    manager->status.screen_off_requested =
        policy.screen_off_ms != 0U && now_ms >= add(manager->status.last_activity_ms, policy.screen_off_ms);
    manager->status.panel_off_requested =
        policy.panel_off_ms != 0U && now_ms >= add(manager->status.last_activity_ms, policy.panel_off_ms);
    set_display(manager, manager->status.screen_off_requested ? 0U : idle_brightness(policy),
                !manager->status.panel_off_requested);
}

bool power_manager_init(power_manager_t* manager, power_policy_t policy, uint64_t now_ms)
{
    if (manager == NULL || !policy_ok(policy)) {
        return false;
    }
    *manager       = (power_manager_t) {0};
    manager->mutex = platform_mutex_create();
    if (manager->mutex == NULL) {
        return false;
    }
    manager->status = (power_status_t) {.state             = POWER_STATE_ACTIVE,
                                        .reason            = POWER_REASON_STARTUP,
                                        .policy            = policy,
                                        .last_activity_ms  = now_ms,
                                        .state_changed_ms  = now_ms,
                                        .suspend_available = true};
    restore_display(manager);
    return true;
}

bool power_manager_register(power_manager_t* manager, const power_participant_registration_t* registration)
{
    if (manager == NULL || registration == NULL || manager->finalized || !name_ok(registration->name) ||
        registration->dependency_count > POWER_DEPENDENCY_CAPACITY ||
        (registration->dependency_count > 0U && registration->dependencies == NULL)) {
        if (manager != NULL) {
            manager->status.suspend_available = false;
            failure(manager, POWER_FAILURE_ARGUMENT, NULL);
        }
        return false;
    }
    if (manager->participant_count == POWER_PARTICIPANT_CAPACITY) {
        manager->status.suspend_available = false;
        failure(manager, POWER_FAILURE_CAPACITY, registration->name);
        return false;
    }
    if (find(manager, registration->name) != POWER_PARTICIPANT_CAPACITY) {
        manager->status.suspend_available = false;
        failure(manager, POWER_FAILURE_DUPLICATE, registration->name);
        return false;
    }
    power_participant_t* participant = &manager->participants[manager->participant_count];
    (void) snprintf(participant->name, POWER_NAME_CAPACITY, "%s", registration->name);
    participant->dependency_count = registration->dependency_count;
    for (size_t i = 0U; i < registration->dependency_count; ++i) {
        if (!name_ok(registration->dependencies[i])) {
            manager->status.suspend_available = false;
            failure(manager, POWER_FAILURE_ARGUMENT, registration->name);
            return false;
        }
        (void) snprintf(participant->dependencies[i], POWER_NAME_CAPACITY, "%s", registration->dependencies[i]);
    }
    participant->blocked = registration->blocked;
    participant->suspend = registration->suspend;
    participant->resume  = registration->resume;
    participant->context = registration->context;
    manager->participant_count++;
    return true;
}

static bool visit(power_manager_t* manager, size_t index, uint8_t* marks, size_t* count)
{
    if (marks[index] == 2U) {
        return true;
    }
    if (marks[index] == 1U) {
        failure(manager, POWER_FAILURE_DEPENDENCY_CYCLE, manager->participants[index].name);
        return false;
    }
    marks[index]                           = 1U;
    const power_participant_t* participant = &manager->participants[index];
    for (size_t i = 0U; i < participant->dependency_count; ++i) {
        const size_t dependency = find(manager, participant->dependencies[i]);
        if (dependency == POWER_PARTICIPANT_CAPACITY) {
            failure(manager, POWER_FAILURE_MISSING_DEPENDENCY, participant->dependencies[i]);
            return false;
        }
        if (!visit(manager, dependency, marks, count)) {
            return false;
        }
    }
    marks[index]                          = 2U;
    manager->dependency_order[(*count)++] = index;
    return true;
}

bool power_manager_finalize(power_manager_t* manager)
{
    if (manager == NULL || manager->finalized) {
        return false;
    }
    uint8_t marks[POWER_PARTICIPANT_CAPACITY] = {0};
    size_t count                              = 0U;
    for (size_t i = 0U; i < manager->participant_count; ++i) {
        if (!visit(manager, i, marks, &count)) {
            manager->status.suspend_available = false;
        }
    }
    manager->finalized = true;
    return manager->status.suspend_available;
}

void power_manager_shutdown(power_manager_t* manager)
{
    if (manager != NULL) {
        platform_mutex_t* mutex = manager->mutex;
        *manager                = (power_manager_t) {0};
        platform_mutex_destroy(mutex);
    }
}

bool power_manager_request_suspend(power_manager_t* manager)
{
    if (manager == NULL || manager->mutex == NULL) {
        return false;
    }
    platform_mutex_lock(manager->mutex);
    bool accepted              = !manager->suspend_requested;
    manager->suspend_requested = true;
    platform_mutex_unlock(manager->mutex);
    if (accepted) {
        platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
    }
    return accepted;
}
void power_manager_request_activity(power_manager_t* manager, uint64_t now_ms)
{
    if (manager == NULL || manager->mutex == NULL) {
        return;
    }
    platform_mutex_lock(manager->mutex);
    manager->activity_requested = true;
    manager->activity_ms        = now_ms;
    platform_mutex_unlock(manager->mutex);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
}

bool power_manager_set_policy(power_manager_t* manager, power_policy_t policy, uint64_t now_ms)
{
    if (manager == NULL || !manager->finalized || !policy_ok(policy) ||
        manager->status.state == POWER_STATE_SHUTTING_DOWN) {
        return false;
    }
    manager->status.policy = policy;
    if (manager->status.dim_inhibitors != 0U) {
        restore_display(manager);
        return true;
    }
    const uint64_t idle_deadline = add(manager->status.last_activity_ms, policy.idle_ms);
    if ((manager->status.state == POWER_STATE_ACTIVE || manager->status.state == POWER_STATE_IDLE) &&
        now_ms >= idle_deadline) {
        manager->status.state            = POWER_STATE_IDLE;
        manager->status.reason           = POWER_REASON_INACTIVITY;
        manager->status.state_changed_ms = now_ms;
        apply_idle_display(manager, now_ms);
    } else if (manager->status.state == POWER_STATE_ACTIVE || manager->status.state == POWER_STATE_IDLE) {
        manager->status.state            = POWER_STATE_ACTIVE;
        manager->status.state_changed_ms = now_ms;
        restore_display(manager);
    }
    return true;
}

void power_manager_set_dim_inhibitors(power_manager_t* manager, uint32_t inhibitors, uint64_t now_ms)
{
    if (manager == NULL || !manager->finalized || manager->status.state == POWER_STATE_SHUTTING_DOWN) {
        return;
    }
    const uint32_t previous        = manager->status.dim_inhibitors;
    manager->status.dim_inhibitors = inhibitors;
    if (inhibitors != 0U) {
        if (manager->status.state == POWER_STATE_IDLE) {
            manager->status.state            = POWER_STATE_ACTIVE;
            manager->status.reason           = POWER_REASON_ACTIVITY;
            manager->status.state_changed_ms = now_ms;
        }
        restore_display(manager);
    } else if (previous != 0U) {
        manager->status.last_activity_ms = now_ms;
        if (manager->status.state == POWER_STATE_IDLE) {
            manager->status.state            = POWER_STATE_ACTIVE;
            manager->status.reason           = POWER_REASON_ACTIVITY;
            manager->status.state_changed_ms = now_ms;
        }
    }
}
void power_manager_complete(power_manager_t* manager, power_completion_token_t token, power_callback_result_t result)
{
    if (manager == NULL || manager->mutex == NULL || result == POWER_CALLBACK_PENDING) {
        return;
    }
    platform_mutex_lock(manager->mutex);
    if (!manager->completion_ready && token.generation == manager->expected.generation &&
        token.participant == manager->expected.participant && token.operation == manager->expected.operation) {
        manager->completion_result = result;
        manager->completion_ready  = true;
    }
    platform_mutex_unlock(manager->mutex);
    platform_runtime_notify(PLATFORM_RUNTIME_EVENT_POWER);
}

static void trace(power_manager_t* manager, power_participant_t* participant, power_operation_t operation)
{
    if (manager->trace_count < POWER_TRACE_CAPACITY) {
        (void) snprintf(manager->trace[manager->trace_count++], POWER_NAME_CAPACITY + 8, "%s:%s", participant->name,
                        operation == POWER_OPERATION_SUSPEND ? "down" : "up");
    }
}
static void begin_resume(power_manager_t* manager, uint64_t now_ms, power_reason_t reason)
{
    manager->status.state             = POWER_STATE_RESUMING;
    manager->status.reason            = reason;
    manager->status.state_changed_ms  = now_ms;
    manager->status.resume_started_ms = now_ms;
    manager->phase                    = POWER_PHASE_RESUME;
    manager->cursor                   = 0U;
}
static void rollback(power_manager_t* manager, uint64_t now_ms, power_failure_code_t code, const char* name)
{
    failure(manager, code, name);
    if (manager->platform_prepared) {
        platform_power_abort_sleep();
    }
    begin_resume(manager, now_ms, POWER_REASON_ROLLBACK);
}
static bool completion(power_manager_t* manager, power_callback_result_t* result)
{
    bool ready;
    platform_mutex_lock(manager->mutex);
    ready = manager->completion_ready;
    if (ready) {
        *result                   = manager->completion_result;
        manager->completion_ready = false;
    }
    platform_mutex_unlock(manager->mutex);
    return ready;
}

static void expect_completion(power_manager_t* manager, power_completion_token_t token)
{
    platform_mutex_lock(manager->mutex);
    manager->expected         = token;
    manager->completion_ready = false;
    platform_mutex_unlock(manager->mutex);
}

static void finish(power_manager_t* manager, uint64_t now_ms)
{
    manager->status.state              = POWER_STATE_ACTIVE;
    manager->status.state_changed_ms   = now_ms;
    manager->status.resume_duration_ms = now_ms - manager->status.resume_started_ms;
    manager->phase                     = POWER_PHASE_NONE;
    manager->cursor                    = 0U;
    manager->suspended_count           = 0U;
    manager->platform_prepared         = false;
    restore_display(manager);
}

static void drive(power_manager_t* manager, uint64_t now_ms)
{
    for (size_t step = 0U; step < POWER_PARTICIPANT_CAPACITY * 2U + 8U; ++step) {
        if (manager->phase == POWER_PHASE_WAIT_SUSPEND || manager->phase == POWER_PHASE_WAIT_RESUME) {
            power_callback_result_t result;
            if (!completion(manager, &result)) {
                return;
            }
            if (result == POWER_CALLBACK_FAILURE) {
                power_participant_t* p = &manager->participants[manager->expected.participant];
                if (manager->phase == POWER_PHASE_WAIT_SUSPEND) {
                    rollback(manager, now_ms, POWER_FAILURE_CALLBACK, p->name);
                    continue;
                }
                failure(manager, POWER_FAILURE_CALLBACK, p->name);
            }
            if (manager->phase == POWER_PHASE_WAIT_SUSPEND) {
                manager->suspended[manager->suspended_count++] = manager->expected.participant;
                manager->phase                                 = POWER_PHASE_SUSPEND;
            } else {
                manager->phase = POWER_PHASE_RESUME;
            }
            manager->cursor++;
            continue;
        }
        if (manager->phase == POWER_PHASE_SUSPEND) {
            if (manager->cursor == manager->participant_count) {
                if (!platform_power_prepare_sleep()) {
                    rollback(manager, now_ms, POWER_FAILURE_PLATFORM_PREPARE, NULL);
                    continue;
                }
                manager->platform_prepared          = true;
                manager->status.state               = POWER_STATE_SUSPENDED;
                manager->status.state_changed_ms    = now_ms;
                manager->status.suspend_duration_ms = now_ms - manager->status.suspend_started_ms;
                manager->phase                      = POWER_PHASE_SLEEP;
                if (!platform_power_enter_light_sleep()) {
                    rollback(manager, now_ms, POWER_FAILURE_PLATFORM_SLEEP, NULL);
                    continue;
                }
                return;
            }
            size_t index           = manager->dependency_order[manager->participant_count - 1U - manager->cursor];
            power_participant_t* p = &manager->participants[index];
            const power_completion_token_t token = {manager->status.generation, (uint16_t) index,
                                                    POWER_OPERATION_SUSPEND};
            expect_completion(manager, token);
            trace(manager, p, POWER_OPERATION_SUSPEND);
            power_callback_result_t result = p->suspend(p->context, token);
            if (result == POWER_CALLBACK_PENDING) {
                manager->phase = POWER_PHASE_WAIT_SUSPEND;
                return;
            }
            if (result == POWER_CALLBACK_FAILURE) {
                rollback(manager, now_ms, POWER_FAILURE_CALLBACK, p->name);
                continue;
            }
            manager->suspended[manager->suspended_count++] = index;
            manager->cursor++;
            continue;
        }
        if (manager->phase == POWER_PHASE_RESUME) {
            if (manager->cursor == manager->suspended_count) {
                finish(manager, now_ms);
                return;
            }
            size_t index                         = manager->suspended[manager->suspended_count - 1U - manager->cursor];
            power_participant_t* p               = &manager->participants[index];
            const power_completion_token_t token = {manager->status.generation, (uint16_t) index,
                                                    POWER_OPERATION_RESUME};
            expect_completion(manager, token);
            trace(manager, p, POWER_OPERATION_RESUME);
            power_callback_result_t result = p->resume(p->context, token);
            if (result == POWER_CALLBACK_PENDING) {
                manager->phase = POWER_PHASE_WAIT_RESUME;
                return;
            }
            if (result == POWER_CALLBACK_FAILURE) {
                failure(manager, POWER_FAILURE_CALLBACK, p->name);
            }
            manager->cursor++;
            continue;
        }
        return;
    }
}

static void start(power_manager_t* manager, uint64_t now_ms)
{
    manager->status.blocker_count = 0U;
    if (!manager->status.suspend_available) {
        failure(manager, POWER_FAILURE_REGISTRATION, NULL);
        return;
    }
    if (manager->status.dim_inhibitors != 0U) {
        manager->status.blocker_count = 1U;
        (void) snprintf(manager->status.blockers[0], POWER_NAME_CAPACITY, "%s",
                        inhibitor_name(manager->status.dim_inhibitors));
        failure(manager, POWER_FAILURE_BLOCKED, manager->status.blockers[0]);
        return;
    }
    for (size_t order = manager->participant_count; order > 0U; --order) {
        power_participant_t* p           = &manager->participants[manager->dependency_order[order - 1U]];
        char reason[POWER_NAME_CAPACITY] = {0};
        if (p->suspend == NULL || p->resume == NULL ||
            (p->blocked != NULL && p->blocked(p->context, reason, sizeof(reason)))) {
            (void) snprintf(manager->status.blockers[manager->status.blocker_count++], POWER_NAME_CAPACITY, "%s",
                            reason[0] != '\0' ? reason : p->name);
        }
    }
    if (manager->status.blocker_count > 0U) {
        failure(manager, POWER_FAILURE_BLOCKED, manager->status.blockers[0]);
        return;
    }
    manager->status.generation++;
    manager->status.state              = POWER_STATE_SUSPENDING;
    manager->status.reason             = POWER_REASON_REQUEST;
    manager->status.state_changed_ms   = now_ms;
    manager->status.suspend_started_ms = now_ms;
    manager->status.failure            = (power_failure_t) {0};
    manager->phase                     = POWER_PHASE_SUSPEND;
    manager->cursor                    = 0U;
    manager->suspended_count           = 0U;
    drive(manager, now_ms);
}

void power_manager_update(power_manager_t* manager, platform_runtime_events_t events, uint64_t now_ms)
{
    if (manager == NULL || !manager->finalized || manager->status.state == POWER_STATE_SHUTTING_DOWN) {
        return;
    }
    platform_mutex_lock(manager->mutex);
    bool request                = manager->suspend_requested;
    manager->suspend_requested  = false;
    bool activity               = manager->activity_requested;
    uint64_t activity_ms        = manager->activity_ms;
    manager->activity_requested = false;
    platform_mutex_unlock(manager->mutex);
    if (activity) {
        manager->status.last_activity_ms = activity_ms;
        if (manager->status.state == POWER_STATE_IDLE) {
            manager->status.state            = POWER_STATE_ACTIVE;
            manager->status.reason           = POWER_REASON_ACTIVITY;
            manager->status.state_changed_ms = now_ms;
        }
        if (manager->status.state == POWER_STATE_ACTIVE) {
            restore_display(manager);
        }
    }
    if ((events & PLATFORM_RUNTIME_EVENT_DEADLINE) != 0U && now_ms >= power_manager_next_deadline(manager)) {
        if (manager->status.state == POWER_STATE_ACTIVE) {
            manager->status.state            = POWER_STATE_IDLE;
            manager->status.reason           = POWER_REASON_INACTIVITY;
            manager->status.state_changed_ms = now_ms;
            apply_idle_display(manager, now_ms);
        } else if (manager->status.state == POWER_STATE_IDLE) {
            apply_idle_display(manager, now_ms);
        }
        if (manager->status.state == POWER_STATE_IDLE && manager->status.policy.automatic_suspend &&
            now_ms >= add(manager->status.last_activity_ms, manager->status.policy.suspend_ms)) {
            request = true;
        }
    }
    if (manager->status.state == POWER_STATE_SUSPENDED && (events & PLATFORM_RUNTIME_EVENT_POWER) != 0U) {
        platform_power_wake_cause_t causes = platform_power_collect_wake_causes();
        if (causes != PLATFORM_POWER_WAKE_NONE) {
            manager->status.wake_causes = causes;
            if (!platform_power_restore()) {
                failure(manager, POWER_FAILURE_PLATFORM_RESTORE, NULL);
            }
            begin_resume(manager, now_ms, POWER_REASON_WAKE);
        }
    }
    if (request && (manager->status.state == POWER_STATE_ACTIVE || manager->status.state == POWER_STATE_IDLE)) {
        start(manager, now_ms);
    } else if (request) {
        failure(manager, POWER_FAILURE_INVALID_STATE, NULL);
    }
    drive(manager, now_ms);
}
uint64_t power_manager_next_deadline(const power_manager_t* manager)
{
    if (manager == NULL || !manager->finalized) {
        return PLATFORM_RUNTIME_DEADLINE_NONE;
    }
    if (manager->status.dim_inhibitors != 0U) {
        return PLATFORM_RUNTIME_DEADLINE_NONE;
    }
    if (manager->status.state == POWER_STATE_ACTIVE) {
        return add(manager->status.last_activity_ms, manager->status.policy.idle_ms);
    }
    if (manager->status.state == POWER_STATE_IDLE) {
        uint64_t deadline = PLATFORM_RUNTIME_DEADLINE_NONE;
        if (!manager->status.screen_off_requested && manager->status.policy.screen_off_ms != 0U) {
            deadline = add(manager->status.last_activity_ms, manager->status.policy.screen_off_ms);
        }
        if (!manager->status.panel_off_requested && manager->status.policy.panel_off_ms != 0U) {
            const uint64_t panel_deadline = add(manager->status.last_activity_ms, manager->status.policy.panel_off_ms);
            if (panel_deadline < deadline) {
                deadline = panel_deadline;
            }
        }
        if (manager->status.policy.automatic_suspend) {
            const uint64_t suspend_deadline = add(manager->status.last_activity_ms, manager->status.policy.suspend_ms);
            if (suspend_deadline < deadline) {
                deadline = suspend_deadline;
            }
        }
        return deadline;
    }
    return PLATFORM_RUNTIME_DEADLINE_NONE;
}
void power_manager_begin_shutdown(power_manager_t* manager, uint64_t now_ms)
{
    if (manager != NULL && manager->finalized) {
        manager->status.state            = POWER_STATE_SHUTTING_DOWN;
        manager->status.reason           = POWER_REASON_SYSTEM_ACTION;
        manager->status.state_changed_ms = now_ms;
        manager->phase                   = POWER_PHASE_NONE;
    }
}
const power_status_t* power_manager_status(const power_manager_t* manager)
{
    return manager != NULL ? &manager->status : NULL;
}
const char* power_manager_ordered_name(const power_manager_t* manager, size_t index)
{
    return manager != NULL && manager->finalized && index < manager->participant_count ?
               manager->participants[manager->dependency_order[index]].name :
               NULL;
}
size_t power_manager_trace_count(const power_manager_t* manager)
{
    return manager != NULL ? manager->trace_count : 0U;
}
const char* power_manager_trace_entry(const power_manager_t* manager, size_t index)
{
    return manager != NULL && index < manager->trace_count ? manager->trace[index] : NULL;
}
