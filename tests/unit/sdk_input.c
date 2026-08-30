#include <tabos/input.h>
#include <tabos/internal/elf_api.h>

#include <errno.h>
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

static const tabos_elf_api_t api = {
    .abi_version = TABOS_ELF_API_VERSION,
    .input_poll  = input_poll,
};

const tabos_elf_api_t* tabos_runtime_api = &api;

int sched_yield(void)
{
    return 0;
}

int main(void)
{
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
