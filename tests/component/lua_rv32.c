#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/display.h>
#include <tabos/internal/input.h>
#include <tabos/internal/runtime.h>
#include <tabos/internal/application.h>
#include <tabos/platform/storage_backend.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// This optional executable takes the actual SDK-built shell as argv[1].
// Override only the host drive mapping; runtime, interpreter and SDK stay real.
static terminal_t terminal;
static char storage_root[] = "/tmp/tabos-lua-rv32-XXXXXX";

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "RV32 Lua test failed: %s\n", message);
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
    *name      = "Shell test";
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

static void pump(void)
{
    for (unsigned int index = 0U; index < 100U; ++index) {
        // Drain real wake notifications without blocking this bounded test pump.
        const platform_runtime_events_t events = platform_runtime_wait_until(platform_time_ms());
        kernel_runtime_update(events);
    }
}

static void key(tabos_key_t code, uint8_t modifiers)
{
    tabos_input_event_t event = {.type = TABOS_INPUT_KEY_DOWN, .key = code, .modifiers = modifiers};
    check(input_submit(&event), "key down");
    event.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&event), "key up");
    if (code == TABOS_KEY_ENTER) {
        const tabos_input_event_t newline = {.type = TABOS_INPUT_TEXT, .text = "\n"};
        check(input_submit(&newline), "normalized enter text");
    }
    pump();
}

static void text(const char* value)
{
    while (*value != '\0') {
        const tabos_input_event_t down = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
        check(input_submit(&down), "text physical key");
        const tabos_input_event_t event = {
            .type = TABOS_INPUT_TEXT, .text = {*value++, '\0'}
        };
        check(input_submit(&event), "text input");
        const tabos_input_event_t up = {.type = TABOS_INPUT_KEY_UP, .key = TABOS_KEY_A};
        check(input_submit(&up), "text key release");
        pump();
    }
}

static void boot(void)
{
    check(kernel_runtime_init() && platform_init(true) && kernel_runtime_start(false), "runtime startup");
    check(terminal_init(&terminal, display_framebuffer(), 2U), "test terminal");
    console_rebind(&terminal);
    check(tabos_app_launch_path("T:/shell") == TABOS_APP_RESULT_OK, "launch real RV32 shell");
    pump();
}

static void stop(void)
{
    kernel_runtime_shutdown();
    terminal_shutdown(&terminal);
    platform_shutdown();
}


