#include "platform_test.h"

#include <tabos/internal/power.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
        power_callback_result_t suspend_result;
        power_callback_result_t resume_result;
        bool blocked;
        power_completion_token_t token;
} participant_context_t;

static power_callback_result_t suspend_participant(void* context, power_completion_token_t token)
{
    participant_context_t* participant = context;
    participant->token                 = token;
    return participant->suspend_result;
}

static power_callback_result_t resume_participant(void* context, power_completion_token_t token)
{
    participant_context_t* participant = context;
    participant->token                 = token;
    return participant->resume_result;
}

static bool participant_blocked(void* context, char* reason, size_t reason_capacity)
{
    participant_context_t* participant = context;
    if (participant->blocked) {
        (void) snprintf(reason, reason_capacity, "busy");
    }
    return participant->blocked;
}

static power_policy_t policy(void)
{
    return (power_policy_t) {.idle_ms           = 100U,
                             .suspend_ms        = 1000U,
                             .active_brightness = 75U,
                             .idle_brightness   = 20U,
                             .automatic_suspend = false};
}

static void register_participant(power_manager_t* manager, const char* name, const char* dependency,
                                 participant_context_t* context)
{
    const char* dependencies[]                          = {dependency};
    const power_participant_registration_t registration = {
        .name             = name,
        .dependencies     = dependency != NULL ? dependencies : NULL,
        .dependency_count = dependency != NULL ? 1U : 0U,
        .blocked          = participant_blocked,
        .suspend          = suspend_participant,
        .resume           = resume_participant,
        .context          = context,
    };
    assert(power_manager_register(manager, &registration));
}

static void test_order_async_and_wake(void)
{
    power_manager_t manager;
    participant_context_t storage = {0};
    participant_context_t media   = {0};
    participant_context_t app     = {.suspend_result = POWER_CALLBACK_PENDING};
    assert(power_manager_init(&manager, policy(), 10U));
    register_participant(&manager, "app", "media", &app);
    register_participant(&manager, "storage", NULL, &storage);
    register_participant(&manager, "media", "storage", &media);
    assert(power_manager_finalize(&manager));
    assert(strcmp(power_manager_ordered_name(&manager, 0U), "storage") == 0);
    assert(strcmp(power_manager_ordered_name(&manager, 1U), "media") == 0);
    assert(strcmp(power_manager_ordered_name(&manager, 2U), "app") == 0);
    assert(power_manager_next_deadline(&manager) == 110U);
    assert(power_manager_request_suspend(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 20U);
    assert(power_manager_status(&manager)->state == POWER_STATE_SUSPENDING);
    assert(power_manager_next_deadline(&manager) == PLATFORM_RUNTIME_DEADLINE_NONE);
    power_completion_token_t stale = app.token;
    stale.generation--;
    power_manager_complete(&manager, stale, POWER_CALLBACK_SUCCESS);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 21U);
    assert(power_manager_status(&manager)->state == POWER_STATE_SUSPENDING);
    power_manager_complete(&manager, app.token, POWER_CALLBACK_SUCCESS);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 22U);
    assert(power_manager_status(&manager)->state == POWER_STATE_SUSPENDED);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 30U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    assert(power_manager_status(&manager)->wake_causes == PLATFORM_POWER_WAKE_KEYBOARD);
    assert(power_manager_trace_count(&manager) == 6U);
    assert(strcmp(power_manager_trace_entry(&manager, 0U), "app:down") == 0);
    assert(strcmp(power_manager_trace_entry(&manager, 5U), "app:up") == 0);
    power_manager_shutdown(&manager);
}

static void test_deadline_blocker_and_failure(void)
{
    power_manager_t manager;
    participant_context_t first  = {0};
    participant_context_t second = {.blocked = true};
    assert(power_manager_init(&manager, policy(), UINT64_MAX - 50U));
    register_participant(&manager, "first", NULL, &first);
    register_participant(&manager, "second", "first", &second);
    assert(power_manager_finalize(&manager));
    assert(power_manager_next_deadline(&manager) == UINT64_MAX);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, UINT64_MAX - 1U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    assert(power_manager_request_suspend(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 5U);
    assert(power_manager_status(&manager)->failure.code == POWER_FAILURE_BLOCKED);
    second.blocked       = false;
    first.suspend_result = POWER_CALLBACK_FAILURE;
    assert(power_manager_request_suspend(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 6U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    assert(power_manager_status(&manager)->failure.code == POWER_FAILURE_CALLBACK);
    assert(strcmp(power_manager_trace_entry(&manager, 2U), "second:up") == 0);
    power_manager_shutdown(&manager);
}

static void test_idle_activity_and_shutdown(void)
{
    power_manager_t manager;
    assert(power_manager_init(&manager, policy(), 10U));
    assert(power_manager_finalize(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 109U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 110U);
    assert(power_manager_status(&manager)->state == POWER_STATE_IDLE);
    power_manager_request_activity(&manager, 111U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 112U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    assert(power_manager_status(&manager)->last_activity_ms == 111U);
    power_manager_begin_shutdown(&manager, 113U);
    assert(power_manager_status(&manager)->state == POWER_STATE_SHUTTING_DOWN);
    power_manager_shutdown(&manager);
}

static void test_registration_failures(void)
{
    power_manager_t manager;
    participant_context_t context = {0};
    assert(power_manager_init(&manager, policy(), 0U));
    register_participant(&manager, "same", NULL, &context);
    const power_participant_registration_t duplicate = {.name = "same"};
    assert(!power_manager_register(&manager, &duplicate));
    assert(!power_manager_finalize(&manager));
    assert(!power_manager_status(&manager)->suspend_available);
    power_manager_shutdown(&manager);

    assert(power_manager_init(&manager, policy(), 0U));
    register_participant(&manager, "orphan", "missing", &context);
    assert(!power_manager_finalize(&manager));
    assert(power_manager_status(&manager)->failure.code == POWER_FAILURE_MISSING_DEPENDENCY);
    power_manager_shutdown(&manager);

    assert(power_manager_init(&manager, policy(), 0U));
    const char* left_dependency[]               = {"right"};
    const char* right_dependency[]              = {"left"};
    const power_participant_registration_t left = {
        .name = "left", .dependencies = left_dependency, .dependency_count = 1U};
    const power_participant_registration_t right = {
        .name = "right", .dependencies = right_dependency, .dependency_count = 1U};
    assert(power_manager_register(&manager, &left));
    assert(power_manager_register(&manager, &right));
    assert(!power_manager_finalize(&manager));
    assert(power_manager_status(&manager)->failure.code == POWER_FAILURE_DEPENDENCY_CYCLE);
    power_manager_shutdown(&manager);

    assert(power_manager_init(&manager, policy(), 0U));
    for (size_t index = 0U; index < POWER_PARTICIPANT_CAPACITY; ++index) {
        char name[POWER_NAME_CAPACITY];
        (void) snprintf(name, sizeof(name), "participant-%zu", index);
        register_participant(&manager, name, NULL, &context);
    }
    const power_participant_registration_t overflow = {.name = "overflow"};
    assert(!power_manager_register(&manager, &overflow));
    assert(power_manager_status(&manager)->failure.code == POWER_FAILURE_CAPACITY);
    power_manager_shutdown(&manager);
}

int main(void)
{
    test_order_async_and_wake();
    test_deadline_blocker_and_failure();
    test_idle_activity_and_shutdown();
    test_registration_failures();
    return 0;
}
