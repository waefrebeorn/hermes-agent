/*
 * skin.c — Skin engine for data-driven CLI theming.
 * MIT License — WuBu Hermes Project
 */

#include "skin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"  /* Reuse libjson */

/* ================================================================
 *  Skin struct
 * ================================================================ */

struct skin_t {
    char    name[64];
    json_t *root;  /* parsed JSON */
};

/* ================================================================
 *  Error
 * ================================================================ */

static char skin_err[256] = "";

const char *skin_error(void) {
    return skin_err[0] ? skin_err : NULL;
}

/* ================================================================
 *  Default skin
 * ================================================================ */

static const char *DEFAULT_SKIN_JSON =
    "{"
    "  \"name\": \"default\","
    "  \"colors\": {"
    "    \"banner\": \"cyan\","
    "    \"prompt\": \"green\","
    "    \"response\": \"white\","
    "    \"error\": \"red\","
    "    \"dim\": \"white\","
    "    \"tool\": \"yellow\","
    "    \"thinking\": \"magenta\""
    "  },"
    "  \"symbols\": {"
    "    \"prompt\": \"slermes>\","
    "    \"tool\": \"⚙\","
    "    \"thinking\": \"⋯\""
    "  },"
    "  \"format\": {"
    "    \"banner_bold\": true,"
    "    \"show_model\": true"
    "  }"
    "}";

skin_t *skin_default(void) {
    return skin_load_string(DEFAULT_SKIN_JSON);
}

/* ================================================================
 *  Load
 * ================================================================ */

skin_t *skin_load(const char *path) {
    if (!path || !*path) {
        snprintf(skin_err, sizeof(skin_err), "NULL path");
        return NULL;
    }

    char *err = NULL;
    json_t *root = json_parse_file(path, &err);
    if (!root) {
        snprintf(skin_err, sizeof(skin_err), "JSON parse error: %s", err ? err : "unknown");
        free(err);
        return NULL;
    }

    skin_t *s = (skin_t *)calloc(1, sizeof(skin_t));
    if (!s) { json_free(root); snprintf(skin_err, sizeof(skin_err), "OOM"); return NULL; }

    s->root = root;
    const char *name = json_get_str(root, "name", "custom");
    snprintf(s->name, sizeof(s->name), "%s", name);

    return s;
}

skin_t *skin_load_string(const char *json) {
    if (!json) return NULL;

    char *err = NULL;
    json_t *root = json_parse(json, &err);
    if (!root) {
        snprintf(skin_err, sizeof(skin_err), "JSON parse error: %s", err ? err : "unknown");
        free(err);
        return NULL;
    }

    skin_t *s = (skin_t *)calloc(1, sizeof(skin_t));
    if (!s) { json_free(root); snprintf(skin_err, sizeof(skin_err), "OOM"); return NULL; }

    s->root = root;
    const char *name = json_get_str(root, "name", "custom");
    snprintf(s->name, sizeof(s->name), "%s", name);

    return s;
}

skin_t *skin_with_overrides(const char *overrides_json) {
    skin_t *s = skin_default();
    if (!s || !overrides_json) return s;

    char *err = NULL;
    json_t *overrides = json_parse(overrides_json, &err);
    if (!overrides) { free(err); return s; }

    /* Merge overrides into default. For each key in overrides, set it in root. */
    /* Simple approach: for known keys, copy from overrides */
    const char *name = json_get_str(overrides, "name", NULL);
    if (name) json_set(s->root, "name", json_string(name));

    /* Merge colors */
    json_t *colors = json_obj_get(overrides, "colors");
    if (colors) {
        json_t *def_colors = json_obj_get(s->root, "colors");
        if (!def_colors) {
            json_set(s->root, "colors", json_copy(colors));
        } else {
            /* Copy each key from overrides */
            /* Simple: just iterate known color names */
            const char *color_keys[] = {"banner", "prompt", "response", "error", "dim", "tool", "thinking", NULL};
            for (int i = 0; color_keys[i]; i++) {
                const char *v = json_get_str(colors, color_keys[i], NULL);
                if (v) json_set(def_colors, color_keys[i], json_string(v));
            }
        }
    }

    /* Merge symbols */
    json_t *symbols = json_obj_get(overrides, "symbols");
    if (symbols) {
        json_t *def_sym = json_obj_get(s->root, "symbols");
        if (!def_sym) {
            json_set(s->root, "symbols", json_copy(symbols));
        } else {
            const char *sym_keys[] = {"prompt", "tool", "thinking", NULL};
            for (int i = 0; sym_keys[i]; i++) {
                const char *v = json_get_str(symbols, sym_keys[i], NULL);
                if (v) json_set(def_sym, sym_keys[i], json_string(v));
            }
        }
    }

    /* Merge format */
    json_t *fmt = json_obj_get(overrides, "format");
    if (fmt) {
        json_t *def_fmt = json_obj_get(s->root, "format");
        if (!def_fmt) {
            json_set(s->root, "format", json_copy(fmt));
        } else {
            if (json_obj_get(fmt, "banner_bold"))
                json_set(def_fmt, "banner_bold", json_bool(json_get_bool(fmt, "banner_bold", true)));
            if (json_obj_get(fmt, "show_model"))
                json_set(def_fmt, "show_model", json_bool(json_get_bool(fmt, "show_model", true)));
        }
    }

    json_free(overrides);
    return s;
}

