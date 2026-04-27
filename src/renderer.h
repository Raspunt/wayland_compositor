#ifndef RENDERER_H
#define RENDERER_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>

// Forward declaration вместо #include
struct compositor_state;

struct compositor_toplevel {
    struct wl_list link;
    struct compositor_state *server;  
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree *scene_tree;
    struct wlr_scene_tree *border_tree;
    struct wlr_scene_rect *borders[4];
    int workspace;
    bool floating;
    
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
};
struct compositor_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

enum direction {
    DIR_LEFT,
    DIR_RIGHT,
    DIR_UP,
    DIR_DOWN,
};

void focus_toplevel(struct compositor_toplevel *toplevel);
struct compositor_toplevel *get_focused_toplevel(struct compositor_state *server);
void focus_direction(struct compositor_state *server, enum direction dir);
void switch_workspace(struct compositor_state *server, int workspace);
void move_toplevel_to_workspace(struct compositor_toplevel *toplevel, int workspace);
void arrange_workspace(struct compositor_state *server, int workspace);
void focus_next(struct compositor_state *server);
void focus_prev(struct compositor_state *server);
void move_toplevel_next(struct compositor_state *server);
void move_toplevel_prev(struct compositor_state *server);
void resize_toplevel_left(struct compositor_state *server);
void resize_toplevel_right(struct compositor_state *server);
void resize_toplevel_up(struct compositor_state *server);
void resize_toplevel_down(struct compositor_state *server);
void toggle_floating(struct compositor_state *server);
struct compositor_toplevel *desktop_toplevel_at(struct compositor_state *server, 
        double lx, double ly, struct wlr_surface **surface, double *sx, double *sy);

// Обработчики toplevel
void server_new_xdg_toplevel(struct wl_listener *listener, void *data);
void server_new_xdg_popup(struct wl_listener *listener, void *data);
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

void server_new_xdg_toplevel_decoration(struct wl_listener *listener, void *data);

#endif