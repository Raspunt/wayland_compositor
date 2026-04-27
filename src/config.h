#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

enum binding_action {
    BINDING_NONE,
    BINDING_EXEC,
    BINDING_EXIT,
    BINDING_WORKSPACE,
    BINDING_MOVETOWORKSPACE,
    BINDING_FOCUS_PREV,
    BINDING_FOCUS_NEXT,
    BINDING_MOVE_PREV,
    BINDING_MOVE_NEXT,
    BINDING_KILL,
    BINDING_RESIZE_LEFT,
    BINDING_RESIZE_RIGHT,
    BINDING_RESIZE_UP,
    BINDING_RESIZE_DOWN,
    BINDING_FOCUS_LEFT,
    BINDING_FOCUS_RIGHT,
    BINDING_FOCUS_UP,
    BINDING_FOCUS_DOWN,
    BINDING_TOGGLE_FLOATING,
    BINDING_RELOAD,
};

#define MAX_BINDINGS 128
#define MAX_VARS 32
#define MAX_EXEC_ONCE 32
#define MAX_ENVS 32

struct config_var {
    char *name;
    char *value;
};

struct keybinding {
    uint32_t modifiers;
    xkb_keysym_t keysym;
    enum binding_action action;
    char *arg;
};

struct compositor_config {
    int gaps;
    int outer_gaps;
    double mfact;
    struct keybinding bindings[MAX_BINDINGS];
    int num_bindings;
    struct config_var vars[MAX_VARS];
    int num_vars;
    char *exec_once[MAX_EXEC_ONCE];
    int num_exec_once;
    struct env_var {
        char *name;
        char *value;
    } envs[MAX_ENVS];
    int num_envs;

    char *kb_layout;
    char *kb_options;

    bool tap_to_click;
    bool focus_follows_mouse;
    int border_width;
    float border_focused[4];
    float border_unfocused[4];
};

int config_load(struct compositor_config *cfg);
void config_free(struct compositor_config *cfg);

#endif
