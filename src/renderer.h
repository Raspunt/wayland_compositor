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
    struct compositor_state *server;  // указатель на неполный тип - ок
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree *scene_tree;
    int workspace;
    
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



void focus_toplevel(struct compositor_toplevel *toplevel);
struct compositor_toplevel *get_focused_toplevel(struct compositor_state *server);
void switch_workspace(struct compositor_state *server, int workspace);
void move_toplevel_to_workspace(struct compositor_toplevel *toplevel, int workspace);
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

#endif