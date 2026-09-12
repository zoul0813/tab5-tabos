#include <desktop/model.h>
#include <tabos/device.h>
#include <tabos/filesystem.h>
#include <tabos/process.h>
#include <tabos/runtime_time.h>
#include <tabos/session.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SEND_ADOPT     = 1U,
    SEND_CANCEL    = 2U,
    SEND_CONFIGURE = 4U,
    SEND_CLOSE     = 8U
};
enum {
    CAPTURE_CONTENT = 1,
    CAPTURE_CONTROL,
    CAPTURE_DRAG,
    CAPTURE_UI
};
typedef struct {
        int pid;
        tabos_wait_source_t source;
        bool forced;
} desktop_child_t;
typedef struct {
        bool active;
        uint32_t contact;
        tabos_device_id_t device;
        int slot;
        int mode;
        int control;
        tabos_gui_rect_t bounds;
} desktop_capture_t;
typedef struct {
        desktop_model_t model;
        tabos_gui_ui_t ui;
        tabos_graphics_t graphics;
        tabos_gui_canvas_t canvas;
        uint16_t* scratch;
        tabos_ipc_channel_t listener;
        tabos_pointer_stream_t pointer;
        tabos_device_subscription_t devices;
        tabos_ipc_channel_t incoming[TABOS_GUI_WINDOW_MAX];
        desktop_child_t children[TABOS_GUI_WINDOW_MAX];
        desktop_capture_t captures[TABOS_POINTER_MAX_CONTACTS];
        uint32_t outgoing[TABOS_GUI_WINDOW_MAX];
        bool running;
        bool quitting;
        int closing_slot;
        int modal;
        int force_slot;
        uint32_t pause_token;
        uint32_t shutdown_token;
        char launch_path[192];
        char message[192];
} desktop_t;

static desktop_t desktop;
static void cancel_window(int slot);
static const char* const launch_paths[] = {"T:/bin/files", "T:/bin/calculator", "T:/bin/editor", "T:/bin/canvas"};
static const char* const launch_names[] = {"    Files", "    Calculator", "    Text editor", "    Canvas"};

static void invalidate_all(void)
{
    desktop_model_damage(&desktop.model, (tabos_gui_rect_t) {0, 0, 1280, 720});
}

static void notify(const char* message)
{
    cancel_window(desktop.model.focus);
    tabos_gui_ui_cancel(&desktop.ui);
    (void) desktop_model_drag_end(&desktop.model, true);
    (void) snprintf(desktop.message, sizeof(desktop.message), "%s", message);
    desktop.modal = 1;
    invalidate_all();
}

static int find_pid(int pid)
{
    for (unsigned int index = 0U; index < TABOS_GUI_WINDOW_MAX; ++index) {
        if (desktop.model.windows[index].occupied && desktop.model.windows[index].pid == pid) {
            return (int) index;
        }
    }
    return -1;
}

static void cancel_window(int slot)
{
    if (slot < 0 || slot >= TABOS_GUI_WINDOW_MAX) {
        return;
    }
    desktop.outgoing[slot]                               |= SEND_CANCEL;
    desktop.model.windows[slot].cancelled_input_sequence  = desktop.model.windows[slot].input_sequence;
    for (size_t index = 0U; index < TABOS_POINTER_MAX_CONTACTS; ++index) {
        if (desktop.captures[index].active && desktop.captures[index].slot == slot) {
            desktop.captures[index].active = false;
        }
    }
}

static void focus_window(unsigned int slot)
{
    if (desktop.model.focus != (int) slot) {
        cancel_window(desktop.model.focus);
    }
    desktop_model_focus(&desktop.model, slot);
}

static void remove_window(unsigned int slot)
{
    desktop_window_t* window = &desktop.model.windows[slot];
    cancel_window((int) slot);
    if (window->channel > 0) {
        (void) tabos_ipc_close(window->channel);
    }
    desktop.outgoing[slot] = 0U;
    desktop_model_remove(&desktop.model, slot);
    if (desktop.closing_slot == (int) slot) {
        desktop.closing_slot = -1;
    }
}

