#include <calculator/calculate.h>
#include <tabos/gui.h>
#include <stdio.h>
#include <string.h>

static char expression[128];
static char status[80]          = "Enter arithmetic; multiplication and division first.";
static const char* const keys[] = {"7", "8", "9", "/", "4", "5", "6", "*", "1", "2", "3", "-", "0", ".", "=", "+"};

static void draw(tabos_gui_t* gui)
{
    const int width  = (int) gui->canvas.width;
    const int height = (int) gui->canvas.height;
    (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                          .id       = 1,
                                          .kind     = TABOS_GUI_TEXT_FIELD,
                                          .bounds   = {12, 12, width - 24, 48},
                                          .text     = expression,
                                          .capacity = sizeof(expression)
    });
    (void) tabos_gui_ui_add(&gui->ui,
                            (tabos_gui_widget_t) {
                                .id = 2, .kind = TABOS_GUI_BUTTON, .bounds = {12, 68, 96, 44},
                                        .label = "Clear"
    });
    (void) tabos_gui_ui_add(&gui->ui,
                            (tabos_gui_widget_t) {
                                .id = 3, .kind = TABOS_GUI_BUTTON, .bounds = {120, 68, 96, 44},
                                        .label = "Erase"
    });
    const int key_width  = (width - 40) / 4;
    const int key_height = (height - 164) / 4 > 44 ? (height - 164) / 4 : 44;
    for (int index = 0; index < 16; ++index) {
        (void) tabos_gui_ui_add(&gui->ui, (tabos_gui_widget_t) {
                                              .id     = 10 + index,
                                              .kind   = TABOS_GUI_BUTTON,
                                              .bounds = {12 + (index % 4) * (key_width + 4),
                                                         120 + (index / 4) * key_height, key_width, key_height - 2},
                                              .label  = keys[index]
        });
    }
    (void) tabos_gui_ui_add(
        &gui->ui, (tabos_gui_widget_t) {
                      .id = 40, .kind = TABOS_GUI_LABEL, .bounds = {12, height - 36, width - 24, 32},
                              .label = status
    });
}

static void evaluate(void)
{
    char result[128];
    if (calculator_evaluate(expression, result, sizeof(result))) {
        strcpy(expression, result);
        strcpy(status, "Result");
    } else {
        strcpy(status, "Invalid arithmetic, division by zero or overflow.");
    }
}

static void action(tabos_gui_t* gui, int id)
{
    const size_t length = strlen(expression);
    if (id == 2) {
        expression[0] = '\0';
    } else if (id == 3 && length > 0U) {
        expression[length - 1U] = '\0';
    } else if (id >= 10 && id < 26) {
        if (id == 24) {
            evaluate();
        } else if (length + 1U < sizeof(expression)) {
            expression[length]      = keys[id - 10][0];
            expression[length + 1U] = '\0';
        }
    }
    gui->ui.focus_cursor = strlen(expression);
    for (size_t index = 0U; index < gui->ui.count; ++index) {
        if (gui->ui.widgets[index].id == 1) {
            gui->ui.widgets[index].cursor = gui->ui.focus_cursor;
        }
    }
    tabos_gui_invalidate(gui);
}

static void input(tabos_gui_t* gui, const tabos_gui_packet_t* packet, uint32_t kind)
{
    if (kind == TABOS_GUI_KEYBOARD && packet->data.keyboard.type == TABOS_INPUT_KEY_DOWN &&
        packet->data.keyboard.key == TABOS_KEY_ENTER && gui->ui.focus == 1) {
        evaluate();
        tabos_gui_invalidate(gui);
    }
}

int main(void)
{
    tabos_gui_t gui = {.draw = draw, .action = action, .input = input};
    if (tabos_gui_open(&gui, "Calculator") != 0) {
        return 1;
    }
    int result;
    do {
        result = tabos_gui_step(&gui, 100U);
    } while (result > 0);
    tabos_gui_shutdown(&gui);
    return result < 0 ? 1 : 0;
}
