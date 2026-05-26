/*
 * skin.c — Skin engine for data-driven CLI theming.
 * MIT License — WuBu Hermes Project
 *
 * Supports:
 *   - Named ANSI colors: black, red, green, yellow, blue, magenta, cyan, white
 *   - With modifier:     cyan:bold, white:dim
 *   - Hex 24-bit:        #FFD700, #CD7F32  → ANSI 38;2;R;G;B
 *   - Built-in skins:    default, ares, mono, slate, daylight
 *   - External JSON skin files loaded at runtime
 */

#include "skin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
 *  Built-in skin JSON definitions
 * ================================================================ */

/* Default — Classic Hermes gold/kawaii (matches Python default) */
static const char *DEFAULT_SKIN_JSON =
    "{"
    "\"name\":\"default\","
    "\"description\":\"Classic Hermes - gold and kawaii\","
    "\"colors\":{"
        "\"banner_border\":\"#CD7F32\","
        "\"banner_title\":\"#FFD700\","
        "\"banner_accent\":\"#FFBF00\","
        "\"banner_dim\":\"#B8860B\","
        "\"banner_text\":\"#FFF8DC\","
        "\"ui_accent\":\"#FFBF00\","
        "\"ui_label\":\"#DAA520\","
        "\"ui_ok\":\"#4caf50\","
        "\"ui_error\":\"#ef5350\","
        "\"ui_warn\":\"#ffa726\","
        "\"prompt\":\"#FFF8DC\","
        "\"input_rule\":\"#CD7F32\","
        "\"response_border\":\"#FFD700\","
        "\"status_bar_bg\":\"#1a1a2e\","
        "\"status_bar_text\":\"#C0C0C0\","
        "\"status_bar_strong\":\"#FFD700\","
        "\"status_bar_dim\":\"#8B8682\","
        "\"status_bar_good\":\"#8FBC8F\","
        "\"status_bar_warn\":\"#FFD700\","
        "\"status_bar_bad\":\"#FF8C00\","
        "\"status_bar_critical\":\"#FF6B6B\","
        "\"session_label\":\"#DAA520\","
        "\"session_border\":\"#8B8682\","
        "\"tool\":\"yellow\","
        "\"thinking\":\"magenta\","
        "\"error\":\"red\","
        "\"dim\":\"white:dim\""
    "},"
    "\"branding\":{"
        "\"agent_name\":\"Slermes Agent\","
        "\"welcome\":\"Welcome to Slermes Agent! Type your message or /help for commands.\","
        "\"goodbye\":\"Goodbye! \\u2695\","
        "\"response_label\":\" \\u2695 Slermes \","
        "\"prompt_symbol\":\"\\u276f\","
        "\"help_header\":\"(^_^)? Available Commands\""
    "},"
    "\"symbols\":{"
        "\"prompt\":\"\\u03bb\","
        "\"tool\":\"\\u2699\","
        "\"thinking\":\"\\u22ef\""
    "},"
    "\"format\":{"
        "\"banner_bold\":true,"
        "\"show_model\":true"
    "}"
    "}";

