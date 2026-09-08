#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <tabos/application.h>
#include <tabos/internal/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/runtime.h>
#include <tabos/platform/storage_backend.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char storage_root[] = "/tmp/tabos-graphics-cleanup-XXXXXX";
static tabos_app_context_t* parent_context;

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "ELF graphics cleanup test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char* letter, char* root, size_t root_size, bool* removable, const char** name)
{
    if (index != 0U || strlen(storage_root) >= root_size) {
        return false;
    }
    strcpy(root, storage_root);
    *letter    = 'T';
    *removable = true;
    *name      = "Graphics cleanup test";
    return true;
}

void storage_backend_unmount(char letter)
{
    (void) letter;
}

bool storage_backend_info(char letter, uint64_t* total_bytes, uint64_t* free_bytes)
{
    (void) letter;
    *total_bytes = 1024U * 1024U;
    *free_bytes  = 512U * 1024U;
    return true;
}

static bool parent_entry(tabos_app_context_t* context)
{
    parent_context = context;
    return tabos_app_console(context) != NULL;
}

static const tabos_app_descriptor_t parent = {
    .abi_version  = TABOS_APPLICATION_ABI_VERSION,
    .name         = "graphics-parent",
    .version      = "1",
    .capabilities = TABOS_APP_CAPABILITY_CONSOLE,
    .entry        = parent_entry,
};

static void write_u32(uint8_t* bytes, uint32_t value)
{
    for (unsigned int index = 0U; index < 4U; ++index) {
        bytes[index] = (uint8_t) (value >> (8U * index));
    }
}

typedef enum {
    CHILD_RETURN,
    CHILD_EXIT,
    CHILD_FAULT,
    CHILD_FORCED,
    CHILD_PRESENT,
} child_end_t;

static void write_child(const char* path, child_end_t ending)
{
    /* Minimal ET_EXEC, one RWX segment at guest address zero. Private RV32
     * table offsets below match the interpreter ABI, not host pointer sizes.
     * No external application toolchain or installed rootfs is required. */
    /* 256 bytes of code/data followed by the 76-byte clipped blit options. */
    uint8_t elf[84U + 332U] = {0};
    memcpy(elf, "\177ELF\1\1\1", 7U);
    elf[16] = 2U;
    elf[18] = 243U;
    write_u32(elf + 20U, 1U);
    write_u32(elf + 28U, 52U);
    elf[40] = 52U;
    elf[42] = 32U;
    elf[44] = 1U;
    write_u32(elf + 52U, 1U);
    write_u32(elf + 56U, 84U);
    write_u32(elf + 68U, 332U);
    write_u32(elf + 72U, 332U);
    write_u32(elf + 76U, 7U);
    write_u32(elf + 80U, 4U);
    static const uint32_t instructions[] = {
        0x00050413U, /* mv s0, a0: retain API */
        0x00008493U, /* mv s1, ra: retain entry return gate */
        0x00010513U, /* mv a0, sp: width output */
        0x00410593U, /* addi a1, sp, 4: height output */
        0x06c42283U, /* lw t0, 108(s0): graphics_open */
        0x000280e7U, /* jalr ra, t0 */
        0x10000293U, /* li t0, 256: blit options */
        0x00810313U, /* addi t1, sp, 8: stack-backed pixel */
        0x0062a023U, /* sw t1, 0(t0): options.pixels */
        0x12300293U, /* li t0, 0x123 */
        0x00512423U, /* sw t0, 8(sp) */
        0x10000513U, /* li a0, 256 */
        0x08842283U, /* lw t0, 136(s0): graphics_blit_ex */
        0x000280e7U, /* jalr ra, t0 */
        0x02442283U, /* lw t0, 36(s0): yield with blit queued */
        0x000280e7U, /* jalr ra, t0 */
    };
    uint8_t* code = elf + 84U;
    for (size_t index = 0U; index < sizeof(instructions) / sizeof(instructions[0]); ++index) {
        write_u32(code + index * 4U, instructions[index]);
    }
    uint8_t* tail = code + sizeof(instructions);
    if (ending == CHILD_EXIT) {
        write_u32(tail, 0x00700513U);      /* li a0, 7 */
        write_u32(tail + 4U, 0x00842283U); /* request_exit */
        write_u32(tail + 8U, 0x000280e7U);
        write_u32(tail + 12U, 0x0000006fU); /* j . */
    } else if (ending == CHILD_FAULT) {
        write_u32(tail, 0xffffffffU);
    } else if (ending == CHILD_FORCED) {
        write_u32(tail, 0x0000006fU);
    } else if (ending == CHILD_PRESENT) {
        write_u32(tail, 0x07c42283U); /* graphics_present: flush and yield */
        write_u32(tail + 4U, 0x000280e7U);
        write_u32(tail + 8U, 0x08042283U); /* explicit graphics_close */
        write_u32(tail + 12U, 0x000280e7U);
        write_u32(tail + 16U, 0x00700513U);
        write_u32(tail + 20U, 0x00048067U); /* jr s1 */
    } else {
        write_u32(tail, 0x00700513U);
        write_u32(tail + 4U, 0x00048067U);
    }
    uint8_t* options = code + 256U;
    write_u32(options + 4U, 1U); /* bitmap width/height */
    write_u32(options + 8U, 1U);
    write_u32(options + 20U, 1U); /* source width/height */
    write_u32(options + 24U, 1U);
    write_u32(options + 28U, 100U); /* destination x/y */
    write_u32(options + 32U, 100U);
    write_u32(options + 36U, 1U); /* destination width/height */
    write_u32(options + 40U, 1U);
    write_u32(options + 44U, 1U); /* rotate 90: force instrumented scalar blit */
    options[50] = 255U;
    FILE* file  = fopen(path, "wb");
    check(file != NULL, "open fixture");
    check(fwrite(elf, 1U, sizeof(elf), file) == sizeof(elf), "write fixture");
    check(fclose(file) == 0, "close fixture");
}

