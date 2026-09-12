#include <tabos/gui.h>
#include <tabos/session.h>
#include <tabos/filesystem.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int tabos_gui_send(tabos_ipc_channel_t channel, uint32_t kind, const tabos_gui_packet_t* packet, bool control)
{
    if (packet == NULL) {
        errno = EINVAL;
        return -1;
    }
    tabos_ipc_message_t message = {.kind = kind, .size = sizeof(*packet)};
    memcpy(message.data, packet, sizeof(*packet));
    return tabos_ipc_send(channel, &message, control);
}

int tabos_gui_receive(tabos_ipc_channel_t channel, uint32_t* kind, tabos_gui_packet_t* packet, uint32_t* sender)
{
    if (kind == NULL || packet == NULL || sender == NULL) {
        errno = EINVAL;
        return -1;
    }
    tabos_ipc_message_t message;
    if (tabos_ipc_receive(channel, &message) != 0) {
        return -1;
    }
    if (message.size != sizeof(*packet)) {
        errno = EINVAL;
        return -1;
    }
    memcpy(packet, message.data, sizeof(*packet));
    if (packet->version != TABOS_GUI_PROTOCOL_VERSION) {
        errno = EINVAL;
        return -1;
    }
    *kind   = message.kind;
    *sender = message.sender_pid;
    return 0;
}

static tabos_gui_packet_t window_packet(const tabos_gui_t* gui)
{
    tabos_gui_packet_t packet  = {.version = TABOS_GUI_PROTOCOL_VERSION, .serial = gui->serial};
    packet.data.window.surface = gui->surface;
    packet.data.window.width   = gui->canvas.width;
    packet.data.window.height  = gui->canvas.height;
    memcpy(packet.data.window.title, gui->title, sizeof(gui->title));
    return packet;
}

static void draw_window(tabos_gui_t* gui)
{
    tabos_gui_fill(&gui->canvas, (tabos_gui_rect_t) {0, 0, (int32_t) gui->canvas.width, (int32_t) gui->canvas.height},
                   TABOS_GUI_FACE);
    tabos_gui_ui_begin(&gui->ui);
    if (gui->draw != NULL) {
        gui->draw(gui);
    }
    tabos_gui_ui_draw(&gui->ui, &gui->canvas);
}

static int publish(tabos_gui_t* gui)
{
    draw_window(gui);
    if (tabos_surface_upload(gui->surface, 0U, 0U, gui->canvas.width, gui->canvas.height, gui->canvas.pixels) != 0 ||
        tabos_surface_commit(gui->surface) != 0) {
        gui->last_error = errno;
        gui->dirty      = false;
        return -1;
    }
    gui->dirty         = false;
    gui->pending_frame = true;
    return 0;
}

static void report_error(tabos_gui_t* gui, uint32_t serial, int error)
{
    gui->last_error           = error;
    tabos_gui_packet_t packet = {.version = TABOS_GUI_PROTOCOL_VERSION, .serial = serial};
    packet.data.error         = error;
    gui->error_serial         = serial;
    gui->pending_error        = error;
    gui->error_pending        = tabos_gui_send(gui->channel, TABOS_GUI_ERROR, &packet, true) != 0;
}

static void resize_window(tabos_gui_t* gui, const tabos_gui_packet_t* packet)
{
    const uint32_t width  = packet->data.window.width;
    const uint32_t height = packet->data.window.height;
    if (packet->serial <= gui->serial || width == 0U || width > TABOS_GUI_SCREEN_WIDTH || height == 0U ||
        height > TABOS_GUI_CONTENT_HEIGHT || gui->retired > 0) {
        report_error(gui, packet->serial, EINVAL);
        return;
    }
    uint16_t* pixels                  = calloc((size_t) width * height, sizeof(*pixels));
    const tabos_surface_t replacement = pixels != NULL ? tabos_surface_create(width, height) : -1;
    if (pixels == NULL || replacement < 0) {
        free(pixels);
        report_error(gui, packet->serial, ENOMEM);
        return;
    }
    const tabos_gui_canvas_t old_canvas = gui->canvas;
    const tabos_surface_t old_surface   = gui->surface;
    gui->canvas                         = (tabos_gui_canvas_t) {.pixels = pixels, .width = width, .height = height};
    gui->surface                        = replacement;
    if (tabos_surface_grant(replacement, gui->channel) != 0 || publish(gui) != 0) {
        const int error = errno;
        (void) tabos_surface_release(replacement);
        free(pixels);
        gui->surface = old_surface;
        gui->canvas  = old_canvas;
        draw_window(gui);
        report_error(gui, packet->serial, error);
        return;
    }
    free(old_canvas.pixels);
    gui->retired = old_surface;
    gui->serial  = packet->serial;
    tabos_gui_ui_cancel(&gui->ui);
}

