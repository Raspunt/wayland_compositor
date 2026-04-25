#include "src/renderer.h"
#include "src/compositor.h"
#include "src/cursor.h"  // Для reset_cursor_mode и begin_interactive

#include <stdlib.h>
#include <assert.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
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

static const float BORDER_FOCUSED[4] = {1.0, 0.85, 0.0, 1.0};   /* Жёлтый как в Hyprland */
static const float BORDER_UNFOCUSED[4] = {0.2, 0.2, 0.2, 1.0}; /* Тёмно-серый */
const int BORDER_WIDTH = 3;

static void update_borders(struct compositor_toplevel *toplevel) {
    if (!toplevel->border_tree) return;
    struct wlr_box geo;
    wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo);
    int bw = BORDER_WIDTH;
    int w = geo.width + 2 * bw;

    wlr_scene_node_set_position(&toplevel->borders[0]->node, 0, 0);
    wlr_scene_rect_set_size(toplevel->borders[0], w, bw);

    wlr_scene_node_set_position(&toplevel->borders[1]->node, 0, geo.height + bw);
    wlr_scene_rect_set_size(toplevel->borders[1], w, bw);

    wlr_scene_node_set_position(&toplevel->borders[2]->node, 0, bw);
    wlr_scene_rect_set_size(toplevel->borders[2], bw, geo.height);

    wlr_scene_node_set_position(&toplevel->borders[3]->node, geo.width + bw, bw);
    wlr_scene_rect_set_size(toplevel->borders[3], bw, geo.height);
}

static void set_border_color(struct compositor_toplevel *toplevel, const float color[4]) {
    for (int i = 0; i < 4; i++) {
        wlr_scene_rect_set_color(toplevel->borders[i], color);
    }
}

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
            wlr_scene_node_set_enabled(&toplevel->border_tree->node, true);
        } else {
            wlr_scene_node_set_enabled(&toplevel->border_tree->node, false);
            wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, false);
        }
    }
    
    server->active_workspace = workspace;
    arrange_workspace(server, workspace);
    
    wl_list_for_each(toplevel, &server->toplevels, link) {
        if (toplevel->workspace == workspace) {
            focus_toplevel(toplevel);
            break;
        }
    }
}

void move_toplevel_to_workspace(struct compositor_toplevel *toplevel, int workspace) {
    if (!toplevel || workspace < 1 || workspace > NUM_WORKSPACES) return;
    
    int old_workspace = toplevel->workspace;
    toplevel->workspace = workspace;
    if (workspace == toplevel->server->active_workspace) {
        wlr_scene_node_set_enabled(&toplevel->border_tree->node, true);
        wl_list_remove(&toplevel->link);
        wl_list_insert(&toplevel->server->toplevels, &toplevel->link);
        focus_toplevel(toplevel);
        arrange_workspace(toplevel->server, workspace);
    } else {
        wlr_scene_node_set_enabled(&toplevel->border_tree->node, false);
        wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, false);
        arrange_workspace(toplevel->server, old_workspace);
    }
}

void arrange_workspace(struct compositor_state *server, int workspace) {
    const int gaps = server->cfg ? server->cfg->gaps : 8;
    const int outer_gaps = server->cfg ? server->cfg->outer_gaps : 8;
    const double mfact = server->cfg ? server->cfg->mfact : 0.55;
    
    struct wlr_box layout_box;
    wlr_output_layout_get_box(server->output_layout, NULL, &layout_box);
    
    int area_x = layout_box.x + outer_gaps;
    int area_y = layout_box.y + outer_gaps;
    int area_w = layout_box.width - 2 * outer_gaps;
    int area_h = layout_box.height - 2 * outer_gaps;
    
    int count = 0;
    struct compositor_toplevel *t;
    wl_list_for_each(t, &server->toplevels, link) {
        if (t->workspace == workspace && !t->floating) {
            count++;
        }
    }
    
    if (count == 0) return;
    
    int bw2 = 2 * BORDER_WIDTH;
    int content_w = area_w - bw2;
    int content_h = area_h - bw2;

    if (count == 1) {
        wl_list_for_each(t, &server->toplevels, link) {
            if (t->workspace == workspace && !t->floating) {
                wlr_scene_node_set_position(&t->border_tree->node, area_x, area_y);
                wlr_xdg_toplevel_set_size(t->xdg_toplevel, content_w, content_h);
                break;
            }
        }
        return;
    }
    
    int master_w = (int)((area_w - gaps) * mfact) - bw2;
    int stack_w = area_w - master_w - gaps - bw2;
    if (master_w < 50) master_w = 50;
    if (stack_w < 50) stack_w = 50;
    
    /* Master — самое новое окно (head списка) */
    struct compositor_toplevel *master = NULL;
    wl_list_for_each(t, &server->toplevels, link) {
        if (t->workspace == workspace && !t->floating) {
            master = t;
            break;
        }
    }
    
    if (master) {
        wlr_scene_node_set_position(&master->border_tree->node, area_x, area_y);
        wlr_xdg_toplevel_set_size(master->xdg_toplevel, master_w, content_h);
    }
    
    int stack_count = count - 1;
    if (stack_count > 0) {
        int stack_h = (area_h - (stack_count - 1) * gaps) / stack_count - bw2;
        if (stack_h < 20) stack_h = 20;
        int stack_x = area_x + master_w + gaps + bw2;
        int stack_y = area_y;
        int i = 0;
        
        wl_list_for_each(t, &server->toplevels, link) {
            if (t->workspace == workspace && !t->floating && t != master) {
                int h = stack_h;
                if (i == stack_count - 1) {
                    h = (area_y + area_h) - stack_y - bw2;
                    if (h < 20) h = 20;
                }
                wlr_scene_node_set_position(&t->border_tree->node, stack_x, stack_y);
                wlr_xdg_toplevel_set_size(t->xdg_toplevel, stack_w, h);
                stack_y += h + gaps + bw2;
                i++;
            }
        }
    }
}