/* Ares — Crimson/bronze war-god theme */
static const char *ARES_SKIN_JSON =
    "{"
    "\"name\":\"ares\","
    "\"description\":\"War-god theme - crimson and bronze\","
    "\"colors\":{"
        "\"banner_border\":\"#9F1C1C\","
        "\"banner_title\":\"#C7A96B\","
        "\"banner_accent\":\"#DD4A3A\","
        "\"banner_dim\":\"#6B1717\","
        "\"banner_text\":\"#F1E6CF\","
        "\"ui_accent\":\"#DD4A3A\","
        "\"ui_label\":\"#C7A96B\","
        "\"ui_ok\":\"#4caf50\","
        "\"ui_error\":\"#ef5350\","
        "\"ui_warn\":\"#ffa726\","
        "\"prompt\":\"#F1E6CF\","
        "\"input_rule\":\"#9F1C1C\","
        "\"response_border\":\"#C7A96B\","
        "\"status_bar_bg\":\"#2A1212\","
        "\"status_bar_text\":\"#F1E6CF\","
        "\"status_bar_strong\":\"#C7A96B\","
        "\"status_bar_dim\":\"#6E584B\","
        "\"status_bar_good\":\"#7BC96F\","
        "\"status_bar_warn\":\"#C7A96B\","
        "\"status_bar_bad\":\"#DD4A3A\","
        "\"status_bar_critical\":\"#EF5350\","
        "\"session_label\":\"#C7A96B\","
        "\"session_border\":\"#6E584B\","
        "\"tool\":\"yellow\","
        "\"thinking\":\"magenta\","
        "\"error\":\"red\","
        "\"dim\":\"white:dim\""
    "},"
    "\"branding\":{"
        "\"agent_name\":\"Ares Agent\","
        "\"welcome\":\"Welcome to Ares Agent! Type your message or /help for commands.\","
        "\"goodbye\":\"Farewell, warrior! \\u2694\","
        "\"response_label\":\" \\u2694 Ares \","
        "\"prompt_symbol\":\"\\u2694\","
        "\"help_header\":\"(\\u2694) Available Commands\""
    "}"
    "}";

/* Mono — Clean grayscale monochrome */
static const char *MONO_SKIN_JSON =
    "{"
    "\"name\":\"mono\","
    "\"description\":\"Monochrome - clean grayscale\","
    "\"colors\":{"
        "\"banner_border\":\"#555555\","
        "\"banner_title\":\"#e6edf3\","
        "\"banner_accent\":\"#aaaaaa\","
        "\"banner_dim\":\"#444444\","
        "\"banner_text\":\"#c9d1d9\","
        "\"ui_accent\":\"#aaaaaa\","
        "\"ui_label\":\"#888888\","
        "\"ui_ok\":\"#888888\","
        "\"ui_error\":\"#cccccc\","
        "\"ui_warn\":\"#999999\","
        "\"prompt\":\"#c9d1d9\","
        "\"input_rule\":\"#444444\","
        "\"response_border\":\"#aaaaaa\","
        "\"status_bar_bg\":\"#1F1F1F\","
        "\"status_bar_text\":\"#C9D1D9\","
        "\"status_bar_strong\":\"#E6EDF3\","
        "\"status_bar_dim\":\"#777777\","
        "\"status_bar_good\":\"#B5B5B5\","
        "\"status_bar_warn\":\"#AAAAAA\","
        "\"status_bar_bad\":\"#D0D0D0\","
        "\"status_bar_critical\":\"#F0F0F0\","
        "\"session_label\":\"#888888\","
        "\"session_border\":\"#555555\","
        "\"tool\":\"white\","
        "\"thinking\":\"white\","
        "\"error\":\"white\","
        "\"dim\":\"white:dim\""
    "},"
    "\"branding\":{"
        "\"agent_name\":\"Hermes Agent\","
        "\"welcome\":\"Welcome to Hermes Agent! Type your message or /help for commands.\","
        "\"goodbye\":\"Goodbye! \\u2695\","
        "\"response_label\":\" \\u2695 Hermes \","
        "\"prompt_symbol\":\"\\u276f\","
        "\"help_header\":\"[?] Available Commands\""
    "}"
    "}";

