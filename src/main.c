#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
#include <wlr/util/box.h>
#include <wlr/util/log.h>


struct compositor_state {
	struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;
    struct wlr_session *wlr_session;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wl_listener new_output;
    struct wl_list outputs; 
};

struct mcw_output {
    struct wlr_output *wlr_output;
    struct compositor_state *server;
    struct timespec last_frame;
    struct wl_listener destroy;
    struct wl_listener frame;

    struct wl_list link;
};

static void output_destroy_notify(struct wl_listener *listener, void *data) {
        struct mcw_output *output = wl_container_of(listener, output, destroy);
        wl_list_remove(&output->link);
        wl_list_remove(&output->destroy.link);
        wl_list_remove(&output->frame.link);
        free(output);
}

static void output_frame_notify(struct wl_listener *listener, void *data) {
    struct mcw_output *output = wl_container_of(listener, output, frame);
    struct wlr_output *wlr_output = data;
    struct wlr_renderer *renderer = output->server->renderer;
    
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    
    if (!wlr_output_configure_primary_swapchain(wlr_output, &state, &wlr_output->swapchain)) {
        wlr_output_state_finish(&state);
        return;
    }
    
    struct wlr_buffer *buffer = wlr_swapchain_acquire(wlr_output->swapchain, NULL);
    if (!buffer) {
        wlr_output_state_finish(&state);
        return;
    }
    
    struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(renderer, buffer, NULL);
    
    float color[4] = {1.0, 0, 0, 1.0};
    wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
        .box = {0, 0, wlr_output->width, wlr_output->height},
        .color = {color[0], color[1], color[2], color[3]},
    });
    
    wlr_render_pass_submit(pass);
    wlr_output_state_set_buffer(&state, buffer);
    wlr_output_commit_state(wlr_output, &state);
    wlr_buffer_unlock(buffer);
    wlr_output_state_finish(&state);
}


static void new_output_notify(struct wl_listener *listener, void *data) {
    struct compositor_state *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    // Инициализируем рендеринг для этого выхода (КЛЮЧЕВОЙ МОМЕНТ)
    if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer)) {
        fprintf(stderr, "Failed to init output render\n");
        return;
    }

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
    clock_gettime(CLOCK_MONOTONIC, &output->last_frame);
    output->server = server;
    output->wlr_output = wlr_output;
    wl_list_insert(&server->outputs, &output->link);

    output->destroy.notify = output_destroy_notify;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    output->frame.notify = output_frame_notify;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
}

int main(void) {
    printf("Hello wlroots %s\n", WLR_VERSION_STR);
    printf("program is works foxs!\n");

    struct compositor_state cs;

    cs.wl_display = wl_display_create();
    assert(cs.wl_display);
    cs.wl_event_loop = wl_display_get_event_loop(cs.wl_display);
    assert(cs.wl_event_loop);

    cs.wlr_session = wlr_session_create(cs.wl_event_loop);

    cs.backend = wlr_backend_autocreate(cs.wl_event_loop,&cs.wlr_session);
    assert(cs.backend);

    cs.renderer = wlr_renderer_autocreate(cs.backend);
    assert(cs.renderer);
    cs.allocator = wlr_allocator_autocreate(cs.backend, cs.renderer);
    assert(cs.allocator);

    wl_list_init(&cs.outputs);

    cs.new_output.notify = new_output_notify;
    wl_signal_add(&cs.backend->events.new_output, &cs.new_output);


    if (!wlr_backend_start(cs.backend)) {
        fprintf(stderr, "Failed to start backend\n");
        wl_display_destroy(cs.wl_display);
        return 1;
    }

    wl_display_run(cs.wl_display);
    wl_display_destroy(cs.wl_display);

    return 0;
}