static void copy(const char* src, const char* dst)
{
    FILE* source      = fopen(src, "rb");
    FILE* destination = fopen(dst, "wb");
    check(source != NULL && destination != NULL, "open artifact");
    char bytes[4096];
    size_t size;
    while ((size = fread(bytes, 1U, sizeof(bytes), source)) > 0U) {
        check(fwrite(bytes, 1U, size, destination) == size, "copy artifact");
    }
    check(!ferror(source) && fclose(source) == 0 && fclose(destination) == 0, "close artifact");
}
static void child(void)
{
    for (size_t i = 0U; i < 100U && (tabos_process_count() != 2U || kernel_application_system_runnable()); ++i) {
        pump();
    }
    check(tabos_process_count() == 2U, "Lua is shell child");
    check(!kernel_application_system_runnable(), "idle RV32 keyboard wait suspends guest");
    check(kernel_runtime_next_deadline() > platform_time_ms(), "idle wait does not spin runtime");
}
static void parent_status(int expected)
{
    for (size_t i = 0U; i < 100U && tabos_process_count() != 1U; ++i) {
        pump();
    }
    int status = -1;
    check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "parent shell resumed");
    check(tabos_app_last_exit_status(&status) && status == expected, "Lua exit status");
}
static void parent(void)
{
    parent_status(0);
}
static bool output_line(const char* wanted)
{
    size_t length = strlen(wanted);
    for (size_t line = 0U; line < terminal.line_capacity; ++line) {
        terminal_cell_t* cells = terminal.cells + line * terminal.columns;
        size_t i               = 0U;
        while (i < length && i < terminal.columns && cells[i].character == wanted[i]) {
            ++i;
        }
        if (i == length) {
            while (i < terminal.columns && (cells[i].character == ' ' || cells[i].character == 0)) {
                ++i;
            }
            if (i == terminal.columns) {
                return true;
            }
        }
    }
    return false;
}
static void command(const char* value)
{
    text(value);
    key(TABOS_KEY_ENTER, 0U);
}
static void fixture(const char* path, const char* contents)
{
    char name[512];
    snprintf(name, sizeof(name), "%s/%s", storage_root, path);
    FILE* file = fopen(name, "wb");
    check(file != NULL && fputs(contents, file) >= 0 && fclose(file) == 0, "write fixture");
}
static void remove_fixture(const char* path)
{
    char name[512];
    snprintf(name, sizeof(name), "%s/%s", storage_root, path);
    check(unlink(name) == 0, "remove fixture");
}
int main(int argc, char** argv)
{
    check(argc == 3, "pass shell and Lua RV32 artifacts");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char shell_path[512], lua_path[512], module_dir[512];
    snprintf(shell_path, sizeof(shell_path), "%s/shell", storage_root);
    snprintf(lua_path, sizeof(lua_path), "%s/lua", storage_root);
    snprintf(module_dir, sizeof(module_dir), "%s/nested", storage_root);
    check(mkdir(module_dir, 0700) == 0, "module directory");
    copy(argv[1], shell_path);
    copy(argv[2], lua_path);
    fixture("nested/init.lua", "return {value=42}");
    fixture("check.lua", "assert(arg[0]=='check.lua' and arg[1]=='first' and arg[2]=='two words'); "
                         "local a,b=...; assert(a==arg[1] and b==arg[2]); "
                         "assert(require('nested').value==42 and require('nested')==require('nested')); "
                         "assert(math.maxinteger+1==math.mininteger and math.abs(math.sqrt(2)^2-2)<1e-12); "
                         "assert(os.date('%Y-%m-%d',0)=='1970-01-01'); "
                         "assert(os.time({year=2024,month=2,day=29,hour=0})==1709164800); "
                         "assert(os.date('%Y-%m-%d',253402300799)=='9999-12-31'); "
                         "assert(not pcall(function() local function f() return 1+f() end; f() end)); "
                         "assert(not pcall(string.rep,'x',4000000)); collectgarbage(); "
                         "assert(not pcall(loadfile,'check.lua'..string.char(0)..'bad')); "
                         "assert(not load(string.char(27)..'Lua','binary','bt')); "
                         "local f=assert(io.open('bytes','wb')); assert(f:write('a'..string.char(0,255)..'z')); "
                         "assert(f:flush()); assert(f:close()); f=assert(io.open('bytes','rb')); "
                         "assert(f:read('a')=='a'..string.char(0,255)..'z'); "
                         "assert(f:seek('set',1)==1 and f:read(1)==string.char(0)); assert(f:close()); "
                         "print('SCRIPT_OK')");
    fixture("exit.lua", "local f=assert(io.open('exit-cleanup','w')); f:write('closed'); os.exit(7)");
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0 && setenv("SDL_AUDIODRIVER", "dummy", 1) == 0, "headless");
    boot();
    command("./lua -v");
    parent();
    check(output_line("Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio"), "version stdout");
    command("./lua -e 'print(1+2)'");
    parent();
    check(output_line("3"), "arithmetic stdout");
    command("./lua check.lua first 'two words'");
    parent();
    check(output_line("SCRIPT_OK"), "actual RV32 language/file/module/UTC assertions");
    char bytes_path[512];
    snprintf(bytes_path, sizeof(bytes_path), "%s/bytes", storage_root);
    FILE* bytes = fopen(bytes_path, "rb");
    char payload[8];
    check(bytes != NULL && fread(payload, 1U, sizeof(payload), bytes) == 4U && memcmp(payload, "a\0\377z", 4U) == 0 &&
              fclose(bytes) == 0,
          "binary file bytes on host drive");
    command("./lua -l n=nested -e 'assert(n.value==42)'");
    parent();
    command("./lua -e 'error(42)'");
    parent_status(1);
    command("./lua exit.lua");
    parent_status(7);
    snprintf(bytes_path, sizeof(bytes_path), "%s/exit-cleanup", storage_root);
    bytes = fopen(bytes_path, "rb");
    check(bytes != NULL && fread(payload, 1U, sizeof(payload), bytes) == 6U && memcmp(payload, "closed", 6U) == 0 &&
              fclose(bytes) == 0,
          "os.exit finalizes buffered file");
    command("./lua -");
    parent_status(1);
    command("./lua -i -e 'kept=41'");
    child();
    command("function add(x)");
    child();
    command("return x+1");
    child();
    command("end");
    child();
    command("add(kept)");
    child();
    check(output_line("42"), "multiline and expression result");
    command("error('expected failure')");
    child();
    command("string.rep('x',4000000)");
    child();
    command("assert(kept==41); print('RECOVERED')");
    child();
    check(output_line("RECOVERED"), "error/OOM recovery preserves state");
    command("while true do end");
    key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
    child();
    command("coroutine.wrap(function() while true do end end)()");
    key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
    child();
    command("x=io.read()");
    child();
    command("hello");
    child();
    command("assert(x=='hello'); print('CONSOLE_OK')");
    child();
    check(output_line("CONSOLE_OK"), "console broker shared with io");
    command("io.stdin:read()");
    child();
    key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
    child();
    key(TABOS_KEY_D, TABOS_MODIFIER_CONTROL);
    parent();
    command("./lua -e 'print(6*7)'");
    parent();
    command("./lua");
    child();
    // Process teardown must release a suspended input wait and all Lua memory.
    stop();
    boot();
    command("./lua -e 'assert(6*7==42)'");
    parent();
    stop();
    remove_fixture("bytes");
    remove_fixture("check.lua");
    remove_fixture("nested/init.lua");
    remove_fixture("exit.lua");
    remove_fixture("exit-cleanup");
    check(rmdir(module_dir) == 0, "remove module directory");
    char history[512], user[512];
    snprintf(history, sizeof(history), "%s/user/history.txt", storage_root);
    snprintf(user, sizeof(user), "%s/user", storage_root);
    check(unlink(shell_path) == 0 && unlink(lua_path) == 0 && unlink(history) == 0 && rmdir(user) == 0 &&
              rmdir(storage_root) == 0,
          "cleanup");
    puts("RV32 Lua scripts, modules, files, REPL, errors, interruption, cleanup and repeat launch passed");
    return 0;
}
