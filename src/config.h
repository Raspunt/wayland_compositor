#ifndef CONFIG_H
#define CONFIG_H

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
};

#define MAX_BINDINGS 128
#define MAX_VARS 32

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
};

int config_load(struct compositor_config *cfg);

#endif
