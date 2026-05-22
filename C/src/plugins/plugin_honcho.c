/*
 * plugin_honcho.c — In-memory memory plugin (P126/P130).
 *
 * Real implementation: stores/retrieves memories in an internal
 * linked list, searchable by keyword. No external dependencies.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_honcho.c -o plugin_honcho.so
 *
 * Plugin API export:
 *   PLUGIN_MEMORY type — provides memory_store, memory_search, memory_clear
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Internal: In-memory storage
 * ================================================================ */

/* Max memories stored */
#define MAX_MEMORIES 1024

/* A single memory entry */
typedef struct {
    char   id[64];          /* unique ID (timestamp-based) */
    char   content[4096];   /* memory content */
    char   metadata[2048];  /* JSON metadata */
    time_t created_at;
    int    access_count;    /* for relevance ranking */
    time_t last_accessed;
} memory_entry_t;

/* Static storage — simple array, no malloc for reliability in .so context */
static memory_entry_t g_memories[MAX_MEMORIES];
static int g_memory_count = 0;
static int g_initialized = 0;

/* Generate a simple unique ID */
static void gen_id(char *buf, size_t sz) {
    static int counter = 0;
    snprintf(buf, sz, "mem-%lu-%d", (unsigned long)time(NULL), counter++);
}

/* ================================================================
 *  Plugin metadata (exported symbols)
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "in-memory-store";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "memory";
}

const char *plugin_meta_description(void) {
    return "In-memory memory provider — stores up to 1024 entries with keyword search and relevance ranking";
}

/* ================================================================
 *  Dependencies
 * ================================================================ */

static plugin_dep_t deps[] = {
    { .name = "", .min_version = {0, 0, 0}, .optional = true } /* no deps */
};

int plugin_deps_count(void) {
    return 0;
}

const plugin_dep_t *plugin_deps_list(void) {
    return deps;
}

/* ================================================================
 *  Interface: memory operations
 * ================================================================ */

static char *memory_store(const char *content, const char *metadata_json) {
    if (!content || !content[0]) {
        return strdup("{\"status\":\"error\",\"message\":\"empty content\"}");
    }
    if (g_memory_count >= MAX_MEMORIES) {
        return strdup("{\"status\":\"error\",\"message\":\"memory full\"}");
    }

    memory_entry_t *entry = &g_memories[g_memory_count];
    gen_id(entry->id, sizeof(entry->id));
    snprintf(entry->content, sizeof(entry->content), "%s", content);
    if (metadata_json)
        snprintf(entry->metadata, sizeof(entry->metadata), "%s", metadata_json);
    else
        entry->metadata[0] = '\0';
    entry->created_at = time(NULL);
    entry->access_count = 0;
    entry->last_accessed = 0;

    char result[512];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"id\":\"%s\",\"index\":%d,\"timestamp\":%ld}",
             entry->id, g_memory_count, (long)entry->created_at);
    g_memory_count++;
    return strdup(result);
}

static char *memory_search(const char *query, int limit) {
    if (!query) query = "";
    if (limit <= 0) limit = 10;
    if (limit > 100) limit = 100;

    /* Count matches first */
    int match_count = 0;
    int indices[MAX_MEMORIES];
    double scores[MAX_MEMORIES];

    for (int i = 0; i < g_memory_count; i++) {
        memory_entry_t *e = &g_memories[i];
        double score = 0.0;

        /* Keyword match in content (weighted by position) */
        if (query[0]) {
            const char *p = e->content;
            int keyword_hits = 0;
            while ((p = strstr(p, query)) != NULL) {
                keyword_hits++;
                /* Position bonus: earlier matches score higher */
                score += 1.0 / (1.0 + (double)(p - e->content) / 100.0);
                p++;
            }
            /* Also search metadata */
            p = e->metadata;
            while ((p = strstr(p, query)) != NULL) {
                score += 0.5;
                p++;
            }
            if (keyword_hits == 0) continue; /* no match */
        } else {
            /* No query — return all (scored by recency) */
            score = (double)e->created_at;
        }

        /* Recency bonus */
        score += (double)e->created_at / 1000000.0;
        /* Access count bonus (frequently accessed = more relevant) */
        score += (double)e->access_count * 0.1;

        indices[match_count] = i;
        scores[match_count] = score;
        match_count++;
    }

    /* Sort by score descending (simple insertion sort) */
    for (int i = 0; i < match_count && i < limit; i++) {
        int best = i;
        for (int j = i + 1; j < match_count; j++) {
            if (scores[j] > scores[best]) best = j;
        }
        if (best != i) {
            int ti = indices[i]; indices[i] = indices[best]; indices[best] = ti;
            double ts = scores[i]; scores[i] = scores[best]; scores[best] = ts;
        }
    }

    int result_count = (match_count < limit) ? match_count : limit;

    /* Build JSON result manually (avoid dependency on JSON lib) */
    /* Estimate buffer size */
    size_t buf_sz = 256 + (size_t)result_count * 4096;
    char *result = (char *)malloc(buf_sz);
    if (!result) return strdup("{\"results\":[],\"count\":0}");

    size_t pos = 0;
    pos += snprintf(result + pos, buf_sz - pos,
                    "{\"count\":%d,\"results\":[", result_count);

    for (int i = 0; i < result_count; i++) {
        memory_entry_t *e = &g_memories[indices[i]];
        /* Escape content for JSON (basic escaping) */
        char escaped[4096];
        size_t epos = 0;
        for (const char *s = e->content; *s && epos < sizeof(escaped) - 6; s++) {
            if (*s == '"' || *s == '\\') {
                escaped[epos++] = '\\';
                escaped[epos++] = *s;
            } else if (*s == '\n') {
                escaped[epos++] = '\\'; escaped[epos++] = 'n';
            } else if (*s == '\t') {
                escaped[epos++] = '\\'; escaped[epos++] = 't';
            } else {
                escaped[epos++] = *s;
            }
        }
        escaped[epos] = '\0';

        if (i > 0) result[pos++] = ',';

        /* Track access for ranking */
        e->access_count++;
        e->last_accessed = time(NULL);

        pos += snprintf(result + pos, buf_sz - pos,
            "{\"id\":\"%s\",\"content\":\"%s\",\"metadata\":\"%s\","
            "\"score\":%.1f,\"created_at\":%ld}",
            e->id, escaped, e->metadata,
            scores[i], (long)e->created_at);
    }
    pos += snprintf(result + pos, buf_sz - pos, "]}");
    return result;
}

static void memory_clear(void) {
    g_memory_count = 0;
    fprintf(stderr, "[in-memory-store] cleared %d entries\n", g_memory_count);
}

/* Get memory count (convenience, not in standard interface) */
static int memory_count(void) {
    return g_memory_count;
}

static plugin_interface_t interface = {
    .type = PLUGIN_MEMORY,
    .memory_store  = memory_store,
    .memory_search = memory_search,
    .memory_clear  = memory_clear,
};

void *plugin_get_interface(void) {
    return &interface;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    if (g_initialized) return 0;
    g_memory_count = 0;
    g_initialized = 1;
    fprintf(stderr, "[in-memory-store] initialized (%d/%d slots)\n",
            g_memory_count, MAX_MEMORIES);
    return 0;
}

int plugin_cleanup(void) {
    memory_clear();
    g_initialized = 0;
    fprintf(stderr, "[in-memory-store] shutdown complete\n");
    return 0;
}

/* Optional: receive config from agent */
int plugin_configure(const char *config_json) {
    fprintf(stderr, "[in-memory-store] configure: %s\n", config_json);
    return 0;
}
