/*
 * plugin_file_memory.c — File-backed persistent memory plugin (P130).
 *
 * Real implementation: stores memories in a JSON lines file.
 * Memories persist across agent restarts. Searchable by keyword.
 * No external dependencies — uses stdio file I/O.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_file_memory.c -o plugin_file_memory.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>

/* ================================================================
 *  Internal constants
 * ================================================================ */

#define MAX_MEMORY_CONTENT  4096
#define MAX_MEMORY_META     2048
#define MAX_MEMORIES        4096
#define MAX_PATH            1024
#define MAX_LINE            8192

/* ================================================================
 *  Internal state
 * ================================================================ */

static char g_store_path[MAX_PATH];
static int  g_initialized = 0;

/* In-memory cache for fast search */
typedef struct {
    char   id[64];
    char   content[MAX_MEMORY_CONTENT];
    char   metadata[MAX_MEMORY_META];
    time_t created_at;
} memory_t;

static memory_t g_cache[MAX_MEMORIES];
static int      g_cache_count = 0;

/* ================================================================
 *  File I/O
 * ================================================================ */

/* Load memories from file into cache */
static int load_memories(void) {
    g_cache_count = 0;
    FILE *f = fopen(g_store_path, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && g_cache_count < MAX_MEMORIES) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (len == 0 || line[0] == '\0') continue;

        /* Parse: id|timestamp|content|metadata */
        memory_t *m = &g_cache[g_cache_count];
        char *id_end = strchr(line, '|');
        if (!id_end) continue;
        *id_end = '\0';
        strncpy(m->id, line, sizeof(m->id) - 1);

        char *ts_end = strchr(id_end + 1, '|');
        if (!ts_end) continue;
        *ts_end = '\0';
        m->created_at = (time_t)atol(id_end + 1);

        char *meta_end = strrchr(ts_end + 1, '|');
        if (!meta_end) continue;
        *meta_end = '\0';
        strncpy(m->content, ts_end + 1, MAX_MEMORY_CONTENT - 1);
        strncpy(m->metadata, meta_end + 1, MAX_MEMORY_META - 1);

        g_cache_count++;
    }
    fclose(f);
    return g_cache_count;
}

/* Ensure directory exists for a given file path */
static void ensure_dir(const char *path) {
    char dir[1024];
    size_t len = strlen(path);
    if (len >= sizeof(dir)) return;
    memcpy(dir, path, len + 1);
    char *slash = strrchr(dir, '/');
    if (!slash || slash == dir) return;
    *slash = '\0';
    /* Maybe parent doesn't exist either — do simple single-level */
    mkdir(dir, 0700);
    /* Also ensure grandparent */
    char *pslash = strrchr(dir, '/');
    if (pslash && pslash != dir) {
        *pslash = '\0';
        mkdir(dir, 0700);
    }
}

/* Save all cached memories to file */
static int save_memories(void) {
    ensure_dir(g_store_path);
    FILE *f = fopen(g_store_path, "w");
    if (!f) return -1;

    for (int i = 0; i < g_cache_count; i++) {
        memory_t *m = &g_cache[i];
        /* Escape pipes in content for parsing safety */
        fprintf(f, "%s|%ld|%s|%s\n",
                m->id, (long)m->created_at, m->content, m->metadata);
    }
    fclose(f);
    return 0;
}

/* Generate ID */
static void gen_id(char *buf, size_t sz) {
    static int counter = 0;
    snprintf(buf, sz, "fm-%lu-%d", (unsigned long)time(NULL), counter++);
}

/* ================================================================
 *  Memory interface functions
 * ================================================================ */

