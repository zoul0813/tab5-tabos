#include <tabos/internal/elf_api.h>
#include <tabos/internal/elf_loader.h>
#include <tabos/platform/platform.h>

#include "hello_elf.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool wrote_expected_message;
static bool wrote_first_argument;
static bool wrote_quoted_argument;
static bool requested_exit;
static int requested_status = -1;
static uint32_t tty_mode;

static void test_console_write(const char* text)
{
    if (text == NULL) {
        return;
    }
    if (strcmp(text, "Hello TabOS!") == 0) {
        wrote_expected_message = true;
    }
    if (strcmp(text, "one") == 0) {
        wrote_first_argument = true;
    }
    if (strcmp(text, "two words") == 0) {
        wrote_quoted_argument = true;
    }
}

static void test_console_write_raw(const char* text)
{
    (void) text;
}

static void test_request_exit(int status)
{
    requested_exit   = true;
    requested_status = status;
}

static int test_tty_get_mode(int descriptor)
{
    return descriptor == 0 ? (int) tty_mode : -1;
}

static int test_tty_set_mode(int descriptor, uint32_t mode)
{
    if (descriptor != 0) {
        return -1;
    }
    tty_mode = mode;
    return 0;
}

static platform_riscv32_result_t execute_raw_with_limits(const uint32_t* instructions, size_t size, unsigned int budget,
                                                         size_t heap_bytes, size_t stack_bytes)
{
    const tabos_elf_api_t api = {
        .abi_version       = TABOS_ELF_API_VERSION,
        .console_write     = test_console_write,
        .console_write_raw = test_console_write_raw,
        .request_exit      = test_request_exit,
        .tty_get_mode      = test_tty_get_mode,
        .tty_set_mode      = test_tty_set_mode,
    };
    int returned_status = -1;
    platform_riscv32_context_t* context =
        platform_riscv32_create(instructions, instructions, size, 0U, heap_bytes, stack_bytes, &api, 0U, NULL, NULL);
    if (context == NULL) {
        return PLATFORM_RISCV32_FAULT;
    }
    const platform_riscv32_result_t result = platform_riscv32_step(context, budget, &returned_status);
    platform_riscv32_destroy(context);
    return result;
}

static platform_riscv32_result_t execute_raw(const uint32_t* instructions, size_t size, unsigned int budget)
{
    return execute_raw_with_limits(instructions, size, budget, 256U * 1024U, 16U * 1024U);
}

int main(void)
{
    loader_elf_image_t image;
    if (loader_elf_load(loader_hello_elf, loader_hello_elf_size, &image) != LOADER_ELF_OK) {
        return 1;
    }

    const tabos_elf_api_t api = {
        .abi_version       = TABOS_ELF_API_VERSION,
        .console_write     = test_console_write,
        .console_write_raw = test_console_write_raw,
        .request_exit      = test_request_exit,
        .tty_get_mode      = test_tty_get_mode,
        .tty_set_mode      = test_tty_set_mode,
    };
    const char* const arguments[]       = {"hello", "one", "two words"};
    int returned_status                 = -1;
    platform_riscv32_context_t* context = platform_riscv32_create(
        image.entry, image.memory, image.memory_size, image.info.minimum_address, image.info.requested_heap_bytes,
        image.info.requested_stack_bytes, &api, 3U, arguments, NULL);
    platform_riscv32_result_t result = PLATFORM_RISCV32_FAULT;
    if (context != NULL) {
        result = platform_riscv32_step(context, 10000U, &returned_status);
    }
    platform_riscv32_destroy(context);
    loader_elf_unload(&image);

    if (result != PLATFORM_RISCV32_RETURNED || !wrote_expected_message || !wrote_first_argument ||
        !wrote_quoted_argument || !requested_exit || requested_status != 0 || returned_status != 0 || tty_mode != 1U) {
        fprintf(stderr, "result=%d message=%d arg1=%d arg2=%d exit=%d status=%d returned=%d\n", result,
                wrote_expected_message, wrote_first_argument, wrote_quoted_argument, requested_exit, requested_status,
                returned_status);
        return 1;
    }

    static const uint32_t illegal_instruction[] = {UINT32_C(0xffffffff)};
    static const uint32_t invalid_load[]        = {
        UINT32_C(0x00200537), /* lui a0, 0x200: address 0x00200000 */
        UINT32_C(0x00052503), /* lw a0, 0(a0): beyond default guest RAM */
    };
    static const uint32_t large_heap_load[] = {
        UINT32_C(0x00200537), /* lui a0, 0x200: address 0x00200000 */
        UINT32_C(0x00052503), /* lw a0, 0(a0): inside a 3 MiB guest heap */
        UINT32_C(0x00008067), /* ret */
    };
    static const uint32_t runaway_loop[] = {
        UINT32_C(0x0000006f), /* jal zero, 0 */
    };
    static const uint32_t resumable_program[] = {
        UINT32_C(0x00000013), /* nop */
        UINT32_C(0x00000013), /* nop */
        UINT32_C(0x00008067), /* ret */
    };
    platform_riscv32_context_t* resumable =
        platform_riscv32_create(resumable_program, resumable_program, sizeof(resumable_program), 0U, 256U * 1024U,
                                16U * 1024U, &api, 0U, NULL, NULL);
    int resumable_status = -1;
    const bool resumed   = resumable != NULL &&
                         platform_riscv32_step(resumable, 2U, &resumable_status) == PLATFORM_RISCV32_YIELDED &&
                         platform_riscv32_step(resumable, 2U, &resumable_status) == PLATFORM_RISCV32_RETURNED;
    platform_riscv32_destroy(resumable);

    platform_riscv32_context_t* capped =
        platform_riscv32_create(resumable_program, resumable_program, sizeof(resumable_program), 0U,
                                24U * 1024U * 1024U, 16U * 1024U, &api, 0U, NULL, NULL);
    const bool cap_rejected = capped == NULL;
    platform_riscv32_destroy(capped);

    const bool illegal_fault =
        execute_raw(illegal_instruction, sizeof(illegal_instruction), 1U) == PLATFORM_RISCV32_FAULT;
    const bool invalid_fault   = execute_raw(invalid_load, sizeof(invalid_load), 2U) == PLATFORM_RISCV32_FAULT;
    const bool large_heap_runs = execute_raw_with_limits(large_heap_load, sizeof(large_heap_load), 4U,
                                                         3U * 1024U * 1024U, 16U * 1024U) == PLATFORM_RISCV32_RETURNED;
    const bool runaway_yields  = execute_raw(runaway_loop, sizeof(runaway_loop), 1000U) == PLATFORM_RISCV32_YIELDED;
    if (!resumed || !cap_rejected || !illegal_fault || !invalid_fault || !large_heap_runs || !runaway_yields) {
        fprintf(stderr, "resumed=%d cap=%d illegal=%d invalid=%d large=%d runaway=%d\n", resumed, cap_rejected,
                illegal_fault, invalid_fault, large_heap_runs, runaway_yields);
        return 1;
    }
    return 0;
}