int tabos_gui_open(tabos_gui_t* gui, const char* title)
{
    if (gui == NULL || title == NULL || gui->connected) {
        errno = EINVAL;
        return -1;
    }
    gui->surface = -1;
    gui->retired = -1;
    gui->channel = tabos_ipc_connect();
    if (gui->channel < 0) {
        return -1;
    }
    gui->connected     = true;
    gui->canvas        = (tabos_gui_canvas_t) {.width = TABOS_GUI_SCREEN_WIDTH, .height = TABOS_GUI_CONTENT_HEIGHT};
    gui->canvas.pixels = calloc((size_t) gui->canvas.width * gui->canvas.height, sizeof(uint16_t));
    if (gui->canvas.pixels != NULL) {
        gui->surface = tabos_surface_create(gui->canvas.width, gui->canvas.height);
    }
    if (gui->canvas.pixels == NULL || gui->surface < 0 || tabos_surface_grant(gui->surface, gui->channel) != 0) {
        const int error = errno;
        tabos_gui_shutdown(gui);
        errno = error;
        return -1;
    }
    size_t length = strlen(title);
    if (length >= sizeof(gui->title)) {
        length = sizeof(gui->title) - 1U;
    }
    memcpy(gui->title, title, length);
    gui->title[length] = '\0';
    gui->running       = true;
    gui->dirty         = true;
    gui->serial        = 0U;
    gui->advertised    = false;
    return 0;
}

void tabos_gui_invalidate(tabos_gui_t* gui)
{
    if (gui != NULL) {
        gui->dirty = true;
    }
}

void tabos_gui_close_reply(tabos_gui_t* gui, bool allow)
{
    if (gui == NULL || !gui->close_pending) {
        return;
    }
    if (allow) {
        gui->running = false;
    } else {
        const tabos_gui_packet_t packet = window_packet(gui);
        if (tabos_gui_send(gui->channel, TABOS_GUI_CLOSE_CANCELLED, &packet, true) != 0) {
            gui->last_error           = errno;
            gui->cancel_reply_pending = true;
        }
    }
    gui->close_pending = false;
}

void tabos_gui_shutdown(tabos_gui_t* gui)
{
    if (gui == NULL) {
        return;
    }
    if (gui->connected) {
        if (gui->surface > 0) {
            (void) tabos_surface_release(gui->surface);
        }
        if (gui->retired > 0) {
            (void) tabos_surface_release(gui->retired);
        }
        (void) tabos_ipc_close(gui->channel);
    }
    free(gui->canvas.pixels);
    gui->canvas.pixels = NULL;
    gui->connected     = false;
    gui->running       = false;
    gui->surface       = -1;
    gui->retired       = -1;
}

int tabos_gui_launch(tabos_gui_t* gui, const char* path)
{
    if (gui == NULL || path == NULL || strlen(path) >= sizeof(((tabos_gui_packet_t*) 0)->data.path)) {
        errno = EINVAL;
        return -1;
    }
    tabos_gui_packet_t packet = {.version = TABOS_GUI_PROTOCOL_VERSION, .serial = gui->serial};
    memcpy(packet.data.path, path, strlen(path) + 1U);
    return tabos_gui_send(gui->channel, TABOS_GUI_LAUNCH, &packet, false);
}