void focus_next(struct compositor_state *server) {
    struct compositor_toplevel *current = get_focused_toplevel(server);
    struct compositor_toplevel *t;
    
    if (!current) {
        wl_list_for_each(t, &server->toplevels, link) {
            if (t->workspace == server->active_workspace) {
                focus_toplevel(t);
                return;
            }
        }
        return;
    }
    
    struct wl_list *pos = current->link.next;
    while (pos != &current->link) {
        if (pos == &server->toplevels) {
            pos = server->toplevels.next;
            continue;
        }
        t = wl_container_of(pos, t, link);
        if (t->workspace == server->active_workspace) {
            focus_toplevel(t);
            return;
        }
        pos = pos->next;
    }
}

void focus_prev(struct compositor_state *server) {
    struct compositor_toplevel *current = get_focused_toplevel(server);
    struct compositor_toplevel *t;
    
    if (!current) {
        wl_list_for_each_reverse(t, &server->toplevels, link) {
            if (t->workspace == server->active_workspace) {
                focus_toplevel(t);
                return;
            }
        }
        return;
    }
    
    struct wl_list *pos = current->link.prev;
    while (pos != &current->link) {
        if (pos == &server->toplevels) {
            pos = server->toplevels.prev;
            continue;
        }
        t = wl_container_of(pos, t, link);
        if (t->workspace == server->active_workspace) {
            focus_toplevel(t);
            return;
        }
        pos = pos->prev;
    }
}

void move_toplevel_next(struct compositor_state *server) {
    struct compositor_toplevel *current = get_focused_toplevel(server);
    if (!current || current->workspace != server->active_workspace) return;
    
    struct compositor_toplevel *t;
    struct wl_list *pos = current->link.next;
    while (pos != &current->link) {
        if (pos == &server->toplevels) {
            pos = server->toplevels.next;
            continue;
        }
        t = wl_container_of(pos, t, link);
        if (t->workspace == server->active_workspace) {
            wl_list_remove(&current->link);
            wl_list_insert(&t->link, &current->link);
            arrange_workspace(server, server->active_workspace);
            return;
        }
        pos = pos->next;
    }
}

void move_toplevel_prev(struct compositor_state *server) {
    struct compositor_toplevel *current = get_focused_toplevel(server);
    if (!current || current->workspace != server->active_workspace) return;
    
    struct compositor_toplevel *t;
    struct wl_list *pos = current->link.prev;
    while (pos != &current->link) {
        if (pos == &server->toplevels) {
            pos = server->toplevels.prev;
            continue;
        }
        t = wl_container_of(pos, t, link);
        if (t->workspace == server->active_workspace) {
            wl_list_remove(&current->link);
            wl_list_insert(pos->prev, &current->link);
            arrange_workspace(server, server->active_workspace);
            return;
        }
        pos = pos->prev;
    }
}

static void apply_mfact_delta(struct compositor_state *server, double delta) {
    if (!server->cfg) return;
    server->cfg->mfact += delta;
    if (server->cfg->mfact < 0.1) server->cfg->mfact = 0.1;
    if (server->cfg->mfact > 0.9) server->cfg->mfact = 0.9;
    arrange_workspace(server, server->active_workspace);
}

