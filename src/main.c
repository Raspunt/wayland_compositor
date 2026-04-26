#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <spawn.h>
#include <wayland-server-core.h>

#include <wlr/version.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_scene.h>  
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>

#include "src/output.h"
#include "src/input.h"   
#include "src/compositor.h"
#include "src/renderer.h"
#include "src/layer_shell.h"

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
    struct compositor_state *server = wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void seat_request_set_primary_selection(struct wl_listener *listener, void *data) {
    struct compositor_state *server = wl_container_of(listener, server, request_set_primary_selection);
    struct wlr_seat_request_set_primary_selection_event *event = data;
    wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}

static int handle_signal(int sig, void *data) {
    (void)sig;
    struct wl_display *display = data;
    wl_display_terminate(display);
    return 0;
}

int main(void) {
    printf("FlottyWM %s (wlroots %s)\n", "0.1.0", WLR_VERSION_STR);

    signal(SIGCHLD, SIG_IGN);

    struct compositor_state cs = {0};
    struct compositor_config cfg = {0};
    config_load(&cfg);
    cs.cfg = &cfg;

    /* Применяем env-переменные из конфига */
    for (int i = 0; i < cfg.num_envs; i++) {
        setenv(cfg.envs[i].name, cfg.envs[i].value, 1);
    }
    
    struct wl_event_source *sigint = NULL;
    struct wl_event_source *sigterm = NULL;

    // Инициализируем listeners, чтобы wl_list_remove был безопасен при cleanup
    wl_list_init(&cs.request_set_cursor.link);
    wl_list_init(&cs.request_set_selection.link);
    wl_list_init(&cs.request_set_primary_selection.link);
    wl_list_init(&cs.new_xdg_toplevel.link);
    wl_list_init(&cs.new_xdg_popup.link);
    wl_list_init(&cs.cursor_motion.link);
    wl_list_init(&cs.cursor_motion_absolute.link);
    wl_list_init(&cs.cursor_button.link);
    wl_list_init(&cs.cursor_axis.link);
    wl_list_init(&cs.cursor_frame.link);
    wl_list_init(&cs.new_input.link);
    wl_list_init(&cs.new_output.link);
    wl_list_init(&cs.new_layer_surface.link);

    cs.active_workspace = 1;
    
    cs.wl_display = wl_display_create();
    if (!cs.wl_display) {
        fprintf(stderr, "Failed to create Wayland display\n");
        return 1;
    }
    
    cs.wl_event_loop = wl_display_get_event_loop(cs.wl_display);
    if (!cs.wl_event_loop) {
        fprintf(stderr, "Failed to get event loop\n");
        goto cleanup;
    }

    // Сессия нужна только для DRM backend; для nested (X11/Wayland) может быть NULL
    cs.wlr_session = wlr_session_create(cs.wl_event_loop);
    
    cs.backend = wlr_backend_autocreate(cs.wl_event_loop, &cs.wlr_session);
    if (!cs.backend) {
        fprintf(stderr, "Failed to create backend\n");
        goto cleanup;
    }

    cs.renderer = wlr_renderer_autocreate(cs.backend);
    if (!cs.renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        goto cleanup;
    }
    
    cs.allocator = wlr_allocator_autocreate(cs.backend, cs.renderer);
    if (!cs.allocator) {
        fprintf(stderr, "Failed to create allocator\n");
        goto cleanup;
    }

    cs.output_layout = wlr_output_layout_create(cs.wl_display);
    if (!cs.output_layout) {
        fprintf(stderr, "Failed to create output layout\n");
        goto cleanup;
    }
    
    cs.seat = wlr_seat_create(cs.wl_display, "seat0");
    if (!cs.seat) {
        fprintf(stderr, "Failed to create seat\n");
        goto cleanup;
    }

    cs.request_set_cursor.notify = seat_request_set_cursor;
    wl_signal_add(&cs.seat->events.request_set_cursor, &cs.request_set_cursor);

    cs.request_set_selection.notify = seat_request_set_selection;
    wl_signal_add(&cs.seat->events.request_set_selection, &cs.request_set_selection);
    cs.request_set_primary_selection.notify = seat_request_set_primary_selection;
    wl_signal_add(&cs.seat->events.request_set_primary_selection, &cs.request_set_primary_selection);

    cs.scene = wlr_scene_create();
    if (!cs.scene) {
        fprintf(stderr, "Failed to create scene\n");
        goto cleanup;
    }
    
    cs.layer_background = wlr_scene_tree_create(&cs.scene->tree);
    cs.layer_bottom = wlr_scene_tree_create(&cs.scene->tree);
    cs.xdg_tree = wlr_scene_tree_create(&cs.scene->tree);
    cs.layer_top = wlr_scene_tree_create(&cs.scene->tree);
    cs.layer_overlay = wlr_scene_tree_create(&cs.scene->tree);
    
    cs.scene_layout = wlr_scene_attach_output_layout(cs.scene, cs.output_layout);
    if (!cs.scene_layout) {
        fprintf(stderr, "Failed to attach output layout to scene\n");
        goto cleanup;
    }

    wl_list_init(&cs.outputs);
    wl_list_init(&cs.toplevels);

    wlr_renderer_init_wl_display(cs.renderer, cs.wl_display);
    wlr_compositor_create(cs.wl_display, 6, cs.renderer);
    wlr_subcompositor_create(cs.wl_display);
    wlr_data_device_manager_create(cs.wl_display);
    wlr_data_control_manager_v1_create(cs.wl_display);
    wlr_primary_selection_v1_device_manager_create(cs.wl_display);
    wlr_xdg_output_manager_v1_create(cs.wl_display, cs.output_layout);

    cs.new_output.notify = new_output_notify;
    wl_signal_add(&cs.backend->events.new_output, &cs.new_output);

    cs.cursor = wlr_cursor_create();
    if (!cs.cursor) {
        fprintf(stderr, "Failed to create cursor\n");
        goto cleanup;
    }
    
    wlr_cursor_attach_output_layout(cs.cursor, cs.output_layout);
    
    cs.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    if (!cs.cursor_mgr) {
        fprintf(stderr, "Failed to create cursor manager\n");
        goto cleanup;
    }
    
    wlr_xcursor_manager_load(cs.cursor_mgr, 1);
    wlr_cursor_set_xcursor(cs.cursor, cs.cursor_mgr, "default");

    cs.cursor_mode = CURSOR_PASSTHROUGH;
    
    cs.cursor_motion.notify = server_cursor_motion;
    wl_signal_add(&cs.cursor->events.motion, &cs.cursor_motion);
    
    cs.cursor_motion_absolute.notify = server_cursor_motion_absolute;
    wl_signal_add(&cs.cursor->events.motion_absolute, &cs.cursor_motion_absolute);
    
    cs.cursor_button.notify = server_cursor_button;
    wl_signal_add(&cs.cursor->events.button, &cs.cursor_button);
    
    cs.cursor_axis.notify = server_cursor_axis;
    wl_signal_add(&cs.cursor->events.axis, &cs.cursor_axis);
    
    cs.cursor_frame.notify = server_cursor_frame;
    wl_signal_add(&cs.cursor->events.frame, &cs.cursor_frame);

    wl_list_init(&cs.keyboards);

    cs.new_input.notify = server_new_input; 
    wl_signal_add(&cs.backend->events.new_input, &cs.new_input);

    cs.xdg_shell = wlr_xdg_shell_create(cs.wl_display, 3);
    if (!cs.xdg_shell) {
        fprintf(stderr, "Failed to create xdg shell\n");
        goto cleanup;
    }
    
    cs.new_xdg_toplevel.notify = server_new_xdg_toplevel;
    wl_signal_add(&cs.xdg_shell->events.new_toplevel, &cs.new_xdg_toplevel);
    cs.new_xdg_popup.notify = server_new_xdg_popup;
    wl_signal_add(&cs.xdg_shell->events.new_popup, &cs.new_xdg_popup);

    cs.layer_shell = wlr_layer_shell_v1_create(cs.wl_display, 4);
    if (!cs.layer_shell) {
        fprintf(stderr, "Failed to create layer shell\n");
        goto cleanup;
    }
    cs.new_layer_surface.notify = server_new_layer_surface;
    wl_signal_add(&cs.layer_shell->events.new_surface, &cs.new_layer_surface);
    
    cs.xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(cs.wl_display);
    if (!cs.xdg_decoration_manager) {
        fprintf(stderr, "Failed to create xdg decoration manager\n");
        goto cleanup;
    }
    cs.new_xdg_toplevel_decoration.notify = server_new_xdg_toplevel_decoration;
    wl_signal_add(&cs.xdg_decoration_manager->events.new_toplevel_decoration, &cs.new_xdg_toplevel_decoration);

    wlr_screencopy_manager_v1_create(cs.wl_display);
    wlr_export_dmabuf_manager_v1_create(cs.wl_display);
    wlr_fractional_scale_manager_v1_create(cs.wl_display, 1);
    wlr_linux_dmabuf_v1_create_with_renderer(cs.wl_display, 4, cs.renderer);
    wlr_single_pixel_buffer_manager_v1_create(cs.wl_display);

    sigint = wl_event_loop_add_signal(cs.wl_event_loop, SIGINT, handle_signal, cs.wl_display);
    sigterm = wl_event_loop_add_signal(cs.wl_event_loop, SIGTERM, handle_signal, cs.wl_display);

    const char *socket = wl_display_add_socket_auto(cs.wl_display);
    if (!socket) {
        fprintf(stderr, "Failed to add socket\n");
        goto cleanup;
    }

    setenv("WAYLAND_DISPLAY", socket, true);
    unsetenv("DISPLAY");

    if (!wlr_backend_start(cs.backend)) {
        fprintf(stderr, "Failed to start backend\n");
        goto cleanup;
    }

    /* Запускаем exec-once команды из конфига */
    extern char **environ;
    for (int i = 0; i < cfg.num_exec_once; i++) {
        pid_t pid;
        char *cmd = cfg.exec_once[i];
        char *argv[] = {"sh", "-c", cmd, NULL};
        posix_spawnp(&pid, "sh", NULL, NULL, argv, environ);
    }
    
    printf("Compositor running on WAYLAND_DISPLAY=%s\n", socket);
    printf("Press Alt+Return for terminal, Alt+Escape to exit\n");

    wl_display_run(cs.wl_display);
    
cleanup:
    wl_display_destroy_clients(cs.wl_display);

    wl_list_remove(&cs.request_set_cursor.link);
    wl_list_remove(&cs.request_set_selection.link);
    wl_list_remove(&cs.request_set_primary_selection.link);
    wl_list_remove(&cs.new_xdg_toplevel.link);
    wl_list_remove(&cs.new_xdg_popup.link);
    wl_list_remove(&cs.cursor_motion.link);
    wl_list_remove(&cs.cursor_motion_absolute.link);
    wl_list_remove(&cs.cursor_button.link);
    wl_list_remove(&cs.cursor_axis.link);
    wl_list_remove(&cs.cursor_frame.link);
    wl_list_remove(&cs.new_input.link);
    wl_list_remove(&cs.new_output.link);
    wl_list_remove(&cs.new_layer_surface.link);

    if (sigint) wl_event_source_remove(sigint);
    if (sigterm) wl_event_source_remove(sigterm);
    
    if (cs.scene) wlr_scene_node_destroy(&cs.scene->tree.node);
    if (cs.cursor_mgr) wlr_xcursor_manager_destroy(cs.cursor_mgr);
    if (cs.cursor) wlr_cursor_destroy(cs.cursor);
    if (cs.allocator) wlr_allocator_destroy(cs.allocator);
    if (cs.renderer) wlr_renderer_destroy(cs.renderer);
    if (cs.backend) wlr_backend_destroy(cs.backend);
    if (cs.output_layout) wlr_output_layout_destroy(cs.output_layout);
    if (cs.seat) wlr_seat_destroy(cs.seat);
    if (cs.wlr_session) wlr_session_destroy(cs.wlr_session);
    wl_display_destroy(cs.wl_display);
    
    return 0;
}
