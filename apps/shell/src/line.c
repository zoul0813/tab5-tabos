#include <shell/line.h>

#include <string.h>

static void replace_line(shell_line_t* line, const char* replacement, FILE* output)
{
    if (replacement == NULL) {
        return;
    }
    // Destructive backspace also crosses soft wraps in the TabOS terminal.
    // Emit one batch so recalling a long command does not present per character.
    char erase[SHELL_LINE_CAPACITY];
    memset(erase, '\b', line->used);
    (void) fwrite(erase, 1U, line->used, output);
    strcpy(line->text, replacement);
    line->used = strlen(line->text);
    (void) fwrite(line->text, 1U, line->used, output);
}

bool shell_line_feed(shell_line_t* line, shell_history_t* history, uint8_t byte, FILE* output)
{
    char character = '\0';
    switch (shell_input_filter(&line->filter, byte, &character)) {
        case SHELL_INPUT_ENTER: (void) fputc('\n', output); return true;
        case SHELL_INPUT_BACKSPACE:
            if (line->used > 0U) {
                shell_history_reset_navigation(history);
                line->text[--line->used] = '\0';
                (void) fputc('\b', output);
            }
            break;
        case SHELL_INPUT_CHARACTER:
            if (line->used < SHELL_LINE_CAPACITY - 1U) {
                shell_history_reset_navigation(history);
                line->text[line->used++] = character;
                line->text[line->used]   = '\0';
                (void) fputc((unsigned char) character, output);
            }
            break;
        case SHELL_INPUT_UP: replace_line(line, shell_history_previous(history, line->text), output); break;
        case SHELL_INPUT_DOWN: replace_line(line, shell_history_next(history), output); break;
        case SHELL_INPUT_NONE: break;
    }
    return false;
}

void shell_line_reset(shell_line_t* line, shell_history_t* history)
{
    *line = (shell_line_t) {0};
    shell_history_reset_navigation(history);
}