int main(void)
{
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0, "headless environment");
    check(kernel_runtime_init() && platform_init(true) && kernel_runtime_start(false), "runtime startup");
    check(application_registry_register(&parent) && tabos_app_launch(parent.name) == TABOS_APP_RESULT_OK, "parent");
    char path[512];
    (void) snprintf(path, sizeof(path), "%s/child", storage_root);
    for (unsigned int round = 0U; round < 2U; ++round) {
        for (child_end_t ending = CHILD_RETURN; ending <= CHILD_PRESENT; ++ending) {
            write_child(path, ending);
            check(tabos_console_clear(tabos_app_console(parent_context)), "clear parent terminal");
            check(tabos_app_exec(parent_context, "T:/child") == TABOS_APP_RESULT_OK, "launch child");
            kernel_application_system_update();
            platform_framebuffer_t* framebuffer = display_framebuffer();
            const size_t pixel                  = 100U * framebuffer->stride_pixels + 100U;
            check(tabos_process_count() == 2U && console_next_deadline() == UINT64_MAX, "child yielded in graphics");
            check(framebuffer->pixels[pixel] == 0U, "queued blit has not rendered");
            if (ending == CHILD_FORCED) {
                check(kernel_process_force_terminate((tabos_process_id_t) (round * 5U + (unsigned int) ending + 1U), 9),
                      "force child termination");
            } else if (ending == CHILD_PRESENT) {
                kernel_application_system_update();
                check(framebuffer->pixels[pixel] == 0x123U, "explicit present flushes live guest pixels");
            }
            for (unsigned int step = 0U; step < 10U && tabos_process_count() > 1U; ++step) {
                kernel_application_system_update();
            }
            int status          = 0;
            int expected_status = 7;
            if (ending == CHILD_FAULT) {
                expected_status = 5;
            } else if (ending == CHILD_FORCED) {
                expected_status = 9;
            }
            check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "parent restored");
            check(tabos_app_take_child_status(parent_context, &status) && status == expected_status, "child status");
            check(console_next_deadline() != UINT64_MAX && framebuffer->pixels[pixel] == 0U, "terminal restored");
            check(tabos_console_write(tabos_app_console(parent_context), "parent alive"), "parent console ownership");
        }
    }
    kernel_runtime_shutdown();
    platform_shutdown();
    check(unlink(path) == 0 && rmdir(storage_root) == 0, "clean storage");
    return EXIT_SUCCESS;
}
