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
    
    printf("LAYER: commit, initial=%d\n", layer_surface->initial_commit);
    
    if (layer_surface->initial_commit) {
        struct wlr_box output_box;
        wlr_output_layout_get_box(layer->server->output_layout, layer_surface->output, &output_box);
        wlr_layer_surface_v1_configure(layer_surface, output_box.width, output_box.height);
    }
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
    
    struct wlr_box output_box;
    wlr_output_layout_get_box(server->output_layout, layer_surface->output, &output_box);
    wlr_layer_surface_v1_configure(layer_surface, output_box.width, output_box.height);
}
