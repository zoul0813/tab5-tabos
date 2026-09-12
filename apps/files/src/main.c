#include <tabos/gui.h>
#include <tabos/filesystem.h>
#include <tabos/process.h>
#include <tabos/posix_compat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

enum {
    ENTRY_MAX = 128
};
static char path[192]      = "T:/bin";
static char directory[192] = "T:/bin";
static char names[ENTRY_MAX][TABOS_FS_NAME_MAX + 1];
static const char* items[ENTRY_MAX];
static uint32_t modes[ENTRY_MAX];
static size_t count, first;
static int selection = -1;
static char status[160];

static void refresh(void)
{
    tabos_posix_dir_t* handle = tabos_posix_opendir(path);
    if (handle == NULL) {
        strcpy(status, "Cannot open directory. Previous listing retained.");
        return;
    }
    count     = 0U;
    first     = 0U;
    selection = -1;
    errno     = 0;
    while (count < ENTRY_MAX && tabos_posix_readdir(handle) != NULL) {
        if (strcmp(handle->entry.d_name, ".") == 0 || strcmp(handle->entry.d_name, "..") == 0) {
            continue;
        }
        strcpy(names[count], handle->entry.d_name);
        modes[count] = handle->entry.d_type == 2U ? TABOS_S_IFDIR : TABOS_S_IFREG;
        items[count] = names[count];
        ++count;
    }
    const int result = errno == 0 ? 0 : -1;
    (void) tabos_posix_closedir(handle);
    strcpy(directory, path);
    (void) snprintf(status, sizeof(status),
                    result < 0         ? "Directory read failed; partial listing." :
                    count == ENTRY_MAX ? "First 128 entries shown. Enter a narrower path." :
                                         "%u entries. Select, then Open / Run.",
                    (unsigned int) count);
}

static void draw(tabos_gui_t* gui)
{
    const int width  = (int) gui->canvas.width;
    const int height = (int) gui->canvas.height;
    (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                          .id       = 1,
                                          .kind     = TABOS_GUI_TEXT_FIELD,
                                          .bounds   = {12, 12, width - 112, 48},
                                          .text     = path,
                                          .capacity = sizeof(path)
    });
    (void) tabos_gui_ui_add(&gui->ui,
                            (tabos_gui_widget_t) {
                                .id = 2, .kind = TABOS_GUI_BUTTON, .bounds = {width - 92, 12, 80, 48},
                                        .label = "Go"
    });
    (void) tabos_gui_ui_add(&gui->ui,
                            (tabos_gui_widget_t) {
                                .id = 3, .kind = TABOS_GUI_BUTTON, .bounds = {12, 68, 64, 48},
                                        .label = "Up"
    });
    (void) tabos_gui_ui_add(&gui->ui,
                            (tabos_gui_widget_t) {
                                .id = 4, .kind = TABOS_GUI_BUTTON, .bounds = {84, 68, 96, 48},
                                        .label = "Apps"
    });
    (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                          .id       = 5,
                                          .kind     = TABOS_GUI_BUTTON,
                                          .bounds   = {188, 68, 120, 48},
                                          .label    = "Open",
                                          .disabled = selection < 0
    });
    (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                          .id         = 6,
                                          .kind       = TABOS_GUI_LIST,
                                          .bounds     = {12, 124, width - 84, height - 168},
                                          .items      = items,
                                          .item_count = count,
                                          .first_item = first,
                                          .value      = selection
    });
    (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                          .id      = 7,
                                          .kind    = TABOS_GUI_SCROLLBAR,
                                          .bounds  = {width - 64, 124, 52, height - 168},
                                          .maximum = count > 0U ? (int) count - 1 : 0,
                                          .value   = (int) first
    });
    (void) tabos_gui_ui_add(
        &gui->ui, (tabos_gui_widget_t) {
                      .id = 8, .kind = TABOS_GUI_LABEL, .bounds = {12, height - 36, width - 24, 32},
                              .label = status
    });
}

static void action(tabos_gui_t* gui, int id)
{
    if (id == 2) {
        refresh();
    } else if (id == 3) {
        strcpy(path, directory);
        char* slash = strrchr(path, '/');
        if (slash != NULL) {
            slash[slash == path + 2 ? 1 : 0] = '\0';
        }
        refresh();
    } else if (id == 4) {
        strcpy(path, "T:/bin");
        refresh();
    } else if (id == 5 && selection >= 0 && (size_t) selection < count) {
        char selected[192];
        const int length = snprintf(selected, sizeof(selected), "%s%s%s", directory,
                                    directory[strlen(directory) - 1U] == '/' ? "" : "/", names[selection]);
        if (length < 0 || (size_t) length >= sizeof(selected)) {
            strcpy(status, "Path too long for GUI launch.");
        } else if ((modes[selection] & TABOS_S_IFDIR) != 0U) {
            strcpy(path, selected);
            refresh();
        } else {
            tabos_program_info_t program;
            if (tabos_program_query(selected, &program) != 0) {
                strcpy(status, "Not an executable. Open documents from Text editor.");
            } else if (tabos_gui_launch(gui, selected) != 0) {
                strcpy(status, "Launch queue busy. Try again.");
            } else {
                strcpy(status, (program.flags & TABOS_PROGRAM_GUI) != 0U ?
                                   "Opening GUI application..." :
                                   "Pausing desktop for fullscreen application...");
            }
        }
    } else if (id == 6 || id == 7) {
        for (size_t index = 0U; index < gui->ui.count; ++index) {
            if (gui->ui.widgets[index].id == id) {
                if (id == 6) {
                    selection = gui->ui.widgets[index].value;
                    first     = gui->ui.widgets[index].first_item;
                } else {
                    first = (size_t) gui->ui.widgets[index].value;
                }
            }
        }
    }
    tabos_gui_invalidate(gui);
}

int main(void)
{
    refresh();
    tabos_gui_t gui = {.draw = draw, .action = action};
    if (tabos_gui_open(&gui, "Files") != 0) {
        return 1;
    }
    int result;
    do {
        result = tabos_gui_step(&gui, 100U);
    } while (result > 0);
    tabos_gui_shutdown(&gui);
    return result < 0 ? 1 : 0;
}
