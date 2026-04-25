#ifndef OUTPUT_H
#define OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>

// Forward declaration - НЕ include!
struct compositor_state;

struct mcw_output {
    struct wlr_output *wlr_output;
    struct compositor_state *server;  // Указатель на неполный тип
    struct wlr_scene_output *scene_output;
    struct wl_listener destroy;
    struct wl_listener frame;
    struct wl_list link;
};

void output_destroy_notify(struct wl_listener *listener, void *data);
void output_frame_notify(struct wl_listener *listener, void *data);
void new_output_notify(struct wl_listener *listener, void *data);

#endif