int tabos_gui_step(tabos_gui_t* gui, uint32_t timeout_ms)
{
    if (gui == NULL || !gui->connected) {
        errno = EINVAL;
        return -1;
    }
    if (!gui->running) {
        return 0;
    }
    const int token = tabos_session_control(TABOS_SESSION_CHECKPOINT, 0U, 0U);
    if (token > 0 && (gui->pause == NULL || gui->pause(gui))) {
        tabos_gui_ui_cancel(&gui->ui);
        const int result = tabos_session_control(TABOS_SESSION_ACKNOWLEDGE, (uint32_t) token, 0U);
        if (result < 0) {
            gui->last_error = -result;
            return -1;
        }
        if (gui->resume != NULL) {
            gui->resume(gui);
        }
        gui->dirty = true;
    }
    for (unsigned int index = 0U; index < 16U; ++index) {
        tabos_gui_packet_t packet;
        uint32_t kind, sender;
        if (tabos_gui_receive(gui->channel, &kind, &packet, &sender) != 0) {
            if (errno == TABOS_EAGAIN) {
                break;
            }
            gui->last_error = errno;
            return -1;
        }
        (void) sender;
        if (kind == TABOS_GUI_CONFIGURE) {
            resize_window(gui, &packet);
        } else if (kind == TABOS_GUI_ADOPT && packet.serial == gui->serial) {
            if (gui->retired > 0) {
                (void) tabos_surface_release(gui->retired);
                gui->retired = -1;
            }
        } else if (kind == TABOS_GUI_CANCEL_INPUT) {
            tabos_gui_ui_cancel(&gui->ui);
            gui->dirty = true;
        } else if (kind == TABOS_GUI_CLOSE && !gui->close_pending) {
            gui->close_pending = true;
            const int close    = gui->closing != NULL ? gui->closing(gui) : 1;
            if (close != 0) {
                tabos_gui_close_reply(gui, close > 0);
            }
            gui->dirty = true;
        } else if (kind == TABOS_GUI_ERROR) {
            gui->last_error = packet.data.error;
            gui->dirty      = true;
        } else if (packet.serial == gui->serial && (kind == TABOS_GUI_POINTER || kind == TABOS_GUI_KEYBOARD)) {
            const int action = kind == TABOS_GUI_POINTER ? tabos_gui_ui_pointer(&gui->ui, &packet.data.pointer) :
                                                           tabos_gui_ui_keyboard(&gui->ui, &packet.data.keyboard);
            gui->dirty       = gui->dirty || gui->ui.changed;
            if (action > 0 && gui->action != NULL) {
                gui->action(gui, action);
            }
            if (gui->input != NULL) {
                gui->input(gui, &packet, kind);
            }
        }
    }
    if (!gui->running) {
        return 0;
    }
    if (gui->cancel_reply_pending) {
        const tabos_gui_packet_t packet = window_packet(gui);
        gui->cancel_reply_pending       = tabos_gui_send(gui->channel, TABOS_GUI_CLOSE_CANCELLED, &packet, true) != 0;
    }
    if (gui->error_pending) {
        report_error(gui, gui->error_serial, gui->pending_error);
    }
    if (gui->dirty && publish(gui) != 0) {
        report_error(gui, gui->serial, gui->last_error);
    }
    if (gui->pending_frame) {
        const tabos_gui_packet_t packet = window_packet(gui);
        if (tabos_gui_send(gui->channel, gui->advertised ? TABOS_GUI_FRAME : TABOS_GUI_HELLO, &packet, false) == 0) {
            gui->pending_frame = false;
            gui->advertised    = true;
        } else if (errno != TABOS_EAGAIN) {
            gui->last_error = errno;
            return -1;
        }
    }
    tabos_wait_item_t pending = {.source = tabos_ipc_wait_source(gui->channel),
                                 .events = TABOS_WAIT_READABLE | TABOS_WAIT_HANGUP};
    if (gui->pending_frame) {
        pending.events |= TABOS_WAIT_WRITABLE;
    }
    if (timeout_ms > 100U) {
        timeout_ms = 100U;
    }
    if (tabos_wait(&pending, 1U, timeout_ms) < 0) {
        gui->last_error = errno;
        return -1;
    }
    return 1;
}
