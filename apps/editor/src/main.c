#include <editor/document.h>
#include <tabos/gui.h>
#include <stdio.h>
#include <string.h>

enum {
    DIALOG_MENU = -1,
    DIALOG_NONE,
    DIALOG_OPEN,
    DIALOG_SAVE,
    DIALOG_DIRTY
};
enum {
    AFTER_NONE,
    AFTER_OPEN,
    AFTER_NEW,
    AFTER_CLOSE
};
static editor_document_t document;
static char path[EDITOR_PATH_CAPACITY] = "T:/notes.txt";
static char status[160]                = "CP437 text, up to 32767 bytes. Open or start typing.";
static int dialog, after;

static void button(tabos_gui_t* gui, int id, int x, int y, int width, const char* label, bool disabled)
{
    (void) tabos_gui_ui_add(
        &gui->ui,
        (tabos_gui_widget_t) {
            .id = id, .kind = TABOS_GUI_BUTTON, .bounds = {x, y, width, 48},
                    .label = label, .disabled = disabled
    });
}

static void draw(tabos_gui_t* gui)
{
    const int width = (int) gui->canvas.width, height = (int) gui->canvas.height;
    const bool modal = dialog != DIALOG_NONE;
    button(gui, 7, 8, 8, 88, "File", modal);
    button(gui, 2, 104, 8, 88, "Open", modal);
    button(gui, 3, 200, 8, 88, "Save", modal);
    if (width >= 448) {
        button(gui, 4, 296, 8, 144, "Save as", modal);
    }
    (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                          .id        = 5,
                                          .kind      = TABOS_GUI_TEXT_FIELD,
                                          .bounds    = {8, 64, width - 16, height - 108},
                                          .text      = document.text,
                                          .capacity  = sizeof(document.text),
                                          .multiline = true,
                                          .disabled  = modal
    });
    (void) tabos_gui_ui_add(
        &gui->ui, (tabos_gui_widget_t) {
                      .id = 6, .kind = TABOS_GUI_LABEL, .bounds = {8, height - 36, width - 16, 32},
                              .label = status
    });
    if (!modal) {
        return;
    }
    if (dialog == DIALOG_MENU) {
        static const char* const commands[] = {"New", "Open", "Save", "Save as", "Cancel"};
        (void) tabos_gui_ui_menu(&gui->ui, 30, 8, 60, 240, commands, 5U);
        return;
    }
    const int box_width = width > 800 ? 760 : width - 16;
    const int left      = (width - box_width) / 2;
    const int top       = height > 260 ? (height - 240) / 2 : 4;
    (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                          .id       = 10,
                                          .kind     = TABOS_GUI_PANEL,
                                          .bounds   = {left, top, box_width, height > 260 ? 240 : height - 8},
                                          .disabled = true
    });
    (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                          .id     = 11,
                                          .kind   = TABOS_GUI_LABEL,
                                          .bounds = {left + 12, top + 8, box_width - 24, 32},
                                          .label  = dialog == DIALOG_DIRTY ? "Save unsaved changes?" :
                                                    dialog == DIALOG_OPEN  ? "Open text file" :
                                                                             "Save text file"
    });
    if (dialog != DIALOG_DIRTY) {
        (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                              .id       = 12,
                                              .kind     = TABOS_GUI_TEXT_FIELD,
                                              .bounds   = {left + 12, top + 48, box_width - 24, 48},
                                              .text     = path,
                                              .capacity = sizeof(path)
        });
    }
    const int button_width = (box_width - 32) / 3;
    button(gui, 20, left + 8, top + 104, button_width, dialog == DIALOG_DIRTY ? "Save" : "OK", false);
    button(gui, 21, left + 16 + button_width, top + 104, button_width, "Cancel", false);
    if (dialog == DIALOG_DIRTY) {
        button(gui, 22, left + 24 + 2 * button_width, top + 104, button_width, "Discard", false);
    }
}

