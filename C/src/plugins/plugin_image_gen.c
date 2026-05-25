/*
 * plugin_image_gen.c — Image generation plugin (PLUGIN_IMAGE_GEN).
 *
 * Provides image generation via FAL.ai REST API.
 * Supports prompt-to-image with configurable aspect ratio.
 * Uses curl via popen() for HTTP — keeps plugin self-contained.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_image_gen.c -o plugin_image_gen.so
 *
 * MIT License — WuBu Slermes Project
 */

#define _GNU_SOURCE
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
#define FAL_API_URL        "https://fal.run/fal-ai/flux-pro"

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
static char g_provider[PROVIDER_KEY_MAX] = "fal-ai";
static char g_api_key[API_KEY_MAX] = "";
static char g_default_size[32] = DEFAULT_SIZE;
static char g_model[64] = "flux-pro";

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
 *  JSON escape helper
 * ================================================================ */

static char *json_escape(const char *s) {
    if (!s) return strdup("");
    size_t cap = strlen(s) * 2 + 2;
    char *out = (char *)malloc(cap);
    if (!out) return strdup("");
    size_t j = 0;
    for (const char *sp = s; *sp && j < cap - 2; sp++) {
        if (*sp == '"' || *sp == '\\') { out[j++] = '\\'; out[j++] = *sp; }
        else if (*sp == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
        else if (*sp == '\t') { out[j++] = '\\'; out[j++] = 't'; }
        else if (*sp == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
        else out[j++] = *sp;
    }
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  FAL.ai REST API via curl popen
 * ================================================================ */

/* Extract a JSON string value by key from a string.
 * Simple parser — finds "key":"value" patterns. */
static const char *json_extract_str(const char *json, const char *key,
                                     char *buf, size_t buf_sz) {
    buf[0] = '\0';
    if (!json || !key) return NULL;
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p && *p != ':') p++;
    if (*p) p++;
    while (*p && *p != '"') p++;
    if (*p) p++;
    const char *end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    if (len >= buf_sz) len = buf_sz - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

/* Call FAL.ai API via curl subprocess.
 * Returns malloc'd response body, or NULL on error. */
/* ================================================================
 *  Testing support — mock replaces fal_api_call in tests
 * ================================================================ */
#ifdef TEST_MODE
/* Mock FAL API call — returns canned success without network */
static char *fal_api_call(const char *json_body) {
    (void)json_body;  /* unused in mock */
    return strdup("{\"images\":[{\"url\":\"https://fal.ai/media/test.png\"}]}\n200");
}
#else
/* ================================================================
 *  Real FAL API call via curl
 * ================================================================ */

static char *fal_api_call(const char *json_body) {
    /* Build temp file path for response */
    char resp_path[256];
    snprintf(resp_path, sizeof(resp_path),
             "/tmp/fal_resp_%d_%ld.json", getpid(), (long)time(NULL));

    /* Get API key */
    const char *api_key = getenv("FAL_API_KEY");
    if (!api_key || !*api_key)
        api_key = getenv("SLERMES_FAL_KEY");
    if (!api_key || !*api_key) {
        return strdup("{\"error\":\"FAL_API_KEY not set. Get a key at https://fal.ai\"}");
    }

    /* Escape the JSON body for shell */
    char *escaped = json_escape(json_body);
    if (!escaped) return NULL;

    /* Build curl command */
    char cmd[32768];
    int n = snprintf(cmd, sizeof(cmd),
        "curl -s -w '\\n%%{http_code}' -X POST '%s' "
        "-H 'Content-Type: application/json' "
        "-H 'Authorization: Key %s' "
        "-d '%s' > '%s' 2>/dev/null",
        FAL_API_URL, api_key, escaped, resp_path);
    free(escaped);

    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        unlink(resp_path);
        return strdup("{\"error\":\"command too long\"}");
    }

    int ret = system(cmd);
    if (ret != 0) {
        unlink(resp_path);
        return strdup("{\"error\":\"curl command failed\"}");
    }

    /* Read the response + HTTP status code */
    FILE *f = fopen(resp_path, "r");
    if (!f) {
        return strdup("{\"error\":\"failed to read response\"}");
    }

    /* Read file content */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *resp_body = NULL;
    if (fsize > 0 && fsize < 1024 * 1024) {
        resp_body = (char *)malloc((size_t)fsize + 1);
        if (resp_body) {
            size_t nread = fread(resp_body, 1, (size_t)fsize, f);
            resp_body[nread] = '\0';
        }
    }
    fclose(f);
    unlink(resp_path);

    if (!resp_body) return strdup("{\"error\":\"empty response\"}");

    /* Parse last line as HTTP status */
    char *last_newline = strrchr(resp_body, '\n');
    int http_status = 0;
    if (last_newline) {
        http_status = atoi(last_newline + 1);
        *last_newline = '\0'; /* Trim status line */
    }

    if (http_status != 200) {
        char *err = NULL;
        if (asprintf(&err,
            "{\"error\":\"FAL API returned HTTP %d: %s\"}",
            http_status, resp_body) < 0) err = NULL;
        free(resp_body);
        return err ? err : strdup("{\"error\":\"OOM\"}");
    }

    return resp_body;
}
#endif /* TEST_MODE */

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

/* Generate an image via FAL.ai REST API */
static char *impl_image_gen(const char *prompt, const char *params_json) {
    if (!prompt || prompt[0] == '\0')
        return strdup("{\"error\":\"empty prompt\"}");

    /* Extract aspect ratio from params, default to 1:1 */
    const char *ar = "1:1";
    char ar_buf[16];
    if (params_json) {
        const char *s = strstr(params_json, "\"aspect_ratio\"");
        if (s) {
            s += 14; while (*s && *s != '"') s++; if (*s) s++;
            const char *end = strchr(s, '"');
            if (end) {
                size_t len = (size_t)(end - s);
                if (len > 0 && len < sizeof(ar_buf)) {
                    memcpy(ar_buf, s, len);
                    ar_buf[len] = '\0';
                    ar = ar_buf;
                }
            }
        }
    }

    /* Build JSON body for FAL API */
    char *esc_prompt = json_escape(prompt);
    if (!esc_prompt) return strdup("{\"error\":\"OOM\"}");

    char body[8192];
    int b = snprintf(body, sizeof(body),
        "{\"prompt\":\"%s\",\"aspect_ratio\":\"%s\"}",
        esc_prompt, ar);
    free(esc_prompt);
    if (b < 0 || (size_t)b >= sizeof(body))
        return strdup("{\"error\":\"prompt too long\"}");

    /* Determine width/height from aspect ratio for history */
    int w = 1024, h = 1024;
    if (strcmp(ar, "16:9") == 0) { w = 1024; h = 576; }
    else if (strcmp(ar, "9:16") == 0) { w = 576; h = 1024; }
    else if (strcmp(ar, "4:3") == 0) { w = 1024; h = 768; }
    else if (strcmp(ar, "3:2") == 0) { w = 1024; h = 683; }

    /* Extract size from params for backward compatibility */
    if (params_json) {
        const char *s = strstr(params_json, "\"size\"");
        if (s) {
            s += 6; while (*s && *s != '"') s++; if (*s) s++;
            const char *end = strchr(s, '"');
            if (end && (size_t)(end - s) < 32) {
                char sz_buf[32];
                size_t len = (size_t)(end - s);
                memcpy(sz_buf, s, len);
                sz_buf[len] = '\0';
                parse_size(sz_buf, &w, &h);
            }
        }
    }

    /* Call FAL API */
    char *resp_body = fal_api_call(body);
    if (!resp_body)
        return strdup("{\"error\":\"API call failed\"}");

    /* Check for error in response */
    if (strstr(resp_body, "\"error\"")) {
        /* Error from FAL — return as-is */
        return resp_body;
    }

    /* Extract image URL from response */
    char url_buf[2048] = "";
    /* FAL returns: {"images":[{"url":"..."}], ...} */
    const char *images_start = strstr(resp_body, "\"images\"");
    if (images_start) {
        /* Skip to first "url" after images */
        const char *url_pos = strstr(images_start, "\"url\"");
        if (url_pos) {
            url_pos += 5;
            while (*url_pos && *url_pos != '"') url_pos++;
            if (*url_pos) url_pos++;
            const char *url_end = strchr(url_pos, '"');
            if (url_end) {
                size_t ulen = (size_t)(url_end - url_pos);
                if (ulen >= sizeof(url_buf)) ulen = sizeof(url_buf) - 1;
                memcpy(url_buf, url_pos, ulen);
                url_buf[ulen] = '\0';
            }
        }
    } else {
        /* Fallback: try "url" at top level */
        json_extract_str(resp_body, "url", url_buf, sizeof(url_buf));
    }

    if (!url_buf[0]) {
        /* No URL found — return raw response for debugging */
        free(resp_body);
        return strdup("{\"error\":\"No image URL in FAL response\"}");
    }

    /* Add to history */
    gen_record_t *rec = &g_history[g_history_count % MAX_GENERATIONS];
    snprintf(rec->prompt, sizeof(rec->prompt), "%s", prompt);
    snprintf(rec->provider, sizeof(rec->provider), "%s", g_provider);
    snprintf(rec->result_url, sizeof(rec->result_url), "%s", url_buf);
    rec->timestamp = (long)time(NULL);
    rec->width = w;
    rec->height = h;
    g_history_count++;
    if (g_history_count > MAX_GENERATIONS) g_history_wrap++;

    state_save();

    /* Build result */
    char *result = NULL;
    asprintf(&result,
        "{\"url\":\"%s\",\"provider\":\"%s\",\"model\":\"%s\","
        "\"width\":%d,\"height\":%d,\"revised_prompt\":\"%s\"}",
        url_buf, g_provider, g_model, w, h, prompt);

    free(resp_body);
    return result ? result : strdup("{\"error\":\"OOM\"}");
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
                (g_history_wrap % MAX_GENERATIONS);
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
    return "2.0.0";
}

const char *plugin_meta_type(void) {
    return "image_gen";
}

const char *plugin_meta_description(void) {
    return "Image generation via FAL.ai REST API — prompt-to-image with real HTTP backend, history tracking";
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
    const char *key = getenv("FAL_API_KEY");
    if (!key) key = getenv("SLERMES_FAL_KEY");
    if (!key) key = getenv("OPENAI_API_KEY");
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
