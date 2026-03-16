#include "src/output.h"
#include "src/cursor.h"
#include "src/renderer.h"

struct compositor_state {
    struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;
    struct wlr_session *wlr_session;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    
    // Сцена (добавь обязательно!)
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    
    struct wl_listener new_output;
    struct wl_list outputs; 
    struct wlr_output_layout *output_layout;
    struct wlr_seat *seat;
    
    // Seat listeners (добавь!)
    struct wl_listener request_set_cursor;
    struct wl_listener request_set_selection;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xdg_popup;
    struct wl_list toplevels;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    enum cursor_mode cursor_mode;
    struct compositor_toplevel *grabbed_toplevel;  
    struct wlr_box grab_geobox;
    uint32_t resize_edges;
    double grab_x, grab_y;

    struct wl_list keyboards;
    struct wl_listener new_input;
};