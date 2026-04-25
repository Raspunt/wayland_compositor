#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/types/wlr_keyboard.h>

#include "config.h"

static char *get_config_path(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg) {
        size_t len = strlen(xdg) + 32;
        char *path = malloc(len);
        snprintf(path, len, "%s/flotty/config", xdg);
        return path;
    }
    const char *home = getenv("HOME");
    if (home) {
        size_t len = strlen(home) + 32;
        char *path = malloc(len);
        snprintf(path, len, "%s/.config/flotty/config", home);
        return path;
    }
    return NULL;
}

static void parse_mods_key(const char *str, uint32_t *mods, xkb_keysym_t *sym) {
    *mods = 0;
    *sym = XKB_KEY_NoSymbol;
    
    char tmp[128];
    strncpy(tmp, str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    
    char *token = strtok(tmp, "+");
    while (token) {
        if (strcmp(token, "Alt") == 0) *mods |= WLR_MODIFIER_ALT;
        else if (strcmp(token, "Shift") == 0) *mods |= WLR_MODIFIER_SHIFT;
        else if (strcmp(token, "Ctrl") == 0) *mods |= WLR_MODIFIER_CTRL;
        else if (strcmp(token, "Super") == 0 || strcmp(token, "Win") == 0 || strcmp(token, "Logo") == 0)
            *mods |= WLR_MODIFIER_LOGO;
        else {
            *sym = xkb_keysym_from_name(token, XKB_KEYSYM_NO_FLAGS);
        }
        token = strtok(NULL, "+");
    }
}

static enum binding_action parse_action(const char *s) {
    if (strcmp(s, "exec") == 0) return BINDING_EXEC;
    if (strcmp(s, "exit") == 0) return BINDING_EXIT;
    if (strcmp(s, "workspace") == 0) return BINDING_WORKSPACE;
    if (strcmp(s, "movetoworkspace") == 0) return BINDING_MOVETOWORKSPACE;
    if (strcmp(s, "focusprev") == 0) return BINDING_FOCUS_PREV;
    if (strcmp(s, "focusnext") == 0) return BINDING_FOCUS_NEXT;
    if (strcmp(s, "moveprev") == 0) return BINDING_MOVE_PREV;
    if (strcmp(s, "movenext") == 0) return BINDING_MOVE_NEXT;
    if (strcmp(s, "kill") == 0) return BINDING_KILL;
    if (strcmp(s, "resizeleft") == 0) return BINDING_RESIZE_LEFT;
    if (strcmp(s, "resizeright") == 0) return BINDING_RESIZE_RIGHT;
    if (strcmp(s, "resizeup") == 0) return BINDING_RESIZE_UP;
    if (strcmp(s, "resizedown") == 0) return BINDING_RESIZE_DOWN;
    return BINDING_NONE;
}

static void set_var(struct compositor_config *cfg, const char *name, const char *value) {
    for (int i = 0; i < cfg->num_vars; i++) {
        if (strcmp(cfg->vars[i].name, name) == 0) {
            free(cfg->vars[i].value);
            cfg->vars[i].value = strdup(value);
            return;
        }
    }
    if (cfg->num_vars >= MAX_VARS) return;
    cfg->vars[cfg->num_vars].name = strdup(name);
    cfg->vars[cfg->num_vars].value = strdup(value);
    cfg->num_vars++;
}

static const char *get_var(struct compositor_config *cfg, const char *name) {
    for (int i = 0; i < cfg->num_vars; i++) {
        if (strcmp(cfg->vars[i].name, name) == 0) {
            return cfg->vars[i].value;
        }
    }
    return NULL;
}

static void expand_vars(struct compositor_config *cfg, char *str, size_t max_len) {
    char result[512] = {0};
    const char *p = str;
    while (*p) {
        if (*p == '$') {
            const char *start = p;
            p++;
            char name[64];
            int i = 0;
            while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
                if (i < 63) name[i++] = *p;
                p++;
            }
            name[i] = '\0';
            char full_name[65];
            snprintf(full_name, sizeof(full_name), "$%s", name);
            const char *val = get_var(cfg, full_name);
            if (val) {
                strncat(result, val, sizeof(result) - strlen(result) - 1);
            } else {
                strncat(result, start, p - start);
            }
        } else {
            size_t len = strlen(result);
            if (len < sizeof(result) - 1) {
                result[len] = *p;
                result[len + 1] = '\0';
            }
            p++;
        }
    }
    if (max_len > 0) {
        strncpy(str, result, max_len);
        str[max_len - 1] = '\0';
    }
}

int config_load(struct compositor_config *cfg) {
    cfg->gaps = 8;
    cfg->outer_gaps = 8;
    cfg->mfact = 0.55;
    cfg->term = strdup("alacritty");
    cfg->launcher = strdup("fuzzel");
    cfg->num_bindings = 0;
    cfg->num_vars = 0;
    
    char *path = get_config_path();
    if (!path) return -1;
    
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;
        
        char key[64], value[512];
        if (sscanf(p, "%63s %511[^\n]", key, value) < 1) continue;
        
        char *v = value;
        while (isspace((unsigned char)*v)) v++;
        size_t len = strlen(v);
        while (len > 0 && isspace((unsigned char)v[len - 1])) v[--len] = '\0';
        
        expand_vars(cfg, v, sizeof(line) - (v - line));
        
        if (key[0] == '$') {
            if (v[0]) {
                set_var(cfg, key, v);
            }
            continue;
        }
        
        if (strcmp(key, "gaps") == 0) cfg->gaps = atoi(v);
        else if (strcmp(key, "outer_gaps") == 0) cfg->outer_gaps = atoi(v);
        else if (strcmp(key, "mfact") == 0) cfg->mfact = atof(v);
        else if (strcmp(key, "term") == 0) {
            free(cfg->term);
            cfg->term = strdup(v);
            set_var(cfg, "$term", v);
        } else if (strcmp(key, "launcher") == 0) {
            free(cfg->launcher);
            cfg->launcher = strdup(v);
            set_var(cfg, "$launcher", v);
        } else if (strcmp(key, "bind") == 0) {
            if (cfg->num_bindings >= MAX_BINDINGS) continue;
            char bind_str[64], action_str[32], arg_str[256];
            int n = sscanf(v, "%63s %31s %255[^\n]", bind_str, action_str, arg_str);
            if (n < 2) continue;
            
            expand_vars(cfg, bind_str, sizeof(bind_str));
            if (n >= 3) expand_vars(cfg, arg_str, sizeof(arg_str));
            
            struct keybinding *kb = &cfg->bindings[cfg->num_bindings++];
            parse_mods_key(bind_str, &kb->modifiers, &kb->keysym);
            kb->action = parse_action(action_str);
            if (n >= 3 && arg_str[0]) {
                kb->arg = strdup(arg_str);
            } else {
                kb->arg = NULL;
            }
        }
    }
    fclose(f);
    return 0;
}
