#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include <wlr/version.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>

#include "src/output.h"
#include "src/compositor.h"

void output_destroy_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct mcw_output *output = wl_container_of(listener, output, destroy);
    
    wl_list_remove(&output->link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->frame.link);
    
    wlr_output_layout_remove(output->server->output_layout, output->wlr_output);
    
    if (output->scene_output) {
        wlr_scene_output_destroy(output->scene_output);
    }
    
    free(output);
}

void output_frame_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct mcw_output *output = wl_container_of(listener, output, frame);
    
    // ПРОВЕРКА: если scene_output не создан, просто выходим
    if (!output || !output->scene_output) {
        printf("Error: output or scene_output is NULL\n");
        return;
    }
    
    // Пробуем коммит
    if (!wlr_scene_output_commit(output->scene_output, NULL)) {
        printf("Warning: scene_output_commit failed\n");
    }
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(output->scene_output, &now);
}

void new_output_notify(struct wl_listener *listener, void *data) {
    struct compositor_state *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer)) {
        fprintf(stderr, "Failed to init output render\n");
        return;
    }

    wlr_output_layout_add(server->output_layout, wlr_output, 0, 0);

    struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
    if (!scene_output) {
        fprintf(stderr, "Failed to create scene output\n");
        return;
    }

    // Настройка режима
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    if (!wl_list_empty(&wlr_output->modes)) {
        struct wlr_output_mode *mode = wl_container_of(wlr_output->modes.prev, mode, link);
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    struct mcw_output *output = calloc(1, sizeof(struct mcw_output));
    output->server = server;
    output->wlr_output = wlr_output;
    output->scene_output = scene_output;  // Сохраняем для рендеринга
    wl_list_insert(&server->outputs, &output->link);

    output->destroy.notify = output_destroy_notify;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    output->frame.notify = output_frame_notify;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    
    printf("Output added: %s\n", wlr_output->name);
}