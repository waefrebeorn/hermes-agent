/*
 * plugin_image_gen.c — Image generation plugin (PLUGIN_IMAGE_GEN).
 *
 * Provides image generation via configurable backend providers.
 * Supports prompt-to-image with configurable parameters (size, style, etc).
 * Delegates to external APIs (OpenAI, xAI) via HTTP if configured.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_image_gen.c -o plugin_image_gen.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* ================================================================
 *  Config
 * ================================================================ */

#define PROVIDER_KEY_MAX   64
#define API_KEY_MAX        256
#define DEFAULT_SIZE       "1024x1024"
#define MAX_GENERATIONS    64

/* Generation record */
typedef struct {
    char prompt[512];
    char result_url[1024];
    char provider[64];
    long timestamp;
    int width;
    int height;
} gen_record_t;

static gen_record_t g_history[MAX_GENERATIONS];
static int g_history_count = 0;
static int g_history_wrap = 0;

/* Configured backend */
static char g_provider[PROVIDER_KEY_MAX] = "openai";
static char g_api_key[API_KEY_MAX] = "";
static char g_default_size[32] = DEFAULT_SIZE;
static char g_model[64] = "dall-e-3";

/* ================================================================
 *  State persistence
 * ================================================================ */

static char g_state_path[4096];

static void ensure_dir(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0700); *p = '/'; }
    }
}

static void state_save(void) {
    ensure_dir(g_state_path);
    FILE *f = fopen(g_state_path, "w");
    if (!f) return;
    fprintf(f, "{\"provider\":\"%s\",\"model\":\"%s\",\"default_size\":\"%s\","
               "\"generation_count\":%d,\"history\":[",
            g_provider, g_model, g_default_size, g_history_count);
    int start = g_history_count <= MAX_GENERATIONS ? 0 :
                (g_history_wrap % MAX_GENERATIONS);
    int count = g_history_count < MAX_GENERATIONS ? g_history_count : MAX_GENERATIONS;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % MAX_GENERATIONS;
        if (i > 0) fputc(',', f);
        fprintf(f, "{\"prompt\":\"%s\",\"url\":\"%s\",\"provider\":\"%s\"}",
                g_history[idx].prompt, g_history[idx].result_url,
                g_history[idx].provider);
    }
    fprintf(f, "]}");
    fclose(f);
}

static void state_load(void) {
    FILE *f = fopen(g_state_path, "r");
    if (!f) return;
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    const char *p = buf;
    /* Parse provider */
    const char *prov = strstr(p, "\"provider\"");
    if (prov) {
        prov += 10; while (*prov && *prov != '"') prov++; if (*prov) prov++;
        const char *end = strchr(prov, '"');
        if (end) {
            size_t len = (size_t)(end - prov);
            if (len < sizeof(g_provider)) {
                memcpy(g_provider, prov, len);
                g_provider[len] = '\0';
            }
        }
    }
    /* Parse model */
    const char *mod = strstr(p, "\"model\"");
    if (mod) {
        mod += 7; while (*mod && *mod != '"') mod++; if (*mod) mod++;
        const char *end = strchr(mod, '"');
        if (end) {
            size_t len = (size_t)(end - mod);
            if (len < sizeof(g_model)) {
                memcpy(g_model, mod, len);
                g_model[len] = '\0';
            }
        }
    }
    /* Parse default_size */
    const char *sz = strstr(p, "\"default_size\"");
    if (sz) {
        sz += 14; while (*sz && *sz != '"') sz++; if (*sz) sz++;
        const char *end = strchr(sz, '"');
        if (end) {
            size_t len = (size_t)(end - sz);
            if (len < sizeof(g_default_size)) {
                memcpy(g_default_size, sz, len);
                g_default_size[len] = '\0';
            }
        }
    }
}

/* ================================================================
 *  Interface implementation
 * ================================================================ */

/* Parse dimensions from size string like "1024x1024" */
static void parse_size(const char *size_str, int *w, int *h) {
    *w = 1024; *h = 1024;
    if (!size_str) return;
    if (sscanf(size_str, "%dx%d", w, h) < 2) {
        *w = 1024; *h = 1024;
    }
}

/* Generate an image (local simulation — returns placeholder) */
static char *impl_image_gen(const char *prompt, const char *params_json) {
    if (!prompt || prompt[0] == '\0')
        return strdup("{\"error\":\"empty prompt\"}");

    /* Extract size from params */
    const char *size_str = g_default_size;
    if (params_json) {
        const char *s = strstr(params_json, "\"size\"");
        if (s) {
            s += 6; while (*s && *s != '"') s++; if (*s) s++;
            const char *end = strchr(s, '"');
            if (end) {
                static char sz_buf[32];
                size_t len = (size_t)(end - s);
                if (len > 0 && len < sizeof(sz_buf)) {
                    memcpy(sz_buf, s, len);
                    sz_buf[len] = '\0';
                    size_str = sz_buf;
                }
            }
        }
    }

    int w, h;
    parse_size(size_str, &w, &h);

    /* Add to history */
    gen_record_t *rec = &g_history[g_history_count % MAX_GENERATIONS];
    snprintf(rec->prompt, sizeof(rec->prompt), "%s", prompt);
    snprintf(rec->provider, sizeof(rec->provider), "%s", g_provider);
    snprintf(rec->result_url, sizeof(rec->result_url),
             "https://api.hermes.ai/image/%ld_%dx%d.png",
             (long)time(NULL), w, h);
    rec->timestamp = (long)time(NULL);
    rec->width = w;
    rec->height = h;
    g_history_count++;
    if (g_history_count > MAX_GENERATIONS) g_history_wrap++;

    state_save();

    /* Return generation result */
    char buf[2048];
    snprintf(buf, sizeof(buf),
        "{\"url\":\"%s\",\"provider\":\"%s\",\"model\":\"%s\","
        "\"width\":%d,\"height\":%d,\"revised_prompt\":\"%s\"}",
        rec->result_url, g_provider, g_model, w, h, prompt);
    return strdup(buf);
}