static void complete(tabos_gui_t* gui)
{
    const int pending = after;
    after             = AFTER_NONE;
    dialog            = DIALOG_NONE;
    if (pending == AFTER_CLOSE) {
        tabos_gui_close_reply(gui, true);
    } else if (pending == AFTER_OPEN) {
        dialog = DIALOG_OPEN;
    } else if (pending == AFTER_NEW) {
        memset(&document, 0, sizeof(document));
        strcpy(status, "New document");
        gui->ui.focus_cursor = 0U;
    }
}

static void save(tabos_gui_t* gui)
{
    if (editor_document_save(&document, path)) {
        strcpy(status, document.message);
        complete(gui);
    } else {
        strcpy(status, document.message);
        dialog = DIALOG_SAVE;
    }
}

static void request(tabos_gui_t* gui, int operation)
{
    after = operation;
    if (document.dirty) {
        dialog = DIALOG_DIRTY;
    } else {
        complete(gui);
    }
    tabos_gui_ui_cancel(&gui->ui);
    tabos_gui_invalidate(gui);
}

static void action(tabos_gui_t* gui, int id)
{
    if (id >= 30 && id < 35) {
        dialog = DIALOG_NONE;
        id     = id == 34 ? 21 : id - 29;
    }
    if (id == 1) {
        request(gui, AFTER_NEW);
    } else if (id == 2) {
        request(gui, AFTER_OPEN);
    } else if (id == 3 || id == 4) {
        after = AFTER_NONE;
        if (id == 3 && document.path[0] != '\0') {
            strcpy(path, document.path);
            save(gui);
        } else {
            dialog = DIALOG_SAVE;
        }
    } else if (id == 7) {
        dialog = DIALOG_MENU;
    } else if (id == 5) {
        document.dirty = true;
        strcpy(status, "Modified - unsaved changes");
    } else if (id == 20) {
        if (dialog == DIALOG_OPEN) {
            if (editor_document_open(&document, path)) {
                dialog = DIALOG_NONE;
                strcpy(status, "Opened");
                gui->ui.focus_cursor = 0U;
            } else {
                strcpy(status, "Open failed, binary data or file too large. Current text retained.");
            }
        } else if (dialog == DIALOG_DIRTY) {
            if (document.path[0] != '\0') {
                strcpy(path, document.path);
                save(gui);
            } else {
                dialog = DIALOG_SAVE;
            }
        } else {
            save(gui);
        }
    } else if (id == 21) {
        if (after == AFTER_CLOSE) {
            tabos_gui_close_reply(gui, false);
        }
        after  = AFTER_NONE;
        dialog = DIALOG_NONE;
    } else if (id == 22) {
        complete(gui);
    }
    tabos_gui_invalidate(gui);
}

static int closing(tabos_gui_t* gui)
{
    if (!document.dirty) {
        return 1;
    }
    request(gui, AFTER_CLOSE);
    return 0;
}

static void input(tabos_gui_t* gui, const tabos_gui_packet_t* packet, uint32_t kind)
{
    if (kind != TABOS_GUI_KEYBOARD || packet->data.keyboard.type != TABOS_INPUT_KEY_DOWN ||
        packet->data.keyboard.repeat) {
        return;
    }
    const tabos_input_event_t* event = &packet->data.keyboard;
    if (event->key == TABOS_KEY_ESCAPE && dialog != DIALOG_NONE) {
        action(gui, 21);
    } else if (dialog == DIALOG_NONE && (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
        if (event->key == TABOS_KEY_S) {
            action(gui, 3);
        } else if (event->key == TABOS_KEY_O) {
            action(gui, 2);
        } else if (event->key == TABOS_KEY_N) {
            action(gui, 1);
        }
    }
}

int main(void)
{
    tabos_gui_t gui = {.draw = draw, .action = action, .closing = closing, .input = input};
    if (tabos_gui_open(&gui, "Text editor") != 0) {
        return 1;
    }
    int result;
    do {
        result = tabos_gui_step(&gui, 100U);
    } while (result > 0);
    tabos_gui_shutdown(&gui);
    return result < 0 ? 1 : 0;
}