/* Slate — Cool blue developer-focused theme */
static const char *SLATE_SKIN_JSON =
    "{"
    "\"name\":\"slate\","
    "\"description\":\"Cool blue - developer-focused\","
    "\"colors\":{"
        "\"banner_border\":\"#4169e1\","
        "\"banner_title\":\"#7eb8f6\","
        "\"banner_accent\":\"#8EA8FF\","
        "\"banner_dim\":\"#4b5563\","
        "\"banner_text\":\"#c9d1d9\","
        "\"ui_accent\":\"#7eb8f6\","
        "\"ui_label\":\"#8EA8FF\","
        "\"ui_ok\":\"#63D0A6\","
        "\"ui_error\":\"#F7A072\","
        "\"ui_warn\":\"#e6a855\","
        "\"prompt\":\"#c9d1d9\","
        "\"input_rule\":\"#4169e1\","
        "\"response_border\":\"#7eb8f6\","
        "\"status_bar_bg\":\"#151C2F\","
        "\"status_bar_text\":\"#C9D1D9\","
        "\"status_bar_strong\":\"#7EB8F6\","
        "\"status_bar_dim\":\"#4B5563\","
        "\"status_bar_good\":\"#63D0A6\","
        "\"status_bar_warn\":\"#E6A855\","
        "\"status_bar_bad\":\"#F7A072\","
        "\"status_bar_critical\":\"#FF7A7A\","
        "\"session_label\":\"#7eb8f6\","
        "\"session_border\":\"#4b5563\""
    "},"
    "\"branding\":{"
        "\"agent_name\":\"Hermes Agent\","
        "\"welcome\":\"Welcome to Hermes Agent! Type your message or /help for commands.\","
        "\"goodbye\":\"Goodbye! \\u2695\","
        "\"response_label\":\" \\u2695 Hermes \","
        "\"prompt_symbol\":\"\\u276f\","
        "\"help_header\":\"(^_^)? Available Commands\""
    "}"
    "}";

/* Daylight — Light background theme with dark text */
static const char *DAYLIGHT_SKIN_JSON =
    "{"
    "\"name\":\"daylight\","
    "\"description\":\"Light theme for bright terminals with dark text\","
    "\"colors\":{"
        "\"banner_border\":\"#2563EB\","
        "\"banner_title\":\"#0F172A\","
        "\"banner_accent\":\"#1D4ED8\","
        "\"banner_dim\":\"#475569\","
        "\"banner_text\":\"#111827\","
        "\"ui_accent\":\"#2563EB\","
        "\"ui_label\":\"#0F766E\","
        "\"ui_ok\":\"#15803D\","
        "\"ui_error\":\"#B91C1C\","
        "\"ui_warn\":\"#B45309\","
        "\"prompt\":\"#111827\","
        "\"input_rule\":\"#93C5FD\","
        "\"response_border\":\"#2563EB\","
        "\"status_bar_bg\":\"#E5EDF8\","
        "\"status_bar_text\":\"#0F172A\","
        "\"status_bar_strong\":\"#1D4ED8\","
        "\"status_bar_dim\":\"#64748B\","
        "\"status_bar_good\":\"#15803D\","
        "\"status_bar_warn\":\"#B45309\","
        "\"status_bar_bad\":\"#B91C1C\","
        "\"status_bar_critical\":\"#991B1B\","
        "\"session_label\":\"#1D4ED8\","
        "\"session_border\":\"#64748B\""
    "},"
    "\"branding\":{"
        "\"agent_name\":\"Hermes Agent\","
        "\"welcome\":\"Welcome to Hermes Agent! Type your message or /help for commands.\","
        "\"goodbye\":\"Goodbye! \\u2695\","
        "\"response_label\":\" \\u2695 Hermes \","
        "\"prompt_symbol\":\"\\u276f\","
        "\"help_header\":\"[?] Available Commands\""
    "}"
    "}";

/* Built-in skin accessor — returns JSON string by index.
 * GCC doesn't treat static const char* as compile-time constants
 * for array initialization, so we use a function. */
static const char *builtin_skin_json(int index) {
    const char *jsons[] = {
        DEFAULT_SKIN_JSON,
        ARES_SKIN_JSON,
        MONO_SKIN_JSON,
        SLATE_SKIN_JSON,
        DAYLIGHT_SKIN_JSON,
    };
    if (index < 0 || index >= (int)(sizeof(jsons)/sizeof(jsons[0]))) return NULL;
    return jsons[index];
}
#define BUILTIN_SKIN_COUNT 5

/* ================================================================
 *  Skin creation
 * ================================================================ */

/* Parse JSON into a new skin_t. Takes ownership of root. */
static skin_t *skin_from_json(json_t *root) {
    if (!root) return NULL;
    skin_t *s = (skin_t *)calloc(1, sizeof(skin_t));
    if (!s) { json_free(root); snprintf(skin_err, sizeof(skin_err), "OOM"); return NULL; }
    s->root = root;
    const char *name = json_get_str(root, "name", "custom");
    snprintf(s->name, sizeof(s->name), "%s", name);
    return s;
}