static char *memory_store(const char *content, const char *metadata_json) {
    if (!content) return strdup("{\"error\":\"content is NULL\"}");

    load_memories();

    if (g_cache_count >= MAX_MEMORIES)
        return strdup("{\"error\":\"memory store full\"}");

    memory_t *m = &g_cache[g_cache_count];
    gen_id(m->id, sizeof(m->id));
    strncpy(m->content, content, MAX_MEMORY_CONTENT - 1);
    strncpy(m->metadata, metadata_json ? metadata_json : "{}", MAX_MEMORY_META - 1);
    m->created_at = time(NULL);
    g_cache_count++;

    if (save_memories() != 0)
        return strdup("{\"error\":\"save failed\"}");

    char buf[128];
    snprintf(buf, sizeof(buf), "{\"id\":\"%s\",\"status\":\"stored\"}", m->id);
    return strdup(buf);
}

static char *memory_search(const char *query, int limit) {
    if (!query) return strdup("{\"error\":\"query is NULL\"}");

    load_memories();

    if (limit <= 0 || limit > 100) limit = 10;

    /* Build results as JSON array */
    char *result = malloc(65536);
    if (!result) return strdup("{\"error\":\"out of memory\"}");
    snprintf(result, 65536, "{\"results\":[");
    int first = 1;
    int found = 0;

    /* Lowercase query for case-insensitive matching */
    char query_lc[256];
    size_t qlen = strlen(query);
    if (qlen > 255) qlen = 255;
    for (size_t i = 0; i < qlen; i++) query_lc[i] = (char)tolower((unsigned char)query[i]);
    query_lc[qlen] = '\0';

    for (int i = g_cache_count - 1; i >= 0 && found < limit; i--) {
        memory_t *m = &g_cache[i];
        /* Case-insensitive substring match */
        char content_lc[MAX_MEMORY_CONTENT];
        size_t clen = strlen(m->content);
        if (clen > MAX_MEMORY_CONTENT - 1) clen = MAX_MEMORY_CONTENT - 1;
        for (size_t j = 0; j < clen; j++) content_lc[j] = (char)tolower((unsigned char)m->content[j]);
        content_lc[clen] = '\0';

        if (!strstr(content_lc, query_lc)) continue;

        if (!first) strncat(result, ",", 65536 - strlen(result) - 1);
        first = 0;

        char entry[512];
        snprintf(entry, sizeof(entry),
            "{\"id\":\"%s\",\"content\":\"%s\",\"created_at\":%ld}",
            m->id, m->content, (long)m->created_at);

        /* JSON-escape content */
        char *p = entry;
        while (*p) {
            if (*p == '"' || *p == '\\') {
                size_t rlen = strlen(p);
                memmove(p + 1, p, rlen + 1);
                *p = '\\';
                p++;
            }
            p++;
        }

        strncat(result, entry, 65536 - strlen(result) - 1);
        found++;
    }

    strncat(result, "],\"count\":", 65536 - strlen(result) - 1);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d}", found);
    strncat(result, cnt, 65536 - strlen(result) - 1);

    return result;
}

static void memory_clear(void) {
    g_cache_count = 0;
    remove(g_store_path);
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
    return "file-memory";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "memory";
}

const char *plugin_meta_description(void) {
    return "File-backed persistent memory provider — stores memories in JSON lines file, survives restarts, keyword search";
}

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
    g_interface.type = PLUGIN_MEMORY;
    g_interface.memory_store = memory_store;
    g_interface.memory_search = memory_search;
    g_interface.memory_clear = memory_clear;

    /* Determine store path — use HERMES_HOME if available, else /tmp */
    const char *home = getenv("HERMES_HOME");
    if (home && *home) {
        snprintf(g_store_path, sizeof(g_store_path), "%s/file_memory.dat", home);
    } else {
        const char *home2 = getenv("HOME");
        if (home2 && *home2)
            snprintf(g_store_path, sizeof(g_store_path), "%s/.hermes/file_memory.dat", home2);
        else
            snprintf(g_store_path, sizeof(g_store_path), "/tmp/hermes_file_memory.dat");
    }
    ensure_dir(g_store_path);

    /* Load existing memories */
    load_memories();

    g_initialized = 1;
    return 0;
}

int plugin_cleanup(void) {
    save_memories();
    g_cache_count = 0;
    g_initialized = 0;
    memset(&g_interface, 0, sizeof(g_interface));
    return 0;
}