/* ================================================================
 *  Queries
 * ================================================================ */

/* Navigate a dotted path into JSON tree */
static json_t *resolve_path(json_t *root, const char *key) {
    if (!root || !key) return root;

    char path[256];
    snprintf(path, sizeof(path), "%s", key);

    json_t *node = root;
    char *part = strtok(path, ".");
    while (part && node) {
        if (json_len(node) == 0 || json_obj_get(node, part)) {
            node = json_obj_get(node, part);
        } else {
            /* Try direct key lookup on the original parent */
            break;
        }
        part = strtok(NULL, ".");
    }
    return node;
}

const char *skin_get(const skin_t *s, const char *key, const char *def) {
    if (!s || !s->root || !key) return def;
    json_t *node = resolve_path(s->root, key);
    if (!node) return def;

    if (json_len(node) == 0) return def; /* Not a leaf */

    const char *str = json_get_str(node, key, NULL);
    if (str) return str;

    /* Try obj_get on parent */
    char *dot = strrchr(key, '.');
    if (dot) {
        char parent[256];
        size_t plen = (size_t)(dot - key);
        memcpy(parent, key, plen);
        parent[plen] = '\0';
        json_t *p = resolve_path(s->root, parent);
        if (p) {
            const char *v = json_get_str(p, dot + 1, NULL);
            if (v) return v;
        }
    }

    /* Try direct key (for flat paths) */
    json_t *direct = json_obj_get(s->root, key);
    if (direct && direct->type == JSON_STRING) return direct->str_val;

    return def;
}

bool skin_get_bool(const skin_t *s, const char *key, bool def) {
    const char *v = skin_get(s, key, NULL);
    if (!v) return def;
    return strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0;
}

const char *skin_name(const skin_t *s) {
    return s ? s->name : "default";
}

/* ================================================================
 *  ANSI color resolution
 * ================================================================ */

const char *skin_ansi_color(const char *name) {
    if (!name) return "";

    /* Parse "color:modifier" format */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", name);
    char *color = buf;
    char *mod = strchr(buf, ':');
    if (mod) { *mod = '\0'; mod++; }

    /* Base color */
    const char *fg = "";
    if (strcmp(color, "black") == 0)     fg = "\033[30m";
    else if (strcmp(color, "red") == 0)       fg = "\033[31m";
    else if (strcmp(color, "green") == 0)     fg = "\033[32m";
    else if (strcmp(color, "yellow") == 0)    fg = "\033[33m";
    else if (strcmp(color, "blue") == 0)      fg = "\033[34m";
    else if (strcmp(color, "magenta") == 0)   fg = "\033[35m";
    else if (strcmp(color, "cyan") == 0)      fg = "\033[36m";
    else if (strcmp(color, "white") == 0)     fg = "\033[37m";
    else return "";

    /* Modifier */
    if (mod) {
        if (strcmp(mod, "bold") == 0) {
            static char bold_ansi[16];
            snprintf(bold_ansi, sizeof(bold_ansi), "\033[1;%dm", fg[2]); /* keep last digit */
            const char *d = fg + 2;
            while (*d >= '0' && *d <= '9') d++;
            int code = (int)(fg[2] - '0');
            snprintf(bold_ansi, sizeof(bold_ansi), "\033[1;%dm", code);
            return bold_ansi;
        }
        if (strcmp(mod, "dim") == 0) {
            static char dim_ansi[16];
            int code = (int)(fg[2] - '0');
            snprintf(dim_ansi, sizeof(dim_ansi), "\033[2;%dm", code);
            return dim_ansi;
        }
    }

    static char result[16];
    snprintf(result, sizeof(result), "%s", fg);
    return result;
}

const char *skin_color(const skin_t *s, const char *key) {
    char color_key[128];
    snprintf(color_key, sizeof(color_key), "colors.%s", key);
    const char *color_name = skin_get(s, color_key, "white");
    return skin_ansi_color(color_name);
}

void skin_apply_color(const skin_t *s, const char *key, const char **fg, int *bold) {
    *fg = skin_color(s, key);
    *bold = 0;

    char bold_key[128];
    snprintf(bold_key, sizeof(bold_key), "format.%s_bold", key);
    if (skin_get_bool(s, bold_key, false)) *bold = 1;

    /* Check for bold in color name itself */
    char color_key[128];
    snprintf(color_key, sizeof(color_key), "colors.%s", key);
    const char *name = skin_get(s, color_key, "white");
    if (strstr(name, "bold")) *bold = 1;
}

/* ================================================================
 *  Free
 * ================================================================ */

void skin_free(skin_t *s) {
    if (!s) return;
    json_free(s->root);
    free(s);
}