static void flush_controls(void)
{
    const uint32_t flags[] = {SEND_ADOPT, SEND_CANCEL, SEND_CONFIGURE, SEND_CLOSE};
    const uint32_t kinds[] = {TABOS_GUI_ADOPT, TABOS_GUI_CANCEL_INPUT, TABOS_GUI_CONFIGURE, TABOS_GUI_CLOSE};
    for (unsigned int slot = 0U; slot < TABOS_GUI_WINDOW_MAX; ++slot) {
        desktop_window_t* window = &desktop.model.windows[slot];
        if (!window->occupied || window->channel <= 0) {
            continue;
        }
        for (unsigned int index = 0U; index < 4U; ++index) {
            if ((desktop.outgoing[slot] & flags[index]) == 0U) {
                continue;
            }
            tabos_gui_packet_t packet = {.version = TABOS_GUI_PROTOCOL_VERSION, .serial = window->serial};
            if (flags[index] == SEND_CANCEL) {
                packet.input_sequence = window->cancelled_input_sequence;
            }
            if (flags[index] == SEND_CONFIGURE) {
                packet.serial             = window->requested_serial;
                packet.data.window.width  = (uint32_t) window->proposed.width;
                packet.data.window.height = (uint32_t) window->proposed.height - TABOS_GUI_TITLE_HEIGHT;
            }
            if (tabos_gui_send(window->channel, kinds[index], &packet, true) != 0) {
                break;
            }
            desktop.outgoing[slot] &= ~flags[index];
        }
    }
}

static void request_close(unsigned int slot)
{
    desktop_window_t* window = &desktop.model.windows[slot];
    if (!window->occupied) {
        return;
    }
    focus_window(slot);
    if (window->closing || window->channel <= 0) {
        desktop.modal      = 2;
        desktop.force_slot = (int) slot;
        (void) snprintf(desktop.message, sizeof(desktop.message), "Force-close %s? Unsaved work will be lost.",
                        window->title);
        invalidate_all();
        return;
    }
    cancel_window((int) slot);
    window->closing         = true;
    desktop.outgoing[slot] |= SEND_CLOSE;
}

static void configure_window(unsigned int slot, tabos_gui_rect_t bounds, bool maximized)
{
    if (desktop_model_configure(&desktop.model, slot, bounds, maximized)) {
        cancel_window((int) slot);
        desktop.outgoing[slot] |= SEND_CONFIGURE;
    }
}

static void request_launch(const char* path)
{
    if (desktop.quitting || desktop.pause_token != 0U) {
        notify("Finish the current transition before launching another application.");
        return;
    }
    tabos_program_info_t program;
    if (tabos_program_query(path, &program) != 0) {
        notify("Cannot inspect this executable. Check its path and TabOS format.");
        return;
    }
    if ((program.flags & TABOS_PROGRAM_GUI) != 0U) {
        unsigned int child_slot = TABOS_GUI_WINDOW_MAX;
        for (unsigned int index = 0U; index < TABOS_GUI_WINDOW_MAX; ++index) {
            if (desktop.children[index].pid == 0) {
                child_slot = index;
                break;
            }
        }
        if (desktop.model.count >= TABOS_GUI_WINDOW_MAX || child_slot == TABOS_GUI_WINDOW_MAX) {
            notify("Window limit reached. Close an application before trying again.");
            return;
        }
        const char* const arguments[] = {path};
        const int pid                 = tabos_spawn(path, 1, arguments);
        if (pid <= 0) {
            notify("Launch failed. Close applications to free RAM, then retry.");
            return;
        }
        desktop.children[child_slot] = (desktop_child_t) {.pid = pid, .source = tabos_process_wait_source(pid)};
        const char* name             = strrchr(path, '/');
        cancel_window(desktop.model.focus);
        (void) desktop_model_add(&desktop.model, pid, name != NULL ? name + 1 : path);
    } else {
        const int token = tabos_session_control(TABOS_SESSION_BEGIN, 0U, 0U);
        if (token <= 0) {
            notify("Cannot pause this desktop session right now.");
            return;
        }
        cancel_window(desktop.model.focus);
        desktop.pause_token = (uint32_t) token;
        (void) snprintf(desktop.launch_path, sizeof(desktop.launch_path), "%s", path);
    }
}

static void open_pointer(void)
{
    if (desktop.pointer >= 0) {
        (void) tabos_pointer_close(desktop.pointer);
    }
    desktop.pointer = -1;
    tabos_device_info_t info;
    if (tabos_device_find(TABOS_DEVICE_NAME_TOUCH, &info) == 0) {
        desktop.pointer = tabos_pointer_open(info.id);
    }
}