skin_t *skin_default(void) {
    return skin_load_string(DEFAULT_SKIN_JSON);
}

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
    return skin_from_json(root);
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
    return skin_from_json(root);
}

skin_t *skin_with_overrides(const char *overrides_json) {
    skin_t *s = skin_default();
    if (!s || !overrides_json) return s;

    char *err = NULL;
    json_t *overrides = json_parse(overrides_json, &err);
    if (!overrides) { free(err); return s; }

    /* Merge overrides into default */
    const char *name = json_get_str(overrides, "name", NULL);
    if (name) json_set(s->root, "name", json_string(name));

    /* Merge colors */
    json_t *colors = json_obj_get(overrides, "colors");
    if (colors) {
        json_t *def_colors = json_obj_get(s->root, "colors");
        if (!def_colors) {
            json_set(s->root, "colors", json_copy(colors));
        } else {
            /* Copy each key from overrides. We'll iterate our known keys
             * and also any unknown keys in the override dict. */
            json_t *override_keys = json_obj_get(colors, "_keys");
            (void)override_keys; /* not used, just iterate all */
            /* walk the override dict using json_obj_keys if available */
            /* Simple: iterate the keys we know about + match by name */
            const char *known_colors[] = {
                "banner_border","banner_title","banner_accent","banner_dim",
                "banner_text","ui_accent","ui_label","ui_ok","ui_error",
                "ui_warn","prompt","input_rule","response_border",
                "status_bar_bg","status_bar_text","status_bar_strong",
                "status_bar_dim","status_bar_good","status_bar_warn",
                "status_bar_bad","status_bar_critical","session_label",
                "session_border","tool","thinking","error","dim", NULL
            };
            for (int i = 0; known_colors[i]; i++) {
                const char *v = json_get_str(colors, known_colors[i], NULL);
                if (v) json_set(def_colors, known_colors[i], json_string(v));
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

    /* Merge branding */
    json_t *branding = json_obj_get(overrides, "branding");
    if (branding) {
        json_t *def_br = json_obj_get(s->root, "branding");
        if (!def_br) {
            json_set(s->root, "branding", json_copy(branding));
        } else {
            const char *br_keys[] = {
                "agent_name","welcome","goodbye","response_label",
                "prompt_symbol","help_header", NULL
            };
            for (int i = 0; br_keys[i]; i++) {
                const char *v = json_get_str(branding, br_keys[i], NULL);
                if (v) json_set(def_br, br_keys[i], json_string(v));
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
                json_set(def_fmt, "banner_bold",
                    json_bool(json_get_bool(fmt, "banner_bold", true)));
            if (json_obj_get(fmt, "show_model"))
                json_set(def_fmt, "show_model",
                    json_bool(json_get_bool(fmt, "show_model", true)));
        }
    }

    json_free(overrides);
    return s;
}

/* ================================================================
 *  Built-in skin enumeration
 * ================================================================ */

/* Return the number of built-in skins available. */
int skin_builtin_count(void) {
    return BUILTIN_SKIN_COUNT;
}

/* Get the name of the Nth built-in skin (0-indexed). Returns NULL if out of bounds. */
const char *skin_builtin_name(int index) {
    const char *json = builtin_skin_json(index);
    if (!json) return NULL;
    const char *name_marker = strstr(json, "\"name\":\"");
    if (!name_marker) return NULL;
    name_marker += 8; /* skip past \"name\":\" */
    const char *end = strchr(name_marker, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - name_marker);
    static char buf[64];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, name_marker, len);
    buf[len] = '\0';
    return buf;
}

/* Load a built-in skin by name. Returns NULL if not found. */
skin_t *skin_load_preset(const char *name) {
    if (!name || !name[0]) return NULL;
    int count = skin_builtin_count();
    for (int i = 0; i < count; i++) {
        const char *json = builtin_skin_json(i);
        if (!json) continue;
        /* Extract name from JSON */
        const char *nm = strstr(json, "\"name\":\"");
        if (!nm) continue;
        nm += 8;
        const char *end = strchr(nm, '"');
        if (!end) continue;
        size_t nlen = (size_t)(end - nm);
        if (nlen == strlen(name) && strncmp(nm, name, nlen) == 0)
            return skin_load_string(json);
    }
    return NULL;
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
            /* Check for non-string types (bool, number) */
            json_t *val = json_obj_get(p, dot + 1);
            if (val) {
                if (val->type == JSON_BOOL)
                    return val->bool_val ? "true" : "false";
                if (val->type == JSON_NUMBER) {
                    static char num_buf[32];
                    snprintf(num_buf, sizeof(num_buf), "%g", val->num_val);
                    return num_buf;
                }
            }
        }
    }

    /* Try direct key (for flat paths) */
    json_t *direct = json_obj_get(s->root, key);
    if (direct && direct->type == JSON_STRING) return direct->str_val;

    return def;
}

const void *skin_get_json(const skin_t *s, const char *key) {
    if (!s || !s->root || !key) return NULL;
    return resolve_path(s->root, key);
}

bool skin_get_bool(const skin_t *s, const char *key, bool def) {
    const char *v = skin_get(s, key, NULL);
    if (!v) return def;
    return strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0;
}

const char *skin_name(const skin_t *s) {
    return s ? s->name : "default";
}

const char *skin_get_branding(const skin_t *s, const char *key, const char *def) {
    char branding_key[128];
    snprintf(branding_key, sizeof(branding_key), "branding.%s", key);
    return skin_get(s, branding_key, def);
}

/* ================================================================
 *  ANSI color resolution
 * ================================================================ */

/* Parse a hex color string like #FFD700. Returns true on success. */
static bool parse_hex_color(const char *hex, int *r, int *g, int *b) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) return false;
    if (!isxdigit((unsigned char)hex[1]) || !isxdigit((unsigned char)hex[2]) ||
        !isxdigit((unsigned char)hex[3]) || !isxdigit((unsigned char)hex[4]) ||
        !isxdigit((unsigned char)hex[5]) || !isxdigit((unsigned char)hex[6]))
        return false;
    unsigned int rgb = (unsigned int)strtoul(hex + 1, NULL, 16);
    *r = (int)((rgb >> 16) & 0xFF);
    *g = (int)((rgb >> 8) & 0xFF);
    *b = (int)(rgb & 0xFF);
    return true;
}

