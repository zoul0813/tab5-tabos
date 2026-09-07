#include <kilo/input.h>
#include <string.h>

static void search_changed(kilo_editor_t* e)
{
    e->query[e->query_size] = '\0';
    e->cx                   = e->saved_x;
    e->cy                   = e->saved_y;
    (void) kilo_search(e, 1, false);
}
kilo_action_t kilo_input(kilo_editor_t* e, const tabos_input_event_t* event)
{
    if (event->type == TABOS_INPUT_KEY_UP) {
        return KILO_IDLE;
    }
    if (event->type == TABOS_INPUT_TEXT) {
        if (e->confirming || e->suppress_text ||
            (event->modifiers & (TABOS_MODIFIER_CONTROL | TABOS_MODIFIER_ALT | TABOS_MODIFIER_GUI)) != 0U) {
            return KILO_IDLE;
        }
        bool changed = false;
        for (size_t i = 0U; i < TABOS_INPUT_TEXT_MAX_BYTES && event->text[i] != '\0'; ++i) {
            const unsigned char c = (unsigned char) event->text[i];
            if (c < 32U || c == 127U) {
                continue;
            }
            if (e->searching) {
                if (e->query_size + 1U < sizeof(e->query)) {
                    e->query[e->query_size++] = (char) c;
                    search_changed(e);
                    changed = true;
                }
            } else {
                (void) kilo_insert(e, c);
                changed = true;
            }
        }
        return changed ? KILO_REDRAW : KILO_IDLE;
    }
    if (event->type != TABOS_INPUT_KEY_DOWN) {
        return KILO_IDLE;
    }
    const bool control    = (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U;
    const tabos_key_t key = event->key;
    e->suppress_text      = false;
    if (e->confirming) {
        e->suppress_text = true;
        if (event->repeat) {
            return KILO_IDLE;
        }
        if (key == TABOS_KEY_Y && !control) {
            e->quit = true;
        }
        if (key == TABOS_KEY_N || key == TABOS_KEY_ESCAPE) {
            e->confirming = false;
        }
        return KILO_REDRAW;
    }
    if (control && (key == TABOS_KEY_Q || key == TABOS_KEY_S || key == TABOS_KEY_F || key == TABOS_KEY_L)) {
        e->suppress_text = true;
        if (event->repeat) {
            return KILO_IDLE;
        }
        if (key == TABOS_KEY_Q) {
            e->confirming = e->dirty;
            e->quit       = !e->dirty;
            return KILO_REDRAW;
        }
        if (key == TABOS_KEY_S) {
            return KILO_SAVE;
        }
        if (key == TABOS_KEY_L) {
            return KILO_RESIZE;
        }
        if (!e->searching) {
            e->searching    = true;
            e->query_size   = 0U;
            e->query[0]     = '\0';
            e->saved_x      = e->cx;
            e->saved_y      = e->cy;
            e->saved_rowoff = e->rowoff;
            e->saved_coloff = e->coloff;
        }
        return KILO_REDRAW;
    }
    if (e->searching) {
        if (key == TABOS_KEY_ESCAPE || key == TABOS_KEY_ENTER) {
            if (key == TABOS_KEY_ESCAPE) {
                e->cx     = e->saved_x;
                e->cy     = e->saved_y;
                e->rowoff = e->saved_rowoff;
                e->coloff = e->saved_coloff;
            }
            e->searching     = false;
            e->suppress_text = true;
        } else if (key == TABOS_KEY_BACKSPACE && e->query_size > 0U) {
            --e->query_size;
            search_changed(e);
        } else if (key == TABOS_KEY_RIGHT || key == TABOS_KEY_DOWN) {
            (void) kilo_search(e, 1, true);
        } else if (key == TABOS_KEY_LEFT || key == TABOS_KEY_UP) {
            (void) kilo_search(e, -1, true);
        } else {
            return KILO_IDLE;
        }
        return KILO_REDRAW;
    }
    switch (key) {
        case TABOS_KEY_LEFT:
            if (control) {
                e->cx = 0U;
            } else {
                kilo_move(e, -1, 0);
            }
            break;
        case TABOS_KEY_RIGHT:
            if (control) {
                e->cx = e->row[e->cy].size;
            } else {
                kilo_move(e, 1, 0);
            }
            break;
        case TABOS_KEY_UP: kilo_move(e, 0, control ? -(int) e->screenrows : -1); break;
        case TABOS_KEY_DOWN: kilo_move(e, 0, control ? (int) e->screenrows : 1); break;
        case TABOS_KEY_HOME: e->cx = 0U; break;
        case TABOS_KEY_END: e->cx = e->row[e->cy].size; break;
        case TABOS_KEY_PAGE_UP: kilo_move(e, 0, -(int) e->screenrows); break;
        case TABOS_KEY_PAGE_DOWN: kilo_move(e, 0, (int) e->screenrows); break;
        case TABOS_KEY_ENTER: (void) kilo_split(e); break;
        case TABOS_KEY_TAB: (void) kilo_insert(e, '\t'); break;
        case TABOS_KEY_BACKSPACE: (void) kilo_delete(e, true); break;
        case TABOS_KEY_DELETE: (void) kilo_delete(e, false); break;
        default: return KILO_IDLE;
    }
    return KILO_REDRAW;
}
