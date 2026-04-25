#include <stdlib.h>
#include <assert.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "src/compositor.h"
#include "src/output.h"
#include "src/layer_shell.h"
#include "src/renderer.h"

struct compositor_layer_surface {
    struct compositor_state *server;
    struct wlr_layer_surface_v1 *layer_surface;
    struct wlr_scene_tree *scene_tree;
    
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener new_popup;
};

static void layer_surface_map(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_layer_surface *layer = wl_container_of(listener, layer, map);
    printf("LAYER: map\n");
    wlr_scene_node_set_enabled(&layer->scene_tree->node, true);
}

static void layer_surface_unmap(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_layer_surface *layer = wl_container_of(listener, layer, unmap);
    wlr_scene_node_set_enabled(&layer->scene_tree->node, false);
}

static void layer_surface_commit(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_layer_surface *layer = wl_container_of(listener, layer, commit);
    struct wlr_layer_surface_v1 *layer_surface = layer->layer_surface;
    
    if (!layer_surface->output) {
        return;
    }
    
    struct wlr_box output_box;
    wlr_output_layout_get_box(layer->server->output_layout, layer_surface->output, &output_box);
    
    uint32_t anchor = layer_surface->current.anchor;
    int32_t margin_top = layer_surface->current.margin.top;
    int32_t margin_right = layer_surface->current.margin.right;
    int32_t margin_bottom = layer_surface->current.margin.bottom;
    int32_t margin_left = layer_surface->current.margin.left;
    
    int32_t width = layer_surface->current.desired_width;
    int32_t height = layer_surface->current.desired_height;
    
    bool anchor_left = anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
    bool anchor_right = anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    bool anchor_top = anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    bool anchor_bottom = anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    
    if (anchor_left && anchor_right) {
        width = output_box.width - margin_left - margin_right;
    }
    if (anchor_top && anchor_bottom) {
        height = output_box.height - margin_top - margin_bottom;
    }
    
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    
    if (layer_surface->current.actual_width != (uint32_t)width ||
        layer_surface->current.actual_height != (uint32_t)height) {
        wlr_layer_surface_v1_configure(layer_surface, width, height);
    }
    
    int32_t x = 0, y = 0;
    if (anchor_left) {
        x = output_box.x + margin_left;
    } else if (anchor_right) {
        x = output_box.x + output_box.width - width - margin_right;
    } else {
        x = output_box.x + (output_box.width - width) / 2;
    }
    
    if (anchor_top) {
        y = output_box.y + margin_top;
    } else if (anchor_bottom) {
        y = output_box.y + output_box.height - height - margin_bottom;
    } else {
        y = output_box.y + (output_box.height - height) / 2;
    }
    
    wlr_scene_node_set_position(&layer->scene_tree->node, x, y);
    
    printf("LAYER: commit, anchor=%d, desired=%dx%d, configured=%dx%d, pos=%d,%d\n",
        anchor, layer_surface->current.desired_width, layer_surface->current.desired_height,
        width, height, x, y);
}

static void layer_surface_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct compositor_layer_surface *layer = wl_container_of(listener, layer, destroy);
    printf("LAYER: destroy\n");
    
    wl_list_remove(&layer->map.link);
    wl_list_remove(&layer->unmap.link);
    wl_list_remove(&layer->commit.link);
    wl_list_remove(&layer->destroy.link);
    wl_list_remove(&layer->new_popup.link);
    
    wlr_scene_node_destroy(&layer->scene_tree->node);
    
    free(layer);
}

static void layer_surface_new_popup(struct wl_listener *listener, void *data) {
    struct compositor_layer_surface *layer = wl_container_of(listener, layer, new_popup);
    struct wlr_xdg_popup *xdg_popup = data;
    (void)layer;
    
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

void server_new_layer_surface(struct wl_listener *listener, void *data) {
    struct compositor_state *server = wl_container_of(listener, server, new_layer_surface);
    struct wlr_layer_surface_v1 *layer_surface = data;
    
    printf("LAYER: new_layer_surface requested, layer=%d, namespace=%s\n",
        layer_surface->pending.layer, layer_surface->namespace ? layer_surface->namespace : "(null)");
    
    if (!layer_surface->output) {
        if (wl_list_empty(&server->outputs)) {
            wlr_log(WLR_ERROR, "No outputs available for layer surface");
            wlr_layer_surface_v1_destroy(layer_surface);
            return;
        }
        struct mcw_output *output = wl_container_of(server->outputs.next, output, link);
        layer_surface->output = output->wlr_output;
    }
    
    struct wlr_scene_tree *layer_tree = NULL;
    switch (layer_surface->pending.layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        layer_tree = server->layer_background;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        layer_tree = server->layer_bottom;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        layer_tree = server->layer_top;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        layer_tree = server->layer_overlay;
        break;
    }
    
    if (!layer_tree) {
        layer_tree = server->layer_overlay;
    }
    
    struct wlr_scene_tree *scene_tree = wlr_scene_subsurface_tree_create(layer_tree, layer_surface->surface);
    if (!scene_tree) {
        wlr_log(WLR_ERROR, "Failed to create scene tree for layer surface");
        wlr_layer_surface_v1_destroy(layer_surface);
        return;
    }
    
    wlr_scene_node_set_enabled(&scene_tree->node, false);
    layer_surface->surface->data = scene_tree;
    
    struct compositor_layer_surface *layer = calloc(1, sizeof(*layer));
    layer->server = server;
    layer->layer_surface = layer_surface;
    layer->scene_tree = scene_tree;
    layer_surface->data = layer;
    
    layer->map.notify = layer_surface_map;
    wl_signal_add(&layer_surface->surface->events.map, &layer->map);
    layer->unmap.notify = layer_surface_unmap;
    wl_signal_add(&layer_surface->surface->events.unmap, &layer->unmap);
    layer->commit.notify = layer_surface_commit;
    wl_signal_add(&layer_surface->surface->events.commit, &layer->commit);
    layer->destroy.notify = layer_surface_destroy;
    wl_signal_add(&layer_surface->events.destroy, &layer->destroy);
    layer->new_popup.notify = layer_surface_new_popup;
    wl_signal_add(&layer_surface->events.new_popup, &layer->new_popup);
}
