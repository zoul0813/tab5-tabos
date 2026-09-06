#include <shell/history.h>
#include <shell/line.h>
#include <shell/parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "shell history test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void feed(shell_line_t* line, shell_history_t* history, FILE* output, const char* text)
{
    while (*text != '\0') {
        check(!shell_line_feed(line, history, (uint8_t) *text++, output), "editing does not submit");
    }
}

int main(void)
{
    shell_history_t history = {0};
    check(shell_history_previous(&history, "") == NULL, "empty up");
    check(shell_history_next(&history) == NULL, "empty down");
    check(!shell_history_add(&history, "") && !shell_history_add(&history, "   "), "blank ignored");
    check(shell_history_add(&history, "one"), "first");
    check(!shell_history_add(&history, "one"), "adjacent duplicate");
    check(shell_history_add(&history, "two") && shell_history_add(&history, "one"), "separated repeat");
    for (unsigned int index = 0U; index < 40U; ++index) {
        char text[32];
        (void) snprintf(text, sizeof(text), "command %u", index);
        check(shell_history_add(&history, text), "append");
    }
    check(history.count == 32U && strcmp(history.entries[0], "command 8") == 0, "evict oldest");
    check(strcmp(shell_history_previous(&history, "draft"), "command 39") == 0, "newest first");
    for (unsigned int index = 1U; index < 32U; ++index) {
        check(shell_history_previous(&history, "ignored") != NULL, "older");
    }
    check(shell_history_previous(&history, "ignored") == NULL, "oldest boundary");
    for (unsigned int index = 0U; index < 31U; ++index) {
        check(shell_history_next(&history) != NULL, "newer");
    }
    check(strcmp(shell_history_next(&history), "draft") == 0, "restore draft");
    check(shell_history_next(&history) == NULL, "newest boundary");

    history           = (shell_history_t) {0};
    shell_line_t line = {0};
    FILE* output      = tmpfile();
    check(output != NULL, "output");
    check(shell_history_add(&history, "hello one \"two words\""), "quoted entry");
    feed(&line, &history, output, "unfinished");
    feed(&line, &history, output, "\033[A");
    check(strcmp(line.text, history.entries[0]) == 0, "recall");
    feed(&line, &history, output, "\033[B");
    check(strcmp(line.text, "unfinished") == 0, "editor restores draft");
    feed(&line, &history, output, "\033[A\bX");
    check(strcmp(history.entries[0], "hello one \"two words\"") == 0, "edit keeps stored entry");
    char edited[SHELL_LINE_CAPACITY];
    strcpy(edited, line.text);
    feed(&line, &history, output, "\033[A\033[B");
    check(strcmp(line.text, edited) == 0, "edited recall becomes draft");
    check(shell_line_feed(&line, &history, '\n', output), "enter submits");
    check(shell_history_add(&history, line.text), "record before parsing");
    char* arguments[16];
    (void) shell_parse_arguments(line.text, arguments, 16U);
    check(strcmp(history.entries[1], edited) == 0, "parser cannot mutate stored text");
    shell_line_reset(&line, &history);
    check(line.used == 0U && history.cursor == history.count, "reset");
    char maximum[SHELL_LINE_CAPACITY];
    memset(maximum, 'z', sizeof(maximum) - 1U);
    maximum[sizeof(maximum) - 1U] = '\0';
    feed(&line, &history, output, maximum);
    feed(&line, &history, output, "overflow");
    check(line.used == 255U && strcmp(line.text, maximum) == 0, "line capacity");
    check(shell_history_add(&history, maximum), "maximum entry");
    char oversized[SHELL_LINE_CAPACITY + 1U];
    memset(oversized, 'x', sizeof(oversized) - 1U);
    oversized[sizeof(oversized) - 1U] = '\0';
    check(!shell_history_add(&history, oversized), "oversized rejected");
    check(!shell_history_add(&history, "a\tb"), "controls rejected");
    check(fclose(output) == 0, "close");
    return EXIT_SUCCESS;
}