void resize_toplevel_left(struct compositor_state *server) {
    struct compositor_toplevel *t = get_focused_toplevel(server);
    if (!t) return;
    if (t->floating) {
        struct wlr_box geo;
        wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
        int w = geo.width - 20;
        if (w < 50) w = 50;
        wlr_xdg_toplevel_set_size(t->xdg_toplevel, w, geo.height);
    } else {
        apply_mfact_delta(server, -0.05);
    }
}

void resize_toplevel_right(struct compositor_state *server) {
    struct compositor_toplevel *t = get_focused_toplevel(server);
    if (!t) return;
    if (t->floating) {
        struct wlr_box geo;
        wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
        int w = geo.width + 20;
        wlr_xdg_toplevel_set_size(t->xdg_toplevel, w, geo.height);
    } else {
        apply_mfact_delta(server, +0.05);
    }
}

void resize_toplevel_up(struct compositor_state *server) {
    struct compositor_toplevel *t = get_focused_toplevel(server);
    if (!t) return;
    if (t->floating) {
        struct wlr_box geo;
        wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
        int h = geo.height - 20;
        if (h < 50) h = 50;
        wlr_xdg_toplevel_set_size(t->xdg_toplevel, geo.width, h);
    }
    /* Для тайлинговых окон вертикальный resize не поддерживается в master-stack layout */
}

void resize_toplevel_down(struct compositor_state *server) {
    struct compositor_toplevel *t = get_focused_toplevel(server);
    if (!t) return;
    if (t->floating) {
        struct wlr_box geo;
        wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
        int h = geo.height + 20;
        wlr_xdg_toplevel_set_size(t->xdg_toplevel, geo.width, h);
    }
    /* Для тайлинговых окон вертикальный resize не поддерживается в master-stack layout */
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
            struct wlr_scene_tree *prev_tree = prev_toplevel->base->data;
            if (prev_tree) {
                struct compositor_toplevel *prev = prev_tree->node.data;
                if (prev) {
                    set_border_color(prev, BORDER_UNFOCUSED);
                }
            }
        }
    }
    
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    
    wlr_scene_node_raise_to_top(&toplevel->border_tree->node);
    wl_list_remove(&toplevel->link);
    wl_list_insert(&server->toplevels, &toplevel->link);
    
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
    set_border_color(toplevel, BORDER_FOCUSED);
    
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
    toplevel->floating = false;

    toplevel->border_tree = wlr_scene_tree_create(server->xdg_tree);
    if (!toplevel->border_tree) {
        free(toplevel);
        return;
    }
    toplevel->border_tree->node.data = toplevel;

    toplevel->scene_tree =
        wlr_scene_xdg_surface_create(toplevel->border_tree, xdg_toplevel->base);
    if (!toplevel->scene_tree) {
        wlr_scene_node_destroy(&toplevel->border_tree->node);
        free(toplevel);
        return;
    }
    toplevel->scene_tree->node.data = toplevel;
    xdg_toplevel->base->data = toplevel->scene_tree;

    wlr_scene_node_set_position(&toplevel->scene_tree->node, BORDER_WIDTH, BORDER_WIDTH);

    for (int i = 0; i < 4; i++) {
        toplevel->borders[i] = wlr_scene_rect_create(toplevel->border_tree, 0, 0, BORDER_UNFOCUSED);
    }
    update_borders(toplevel);

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
    arrange_workspace(toplevel->server, toplevel->workspace);
    if (toplevel->workspace != toplevel->server->active_workspace) {
        wlr_scene_node_set_enabled(&toplevel->border_tree->node, false);
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
    arrange_workspace(toplevel->server, toplevel->workspace);
}

void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, commit);
    
    // На первом коммите отправляем configure
    if (toplevel->xdg_toplevel->base->initial_commit) {
        wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
    }

    update_borders(toplevel);
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

    if (toplevel->border_tree) {
        wlr_scene_node_destroy(&toplevel->border_tree->node);
    }

    free(toplevel);
}

void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_toplevel *toplevel = wl_container_of(listener, toplevel, request_move);
    begin_interactive(toplevel, CURSOR_MOVE, 0);
}

void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
    (void)listener;
    (void)data;
    // Resize мышкой отключён — тайлинговый compositor управляет размерами
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

void server_new_xdg_toplevel_decoration(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
    wlr_xdg_toplevel_decoration_v1_set_mode(decoration,
        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}