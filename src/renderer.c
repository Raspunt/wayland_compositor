#include "src/renderer.h"
#include "src/compositor.h"
#include "src/cursor.h"  // Для reset_cursor_mode и begin_interactive

#include <stdlib.h>
#include <assert.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

// Forward declarations
void reset_cursor_mode(struct compositor_state *server);
void begin_interactive(struct compositor_toplevel *toplevel, enum cursor_mode mode, uint32_t edges);

void xdg_toplevel_map(struct wl_listener *listener, void *data);
void xdg_toplevel_unmap(struct wl_listener *listener, void *data);
void xdg_toplevel_commit(struct wl_listener *listener, void *data);
void xdg_toplevel_destroy(struct wl_listener *listener, void *data);
void xdg_toplevel_request_move(struct wl_listener *listener, void *data);
void xdg_toplevel_request_resize(struct wl_listener *listener, void *data);
void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data);
void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data);

void xdg_popup_commit(struct wl_listener *listener, void *data);
void xdg_popup_destroy(struct wl_listener *listener, void *data);

struct compositor_toplevel *get_focused_toplevel(struct compositor_state *server) {
    struct wlr_surface *focused = server->seat->keyboard_state.focused_surface;
    if (!focused) return NULL;
    
    struct compositor_toplevel *toplevel;
    wl_list_for_each(toplevel, &server->toplevels, link) {
        if (toplevel->xdg_toplevel->base->surface == focused) {
            return toplevel;
        }
    }
    return NULL;
}

void switch_workspace(struct compositor_state *server, int workspace) {
    if (workspace < 1 || workspace > NUM_WORKSPACES) return;
    if (workspace == server->active_workspace) return;
    
    struct compositor_toplevel *toplevel;
    wl_list_for_each(toplevel, &server->toplevels, link) {
        if (toplevel->workspace == workspace) {
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
        } else {
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
            wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, false);
        }
    }
    
    server->active_workspace = workspace;
    
    wl_list_for_each(toplevel, &server->toplevels, link) {
        if (toplevel->workspace == workspace) {
            focus_toplevel(toplevel);
            break;
        }
    }
}

void move_toplevel_to_workspace(struct compositor_toplevel *toplevel, int workspace) {
    if (!toplevel || workspace < 1 || workspace > NUM_WORKSPACES) return;
    
    toplevel->workspace = workspace;
    if (workspace == toplevel->server->active_workspace) {
        wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
        focus_toplevel(toplevel);
    } else {
        wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
        wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, false);
    }
}

void focus_toplevel(struct compositor_toplevel *toplevel) {
    if (toplevel == NULL) {
        return;
    }
    
    struct compositor_state *server = toplevel->server;
    struct wlr_seat *seat = server->seat;
    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
    struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
    
    if (prev_surface == surface) {
        return;
    }
    
    if (prev_surface) {
        struct wlr_xdg_toplevel *prev_toplevel =
            wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
        if (prev_toplevel != NULL) {
            wlr_xdg_toplevel_set_activated(prev_toplevel, false);
        }
    }
    
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    
    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
    wl_list_remove(&toplevel->link);
    wl_list_insert(&server->toplevels, &toplevel->link);
    
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
    
    if (keyboard != NULL) {
        wlr_seat_keyboard_notify_enter(seat, surface,
            keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    }
}

void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
    struct compositor_state *server = wl_container_of(listener, server, new_xdg_toplevel);
    struct wlr_xdg_toplevel *xdg_toplevel = data;

    struct compositor_toplevel *toplevel = calloc(1, sizeof(*toplevel));
    if (!toplevel) {
        wlr_log(WLR_ERROR, "Failed to allocate toplevel");
        return;
    }
    wl_list_init(&toplevel->link);

    toplevel->server = server;
    toplevel->xdg_toplevel = xdg_toplevel;
    toplevel->workspace = server->active_workspace;
    toplevel->scene_tree =
        wlr_scene_xdg_surface_create(server->xdg_tree, xdg_toplevel->base);
    if (!toplevel->scene_tree) {
        free(toplevel);
        return;
    }
    toplevel->scene_tree->node.data = toplevel;
    xdg_toplevel->base->data = toplevel->scene_tree;

    toplevel->map.notify = xdg_toplevel_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
    toplevel->unmap.notify = xdg_toplevel_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
    toplevel->commit.notify = xdg_toplevel_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

    toplevel->destroy.notify = xdg_toplevel_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

    toplevel->request_move.notify = xdg_toplevel_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
    toplevel->request_resize.notify = xdg_toplevel_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
    toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
    toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
}

void server_new_xdg_popup(struct wl_listener *listener, void *data) {
    struct wlr_xdg_popup *xdg_popup = data;
    struct compositor_state *server = wl_container_of(listener, server, new_xdg_popup);

    struct compositor_popup *popup = calloc(1, sizeof(*popup));
    if (!popup) {
        return;
    }
    popup->xdg_popup = xdg_popup;

    struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
    assert(parent != NULL);
    struct wlr_scene_tree *parent_tree = parent->data;
    xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

    popup->commit.notify = xdg_popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

    popup->destroy.notify = xdg_popup_destroy;
    wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}

void xdg_popup_commit(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_popup *popup = wl_container_of(listener, popup, commit);

    if (popup->xdg_popup->base->initial_commit) {
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    }
}

void xdg_popup_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_popup *popup = wl_container_of(listener, popup, destroy);

    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->destroy.link);

    free(popup);
}

void xdg_toplevel_map(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, map);

    wl_list_insert(&toplevel->server->toplevels, &toplevel->link);
    if (toplevel->workspace != toplevel->server->active_workspace) {
        wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
    } else {
        focus_toplevel(toplevel);
    }
}

void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);

    if (toplevel == toplevel->server->grabbed_toplevel) {
        reset_cursor_mode(toplevel->server);
    }

    wl_list_remove(&toplevel->link);
    wl_list_init(&toplevel->link);
}

void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, commit);
    
    // На первом коммите отправляем configure
    if (toplevel->xdg_toplevel->base->initial_commit) {
        wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
    }
}

void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);

    if (toplevel->server->grabbed_toplevel == toplevel) {
        reset_cursor_mode(toplevel->server);
    }

    wl_list_remove(&toplevel->link);
    wl_list_remove(&toplevel->map.link);
    wl_list_remove(&toplevel->unmap.link);
    wl_list_remove(&toplevel->commit.link);
    wl_list_remove(&toplevel->destroy.link);
    wl_list_remove(&toplevel->request_move.link);
    wl_list_remove(&toplevel->request_resize.link);
    wl_list_remove(&toplevel->request_maximize.link);
    wl_list_remove(&toplevel->request_fullscreen.link);

    if (toplevel->scene_tree) {
        wlr_scene_node_destroy(&toplevel->scene_tree->node);
    }

    free(toplevel);
}

void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, request_move);
    begin_interactive(toplevel, CURSOR_MOVE, 0);
}

void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_resize_event *event = data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
    begin_interactive(toplevel, CURSOR_RESIZE, event->edges);
}

void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, request_maximize);
    if (toplevel->xdg_toplevel->base->initialized) {
        wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
    }
}

void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, request_fullscreen);
    if (toplevel->xdg_toplevel->base->initialized) {
        wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
    }
}