/* ANSI 24-bit foreground: \033[38;2;R;G;Bm */
static const char *ansi_24bit_fg(int r, int g, int b) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm", r, g, b);
    return buf;
}

const char *skin_ansi_color(const char *name) {
    if (!name) return "";

    /* Check for hex color (#RRGGBB) first */
    if (name[0] == '#') {
        int r, g, b;
        if (parse_hex_color(name, &r, &g, &b))
            return ansi_24bit_fg(r, g, b);
        return ""; /* Invalid hex */
    }

    /* Parse "color:modifier" format */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", name);
    char *color = buf;
    char *mod = strchr(buf, ':');
    if (mod) { *mod = '\0'; mod++; }

    /* Base color — 8 standard ANSI colors */
    const char *fg = "";
    if (strcmp(color, "black") == 0)       fg = "\033[30m";
    else if (strcmp(color, "red") == 0)    fg = "\033[31m";
    else if (strcmp(color, "green") == 0)  fg = "\033[32m";
    else if (strcmp(color, "yellow") == 0) fg = "\033[33m";
    else if (strcmp(color, "blue") == 0)   fg = "\033[34m";
    else if (strcmp(color, "magenta") == 0) fg = "\033[35m";
    else if (strcmp(color, "cyan") == 0)   fg = "\033[36m";
    else if (strcmp(color, "white") == 0)  fg = "\033[37m";
    else return ""; /* Unknown color */

    /* Modifier */
    if (mod) {
        if (strcmp(mod, "bold") == 0) {
            static char bold_ansi[16];
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