static void advance_handoff(void)
{
    if (desktop.pause_token == 0U) {
        return;
    }
    const int pending = tabos_session_control(TABOS_SESSION_STATUS, desktop.pause_token, 0U);
    if (pending > 0) {
        return;
    }
    const uint32_t token = desktop.pause_token;
    desktop.pause_token  = 0U;
    if (pending < 0) {
        const int blocker = tabos_session_control(TABOS_SESSION_BLOCKER, 0U, 0U);
        (void) tabos_session_control(TABOS_SESSION_RESUME, token, 0U);
        char message[128];
        (void) snprintf(message, sizeof(message),
                        "Pause timed out. Process %d did not reach a safe point. Close it, then retry.", blocker);
        notify(message);
        return;
    }
    if (tabos_graphics_close(&desktop.graphics) != 0) {
        (void) tabos_session_control(TABOS_SESSION_RESUME, token, 0U);
        notify("Display handoff failed; desktop resumed.");
        return;
    }
    const char* const arguments[] = {desktop.launch_path};
    const int status              = tabos_exec(desktop.launch_path, 1, arguments);
    desktop.graphics              = (tabos_graphics_t) {0};
    if (tabos_graphics_open(&desktop.graphics) != 0) {
        desktop.running = false;
        return;
    }
    (void) tabos_graphics_set_overlays(&desktop.graphics, TABOS_GRAPHICS_OVERLAY_NONE);
    open_pointer();
    memset(desktop.captures, 0, sizeof(desktop.captures));
    (void) desktop_model_drag_end(&desktop.model, true);
    (void) tabos_session_control(TABOS_SESSION_RESUME, token, 0U);
    invalidate_all();
    if (status < 0) {
        notify("Fullscreen launch failed. GUI state retained. Close applications to free RAM, then retry.");
    }
}

static bool accept_frame(unsigned int slot, uint32_t kind, tabos_gui_packet_t* packet)
{
    tabos_surface_info_t info;
    if (tabos_surface_info(packet->data.window.surface, &info) != 0 || info.revision == 0U ||
        info.width != packet->data.window.width || info.height != packet->data.window.height ||
        !desktop_model_adopt(&desktop.model, slot, packet->serial, packet->data.window.surface, info.width,
                             info.height)) {
        return false;
    }
    if (kind == TABOS_GUI_HELLO) {
        packet->data.window.title[sizeof(packet->data.window.title) - 1U] = '\0';
        (void) snprintf(desktop.model.windows[slot].title, sizeof(desktop.model.windows[slot].title), "%s",
                        packet->data.window.title);
    }
    desktop.outgoing[slot] |= SEND_ADOPT;
    return true;
}

static void receive_clients(void)
{
    for (unsigned int index = 0U; index < TABOS_GUI_WINDOW_MAX; ++index) {
        if (desktop.incoming[index] <= 0) {
            desktop.incoming[index] = tabos_ipc_accept(desktop.listener);
        }
        if (desktop.incoming[index] <= 0) {
            continue;
        }
        uint32_t kind, sender;
        tabos_gui_packet_t packet;
        if (tabos_gui_receive(desktop.incoming[index], &kind, &packet, &sender) != 0) {
            if (errno != TABOS_EAGAIN) {
                (void) tabos_ipc_close(desktop.incoming[index]);
                desktop.incoming[index] = -1;
            }
            continue;
        }
        int slot = find_pid((int) sender);
        if (slot < 0 && kind == TABOS_GUI_HELLO) {
            cancel_window(desktop.model.focus);
            slot = desktop_model_add(&desktop.model, (int) sender, "Application");
        }
        if (slot >= 0 && kind == TABOS_GUI_HELLO && desktop.model.windows[slot].channel <= 0 &&
            accept_frame((unsigned int) slot, kind, &packet)) {
            desktop.model.windows[slot].channel = desktop.incoming[index];
        } else {
            (void) tabos_ipc_close(desktop.incoming[index]);
        }
        desktop.incoming[index] = -1;
    }
    for (unsigned int slot = 0U; slot < TABOS_GUI_WINDOW_MAX; ++slot) {
        desktop_window_t* window = &desktop.model.windows[slot];
        if (!window->occupied || window->channel <= 0) {
            continue;
        }
        for (unsigned int count = 0U; count < 8U; ++count) {
            uint32_t kind, sender;
            tabos_gui_packet_t packet;
            if (tabos_gui_receive(window->channel, &kind, &packet, &sender) != 0) {
                if (errno != TABOS_EAGAIN) {
                    remove_window(slot);
                }
                break;
            }
            if (sender != (uint32_t) window->pid) {
                continue;
            }
            if (kind == TABOS_GUI_FRAME) {
                (void) accept_frame(slot, kind, &packet);
            } else if (kind == TABOS_GUI_LAUNCH) {
                packet.data.path[sizeof(packet.data.path) - 1U] = '\0';
                request_launch(packet.data.path);
            } else if (kind == TABOS_GUI_CLOSE_CANCELLED) {
                window->closing      = false;
                desktop.closing_slot = -1;
                if (desktop.quitting) {
                    (void) tabos_session_control(TABOS_SESSION_RESUME, desktop.shutdown_token, 0U);
                    desktop.quitting = false;
                }
            } else if (kind == TABOS_GUI_ERROR) {
                desktop_model_resize_failed(&desktop.model, slot, packet.serial);
                notify("Application could not complete drawing or resize. Previous window image retained.");
            }
        }
    }
}

