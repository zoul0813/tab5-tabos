#ifndef TABOS_ANSI_H
#define TABOS_ANSI_H

#define ANSI_ESCAPE "\x1b["

#define ANSI_STRINGIFY_IMPL(value) #value
#define ANSI_STRINGIFY(value) ANSI_STRINGIFY_IMPL(value)

#define ANSI_RESET          ANSI_ESCAPE "0m"
#define ANSI_REVERSE        ANSI_ESCAPE "7m"

#define ANSI_CLEAR_SCREEN   ANSI_ESCAPE "2J"
#define ANSI_ERASE_LINE     ANSI_ESCAPE "K"
#define ANSI_ERASE_LINE_ALL ANSI_ESCAPE "2K"
#define ANSI_CURSOR_HOME    ANSI_ESCAPE "H"
#define ANSI_SAVE_CURSOR    ANSI_ESCAPE "s"
#define ANSI_RESTORE_CURSOR ANSI_ESCAPE "u"

#define ANSI_CURSOR_UP(count) \
    ANSI_ESCAPE ANSI_STRINGIFY(count) "A"
#define ANSI_CURSOR_DOWN(count) \
    ANSI_ESCAPE ANSI_STRINGIFY(count) "B"
#define ANSI_CURSOR_FORWARD(count) \
    ANSI_ESCAPE ANSI_STRINGIFY(count) "C"
#define ANSI_CURSOR_BACK(count) \
    ANSI_ESCAPE ANSI_STRINGIFY(count) "D"
#define ANSI_CURSOR_COLUMN(column) \
    ANSI_ESCAPE ANSI_STRINGIFY(column) "G"
#define ANSI_CURSOR_ROW(row) \
    ANSI_ESCAPE ANSI_STRINGIFY(row) "H"

#define ANSI_BLACK          "\x1b[30m"
#define ANSI_RED            "\x1b[31m"
#define ANSI_GREEN          "\x1b[32m"
#define ANSI_YELLOW         "\x1b[33m"
#define ANSI_BLUE           "\x1b[34m"
#define ANSI_MAGENTA        "\x1b[35m"
#define ANSI_CYAN           "\x1b[36m"
#define ANSI_WHITE          "\x1b[37m"

#define ANSI_BG_BLACK       ANSI_ESCAPE "40m"
#define ANSI_BG_RED         ANSI_ESCAPE "41m"
#define ANSI_BG_GREEN       ANSI_ESCAPE "42m"
#define ANSI_BG_YELLOW      ANSI_ESCAPE "43m"
#define ANSI_BG_BLUE        ANSI_ESCAPE "44m"
#define ANSI_BG_MAGENTA     ANSI_ESCAPE "45m"
#define ANSI_BG_CYAN        ANSI_ESCAPE "46m"
#define ANSI_BG_WHITE       ANSI_ESCAPE "47m"

#define ANSI_BG_BRIGHT_BLACK   ANSI_ESCAPE "100m"
#define ANSI_BG_BRIGHT_RED     ANSI_ESCAPE "101m"
#define ANSI_BG_BRIGHT_GREEN   ANSI_ESCAPE "102m"
#define ANSI_BG_BRIGHT_YELLOW  ANSI_ESCAPE "103m"
#define ANSI_BG_BRIGHT_BLUE    ANSI_ESCAPE "104m"
#define ANSI_BG_BRIGHT_MAGENTA ANSI_ESCAPE "105m"
#define ANSI_BG_BRIGHT_CYAN    ANSI_ESCAPE "106m"
#define ANSI_BG_BRIGHT_WHITE   ANSI_ESCAPE "107m"

#define ANSI_BRIGHT_BLACK   "\x1b[90m"
#define ANSI_BRIGHT_RED     "\x1b[91m"
#define ANSI_BRIGHT_GREEN   "\x1b[92m"
#define ANSI_BRIGHT_YELLOW  "\x1b[93m"
#define ANSI_BRIGHT_BLUE    "\x1b[94m"
#define ANSI_BRIGHT_MAGENTA "\x1b[95m"
#define ANSI_BRIGHT_CYAN    "\x1b[96m"
#define ANSI_BRIGHT_WHITE   "\x1b[97m"

#endif