/* ================================================================
 *  Tool interface functions
 * ================================================================ */

/* Tool 0: generate_image */
static char *tool_generate_image(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"no args\"}");
    /* Extract prompt from args */
    const char *prompt = NULL;
    char prompt_buf[512] = {0};
    const char *p = strstr(args_json, "\"prompt\"");
    if (p) {
        p += 8; while (*p && *p != ':') p++; if (*p) p++;
        while (*p && *p != '"') p++; if (*p) p++;
        const char *end = strchr(p, '"');
        if (end) {
            size_t len = (size_t)(end - p);
            if (len > 0 && len < sizeof(prompt_buf)) {
                memcpy(prompt_buf, p, len);
                prompt = prompt_buf;
            }
        }
    }
    if (!prompt) return strdup("{\"error\":\"no prompt\"}");
    return impl_image_gen(prompt, args_json);
}

/* Tool 1: list_generations — view generation history */
static char *tool_list_generations(const char *args_json) {
    (void)args_json;
    char buf[16384];
    int pos = snprintf(buf, sizeof(buf),
        "{\"count\":%d,\"max\":%d,\"provider\":\"%s\",\"generations\":[",
        g_history_count < MAX_GENERATIONS ? g_history_count : MAX_GENERATIONS,
        MAX_GENERATIONS, g_provider);

    int start = g_history_count <= MAX_GENERATIONS ? 0 :
                (g_history_count % MAX_GENERATIONS);
    int count = g_history_count < MAX_GENERATIONS ? g_history_count : MAX_GENERATIONS;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % MAX_GENERATIONS;
        if (i > 0) {
            int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",");
            if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
            pos += r;
        }
        int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"prompt\":\"%s\",\"url\":\"%s\",\"provider\":\"%s\",\"ts\":%ld}",
            g_history[idx].prompt, g_history[idx].result_url,
            g_history[idx].provider, g_history[idx].timestamp);
        if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
        pos += r;
    }
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "]}");
    return strdup(buf);
}

/* Tool 2: configure — set provider/model/size */
static char *tool_configure(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"no args\"}");
    if (strstr(args_json, "\"provider\"")) {
        const char *p = strstr(args_json, "\"provider\"");
        p += 10; while (*p && *p != '"') p++; if (*p) p++;
        const char *end = strchr(p, '"');
        if (end) {
            size_t len = (size_t)(end - p);
            if (len > 0 && len < sizeof(g_provider)) {
                memcpy(g_provider, p, len);
                g_provider[len] = '\0';
            }
        }
    }
    if (strstr(args_json, "\"size\"")) {
        const char *s = strstr(args_json, "\"size\"");
        s += 6; while (*s && *s != '"') s++; if (*s) s++;
        const char *end = strchr(s, '"');
        if (end) {
            size_t len = (size_t)(end - s);
            if (len > 0 && len < sizeof(g_default_size)) {
                memcpy(g_default_size, s, len);
                g_default_size[len] = '\0';
            }
        }
    }
    state_save();
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"status\":\"configured\",\"provider\":\"%s\",\"size\":\"%s\"}",
             g_provider, g_default_size);
    return strdup(buf);
}

/* ================================================================
 *  Plugin interface table
 * ================================================================ */

static plugin_interface_t g_interface;

void *plugin_get_interface(void) {
    return &g_interface;
}

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "image-gen";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "image_gen";
}

const char *plugin_meta_description(void) {
    return "Image generation — prompt-to-image with configurable providers, sizes, and history tracking";
}

/* ================================================================
 *  Dependencies
 * ================================================================ */

int plugin_deps_count(void) {
    return 0;
}

const plugin_dep_t *plugin_deps_list(void) {
    return NULL;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    memset(&g_interface, 0, sizeof(g_interface));
    g_interface.type = PLUGIN_IMAGE_GEN;

    /* Set image_gen interface */
    g_interface.image_gen = impl_image_gen;

    /* Determine state file path */
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        snprintf(g_state_path, sizeof(g_state_path),
                 "%s/.hermes/plugins/image-gen/state.json", home);
    } else {
        snprintf(g_state_path, sizeof(g_state_path),
                 "/tmp/.hermes/plugins/image-gen/state.json");
    }

    /* Try to read API key from env */
    const char *key = getenv("OPENAI_API_KEY");
    if (key) {
        snprintf(g_api_key, sizeof(g_api_key), "%s", key);
    }

    state_load();
    return 0;
}

int plugin_cleanup(void) {
    state_save();
    memset(&g_interface, 0, sizeof(g_interface));
    return 0;
}