static void reap_children(void)
{
    for (unsigned int index = 0U; index < TABOS_GUI_WINDOW_MAX; ++index) {
        desktop_child_t* child = &desktop.children[index];
        if (child->pid <= 0) {
            continue;
        }
        tabos_wait_item_t item = {.source = child->source, .events = TABOS_WAIT_READABLE};
        if (tabos_wait(&item, 1U, 0U) <= 0) {
            continue;
        }
        int status     = 0;
        const int slot = find_pid(child->pid);
        if (slot >= 0) {
            remove_window((unsigned int) slot);
        }
        (void) tabos_waitpid(child->pid, &status);
        const bool forced = child->forced;
        *child            = (desktop_child_t) {0};
        if (status != 0 && !desktop.quitting && !forced) {
            notify("An application exited with an error. Other windows remain available.");
        }
    }
}

static void begin_exit(void)
{
    if (desktop.pause_token != 0U) {
        return;
    }
    const int token = tabos_session_control(TABOS_SESSION_SHUTDOWN_BEGIN, 0U, 0U);
    if (token <= 0) {
        notify("Cannot close the desktop during this transition.");
        return;
    }
    desktop.shutdown_token = (uint32_t) token;
    desktop.quitting       = true;
    desktop.closing_slot   = -1;
}

static void advance_exit(void)
{
    if (!desktop.quitting || desktop.closing_slot >= 0 || desktop.modal != 0) {
        return;
    }
    if (desktop.model.count == 0U) {
        desktop.running = false;
        return;
    }
    const unsigned int slot = desktop.model.stack[desktop.model.count - 1U];
    desktop.closing_slot    = (int) slot;
    request_close(slot);
}

static void ui_action(int id)
{
    if (id == 100) {
        request_launch(launch_paths[0]);
    } else if (id == 101) {
        begin_exit();
    } else if (id >= 1000 && id < 1000 + TABOS_GUI_WINDOW_MAX) {
        focus_window((unsigned int) (id - 1000));
    } else if (id >= 2000 && id < 2004) {
        request_launch(launch_paths[id - 2000]);
    } else if (id == 3000) {
        if (desktop.modal == 2 && desktop.quitting) {
            desktop.quitting     = false;
            desktop.closing_slot = -1;
            (void) tabos_session_control(TABOS_SESSION_RESUME, desktop.shutdown_token, 0U);
        }
        desktop.modal = 0;
        invalidate_all();
    } else if (id == 3001 && desktop.modal == 2) {
        const int slot = desktop.force_slot;
        if (slot >= 0 && desktop.model.windows[slot].occupied) {
            const int pid = desktop.model.windows[slot].pid;
            if (tabos_session_control(TABOS_SESSION_FORCE_CLOSE, 0U, (uint32_t) pid) == 0) {
                for (size_t index = 0U; index < TABOS_GUI_WINDOW_MAX; ++index) {
                    if (desktop.children[index].pid == pid) {
                        desktop.children[index].forced = true;
                    }
                }
            }
        }
        desktop.modal = 0;
        invalidate_all();
    }
}

static void add_button(int id, tabos_gui_rect_t bounds, const char* label, bool disabled)
{
    (void) tabos_gui_ui_add(
        &desktop.ui, (tabos_gui_widget_t) {
                         .id = id, .kind = TABOS_GUI_BUTTON, .bounds = bounds, .label = label, .disabled = disabled});
}

