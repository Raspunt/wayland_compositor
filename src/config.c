#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/types/wlr_keyboard.h>

#include "config.h"

static char *get_config_path(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg) {
        size_t len = strlen(xdg) + 32;
        char *path = malloc(len);
        snprintf(path, len, "%s/flottywm/config", xdg);
        return path;
    }
    const char *home = getenv("HOME");
    if (home) {
        size_t len = strlen(home) + 32;
        char *path = malloc(len);
        snprintf(path, len, "%s/.config/flottywm/config", home);
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
    if (strcmp(s, "focusleft") == 0) return BINDING_FOCUS_LEFT;
    if (strcmp(s, "focusright") == 0) return BINDING_FOCUS_RIGHT;
    if (strcmp(s, "focusup") == 0) return BINDING_FOCUS_UP;
    if (strcmp(s, "focusdown") == 0) return BINDING_FOCUS_DOWN;
    if (strcmp(s, "togglefloating") == 0) return BINDING_TOGGLE_FLOATING;
    if (strcmp(s, "reload") == 0) return BINDING_RELOAD;
    return BINDING_NONE;
}

void config_free(struct compositor_config *cfg) {
    for (int i = 0; i < cfg->num_bindings; i++) {
        free(cfg->bindings[i].arg);
    }
    for (int i = 0; i < cfg->num_vars; i++) {
        free(cfg->vars[i].name);
        free(cfg->vars[i].value);
    }
    for (int i = 0; i < cfg->num_exec_once; i++) {
        free(cfg->exec_once[i]);
    }
    for (int i = 0; i < cfg->num_envs; i++) {
        free(cfg->envs[i].name);
        free(cfg->envs[i].value);
    }
    free(cfg->kb_layout);
    free(cfg->kb_options);
    memset(cfg, 0, sizeof(*cfg));
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

static bool copy_dir_contents(const char *src_dir, const char *dst_dir) {
    DIR *d = opendir(src_dir);
    if (!d) return false;
    
    struct dirent *entry;
    bool ok = true;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char src_path[2048];
        char dst_path[2048];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);
        
        FILE *src = fopen(src_path, "rb");
        if (!src) { ok = false; continue; }
        FILE *dst = fopen(dst_path, "wb");
        if (!dst) { fclose(src); ok = false; continue; }
        
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
            fwrite(buf, 1, n, dst);
        }
        fclose(src);
        fclose(dst);
    }
    closedir(d);
    return ok;
}

static bool copy_default_config(const char *dest_path) {
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) return false;
    exe_path[len] = '\0';
    
    char *last_slash = strrchr(exe_path, '/');
    if (!last_slash) return false;
    *last_slash = '\0';
    
    char src_dir[2048];
    snprintf(src_dir, sizeof(src_dir), "%s/config", exe_path);
    
    char *dest_dir = strdup(dest_path);
    char *last_slash2 = strrchr(dest_dir, '/');
    if (last_slash2) {
        *last_slash2 = '\0';
        mkdir(dest_dir, 0755);
    }
    bool ok = copy_dir_contents(src_dir, dest_dir);
    free(dest_dir);
    return ok;
}

int config_load(struct compositor_config *cfg) {
    cfg->gaps = 8;
    cfg->outer_gaps = 8;
    cfg->mfact = 0.55;
    cfg->num_bindings = 0;
    cfg->num_vars = 0;
    cfg->num_exec_once = 0;
    cfg->num_envs = 0;
    cfg->focus_follows_mouse = true;
    cfg->border_width = 3;
    cfg->border_focused[0] = 1.0f;
    cfg->border_focused[1] = 0.85f;
    cfg->border_focused[2] = 0.0f;
    cfg->border_focused[3] = 1.0f;
    cfg->border_unfocused[0] = 0.2f;
    cfg->border_unfocused[1] = 0.2f;
    cfg->border_unfocused[2] = 0.2f;
    cfg->border_unfocused[3] = 1.0f;
    
    char *path = get_config_path();
    if (!path) return -1;
    
    FILE *f = fopen(path, "r");
    if (!f) {
        char *dir = strdup(path);
        char *last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            mkdir(dir, 0755);
        }
        free(dir);
        
        if (copy_default_config(path)) {
            printf("FlottyWM: created default config at %s\n", path);
        }
        
        f = fopen(path, "r");
    }
    if (!f) {
        free(path);
        return -1;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;
        
        char *comment = strchr(p, '#');
        if (comment) *comment = '\0';
        
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
        else if (strcmp(key, "exec-once") == 0) {
            if (cfg->num_exec_once >= MAX_EXEC_ONCE) continue;
            cfg->exec_once[cfg->num_exec_once++] = strdup(v);
        }
        else if (strcmp(key, "env") == 0) {
            if (cfg->num_envs >= MAX_ENVS) continue;
            char env_name[64], env_value[256];
            if (sscanf(v, "%63s %255[^\n]", env_name, env_value) >= 1) {
                cfg->envs[cfg->num_envs].name = strdup(env_name);
                cfg->envs[cfg->num_envs].value = strdup(env_value);
                cfg->num_envs++;
            }
        }
        else if (strcmp(key, "kb_layout") == 0) {
            free(cfg->kb_layout);
            cfg->kb_layout = strdup(v);
        }
        else if (strcmp(key, "kb_options") == 0) {
            free(cfg->kb_options);
            cfg->kb_options = strdup(v);
        }
        else if (strcmp(key, "tap_to_click") == 0) {
            cfg->tap_to_click = (strcmp(v, "true") == 0 ||
                                 strcmp(v, "1") == 0 ||
                                 strcmp(v, "yes") == 0 ||
                                 strcmp(v, "on") == 0);
        }
        else if (strcmp(key, "focus_follows_mouse") == 0) {
            cfg->focus_follows_mouse = (strcmp(v, "true") == 0 ||
                                        strcmp(v, "1") == 0 ||
                                        strcmp(v, "yes") == 0 ||
                                        strcmp(v, "on") == 0);
        }
        else if (strcmp(key, "border_width") == 0) {
            cfg->border_width = atoi(v);
        }
        else if (strcmp(key, "border_focused") == 0) {
            float r, g, b, a;
            if (sscanf(v, "%f %f %f %f", &r, &g, &b, &a) == 4) {
                cfg->border_focused[0] = r;
                cfg->border_focused[1] = g;
                cfg->border_focused[2] = b;
                cfg->border_focused[3] = a;
            }
        }
        else if (strcmp(key, "border_unfocused") == 0) {
            float r, g, b, a;
            if (sscanf(v, "%f %f %f %f", &r, &g, &b, &a) == 4) {
                cfg->border_unfocused[0] = r;
                cfg->border_unfocused[1] = g;
                cfg->border_unfocused[2] = b;
                cfg->border_unfocused[3] = a;
            }
        }
        else if (strcmp(key, "bind") == 0) {
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
    free(path);
    return 0;
}
