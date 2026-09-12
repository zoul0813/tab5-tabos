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
    assert(power_manager_status(&manager)->brightness_valid);
    assert(power_manager_status(&manager)->effective_brightness == 75U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 109U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 110U);
    assert(power_manager_status(&manager)->state == POWER_STATE_IDLE);
    assert(power_manager_status(&manager)->desired_brightness == 20U);
    assert(power_manager_status(&manager)->effective_brightness == 20U);
    power_manager_request_activity(&manager, 111U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 112U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    assert(power_manager_status(&manager)->last_activity_ms == 111U);
    assert(power_manager_status(&manager)->effective_brightness == 75U);
    power_manager_begin_shutdown(&manager, 113U);
    assert(power_manager_status(&manager)->state == POWER_STATE_SHUTTING_DOWN);
    power_manager_shutdown(&manager);
}

static void test_activity_race_and_inhibitors(void)
{
    power_manager_t manager;
    assert(power_manager_init(&manager, policy(), 0U));
    assert(power_manager_finalize(&manager));

    power_manager_request_activity(&manager, 100U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER | PLATFORM_RUNTIME_EVENT_DEADLINE, 100U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    assert(power_manager_next_deadline(&manager) == 200U);

    power_manager_set_dim_inhibitors(&manager, POWER_INHIBITOR_KEYBOARD, 200U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 1000U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    assert(power_manager_next_deadline(&manager) == PLATFORM_RUNTIME_DEADLINE_NONE);
    assert(power_manager_request_suspend(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 1001U);
    assert(power_manager_status(&manager)->failure.code == POWER_FAILURE_BLOCKED);

    power_manager_set_dim_inhibitors(&manager, POWER_INHIBITOR_FULLSCREEN | POWER_INHIBITOR_MEDIA, 1100U);
    power_manager_set_dim_inhibitors(&manager, 0U, 1200U);
    assert(power_manager_status(&manager)->last_activity_ms == 1200U);
    assert(power_manager_next_deadline(&manager) == 1300U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 1299U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 1300U);
    assert(power_manager_status(&manager)->state == POWER_STATE_IDLE);
    power_manager_shutdown(&manager);
}

static void test_policy_and_brightness_failures(void)
{
    power_manager_t manager;
    assert(power_manager_init(&manager, policy(), 0U));
    assert(power_manager_finalize(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 100U);
    assert(power_manager_status(&manager)->state == POWER_STATE_IDLE);

    power_policy_t changed    = policy();
    changed.active_brightness = 50U;
    changed.idle_brightness   = 30U;
    assert(power_manager_set_policy(&manager, changed, 100U));
    assert(power_manager_status(&manager)->effective_brightness == 30U);
    changed.active_brightness = 10U;
    assert(power_manager_set_policy(&manager, changed, 100U));
    assert(power_manager_status(&manager)->effective_brightness == 10U);

    power_manager_shutdown(&manager);

    assert(power_manager_init(&manager, policy(), 0U));
    assert(power_manager_finalize(&manager));
    test_platform_fail_brightness_once();
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 100U);
    assert(power_manager_status(&manager)->state == POWER_STATE_IDLE);
    assert(power_manager_status(&manager)->desired_brightness == 20U);
    assert(power_manager_status(&manager)->effective_brightness == 75U);
    assert(power_manager_status(&manager)->failure.code == POWER_FAILURE_BRIGHTNESS);
    assert(test_platform_brightness() == 75U);
    power_manager_shutdown(&manager);

    assert(power_manager_init(&manager, policy(), 0U));
    assert(power_manager_finalize(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 100U);
    test_platform_fail_brightness_once();
    power_manager_request_activity(&manager, 101U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 101U);
    assert(power_manager_status(&manager)->state == POWER_STATE_ACTIVE);
    assert(power_manager_status(&manager)->desired_brightness == 75U);
    assert(power_manager_status(&manager)->effective_brightness == 20U);
    assert(power_manager_status(&manager)->failure.code == POWER_FAILURE_BRIGHTNESS);
    power_manager_shutdown(&manager);
}

static void test_screen_off(void)
{
    power_manager_t manager;
    power_policy_t display_policy = policy();
    display_policy.idle_ms        = 60000U;
    display_policy.screen_off_ms  = 180000U;
    assert(power_manager_init(&manager, display_policy, 10U));
    assert(power_manager_finalize(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 60010U);
    assert(test_platform_brightness() == 20U);
    assert(power_manager_next_deadline(&manager) == 180010U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 180009U);
    assert(test_platform_brightness() == 20U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 180010U);
    assert(test_platform_brightness() == 0U);
    assert(manager.status.screen_off_requested);
    assert(manager.status.state == POWER_STATE_IDLE);
    assert(manager.status.generation == 0U);
    assert(power_manager_next_deadline(&manager) == PLATFORM_RUNTIME_DEADLINE_NONE);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 600010U);
    assert(manager.status.state == POWER_STATE_IDLE);
    assert(test_platform_brightness() == 0U);

    /* Repeated activity restores the active setting and restarts both deadlines. */
    for (uint64_t cycle = 1U; cycle <= 20U; ++cycle) {
        const uint64_t now = cycle * 1000000U;
        power_manager_request_activity(&manager, now);
        power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, now);
        assert(test_platform_brightness() == 75U);
        assert(!manager.status.screen_off_requested);
        assert(power_manager_next_deadline(&manager) == now + 60000U);
        /* A delayed dispatch must go directly to off. */
        power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, now + 180000U);
        assert(test_platform_brightness() == 0U);
        assert(manager.status.state == POWER_STATE_IDLE);
    }
    power_manager_shutdown(&manager);
}

static void test_screen_off_races_policy_and_failures(void)
{
    power_manager_t manager;
    power_policy_t configured = policy();
    configured.screen_off_ms  = 300U;
    assert(power_manager_init(&manager, configured, 0U));
    assert(power_manager_finalize(&manager));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 100U);
    power_manager_request_activity(&manager, 300U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER | PLATFORM_RUNTIME_EVENT_DEADLINE, 300U);
    assert(test_platform_brightness() == 75U);
    assert(manager.status.last_activity_ms == 300U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 600U);
    assert(test_platform_brightness() == 0U);

    const uint32_t inhibitors[] = {POWER_INHIBITOR_KEYBOARD, POWER_INHIBITOR_POINTER, POWER_INHIBITOR_FULLSCREEN,
                                   POWER_INHIBITOR_MEDIA};
    for (size_t i = 0U; i < sizeof(inhibitors) / sizeof(inhibitors[0]); ++i) {
        const uint64_t now = 1000U + i * 1000U;
        power_manager_set_dim_inhibitors(&manager, inhibitors[i], now);
        assert(test_platform_brightness() == 75U);
        assert(!manager.status.screen_off_requested);
        power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, now + 500U);
        assert(test_platform_brightness() == 75U);
        assert(power_manager_next_deadline(&manager) == PLATFORM_RUNTIME_DEADLINE_NONE);
        power_manager_set_dim_inhibitors(&manager, 0U, now + 500U);
        assert(power_manager_next_deadline(&manager) == now + 600U);
        power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, now + 800U);
        assert(test_platform_brightness() == 0U);
    }

    configured.screen_off_ms = 0U;
    assert(power_manager_set_policy(&manager, configured, 4800U));
    assert(test_platform_brightness() == 20U);
    assert(!manager.status.screen_off_requested);
    assert(power_manager_next_deadline(&manager) == PLATFORM_RUNTIME_DEADLINE_NONE);
    configured.screen_off_ms = 50U;
    assert(!power_manager_set_policy(&manager, configured, 4800U));
    assert(manager.status.policy.screen_off_ms == 0U);
    configured.screen_off_ms = 300U;
    test_platform_fail_brightness_once();
    assert(power_manager_set_policy(&manager, configured, 4800U));
    assert(manager.status.desired_brightness == 0U);
    assert(!manager.status.brightness_valid);
    assert(manager.status.failure.code == POWER_FAILURE_BRIGHTNESS);
    assert(power_manager_next_deadline(&manager) == PLATFORM_RUNTIME_DEADLINE_NONE);
    power_manager_request_activity(&manager, 4801U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 4801U);
    assert(test_platform_brightness() == 75U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 5101U);
    assert(test_platform_brightness() == 0U);
    test_platform_fail_brightness_once();
    power_manager_request_activity(&manager, 5102U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 5102U);
    assert(!manager.status.brightness_valid);
    assert(test_platform_brightness() == 0U);
    /* Retry on next real activity, including when policy state is already active. */
    power_manager_request_activity(&manager, 5103U);
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_POWER, 5103U);
    assert(manager.status.brightness_valid);
    assert(test_platform_brightness() == 75U);
    configured.automatic_suspend = true;
    configured.suspend_ms        = 500U;
    assert(power_manager_set_policy(&manager, configured, 5103U));
    power_manager_update(&manager, PLATFORM_RUNTIME_EVENT_DEADLINE, 5403U);
    assert(manager.status.state == POWER_STATE_IDLE);
    assert(power_manager_next_deadline(&manager) == 5603U);
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
    test_activity_race_and_inhibitors();
    test_policy_and_brightness_failures();
    test_screen_off();
    test_screen_off_races_policy_and_failures();
    test_registration_failures();
    return 0;
}