static void draw_ui(void)
{
    tabos_gui_ui_begin(&desktop.ui);
    const bool blocked = desktop.modal != 0;
    add_button(100, (tabos_gui_rect_t) {8, 652, 120, 56}, "Apps", blocked);
    add_button(101, (tabos_gui_rect_t) {1152, 652, 120, 56}, "Exit", blocked);
    for (unsigned int slot = 0U; slot < TABOS_GUI_WINDOW_MAX; ++slot) {
        if (desktop.model.windows[slot].occupied) {
            add_button(1000 + (int) slot, (tabos_gui_rect_t) {136 + (int32_t) slot * 126, 652, 120, 56},
                       desktop.model.windows[slot].title, blocked);
        }
    }
    if (desktop.model.focus < 0) {
        tabos_gui_text(&desktop.canvas, 64, 52, "TabOS Desktop", TABOS_GUI_LIGHT, 3U);
        for (int index = 0; index < 4; ++index) {
            add_button(2000 + index, (tabos_gui_rect_t) {64 + index * 288, 132, 264, 112}, launch_names[index],
                       blocked);
        }
        tabos_gui_text(&desktop.canvas, 64, 296, "Open an app. Your windows stay here while fullscreen programs run.",
                       TABOS_GUI_LIGHT, 2U);
    }
    if (desktop.modal != 0) {
        (void) tabos_gui_ui_add(
            &desktop.ui, (tabos_gui_widget_t) {
                             .id = 2900, .kind = TABOS_GUI_PANEL, .bounds = {120, 220, 1040, 280},
                                     .disabled = true
        });
        add_button(3000, (tabos_gui_rect_t) {432, 420, 192, 56}, desktop.modal == 2 ? "Cancel" : "OK", false);
        if (desktop.modal == 2) {
            add_button(3001, (tabos_gui_rect_t) {656, 420, 192, 56}, "Force close", false);
        }
    }
    tabos_gui_ui_draw(&desktop.ui, &desktop.canvas);
    if (desktop.model.focus < 0 && desktop.modal == 0) {
        for (int index = 0; index < 4; ++index) {
            const int x = 80 + index * 288, y = 164;
            if (index == 0) {
                tabos_gui_fill(&desktop.canvas, (tabos_gui_rect_t) {x, y, 24, 12}, 0xf5c4U);
                tabos_gui_fill(&desktop.canvas, (tabos_gui_rect_t) {x, y + 8, 48, 36}, 0xf6e8U);
                tabos_gui_fill(&desktop.canvas, (tabos_gui_rect_t) {x + 4, y + 16, 40, 2}, TABOS_GUI_LIGHT);
            } else if (index == 1) {
                tabos_gui_fill(&desktop.canvas, (tabos_gui_rect_t) {x, y, 48, 48}, TABOS_GUI_SHADOW);
                tabos_gui_fill(&desktop.canvas, (tabos_gui_rect_t) {x + 4, y + 4, 40, 12}, 0xaf56U);
                for (int key = 0; key < 6; ++key) {
                    tabos_gui_fill(&desktop.canvas,
                                   (tabos_gui_rect_t) {x + 6 + (key % 3) * 14, y + 22 + (key / 3) * 14, 8, 8},
                                   TABOS_GUI_LIGHT);
                }
            } else if (index == 2) {
                tabos_gui_fill(&desktop.canvas, (tabos_gui_rect_t) {x + 4, y, 40, 48}, TABOS_GUI_LIGHT);
                for (int line = 0; line < 4; ++line) {
                    tabos_gui_fill(&desktop.canvas, (tabos_gui_rect_t) {x + 10, y + 8 + line * 9, 28, 2},
                                   TABOS_GUI_ACCENT);
                }
            } else {
                tabos_gui_fill(&desktop.canvas, (tabos_gui_rect_t) {x, y, 48, 48}, TABOS_GUI_LIGHT);
                const uint16_t colors[] = {0xe986U, 0x34e8U, TABOS_GUI_ACCENT, 0xf6e8U};
                for (int paint = 0; paint < 4; ++paint) {
                    tabos_gui_fill(&desktop.canvas,
                                   (tabos_gui_rect_t) {x + 6 + (paint % 2) * 20, y + 6 + (paint / 2) * 20, 16, 16},
                                   colors[paint]);
                }
            }
        }
    }
    if (desktop.modal != 0) {
        size_t position = 0U;
        for (int line = 0; line < 4 && desktop.message[position] != '\0'; ++line) {
            char text[61];
            size_t used = 0U;
            while (used < 60U && desktop.message[position] != '\0') {
                text[used++] = desktop.message[position++];
            }
            text[used] = '\0';
            tabos_gui_text(&desktop.canvas, 152, 252 + line * 30, text, TABOS_GUI_INK, 2U);
        }
    }
}

