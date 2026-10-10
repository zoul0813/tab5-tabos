#include <tabos/input.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
#include <assert.h>
#include <stddef.h>

static unsigned int polls;

static int input_poll(tabos_input_event_t* event)
{
    ++polls;
    if (polls == 1U) {
        return 0;
    }
    *event = (tabos_input_event_t) {
        .type      = TABOS_INPUT_KEY_DOWN,
        .key       = TABOS_KEY_UP,
        .modifiers = TABOS_MODIFIER_CONTROL,
        .repeat    = true,
    };
    return 1;
}

static int snapshot_result;
static int snapshot(tabos_input_state_t* state, int resynchronize)
{
    state->generation           = resynchronize ? 42U : 41U;
    state->pressed[TABOS_KEY_A] = true;
    return snapshot_result;
}
static tabos_elf_api_t api = {
    .abi_version     = TABOS_ELF_API_VERSION,
    .input_poll      = input_poll,
    .input_get_state = snapshot,
};

const tabos_elf_api_t* tabos_runtime_api = &api;

int sched_yield(void)
{
    return 0;
}

int main(void)
{
    tabos_input_state_t state = {0};
    assert(tabos_input_get_state(NULL, false) == -1 && errno == EINVAL);
    assert(tabos_input_get_state(&state, false) == 0 && state.generation == 41U && state.pressed[TABOS_KEY_A]);
    assert(tabos_input_get_state(&state, true) == 0 && state.generation == 42U);
    snapshot_result = -EACCES;
    assert(tabos_input_get_state(&state, false) == -1 && errno == EACCES);
    api.input_get_state = NULL;
    assert(tabos_input_get_state(&state, false) == -1 && errno == ENOSYS);
    tabos_runtime_api = NULL;
    assert(tabos_input_get_state(&state, false) == -1 && errno == ENOSYS);
    tabos_runtime_api = &api;
    tabos_input_event_t event;
    errno = 0;
    if (tabos_input_poll(NULL) || errno != EINVAL) {
        return 1;
    }
    if (tabos_input_poll(&event)) {
        return 1;
    }
    if (!tabos_input_wait(&event)) {
        return 1;
    }
    return event.type == TABOS_INPUT_KEY_DOWN && event.key == TABOS_KEY_UP &&
                   event.modifiers == TABOS_MODIFIER_CONTROL && event.repeat ?
               0 :
               1;
}