static void route_input(unsigned int slot, uint32_t kind, tabos_gui_packet_t* packet)
{
    desktop_window_t* window = &desktop.model.windows[slot];
    if (window->channel <= 0 || window->pending || desktop.pause_token != 0U) {
        return;
    }
    packet->version = TABOS_GUI_PROTOCOL_VERSION;
    packet->serial  = window->serial;
    if (window->input_sequence == UINT32_MAX) {
        cancel_window((int) slot);
        return;
    }
    packet->input_sequence = ++window->input_sequence;
    if (tabos_gui_send(window->channel, kind, packet, false) != 0) {
        if (kind == TABOS_GUI_POINTER && packet->data.pointer.type == TABOS_POINTER_MOVE) {
            return;
        }
        cancel_window((int) slot);
        notify("Application input queue is full. Input cancelled; try again.");
    }
}

static void pointer_event(tabos_pointer_event_t event)
{
    if (event.type == TABOS_POINTER_HOVER || event.type == TABOS_POINTER_WHEEL) {
        const int slot = desktop_model_hit(&desktop.model, event.x, event.y);
        if (desktop.modal == 0 && slot >= 0 && event.y >= desktop.model.windows[slot].bounds.y + 48) {
            tabos_gui_packet_t packet  = {.data.pointer = event};
            packet.data.pointer.x     -= desktop.model.windows[slot].bounds.x;
            packet.data.pointer.y     -= desktop.model.windows[slot].bounds.y + 48;
            route_input((unsigned int) slot, TABOS_GUI_POINTER, &packet);
        }
        return;
    }
    desktop_capture_t* capture = NULL;
    for (size_t index = 0U; index < TABOS_POINTER_MAX_CONTACTS; ++index) {
        if (desktop.captures[index].active && desktop.captures[index].contact == event.contact_id &&
            desktop.captures[index].device == event.device_id) {
            capture = &desktop.captures[index];
            break;
        }
    }
    if (event.type == TABOS_POINTER_DOWN && capture == NULL) {
        for (size_t index = 0U; index < TABOS_POINTER_MAX_CONTACTS; ++index) {
            if (!desktop.captures[index].active) {
                capture = &desktop.captures[index];
                break;
            }
        }
        if (capture == NULL) {
            return;
        }
        *capture =
            (desktop_capture_t) {.active = true, .contact = event.contact_id, .device = event.device_id, .slot = -1};
        const int slot = desktop_model_hit(&desktop.model, event.x, event.y);
        if (desktop.modal != 0 || slot < 0) {
            capture->mode = CAPTURE_UI;
        } else {
            focus_window((unsigned int) slot);
            capture->slot              = slot;
            desktop_window_t* window   = &desktop.model.windows[slot];
            const tabos_gui_rect_t box = window->bounds;
            if (event.y < box.y + 48) {
                const int control = (box.x + box.width - event.x - 1) / 48;
                if (control < 3) {
                    capture->mode    = CAPTURE_CONTROL;
                    capture->control = control;
                    capture->bounds  = (tabos_gui_rect_t) {box.x + box.width - (control + 1) * 48, box.y, 48, 48};
                } else {
                    if (desktop.model.dragging) {
                        capture->active = false;
                        return;
                    }
                    capture->mode = CAPTURE_DRAG;
                    desktop_model_drag_begin(&desktop.model, (unsigned int) slot, event.x, event.y, false);
                }
            } else if (!window->maximized && event.x >= box.x + box.width - 44 && event.y >= box.y + box.height - 44) {
                if (desktop.model.dragging) {
                    capture->active = false;
                    return;
                }
                capture->mode = CAPTURE_DRAG;
                desktop_model_drag_begin(&desktop.model, (unsigned int) slot, event.x, event.y, true);
            } else {
                capture->mode = CAPTURE_CONTENT;
            }
        }
    }
    if (capture == NULL || !capture->active) {
        return;
    }
    const int slot = capture->slot;
    if (capture->mode == CAPTURE_UI) {
        const int action = tabos_gui_ui_pointer(&desktop.ui, &event);
        if (desktop.ui.changed) {
            invalidate_all();
        }
        if (action > 0) {
            ui_action(action);
        }
    } else if (capture->mode == CAPTURE_CONTENT && slot >= 0) {
        tabos_gui_packet_t packet  = {.data.pointer = event};
        packet.data.pointer.x     -= desktop.model.windows[slot].bounds.x;
        packet.data.pointer.y     -= desktop.model.windows[slot].bounds.y + 48;
        route_input((unsigned int) slot, TABOS_GUI_POINTER, &packet);
    } else if (capture->mode == CAPTURE_DRAG) {
        if (event.type == TABOS_POINTER_MOVE) {
            desktop_model_drag_move(&desktop.model, event.x, event.y);
        }
        if (event.type == TABOS_POINTER_UP || event.type == TABOS_POINTER_CANCEL) {
            if (desktop_model_drag_end(&desktop.model, event.type == TABOS_POINTER_CANCEL) && slot >= 0) {
                cancel_window(slot);
                desktop.outgoing[slot] |= SEND_CONFIGURE;
            }
        }
    } else if (capture->mode == CAPTURE_CONTROL && event.type == TABOS_POINTER_UP && slot >= 0 &&
               tabos_gui_contains(capture->bounds, event.x, event.y)) {
        desktop_window_t* window = &desktop.model.windows[slot];
        if (capture->control == 0) {
            request_close((unsigned int) slot);
        } else if (capture->control == 1) {
            configure_window((unsigned int) slot,
                             window->maximized ? window->restored : (tabos_gui_rect_t) {0, 0, 1280, 640},
                             !window->maximized);
        } else {
            cancel_window(slot);
            desktop_model_minimize(&desktop.model, (unsigned int) slot);
        }
    }
    if (event.type == TABOS_POINTER_UP || event.type == TABOS_POINTER_CANCEL) {
        capture->active = false;
    }
}

static void keyboard_event(const tabos_input_event_t* event)
{
    if ((event->flags & TABOS_INPUT_EVENT_OVERFLOW) != 0U) {
        cancel_window(desktop.model.focus);
        tabos_gui_ui_cancel(&desktop.ui);
    }
    if (desktop.modal != 0 || desktop.model.focus < 0) {
        const int action = tabos_gui_ui_keyboard(&desktop.ui, event);
        if (desktop.ui.changed) {
            invalidate_all();
        }
        if (action > 0) {
            ui_action(action);
        }
        return;
    }
    if (event->type == TABOS_INPUT_KEY_DOWN && !event->repeat && (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
        if (event->key == TABOS_KEY_TAB && desktop.model.count > 0U) {
            focus_window(desktop.model.stack[0]);
            return;
        }
        if (event->key == TABOS_KEY_Q) {
            request_close((unsigned int) desktop.model.focus);
            return;
        }
        if (event->key == TABOS_KEY_ESCAPE) {
            request_launch(launch_paths[0]);
            return;
        }
    }
    if (event->type == TABOS_INPUT_TEXT &&
        (event->modifiers & (TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_ALT | TABOS_MODIFIER_GUI)) != 0U) {
        return;
    }
    tabos_gui_packet_t packet = {.data.keyboard = *event};
    route_input((unsigned int) desktop.model.focus, TABOS_GUI_KEYBOARD, &packet);
}

static int read_surface(void* user, tabos_surface_t surface, uint32_t width, uint32_t height, uint16_t* pixels)
{
    (void) user;
    return tabos_surface_read(surface, 0U, 0U, width, height, pixels);
}

static bool present(void)
{
    if (!desktop.model.damaged) {
        return true;
    }
    (void) desktop_render(&desktop.model, &desktop.canvas, desktop.scratch, 1280U * 592U, read_surface, NULL);
    draw_ui();
    const tabos_gui_rect_t damage            = desktop.model.damage;
    const tabos_graphics_blit_options_t blit = {
        .pixels        = desktop.canvas.pixels,
        .bitmap_width  = 1280U,
        .bitmap_height = 720U,
        .source = {     .x = damage.x,.y = damage.y,.width = (uint32_t) damage.width,.height = (uint32_t) damage.height                  },
        .destination = {.x      = damage.x,
                   .y      = damage.y,
                   .width  = (uint32_t) damage.width,
                   .height = (uint32_t) damage.height},
        .opacity     = 255U
    };
    if (tabos_graphics_blit_ex(&desktop.graphics, &blit) != 0 || tabos_graphics_present(&desktop.graphics) != 0) {
        return false;
    }
    desktop.model.damaged = false;
    return true;
}

static void idle_wait(void)
{
    tabos_wait_item_t items[TABOS_WAIT_MAX];
    uint32_t count = 0U;
    items[count++] =
        (tabos_wait_item_t) {.source = tabos_ipc_wait_source(desktop.listener), .events = TABOS_WAIT_READABLE};
    if (desktop.devices >= 0) {
        items[count++] = (tabos_wait_item_t) {.source = tabos_device_subscription_wait_source(desktop.devices),
                                              .events = TABOS_WAIT_READABLE};
    }
    const tabos_wait_source_t keyboard = tabos_input_wait_source();
    if (keyboard >= 0) {
        items[count++] = (tabos_wait_item_t) {.source = keyboard, .events = TABOS_WAIT_READABLE};
    }
    if (desktop.pointer >= 0) {
        items[count++] =
            (tabos_wait_item_t) {.source = tabos_pointer_wait_source(desktop.pointer), .events = TABOS_WAIT_READABLE};
    }
    for (unsigned int slot = 0U; slot < TABOS_GUI_WINDOW_MAX && count < TABOS_WAIT_MAX; ++slot) {
        if (desktop.model.windows[slot].occupied && desktop.model.windows[slot].channel > 0) {
            items[count++] = (tabos_wait_item_t) {.source = tabos_ipc_wait_source(desktop.model.windows[slot].channel),
                                                  .events = TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP};
        }
    }
    for (unsigned int index = 0U; index < TABOS_GUI_WINDOW_MAX && count < TABOS_WAIT_MAX; ++index) {
        if (desktop.incoming[index] > 0) {
            items[count++] = (tabos_wait_item_t) {.source = tabos_ipc_wait_source(desktop.incoming[index]),
                                                  .events = TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP};
        }
    }
    (void) tabos_wait(items, count, 100U);
}

int main(void)
{
    desktop_model_init(&desktop.model);
    desktop.pointer      = -1;
    desktop.devices      = -1;
    desktop.closing_slot = -1;
    desktop.force_slot   = -1;
    if (tabos_session_open() <= 0) {
        fprintf(stderr, "desktop: launch from the shell\n");
        return 1;
    }
    desktop.listener      = tabos_ipc_listen();
    desktop.canvas        = (tabos_gui_canvas_t) {.width = 1280U, .height = 720U};
    desktop.canvas.pixels = calloc(1280U * 720U, sizeof(uint16_t));
    desktop.scratch       = malloc(1280U * 592U * sizeof(uint16_t));
    if (desktop.listener <= 0 || desktop.canvas.pixels == NULL || desktop.scratch == NULL ||
        tabos_graphics_open(&desktop.graphics) != 0) {
        fprintf(stderr, "desktop: could not allocate display resources\n");
        free(desktop.canvas.pixels);
        free(desktop.scratch);
        return 1;
    }
    if (desktop.graphics.width != 1280U || desktop.graphics.height != 720U) {
        (void) tabos_graphics_close(&desktop.graphics);
        free(desktop.canvas.pixels);
        free(desktop.scratch);
        return 1;
    }
    (void) tabos_graphics_set_overlays(&desktop.graphics, TABOS_GRAPHICS_OVERLAY_NONE);
    open_pointer();
    desktop.devices = tabos_device_subscribe();
    desktop.running = true;
    while (desktop.running) {
        tabos_device_event_t device;
        for (unsigned int count = 0U;
             count < 16U && desktop.devices >= 0 && tabos_device_event_read(desktop.devices, &device) == 0; ++count) {
            if ((device.flags & TABOS_DEVICE_EVENT_OVERFLOW) != 0U ||
                device.device.device_class == TABOS_DEVICE_CLASS_KEYBOARD ||
                device.device.device_class == TABOS_DEVICE_CLASS_POINTER) {
                cancel_window(desktop.model.focus);
                tabos_gui_ui_cancel(&desktop.ui);
                (void) desktop_model_drag_end(&desktop.model, true);
                memset(desktop.captures, 0, sizeof(desktop.captures));
                if (device.device.device_class == TABOS_DEVICE_CLASS_POINTER) {
                    open_pointer();
                }
                invalidate_all();
            }
        }
        receive_clients();
        reap_children();
        flush_controls();
        advance_handoff();
        advance_exit();
        tabos_pointer_event_t pointer;
        for (unsigned int count = 0U;
             count < 32U && desktop.pointer >= 0 && tabos_pointer_read(desktop.pointer, &pointer) == 0; ++count) {
            pointer_event(pointer);
        }
        tabos_input_event_t key;
        for (unsigned int count = 0U; count < 32U && tabos_input_poll(&key); ++count) {
            keyboard_event(&key);
        }
        flush_controls();
        if (!desktop.running || !present()) {
            break;
        }
        idle_wait();
    }
    if (desktop.pointer >= 0) {
        (void) tabos_pointer_close(desktop.pointer);
    }
    (void) tabos_graphics_close(&desktop.graphics);
    (void) tabos_ipc_close(desktop.listener);
    if (desktop.devices >= 0) {
        (void) tabos_device_subscription_close(desktop.devices);
    }
    free(desktop.canvas.pixels);
    free(desktop.scratch);
    return 0;
}
