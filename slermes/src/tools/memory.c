/*
 * memory.c — Memory system depth for Hermes C.
 * P151-P158: Storage abstraction, TTL, prioritization, dedup, search,
 *            auto-save, import/export, compression.
 *
 * Provides a unified memory abstraction with multiple backends:
 *   - In-memory (hash map)
 *   - File-based (JSON file, backward-compatible with existing memory.json)
 *   - SQLite (via libdb)
 *   - Plugin (delegates to PLUGIN_MEMORY plugin)
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes.h"
#include "hermes_memory.h"
#include "hermes_json.h"
#include "hermes_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "sqlite3.h"

/* ================================================================
 *  Forward declarations of built-in backend vtables
 * ================================================================ */

static bool vtable_inmem_open(memory_storage_t *st, const char *uri);
static void vtable_inmem_close(memory_storage_t *st);
static bool vtable_inmem_store(memory_storage_t *st, memory_entry_t *entry);
static bool vtable_inmem_get(memory_storage_t *st, const char *key, memory_entry_t *entry);
static bool vtable_inmem_delete(memory_storage_t *st, const char *key);
static void vtable_inmem_clear(memory_storage_t *st);
static size_t vtable_inmem_count(memory_storage_t *st);
static char **vtable_inmem_list_keys(memory_storage_t *st, size_t *count);
static memory_entry_t *vtable_inmem_search(memory_storage_t *st, const char *query, int limit);
static int vtable_inmem_import_json(memory_storage_t *st, const json_t *entries);
static json_t *vtable_inmem_export_json(memory_storage_t *st);
static bool vtable_inmem_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry);
static bool vtable_inmem_persist(memory_storage_t *st);
static bool vtable_inmem_load(memory_storage_t *st);
static int vtable_inmem_compress_old(memory_storage_t *st, time_t before,
                                      char *(*compress_cb)(const char *content));
static memory_entry_t *vtable_inmem_get_prioritized(memory_storage_t *st, size_t limit, size_t *count);

static bool vtable_file_open(memory_storage_t *st, const char *uri);
static void vtable_file_close(memory_storage_t *st);
static bool vtable_file_store(memory_storage_t *st, memory_entry_t *entry);
static bool vtable_file_get(memory_storage_t *st, const char *key, memory_entry_t *entry);
static bool vtable_file_delete(memory_storage_t *st, const char *key);
static void vtable_file_clear(memory_storage_t *st);
static size_t vtable_file_count(memory_storage_t *st);
static char **vtable_file_list_keys(memory_storage_t *st, size_t *count);
static memory_entry_t *vtable_file_search(memory_storage_t *st, const char *query, int limit);
static int vtable_file_import_json(memory_storage_t *st, const json_t *entries);
static json_t *vtable_file_export_json(memory_storage_t *st);
static bool vtable_file_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry);
static bool vtable_file_persist(memory_storage_t *st);
static bool vtable_file_load(memory_storage_t *st);
static int vtable_file_compress_old(memory_storage_t *st, time_t before,
                                     char *(*compress_cb)(const char *content));
static memory_entry_t *vtable_file_get_prioritized(memory_storage_t *st, size_t limit, size_t *count);

/* SQLite backend vtable functions */
static bool vtable_sqlite_open(memory_storage_t *st, const char *uri);
static void vtable_sqlite_close(memory_storage_t *st);
static bool vtable_sqlite_store(memory_storage_t *st, memory_entry_t *entry);
static bool vtable_sqlite_get(memory_storage_t *st, const char *key, memory_entry_t *entry);
static bool vtable_sqlite_delete(memory_storage_t *st, const char *key);
static void vtable_sqlite_clear(memory_storage_t *st);
static size_t vtable_sqlite_count(memory_storage_t *st);
static char **vtable_sqlite_list_keys(memory_storage_t *st, size_t *count);
static memory_entry_t *vtable_sqlite_search(memory_storage_t *st, const char *query, int limit);
static int vtable_sqlite_import_json(memory_storage_t *st, const json_t *entries);
static json_t *vtable_sqlite_export_json(memory_storage_t *st);
static bool vtable_sqlite_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry);
static bool vtable_sqlite_persist(memory_storage_t *st);
static bool vtable_sqlite_load(memory_storage_t *st);
static int vtable_sqlite_compress_old(memory_storage_t *st, time_t before,
                                      char *(*compress_cb)(const char *content));
static memory_entry_t *vtable_sqlite_get_prioritized(memory_storage_t *st, size_t limit, size_t *count);

/* ================================================================
 *  Internal: In-memory storage data
 * ================================================================ */

#define INMEM_CAPACITY 256

typedef struct {
    memory_entry_t *entries;
    size_t          count;
    size_t          capacity;
} inmem_data_t;

/* ================================================================
 *  Internal: File storage data
 * ================================================================ */

typedef struct {
    char    path[512];
    json_t *cache;   /* cached JSON object, NULL if not loaded */
    bool    dirty;   /* true if cache was modified since last persist */
} filedata_t;

/* ================================================================
 *  FNV-1a hash for dedup (P154)
 * ================================================================ */

uint64_t memory_hash_content(const char *content) {
    if (!content) return 0;
    uint64_t hash = 14695981039346656037ULL; /* FNV offset basis */
    const unsigned char *p = (const unsigned char *)content;
    while (*p) {
        hash ^= (uint64_t)(*p);
        hash *= 1099511628211ULL; /* FNV prime */
        p++;
    }
    return hash;
}

/* ================================================================
 *  Entry helpers
 * ================================================================ */

bool memory_entry_expired(const memory_entry_t *entry) {
    if (!entry) return true;
    if (entry->expires_at == 0) return false;
    return time(NULL) >= entry->expires_at;
}

bool memory_entry_from_json(memory_entry_t *entry, const json_t *obj) {
    if (!entry || !obj) return false;
    memset(entry, 0, sizeof(*entry));

    const char *key = json_get_str(obj, "key", "");
    snprintf(entry->key, sizeof(entry->key), "%s", key);

    const char *content = json_get_str(obj, "content", "");
    snprintf(entry->content, sizeof(entry->content), "%s", content);

    entry->created_at    = (time_t)json_get_num(obj, "created_at", 0);
    entry->updated_at    = (time_t)json_get_num(obj, "updated_at", 0);
    entry->expires_at    = (time_t)json_get_num(obj, "expires_at", 0);
    entry->priority      = (int)json_get_num(obj, "priority", 0);
    entry->hash          = (uint64_t)json_get_num(obj, "hash", 0);
    entry->compressed    = json_get_bool(obj, "compressed", false);
    entry->access_count  = (int)json_get_num(obj, "access_count", 0);
    entry->last_accessed = (time_t)json_get_num(obj, "last_accessed", 0);

    /* Parse tags */
    const json_t *tags_arr = json_obj_get(obj, "tags");
    if (tags_arr && tags_arr->type == JSON_ARRAY) {
        size_t n = json_len(tags_arr);
        for (size_t i = 0; i < n && entry->tag_count < MEMORY_TAGS_MAX; i++) {
            json_t *tag = json_get(tags_arr, i);
            if (tag && tag->type == JSON_STRING) {
                snprintf(entry->tags[entry->tag_count], MEMORY_TAG_MAX, "%s", tag->str_val);
                entry->tag_count++;
            }
        }
    }

    return true;
}

json_t *memory_entry_to_json(const memory_entry_t *entry) {
    if (!entry) return json_null();
    json_t *obj = json_new_object();
    if (!obj) return NULL;

    json_set(obj, "key", json_string(entry->key));
    json_set(obj, "content", json_string(entry->content));
    json_set(obj, "created_at", json_number((double)entry->created_at));
    json_set(obj, "updated_at", json_number((double)entry->updated_at));
    json_set(obj, "expires_at", json_number((double)entry->expires_at));
    json_set(obj, "priority", json_number((double)entry->priority));
    json_set(obj, "hash", json_number((double)entry->hash));
    json_set(obj, "compressed", json_bool(entry->compressed));
    json_set(obj, "access_count", json_number((double)entry->access_count));
    json_set(obj, "last_accessed", json_number((double)entry->last_accessed));

    if (entry->tag_count > 0) {
        json_t *tags = json_new_array();
        for (int i = 0; i < entry->tag_count; i++) {
            json_append(tags, json_string(entry->tags[i]));
        }
        json_set(obj, "tags", tags);
    }

    return obj;
}

/* ================================================================
 *  Default storage vtable for in-memory
 * ================================================================ */

static memory_storage_vtable_t inmem_vtable = {
    .name              = "inmem",
    .open              = vtable_inmem_open,
    .close             = vtable_inmem_close,
    .store             = vtable_inmem_store,
    .get               = vtable_inmem_get,
    .delete            = vtable_inmem_delete,
    .clear             = vtable_inmem_clear,
    .count             = vtable_inmem_count,
    .list_keys         = vtable_inmem_list_keys,
    .search            = vtable_inmem_search,
    .import_json       = vtable_inmem_import_json,
    .export_json       = vtable_inmem_export_json,
    .get_by_hash       = vtable_inmem_get_by_hash,
    .persist           = vtable_inmem_persist,
    .load              = vtable_inmem_load,
    .compress_old      = vtable_inmem_compress_old,
    .get_prioritized   = vtable_inmem_get_prioritized,
};

/* ================================================================
 *  In-memory backend implementation
 * ================================================================ */

bool vtable_inmem_open(memory_storage_t *st, const char *uri) {
    (void)uri;
    inmem_data_t *d = (inmem_data_t *)calloc(1, sizeof(inmem_data_t));
    if (!d) return false;
    d->capacity = INMEM_CAPACITY;
    d->entries = (memory_entry_t *)calloc(d->capacity, sizeof(memory_entry_t));
    if (!d->entries) { free(d); return false; }
    st->data = d;
    return true;
}

void vtable_inmem_close(memory_storage_t *st) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (d) {
        free(d->entries);
        free(d);
    }
    st->data = NULL;
}

/* Find entry by key using linear search */
static memory_entry_t *inmem_find(inmem_data_t *d, const char *key) {
    for (size_t i = 0; i < d->count; i++) {
        if (strcmp(d->entries[i].key, key) == 0)
            return &d->entries[i];
    }
    return NULL;
}

bool vtable_inmem_store(memory_storage_t *st, memory_entry_t *entry) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !entry) return false;

    /* Auto-generate key if empty */
    if (!entry->key[0]) {
        snprintf(entry->key, sizeof(entry->key), "entry_%zu", d->count);
    }

    /* Check if key exists — update in place */
    memory_entry_t *existing = inmem_find(d, entry->key);
    if (existing) {
        memcpy(existing, entry, sizeof(memory_entry_t));
        return true;
    }

    /* Need to grow? */
    if (d->count >= d->capacity) {
        size_t newcap = d->capacity * 2;
        memory_entry_t *newents = (memory_entry_t *)realloc(d->entries, newcap * sizeof(memory_entry_t));
        if (!newents) return false;
        d->entries = newents;
        d->capacity = newcap;
    }

    memcpy(&d->entries[d->count], entry, sizeof(memory_entry_t));
    d->count++;
    return true;
}

bool vtable_inmem_get(memory_storage_t *st, const char *key, memory_entry_t *entry) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !key || !entry) return false;

    memory_entry_t *found = inmem_find(d, key);
    if (!found) return false;

    memcpy(entry, found, sizeof(memory_entry_t));
    return true;
}

bool vtable_inmem_delete(memory_storage_t *st, const char *key) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !key) return false;

    for (size_t i = 0; i < d->count; i++) {
        if (strcmp(d->entries[i].key, key) == 0) {
            /* Shift remaining left */
            if (i + 1 < d->count) {
                memmove(&d->entries[i], &d->entries[i + 1],
                        (d->count - i - 1) * sizeof(memory_entry_t));
            }
            d->count--;
            return true;
        }
    }
    return false;
}

void vtable_inmem_clear(memory_storage_t *st) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (d) d->count = 0;
}

size_t vtable_inmem_count(memory_storage_t *st) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    return d ? d->count : 0;
}

char **vtable_inmem_list_keys(memory_storage_t *st, size_t *count) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !count) return NULL;

    *count = d->count;
    if (d->count == 0) return NULL;

    char **keys = (char **)calloc(d->count, sizeof(char *));
    if (!keys) { *count = 0; return NULL; }

    for (size_t i = 0; i < d->count; i++) {
        keys[i] = strdup(d->entries[i].key);
        if (!keys[i]) {
            for (size_t j = 0; j < i; j++) free(keys[j]);
            free(keys);
            *count = 0;
            return NULL;
        }
    }
    return keys;
}

memory_entry_t *vtable_inmem_search(memory_storage_t *st, const char *query, int limit) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !query) return NULL;

    if (limit <= 0) limit = 10;

    /* Simple keyword matching (case-insensitive substring) */
    memory_entry_t *results = (memory_entry_t *)calloc((size_t)limit, sizeof(memory_entry_t));
    if (!results) return NULL;

    size_t found = 0;
    char query_lower[256];
    size_t qlen = strlen(query);
    for (size_t i = 0; i < qlen && i < sizeof(query_lower) - 1; i++)
        query_lower[i] = (char)tolower((unsigned char)query[i]);
    query_lower[qlen < sizeof(query_lower) ? qlen : sizeof(query_lower) - 1] = '\0';

    for (size_t i = 0; i < d->count && (int)found < limit; i++) {
        /* Skip expired */
        if (memory_entry_expired(&d->entries[i])) continue;

        /* Check content */
        bool matched = false;
        if (strstr(d->entries[i].content, query)) {
            matched = true;
        } else {
            /* Case-insensitive check */
            size_t clen = strlen(d->entries[i].content);
            char *clower = (char *)malloc(clen + 1);
            if (clower) {
                for (size_t j = 0; j < clen; j++)
                    clower[j] = (char)tolower((unsigned char)d->entries[i].content[j]);
                clower[clen] = '\0';
                if (strstr(clower, query_lower)) matched = true;
                free(clower);
            }
        }

        /* Check key */
        if (!matched && strstr(d->entries[i].key, query)) {
            matched = true;
        }

        /* Check tags */
        if (!matched) {
            for (int t = 0; t < d->entries[i].tag_count; t++) {
                if (strstr(d->entries[i].tags[t], query)) {
                    matched = true;
                    break;
                }
            }
        }

        if (matched) {
            memcpy(&results[found], &d->entries[i], sizeof(memory_entry_t));
            found++;
        }
    }

    if (found == 0) {
        free(results);
        return NULL;
    }

    return results; /* Caller must free — but count isn't returned here */
    /* Note: this returns up to `limit` entries, but caller doesn't know count.
       We fix this in the memory_search wrapper. */
}

/* Internal version of search that returns count correctly */
static memory_entry_t *inmem_search_internal(memory_storage_t *st, const char *query, int limit, size_t *out_count) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !query || !out_count) { if (out_count) *out_count = 0; return NULL; }

    if (limit <= 0) limit = 10;

    /* Allocate enough for all matches */
    memory_entry_t *results = (memory_entry_t *)calloc(d->count, sizeof(memory_entry_t));
    if (!results) { *out_count = 0; return NULL; }

    size_t found = 0;
    char query_lower[256];
    size_t qlen = strlen(query);
    for (size_t i = 0; i < qlen && i < sizeof(query_lower) - 1; i++)
        query_lower[i] = (char)tolower((unsigned char)query[i]);
    query_lower[qlen < sizeof(query_lower) ? qlen : sizeof(query_lower) - 1] = '\0';

    for (size_t i = 0; i < d->count && (int)found < limit; i++) {
        if (memory_entry_expired(&d->entries[i])) continue;

        const memory_entry_t *e = &d->entries[i];
        bool matched = (strstr(e->content, query) != NULL);

        if (!matched) {
            size_t clen = strlen(e->content);
            char *clower = (char *)malloc(clen + 1);
            if (clower) {
                for (size_t j = 0; j < clen; j++)
                    clower[j] = (char)tolower((unsigned char)e->content[j]);
                clower[clen] = '\0';
                if (strstr(clower, query_lower)) matched = true;
                free(clower);
            }
        }

        if (!matched && strstr(e->key, query)) matched = true;
        if (!matched) {
            for (int t = 0; t < e->tag_count; t++) {
                if (strstr(e->tags[t], query)) { matched = true; break; }
            }
        }

        if (matched) {
            memcpy(&results[found], e, sizeof(memory_entry_t));
            found++;
        }
    }

    if (found == 0) {
        free(results);
        *out_count = 0;
        return NULL;
    }

    *out_count = found;
    return results;
}

int vtable_inmem_import_json(memory_storage_t *st, const json_t *entries) {
    if (!st || !entries || entries->type != JSON_ARRAY) return 0;
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d) return 0;

    size_t n = json_len(entries);
    int imported = 0;

    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(entries, i);
        if (!item || item->type != JSON_OBJECT) continue;

        memory_entry_t entry;
        if (memory_entry_from_json(&entry, item)) {
            /* Auto-fill timestamps if missing */
            if (entry.created_at == 0) entry.created_at = time(NULL);
            if (entry.updated_at == 0) entry.updated_at = entry.created_at;

            /* Check dedup by hash — skip if duplicate */
            bool dup = false;
            for (size_t j = 0; j < d->count; j++) {
                if (d->entries[j].hash == entry.hash && entry.hash != 0) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;

            /* Auto-generate key if needed */
            if (!entry.key[0]) {
                snprintf(entry.key, sizeof(entry.key), "entry_%zu", d->count + imported);
            }

            /* Store */
            if (vtable_inmem_store(st, &entry)) {
                imported++;
            }
        }
    }
    return imported;
}

json_t *vtable_inmem_export_json(memory_storage_t *st) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d) return json_new_array();

    json_t *arr = json_new_array();
    if (!arr) return NULL;

    for (size_t i = 0; i < d->count; i++) {
        json_t *obj = memory_entry_to_json(&d->entries[i]);
        json_append(arr, obj);
    }
    return arr;
}

bool vtable_inmem_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !entry || hash == 0) return false;

    for (size_t i = 0; i < d->count; i++) {
        if (d->entries[i].hash == hash) {
            memcpy(entry, &d->entries[i], sizeof(memory_entry_t));
            return true;
        }
    }
    return false;
}

bool vtable_inmem_persist(memory_storage_t *st) {
    (void)st;
    return true; /* In-memory: no-op */
}

bool vtable_inmem_load(memory_storage_t *st) {
    (void)st;
    return true; /* In-memory: no-op */
}

int vtable_inmem_compress_old(memory_storage_t *st, time_t before,
                               char *(*compress_cb)(const char *content)) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !compress_cb) return 0;

    int compressed = 0;
    for (size_t i = 0; i < d->count; i++) {
        if (d->entries[i].compressed) continue;
        if (d->entries[i].created_at >= before) continue;

        char *compressed_content = compress_cb(d->entries[i].content);
        if (compressed_content) {
            snprintf(d->entries[i].content, sizeof(d->entries[i].content), "%s", compressed_content);
            d->entries[i].compressed = true;
            d->entries[i].updated_at = time(NULL);
            free(compressed_content);
            compressed++;
        }
    }
    return compressed;
}

static int entry_cmp_priority_desc(const void *a, const void *b) {
    const memory_entry_t *ea = (const memory_entry_t *)a;
    const memory_entry_t *eb = (const memory_entry_t *)b;
    if (eb->priority != ea->priority)
        return eb->priority - ea->priority; /* higher first */
    /* Secondary sort: LRU (last_accessed ascending = least recently used first, but
     * for sliding window we want most recently accessed first) */
    if (eb->last_accessed != ea->last_accessed)
        return (int)(eb->last_accessed - ea->last_accessed);
    return (int)(eb->created_at - ea->created_at);
}

memory_entry_t *vtable_inmem_get_prioritized(memory_storage_t *st, size_t limit, size_t *count) {
    inmem_data_t *d = (inmem_data_t *)st->data;
    if (!d || !count) { if (count) *count = 0; return NULL; }

    if (d->count == 0) { *count = 0; return NULL; }

    /* Copy entries */
    memory_entry_t *sorted = (memory_entry_t *)calloc(d->count, sizeof(memory_entry_t));
    if (!sorted) { *count = 0; return NULL; }

    size_t active = 0;
    for (size_t i = 0; i < d->count; i++) {
        if (!memory_entry_expired(&d->entries[i])) {
            memcpy(&sorted[active], &d->entries[i], sizeof(memory_entry_t));
            active++;
        }
    }

    if (active == 0) { free(sorted); *count = 0; return NULL; }

    /* Sort by priority (desc), then last_accessed (desc) */
    qsort(sorted, active, sizeof(memory_entry_t), entry_cmp_priority_desc);

    if (limit > 0 && active > limit) active = limit;

    memory_entry_t *result = (memory_entry_t *)calloc(active, sizeof(memory_entry_t));
    if (!result) { free(sorted); *count = 0; return NULL; }
    memcpy(result, sorted, active * sizeof(memory_entry_t));
    free(sorted);

    *count = active;
    return result;
}

/* ================================================================
 *  File backend: vtable implementations (P151)
 * ================================================================ */

static memory_storage_vtable_t file_vtable = {
    .name              = "file",
    .open              = vtable_file_open,
    .close             = vtable_file_close,
    .store             = vtable_file_store,
    .get               = vtable_file_get,
    .delete            = vtable_file_delete,
    .clear             = vtable_file_clear,
    .count             = vtable_file_count,
    .list_keys         = vtable_file_list_keys,
    .search            = vtable_file_search,
    .import_json       = vtable_file_import_json,
    .export_json       = vtable_file_export_json,
    .get_by_hash       = vtable_file_get_by_hash,
    .persist           = vtable_file_persist,
    .load              = vtable_file_load,
    .compress_old      = vtable_file_compress_old,
    .get_prioritized   = vtable_file_get_prioritized,
};

/* ================================================================
 *  SQLite backend: vtable (F06)
 * ================================================================ */

static memory_storage_vtable_t sqlite_vtable = {
    .name              = "sqlite",
    .open              = vtable_sqlite_open,
    .close             = vtable_sqlite_close,
    .store             = vtable_sqlite_store,
    .get               = vtable_sqlite_get,
    .delete            = vtable_sqlite_delete,
    .clear             = vtable_sqlite_clear,
    .count             = vtable_sqlite_count,
    .list_keys         = vtable_sqlite_list_keys,
    .search            = vtable_sqlite_search,
    .import_json       = vtable_sqlite_import_json,
    .export_json       = vtable_sqlite_export_json,
    .get_by_hash       = vtable_sqlite_get_by_hash,
    .persist           = vtable_sqlite_persist,
    .load              = vtable_sqlite_load,
    .compress_old      = vtable_sqlite_compress_old,
    .get_prioritized   = vtable_sqlite_get_prioritized,
};

/* ================================================================
 *  SQLite backend: implementations
 * ================================================================ */

/* Tags helpers: serialize to/from comma-separated string */
static char *tags_to_string(const memory_entry_t *entry) {
    static char buf[1024];
    buf[0] = '\0';
    for (int i = 0; i < entry->tag_count && i < MEMORY_TAGS_MAX; i++) {
        if (i > 0) strcat(buf, ",");
        strcat(buf, entry->tags[i]);
    }
    return buf;
}

static void string_to_tags(const char *s, memory_entry_t *entry) {
    entry->tag_count = 0;
    if (!s || !*s) return;
    char buf[1024];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tok = strtok(buf, ",");
    while (tok && entry->tag_count < MEMORY_TAGS_MAX) {
        strncpy(entry->tags[entry->tag_count], tok, MEMORY_TAG_MAX - 1);
        entry->tags[entry->tag_count][MEMORY_TAG_MAX - 1] = '\0';
        entry->tag_count++;
        tok = strtok(NULL, ",");
    }
}

static bool vtable_sqlite_open(memory_storage_t *st, const char *uri) {
    if (!st || !uri) return false;
    sqlite3 *db = NULL;
    if (sqlite3_open(uri, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    const char *sql = "CREATE TABLE IF NOT EXISTS memory_entries ("
        "key TEXT PRIMARY KEY,"
        "content TEXT NOT NULL,"
        "hash INTEGER NOT NULL DEFAULT 0,"
        "priority INTEGER NOT NULL DEFAULT 0,"
        "created_at INTEGER NOT NULL DEFAULT 0,"
        "updated_at INTEGER NOT NULL DEFAULT 0,"
        "expires_at INTEGER NOT NULL DEFAULT 0,"
        "tags TEXT NOT NULL DEFAULT '',"
        "compressed INTEGER NOT NULL DEFAULT 0,"
        "access_count INTEGER NOT NULL DEFAULT 0,"
        "last_accessed INTEGER NOT NULL DEFAULT 0"
    ")";
    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }
    st->data = db;
    return true;
}

static void vtable_sqlite_close(memory_storage_t *st) {
    if (st && st->data) {
        sqlite3_close((sqlite3 *)st->data);
        st->data = NULL;
    }
}

/* Bind entry fields to a prepared INSERT OR REPLACE statement */
static void bind_entry(sqlite3_stmt *stmt, const memory_entry_t *entry) {
    sqlite3_bind_text(stmt, 1, entry->key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entry->content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)entry->hash);
    sqlite3_bind_int(stmt, 4, entry->priority);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)entry->created_at);
    sqlite3_bind_int64(stmt, 6, (sqlite3_int64)entry->updated_at);
    sqlite3_bind_int64(stmt, 7, (sqlite3_int64)entry->expires_at);
    char *tags = tags_to_string(entry);
    sqlite3_bind_text(stmt, 8, tags, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, entry->compressed ? 1 : 0);
    sqlite3_bind_int(stmt, 10, entry->access_count);
    sqlite3_bind_int64(stmt, 11, (sqlite3_int64)entry->last_accessed);
}

/* Read a row into an entry. Returns true if row had data. */
static bool read_row(sqlite3_stmt *stmt, memory_entry_t *entry) {
    if (sqlite3_step(stmt) != SQLITE_ROW) return false;
    memset(entry, 0, sizeof(*entry));
    const char *k = (const char *)sqlite3_column_text(stmt, 0);
    if (k) strncpy(entry->key, k, sizeof(entry->key) - 1);
    const char *c = (const char *)sqlite3_column_text(stmt, 1);
    if (c) strncpy(entry->content, c, sizeof(entry->content) - 1);
    entry->hash = (uint64_t)sqlite3_column_int64(stmt, 2);
    entry->priority = sqlite3_column_int(stmt, 3);
    entry->created_at = (time_t)sqlite3_column_int64(stmt, 4);
    entry->updated_at = (time_t)sqlite3_column_int64(stmt, 5);
    entry->expires_at = (time_t)sqlite3_column_int64(stmt, 6);
    const char *t = (const char *)sqlite3_column_text(stmt, 7);
    if (t) string_to_tags(t, entry);
    entry->compressed = sqlite3_column_int(stmt, 8) != 0;
    entry->access_count = sqlite3_column_int(stmt, 9);
    entry->last_accessed = (time_t)sqlite3_column_int64(stmt, 10);
    return true;
}

static bool vtable_sqlite_store(memory_storage_t *st, memory_entry_t *entry) {
    if (!st || !st->data || !entry) return false;
    sqlite3 *db = (sqlite3 *)st->data;
    const char *sql = "INSERT OR REPLACE INTO memory_entries "
        "(key,content,hash,priority,created_at,updated_at,expires_at,"
        "tags,compressed,access_count,last_accessed) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    bind_entry(stmt, entry);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

static bool vtable_sqlite_get(memory_storage_t *st, const char *key, memory_entry_t *entry) {
    if (!st || !st->data || !key || !entry) return false;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM memory_entries WHERE key=?1", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    bool found = read_row(stmt, entry);
    sqlite3_finalize(stmt);
    return found;
}

static bool vtable_sqlite_delete(memory_storage_t *st, const char *key) {
    if (!st || !st->data || !key) return false;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM memory_entries WHERE key=?1", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

static void vtable_sqlite_clear(memory_storage_t *st) {
    if (!st || !st->data) return;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_exec(db, "DELETE FROM memory_entries", NULL, NULL, NULL);
}

static size_t vtable_sqlite_count(memory_storage_t *st) {
    if (!st || !st->data) return 0;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM memory_entries", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    size_t n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) n = (size_t)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

static char **vtable_sqlite_list_keys(memory_storage_t *st, size_t *count) {
    if (!st || !st->data) { if (count) *count = 0; return NULL; }
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT key FROM memory_entries ORDER BY key", -1, &stmt, NULL) != SQLITE_OK) {
        if (count) *count = 0;
        return NULL;
    }
    size_t cap = 64, n = 0;
    char **keys = (char **)calloc(cap, sizeof(char *));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (n >= cap) { cap *= 2; keys = (char **)realloc(keys, cap * sizeof(char *)); }
        const char *k = (const char *)sqlite3_column_text(stmt, 0);
        keys[n] = k ? strdup(k) : NULL;
        n++;
    }
    sqlite3_finalize(stmt);
    if (count) *count = n;
    return keys;
}

static memory_entry_t *vtable_sqlite_search(memory_storage_t *st, const char *query, int limit) {
    if (!st || !st->data || !query) return NULL;
    sqlite3 *db = (sqlite3 *)st->data;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM memory_entries WHERE content LIKE '%%%s%%' OR key LIKE '%%%s%%' ORDER BY priority DESC",
        query, query);
    if (limit > 0) { char *p = sql + strlen(sql); snprintf(p, sizeof(sql) - (size_t)(p - sql), " LIMIT %d", limit); }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    size_t cap = 16, n = 0;
    memory_entry_t *results = (memory_entry_t *)calloc(cap, sizeof(memory_entry_t));
    while (read_row(stmt, &results[n])) {
        n++;
        if (n >= cap) { cap *= 2; results = (memory_entry_t *)realloc(results, cap * sizeof(memory_entry_t)); memset(&results[n], 0, (cap - n) * sizeof(memory_entry_t)); }
    }
    sqlite3_finalize(stmt);
    return results;
}

static int vtable_sqlite_import_json(memory_storage_t *st, const json_t *entries) {
    if (!st || !st->data || !entries) return 0;
    size_t n = json_len(entries);
    int imported = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(entries, i);
        memory_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        if (memory_entry_from_json(&entry, obj) && vtable_sqlite_store(st, &entry))
            imported++;
    }
    return imported;
}

static json_t *vtable_sqlite_export_json(memory_storage_t *st) {
    if (!st || !st->data) return NULL;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM memory_entries ORDER BY key", -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    json_t *arr = json_new_array();
    memory_entry_t entry;
    while (read_row(stmt, &entry)) {
        json_t *obj = memory_entry_to_json(&entry);
        if (obj) json_append(arr, obj);
    }
    sqlite3_finalize(stmt);
    return arr;
}

static bool vtable_sqlite_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry) {
    if (!st || !st->data || !entry) return false;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM memory_entries WHERE hash=?1", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)hash);
    bool found = read_row(stmt, entry);
    sqlite3_finalize(stmt);
    return found;
}

static bool vtable_sqlite_persist(memory_storage_t *st) { (void)st; return true; } /* SQLite auto-persists */
static bool vtable_sqlite_load(memory_storage_t *st) { (void)st; return true; }    /* SQLite auto-loads */

static int vtable_sqlite_compress_old(memory_storage_t *st, time_t before,
                                      char *(*compress_cb)(const char *content)) {
    if (!st || !st->data || !compress_cb) return 0;
    sqlite3 *db = (sqlite3 *)st->data;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM memory_entries WHERE updated_at<?1 AND compressed=0",
                           -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)before);
    int count = 0;
    memory_entry_t entry;
    while (read_row(stmt, &entry)) {
        char *compressed = compress_cb(entry.content);
        if (compressed) {
            strncpy(entry.content, compressed, sizeof(entry.content) - 1);
            entry.compressed = true;
            entry.updated_at = time(NULL);
            vtable_sqlite_store(st, &entry);
            free(compressed);
            count++;
        }
    }
    sqlite3_finalize(stmt);
    return count;
}

static memory_entry_t *vtable_sqlite_get_prioritized(memory_storage_t *st, size_t limit, size_t *count) {
    if (!st || !st->data) { if (count) *count = 0; return NULL; }
    sqlite3 *db = (sqlite3 *)st->data;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT * FROM memory_entries ORDER BY priority DESC%s",
             limit > 0 ? " LIMIT ?1" : "");
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { if (count) *count = 0; return NULL; }
    if (limit > 0) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)limit);
    size_t cap = 16, n = 0;
    memory_entry_t *results = (memory_entry_t *)calloc(cap, sizeof(memory_entry_t));
    while (read_row(stmt, &results[n])) {
        n++;
        if (n >= cap) { cap *= 2; results = (memory_entry_t *)realloc(results, cap * sizeof(memory_entry_t)); memset(&results[n], 0, (cap - n) * sizeof(memory_entry_t)); }
    }
    sqlite3_finalize(stmt);
    if (count) *count = n;
    return results;
}

/* Convert JSON entry object (stored as {key: {content, ...}}) to memory_entry_t */
/* (file_json_to_entry replaced by memory_entry_from_json) */

bool vtable_file_open(memory_storage_t *st, const char *uri) {
    filedata_t *fd = (filedata_t *)calloc(1, sizeof(filedata_t));
    if (!fd) return false;

    if (uri) snprintf(fd->path, sizeof(fd->path), "%s", uri);

    /* Ensure directory exists */
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", fd->path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }

    st->data = fd;

    /* Try to load existing file */
    vtable_file_load(st);
    return true;
}

void vtable_file_close(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (fd) {
        if (fd->dirty) vtable_file_persist(st);
        if (fd->cache) json_free(fd->cache);
        free(fd);
    }
    st->data = NULL;
}

/* Store entry as JSON object under its key. To maintain backward compatibility,
 * we store entries as {key: {content, metadata...}} inside the root object. */
bool vtable_file_store(memory_storage_t *st, memory_entry_t *entry) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !entry) return false;

    if (!fd->cache) {
        fd->cache = json_new_object();
        if (!fd->cache) return false;
    }

    /* Auto-generate key if empty */
    if (!entry->key[0]) {
        size_t n = json_len(fd->cache);
        snprintf(entry->key, sizeof(entry->key), "entry_%zu", n);
    }

    /* Set timestamps */
    if (entry->created_at == 0) entry->created_at = time(NULL);
    entry->updated_at = time(NULL);

    /* Compute hash if empty */
    if (entry->hash == 0) {
        entry->hash = memory_hash_content(entry->content);
    }

    json_t *obj = memory_entry_to_json(entry);
    if (!obj) return false;

    json_set(fd->cache, entry->key, obj);
    fd->dirty = true;
    return true;
}

bool vtable_file_get(memory_storage_t *st, const char *key, memory_entry_t *entry) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !key || !entry) return false;

    if (!fd->cache) return false;

    json_t *obj = json_obj_get(fd->cache, key);
    if (!obj) return false;

    return memory_entry_from_json(entry, obj);
}

bool vtable_file_delete(memory_storage_t *st, const char *key) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !key || !fd->cache) return false;

    json_t *existing = json_obj_get(fd->cache, key);
    if (!existing) return false;

    /* Remove by deleting and recreating the object */
    json_t *old = fd->cache;
    fd->cache = json_new_object();
    if (!fd->cache) { fd->cache = old; return false; }

    /* Copy all keys except the one to delete */
    size_t n = json_len(old);
    for (size_t i = 0; i < n; i++) {
        const char *k = old->c.keys[i];
        if (strcmp(k, key) != 0) {
            json_set(fd->cache, k, json_copy(json_get(old, i)));
        }
    }
    json_free(old);
    fd->dirty = true;
    return true;
}

void vtable_file_clear(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd) return;
    if (fd->cache) json_free(fd->cache);
    fd->cache = json_new_object();
    fd->dirty = true;
}

size_t vtable_file_count(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache) return 0;
    return json_len(fd->cache);
}

char **vtable_file_list_keys(memory_storage_t *st, size_t *count) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !count) {
        if (count) *count = 0;
        return NULL;
    }

    size_t n = json_len(fd->cache);
    if (n == 0) { *count = 0; return NULL; }

    char **keys = (char **)calloc(n, sizeof(char *));
    if (!keys) { *count = 0; return NULL; }

    size_t idx = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = fd->cache->c.keys[i];
        if (k) {
            keys[idx] = strdup(k);
            if (!keys[idx]) {
                for (size_t j = 0; j < idx; j++) free(keys[j]);
                free(keys);
                *count = 0;
                return NULL;
            }
            idx++;
        }
    }

    *count = idx;
    return keys;
}

memory_entry_t *vtable_file_search(memory_storage_t *st, const char *query, int limit) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !query) return NULL;

    if (limit <= 0) limit = 10;
    size_t n = json_len(fd->cache);

    memory_entry_t *results = (memory_entry_t *)calloc((size_t)limit, sizeof(memory_entry_t));
    if (!results) return NULL;

    size_t found = 0;
    char query_lower[256];
    size_t qlen = strlen(query);
    for (size_t i = 0; i < qlen && i < sizeof(query_lower) - 1; i++)
        query_lower[i] = (char)tolower((unsigned char)query[i]);
    query_lower[qlen < sizeof(query_lower) ? qlen : sizeof(query_lower) - 1] = '\0';

    for (size_t i = 0; i < n && (int)found < limit; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (!obj || obj->type != JSON_OBJECT) continue;

        memory_entry_t entry;
        if (!memory_entry_from_json(&entry, obj)) continue;
        if (memory_entry_expired(&entry)) continue;

        bool matched = (strstr(entry.content, query) != NULL);
        if (!matched) {
            size_t clen = strlen(entry.content);
            char *clower = (char *)malloc(clen + 1);
            if (clower) {
                for (size_t j = 0; j < clen; j++)
                    clower[j] = (char)tolower((unsigned char)entry.content[j]);
                clower[clen] = '\0';
                if (strstr(clower, query_lower)) matched = true;
                free(clower);
            }
        }
        if (!matched && strstr(entry.key, query)) matched = true;
        if (!matched) {
            for (int t = 0; t < entry.tag_count; t++) {
                if (strstr(entry.tags[t], query)) { matched = true; break; }
            }
        }

        if (matched) {
            memcpy(&results[found], &entry, sizeof(memory_entry_t));
            found++;
        }
    }

    if (found == 0) { free(results); return NULL; }

    return results;
}

int vtable_file_import_json(memory_storage_t *st, const json_t *entries) {
    if (!st || !entries || entries->type != JSON_ARRAY) return 0;
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd) return 0;

    if (!fd->cache) {
        fd->cache = json_new_object();
        if (!fd->cache) return 0;
    }

    size_t n = json_len(entries);
    int imported = 0;

    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(entries, i);
        if (!item || item->type != JSON_OBJECT) continue;

        memory_entry_t entry;
        if (memory_entry_from_json(&entry, item)) {
            if (!entry.key[0]) continue; /* key required for file backend */
            if (entry.hash == 0) entry.hash = memory_hash_content(entry.content);
            if (entry.created_at == 0) entry.created_at = time(NULL);

            /* Check dedup */
            if (entry.hash != 0) {
                json_t *existing = json_obj_get(fd->cache, entry.key);
                if (existing) {
                    uint64_t ehash = (uint64_t)json_get_num(existing, "hash", 0);
                    if (ehash == entry.hash) continue; /* skip duplicate */
                }
            }

            json_t *obj = memory_entry_to_json(&entry);
            if (obj) {
                json_set(fd->cache, entry.key, obj);
                imported++;
                fd->dirty = true;
            }
        }
    }
    return imported;
}

json_t *vtable_file_export_json(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache) return json_new_array();

    json_t *arr = json_new_array();
    if (!arr) return NULL;

    size_t n = json_len(fd->cache);
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (obj) {
            json_append(arr, json_copy(obj));
        }
    }
    return arr;
}

bool vtable_file_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !entry || hash == 0) return false;

    size_t n = json_len(fd->cache);
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (!obj || obj->type != JSON_OBJECT) continue;
        uint64_t ehash = (uint64_t)json_get_num(obj, "hash", 0);
        if (ehash == hash) {
            return memory_entry_from_json(entry, obj);
        }
    }
    return false;
}

bool vtable_file_persist(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !fd->dirty) return true;

    char *json_str = json_serialize(fd->cache);
    if (!json_str) return false;

    FILE *f = fopen(fd->path, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);

    fd->dirty = false;
    return true;
}

bool vtable_file_load(memory_storage_t *st) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd) return false;

    if (fd->cache) json_free(fd->cache);

    char *err = NULL;
    fd->cache = json_parse_file(fd->path, &err);
    if (err) free(err);

    if (!fd->cache) {
        fd->cache = json_new_object();
        if (!fd->cache) return false;
    }

    fd->dirty = false;
    return true;
}

int vtable_file_compress_old(memory_storage_t *st, time_t before,
                              char *(*compress_cb)(const char *content)) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !compress_cb) return 0;

    size_t n = json_len(fd->cache);
    int compressed = 0;

    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (!obj || obj->type != JSON_OBJECT) continue;

        time_t created = (time_t)json_get_num(obj, "created_at", 0);
        bool already_compressed = json_get_bool(obj, "compressed", false);
        if (already_compressed) continue;
        if (created >= before) continue;

        const char *content = json_get_str(obj, "content", "");
        if (!content || !*content) continue;

        char *compressed_content = compress_cb(content);
        if (compressed_content) {
            const char *key = fd->cache->c.keys[i];
            json_set(obj, "content", json_string(compressed_content));
            json_set(obj, "compressed", json_bool(true));
            json_set(obj, "updated_at", json_number((double)time(NULL)));
            free(compressed_content);

            /* Re-set in cache so it writes back */
            if (key) json_set(fd->cache, key, obj);
            fd->dirty = true;
            compressed++;
        }
    }
    return compressed;
}

memory_entry_t *vtable_file_get_prioritized(memory_storage_t *st, size_t limit, size_t *count) {
    filedata_t *fd = (filedata_t *)st->data;
    if (!fd || !fd->cache || !count) { if (count) *count = 0; return NULL; }

    size_t n = json_len(fd->cache);
    if (n == 0) { *count = 0; return NULL; }

    /* Collect all entries */
    memory_entry_t *all = (memory_entry_t *)calloc(n, sizeof(memory_entry_t));
    if (!all) { *count = 0; return NULL; }

    size_t active = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *obj = json_get(fd->cache, i);
        if (!obj || obj->type != JSON_OBJECT) continue;
        memory_entry_t entry;
        if (memory_entry_from_json(&entry, obj)) {
            if (!memory_entry_expired(&entry)) {
                memcpy(&all[active], &entry, sizeof(memory_entry_t));
                active++;
            }
        }
    }

    if (active == 0) { free(all); *count = 0; return NULL; }

    qsort(all, active, sizeof(memory_entry_t), entry_cmp_priority_desc);

    if (limit > 0 && active > limit) active = limit;

    memory_entry_t *result = (memory_entry_t *)calloc(active, sizeof(memory_entry_t));
    if (!result) { free(all); *count = 0; return NULL; }
    memcpy(result, all, active * sizeof(memory_entry_t));
    free(all);

    *count = active;
    return result;
}

/* ================================================================
 *  Storage backend constructors
 * ================================================================ */

bool memory_storage_inmem_init(memory_storage_t *st) {
    if (!st) return false;
    memset(st, 0, sizeof(*st));
    st->type = MEMORY_STORAGE_INMEM;
    st->vtable = inmem_vtable;
    return st->vtable.open(st, "");
}

bool memory_storage_file_init(memory_storage_t *st, const char *path) {
    if (!st || !path) return false;
    memset(st, 0, sizeof(*st));
    st->type = MEMORY_STORAGE_FILE;
    st->vtable = file_vtable;
    snprintf(st->uri, sizeof(st->uri), "%s", path);
    return st->vtable.open(st, path);
}

bool memory_storage_sqlite_init(memory_storage_t *st, const char *path) {
    if (!st) return false;
    memset(st, 0, sizeof(*st));
    st->type = MEMORY_STORAGE_SQLITE;
    st->vtable = sqlite_vtable;
    snprintf(st->uri, sizeof(st->uri), "%s", path ? path : "memory.db");
    return st->vtable.open(st, path ? path : "memory.db");
}

/* ================================================================
 *  Plugin-backed vtable — delegates to plugin_interface_t
 * ================================================================ */

static bool plugin_open(memory_storage_t *st, const char *uri) {
    (void)st; (void)uri;
    return true; /* plugin already initialized */
}

static void plugin_close(memory_storage_t *st) {
    if (st && st->plugin_iface && st->plugin_iface->memory_clear)
        st->plugin_iface->memory_clear();
}

__attribute__((unused)) static bool plugin_store(memory_storage_t *st, memory_entry_t *entry) {
    if (!st || !st->plugin_iface || !st->plugin_iface->memory_store || !entry)
        return false;
    char *result = st->plugin_iface->memory_store(
        entry->content,
        "{\"key\":\"%s\"}" /* minimal metadata with key */);
    bool ok = (result && strstr(result, "\"status\":\"ok\"") != NULL);
    free(result);
    return ok;
}

__attribute__((unused)) static bool plugin_get(memory_storage_t *st, const char *key, memory_entry_t *entry) {
    (void)st; (void)key; (void)entry;
    return false; /* plugin search is query-based, not key-based */
}

__attribute__((unused)) static bool plugin_delete(memory_storage_t *st, const char *key) {
    if (!st || !st->plugin_iface || !st->plugin_iface->memory_store || !key)
        return false;
    /* Delete by setting empty content with matching key metadata */
    char metadata[256];
    snprintf(metadata, sizeof(metadata),
             "{\"key\":\"%s\",\"operation\":\"delete\"}", key);
    char *result = st->plugin_iface->memory_store("", metadata);
    bool ok = (result != NULL);
    free(result);
    return ok;
}

__attribute__((unused)) static void plugin_clear(memory_storage_t *st) {
    if (st && st->plugin_iface && st->plugin_iface->memory_clear)
        st->plugin_iface->memory_clear();
}

__attribute__((unused)) static size_t plugin_count(memory_storage_t *st) {
    (void)st;
    return 0; /* plugin doesn't expose count in standard interface */
}

__attribute__((unused)) static char **plugin_list_keys(memory_storage_t *st, size_t *count) {
    (void)st;
    if (count) *count = 0;
    return NULL;
}

/* Plugin search via plugin's memory_search */
__attribute__((unused)) static memory_entry_t *plugin_search(memory_storage_t *st, const char *query, int limit) {
    if (!st || !st->plugin_iface || !st->plugin_iface->memory_search)
        return NULL;
    char *result_json = st->plugin_iface->memory_search(query, limit);
    if (!result_json) return NULL;

    /* Parse the JSON result to extract entries */
    /* Simple approach: return a single entry with the raw JSON */
    memory_entry_t *entries = (memory_entry_t *)calloc(1, sizeof(memory_entry_t));
    if (entries) {
        entries->key[0] = '\0';
        snprintf(entries->content, sizeof(entries->content), "%s", result_json);
        entries->created_at = time(NULL);
        entries->access_count = 5;
    }
    free(result_json);
    return entries;
}

static int plugin_import_json(memory_storage_t *st, const json_t *entries) {
    if (!st || !entries || entries->type != JSON_ARRAY) return 0;
    if (!st->plugin_iface || !st->plugin_iface->memory_store) return 0;

    size_t n = json_len(entries);
    int imported = 0;

    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(entries, i);
        if (!item || item->type != JSON_OBJECT) continue;

        memory_entry_t entry;
        if (memory_entry_from_json(&entry, item)) {
            if (entry.created_at == 0) entry.created_at = time(NULL);
            if (entry.updated_at == 0) entry.updated_at = entry.created_at;
            if (!entry.key[0]) {
                snprintf(entry.key, sizeof(entry.key), "entry_imported_%zu", (size_t)imported);
            }

            /* Store via plugin interface */
            char metadata[128];
            snprintf(metadata, sizeof(metadata),
                     "{\"key\":\"%s\",\"created_at\":%ld,\"updated_at\":%ld}",
                     entry.key, (long)entry.created_at, (long)entry.updated_at);
            char *result = st->plugin_iface->memory_store(entry.content, metadata);
            if (result) {
                imported++;
                free(result);
            }
        }
    }
    return imported;
}

static json_t *plugin_export_json(memory_storage_t *st) {
    (void)st;
    /* Plugin backend doesn't support full enumeration.
     * Return empty array — callers can use search for specific queries. */
    return json_new_array();
}

static bool plugin_vtable_persist(memory_storage_t *st) {
    (void)st; return true; /* plugin manages its own state */
}

static bool plugin_vtable_load(memory_storage_t *st) {
    (void)st; return true;
}

static bool plugin_get_by_hash(memory_storage_t *st, uint64_t hash, memory_entry_t *entry) {
    (void)st; (void)hash; (void)entry;
    /* Plugin backend doesn't support hash-based dedup lookup.
     * Return false = "not found" — dedup best-effort for plugin. */
    return false;
}

static int plugin_compress_old(memory_storage_t *st, time_t before, memory_compress_fn_t compress_cb) {
    (void)st; (void)before; (void)compress_cb;
    /* Plugin manages its own storage — no local compression needed. */
    return 0;
}

static memory_entry_t *plugin_get_prioritized(memory_storage_t *st, size_t limit, size_t *count) {
    (void)st; (void)limit;
    if (count) *count = 0;
    /* Plugin backend doesn't support priority enumeration. */
    return NULL;
}

static memory_storage_vtable_t plugin_vtable = {
    .name        = "plugin",
    .open        = plugin_open,
    .close       = plugin_close,
    .import_json = plugin_import_json,
    .export_json = plugin_export_json,
    .get_by_hash = plugin_get_by_hash,
    .persist     = plugin_vtable_persist,
    .load        = plugin_vtable_load,
    .compress_old = plugin_compress_old,
    .get_prioritized  = plugin_get_prioritized,
};

bool memory_storage_plugin_init(memory_storage_t *st, void *plugin_reg, const char *plugin_name_str) {
    if (!st) return false;
    memset(st, 0, sizeof(*st));
    st->type = MEMORY_STORAGE_PLUGIN;

    /* Search plugin registry for a PLUGIN_MEMORY plugin */
    if (plugin_reg) {
        plugin_registry_t *reg = (plugin_registry_t *)plugin_reg;
        const char *target_name = plugin_name_str && plugin_name_str[0] ? plugin_name_str : "in-memory-store";

        /* Find the plugin by name */
        plugin_t *plug = plugin_registry_find(reg, target_name);
        if (!plug) {
            /* Try loading from source tree if not found in registry */
            char plugin_path[512];
            const char *home = getenv("HOME");
            if (!home) home = "/tmp";
            snprintf(plugin_path, sizeof(plugin_path),
                     "%s/hermes-agent-dev/C/src/plugins/plugin_honcho.so", home);
            plug = plugin_load(plugin_path);
            if (plug) {
                plugin_registry_add(reg, plug);
                /* Initialize */
                typedef int (*init_fn_t)(void);
                init_fn_t init_fn = (init_fn_t)plugin_symbol(plug, "plugin_init");
                if (init_fn) init_fn();
            } else {
                fprintf(stderr, "[memory] plugin_load failed: %s\n",
                        plugin_error());
            }
        }

        if (plug && plugin_type(plug) == PLUGIN_MEMORY) {
            void *(*get_iface)(void) = (void *(*)(void))plugin_symbol(plug, "plugin_get_interface");
            if (get_iface) {
                plugin_interface_t *iface = (plugin_interface_t *)get_iface();
                if (iface && iface->memory_store && iface->memory_search) {
                    st->plugin_plug = plug;
                    st->plugin_iface = iface;
                    st->vtable = plugin_vtable;
                    fprintf(stderr, "[memory] using plugin: %s\n", plugin_name(plug));
                    return true;
                }
            }
        }
    }

    /* Fallback to in-memory if plugin not available */
    fprintf(stderr, "[memory] plugin not found, falling back to in-memory\n");
    return memory_storage_inmem_init(st);
}

/* ================================================================
 *  Memory manager implementation
 * ================================================================ */

/* Default memory path for file storage */
static void get_default_memory_path(char *buf, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sz, "%s/.hermes/memory.json", home);
}

bool memory_init(memory_t *mem, memory_storage_type_t type, const char *uri) {
    if (!mem) return false;
    memset(mem, 0, sizeof(*mem));

    pthread_mutex_init(&mem->lock, NULL);

    mem->autosave = MEMORY_AUTOSAVE_OFF;
    mem->autosave_interval_sec = 60;
    mem->ttl_days = 30;
    mem->dedup_enabled = true;
    mem->search_limit = 20;
    mem->compression.enabled = false;
    mem->compression.min_entries = 50;
    mem->compression.max_batch = 10;
    mem->compression.older_than_days = 30;

    mem->ttl_running = false;
    mem->autosave_running = false;

    /* Initialize storage backend */
    switch (type) {
        case MEMORY_STORAGE_INMEM:
            return memory_storage_inmem_init(&mem->storage);

        case MEMORY_STORAGE_FILE: {
            char path[512];
            if (uri && uri[0])
                snprintf(path, sizeof(path), "%s", uri);
            else
                get_default_memory_path(path, sizeof(path));
            return memory_storage_file_init(&mem->storage, path);
        }

        case MEMORY_STORAGE_SQLITE: {
            char path[512];
            if (uri && uri[0])
                snprintf(path, sizeof(path), "%s", uri);
            else
                snprintf(path, sizeof(path), "memory.db");
            return memory_storage_sqlite_init(&mem->storage, path);
        }

        case MEMORY_STORAGE_PLUGIN:
            return memory_storage_plugin_init(&mem->storage, NULL, NULL);

        default:
            return false;
    }
}

bool memory_init_from_config(memory_t *mem, const memory_config_t *cfg) {
    if (!mem || !cfg) return false;
    if (!memory_init(mem, (memory_storage_type_t)cfg->storage_type, cfg->storage_path))
        return false;

    mem->ttl_days = cfg->ttl_days > 0 ? cfg->ttl_days : 30;
    mem->dedup_enabled = cfg->dedup_enabled;
    mem->search_limit = cfg->search_limit > 0 ? cfg->search_limit : 20;
    mem->compression.enabled = cfg->compression_enabled;
    mem->compression.min_entries = cfg->compression_min_entries > 0 ? cfg->compression_min_entries : 50;
    mem->compression.max_batch = 10;
    mem->compression.older_than_days = cfg->ttl_days > 0 ? (time_t)cfg->ttl_days : 30;

    if (cfg->auto_save) {
        int interval = cfg->auto_save_interval > 0 ? cfg->auto_save_interval : 60;
        memory_autosave_start_thread(mem, interval);
    }

    if (cfg->ttl_days > 0) {
        memory_ttl_start_thread(mem, 300); /* Check TTL every 5 minutes */
    }

    mem->config = cfg;
    return true;
}

void memory_cleanup(memory_t *mem) {
    if (!mem) return;

    /* Stop threads */
    memory_ttl_stop_thread(mem);
    memory_autosave_stop_thread(mem);

    pthread_mutex_lock(&mem->lock);

    /* Persist if dirty */
    mem->storage.vtable.persist(&mem->storage);

    /* Close storage */
    mem->storage.vtable.close(&mem->storage);

    pthread_mutex_unlock(&mem->lock);
    pthread_mutex_destroy(&mem->lock);
}

bool memory_store(memory_t *mem, memory_entry_t *entry) {
    if (!mem || !entry) return false;

    pthread_mutex_lock(&mem->lock);

    /* Set timestamps */
    if (entry->created_at == 0) entry->created_at = time(NULL);
    entry->updated_at = time(NULL);

    /* Compute hash for dedup */
    if (entry->hash == 0) {
        entry->hash = memory_hash_content(entry->content);
    }

    /* Dedup check (P154) */
    if (mem->dedup_enabled && entry->hash != 0) {
        memory_entry_t existing;
        if (mem->storage.vtable.get_by_hash(&mem->storage, entry->hash, &existing)) {
            /* Duplicate found — update existing entry's access count and content */
            existing.updated_at = time(NULL);
            existing.access_count++;
            if (entry->priority > 0) existing.priority = entry->priority;
            /* Update content if different */
            if (strcmp(existing.content, entry->content) != 0) {
                snprintf(existing.content, sizeof(existing.content), "%s", entry->content);
            }
            /* Merge tags */
            for (int t = 0; t < entry->tag_count && existing.tag_count < MEMORY_TAGS_MAX; t++) {
                bool found = false;
                for (int et = 0; et < existing.tag_count; et++) {
                    if (strcmp(existing.tags[et], entry->tags[t]) == 0) { found = true; break; }
                }
                if (!found) {
                    snprintf(existing.tags[existing.tag_count], MEMORY_TAG_MAX, "%s", entry->tags[t]);
                    existing.tag_count++;
                }
            }

            bool ok = mem->storage.vtable.store(&mem->storage, &existing);
            pthread_mutex_unlock(&mem->lock);
            return ok;
        }
    }

    bool ok = mem->storage.vtable.store(&mem->storage, entry);
    pthread_mutex_unlock(&mem->lock);
    return ok;
}

bool memory_store_simple(memory_t *mem, const char *content, int priority, int ttl_seconds) {
    if (!mem || !content) return false;

    memory_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.content, sizeof(entry.content), "%s", content);
    entry.priority = priority;
    entry.created_at = time(NULL);
    entry.updated_at = entry.created_at;

    if (ttl_seconds > 0) {
        entry.expires_at = entry.created_at + ttl_seconds;
    }

    entry.hash = memory_hash_content(content);
    entry.access_count = 1;
    entry.last_accessed = entry.created_at;

    return memory_store(mem, &entry);
}

bool memory_get(memory_t *mem, const char *key, memory_entry_t *entry) {
    if (!mem || !key || !entry) return false;

    pthread_mutex_lock(&mem->lock);
    bool found = mem->storage.vtable.get(&mem->storage, key, entry);
    if (found) {
        if (memory_entry_expired(entry)) {
            /* Auto-delete expired entries on access */
            mem->storage.vtable.delete(&mem->storage, key);
            pthread_mutex_unlock(&mem->lock);
            return false;
        }
        entry->access_count++;
        entry->last_accessed = time(NULL);
    }
    pthread_mutex_unlock(&mem->lock);
    return found;
}

bool memory_delete(memory_t *mem, const char *key) {
    if (!mem || !key) return false;
    pthread_mutex_lock(&mem->lock);
    bool ok = mem->storage.vtable.delete(&mem->storage, key);
    pthread_mutex_unlock(&mem->lock);
    return ok;
}

void memory_clear(memory_t *mem) {
    if (!mem) return;
    pthread_mutex_lock(&mem->lock);
    mem->storage.vtable.clear(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
}

size_t memory_count(memory_t *mem) {
    if (!mem) return 0;
    pthread_mutex_lock(&mem->lock);
    size_t c = mem->storage.vtable.count(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
    return c;
}

char **memory_list_keys(memory_t *mem, size_t *count) {
    if (!mem || !count) { if (count) *count = 0; return NULL; }
    pthread_mutex_lock(&mem->lock);
    char **keys = mem->storage.vtable.list_keys(&mem->storage, count);
    pthread_mutex_unlock(&mem->lock);
    return keys;
}

memory_entry_t *memory_search(memory_t *mem, const char *query, int limit) {
    if (!mem || !query) return NULL;

    pthread_mutex_lock(&mem->lock);

    if (limit <= 0) limit = mem->search_limit;

    memory_entry_t *results;

    /* Use inline search for inmem backend; file backend via vtable */
    if (mem->storage.type == MEMORY_STORAGE_INMEM) {
        size_t count = 0;
        results = inmem_search_internal(&mem->storage, query, limit, &count);
        /* Re-wrap: inmem_search_internal already returns count-limited */
        (void)count;
    } else {
        results = mem->storage.vtable.search(&mem->storage, query, limit);
    }

    pthread_mutex_unlock(&mem->lock);
    return results;
}

memory_entry_t *memory_get_prioritized(memory_t *mem, size_t limit, size_t *count) {
    if (!mem || !count) { if (count) *count = 0; return NULL; }
    pthread_mutex_lock(&mem->lock);
    memory_entry_t *results = mem->storage.vtable.get_prioritized(&mem->storage, limit, count);
    pthread_mutex_unlock(&mem->lock);
    return results;
}

size_t memory_ttl_cleanup(memory_t *mem) {
    if (!mem) return 0;

    pthread_mutex_lock(&mem->lock);

    size_t removed = 0;
    size_t nkeys = 0;
    char **keys = mem->storage.vtable.list_keys(&mem->storage, &nkeys);

    if (keys) {
        for (size_t i = 0; i < nkeys; i++) {
            memory_entry_t entry;
            if (mem->storage.vtable.get(&mem->storage, keys[i], &entry)) {
                if (memory_entry_expired(&entry)) {
                    mem->storage.vtable.delete(&mem->storage, keys[i]);
                    removed++;
                }
            }
            free(keys[i]);
        }
        free(keys);
    }

    /* Persist after cleanup */
    mem->storage.vtable.persist(&mem->storage);

    pthread_mutex_unlock(&mem->lock);
    return removed;
}

/* TTL cleanup thread */
static void *ttl_thread_fn(void *arg) {
    memory_t *mem = (memory_t *)arg;
    while (mem->ttl_running) {
        sleep(300); /* Check every 5 minutes */
        if (!mem->ttl_running) break;
        memory_ttl_cleanup(mem);
    }
    return NULL;
}

bool memory_ttl_start_thread(memory_t *mem, int interval_sec) {
    if (!mem || mem->ttl_running) return false;
    (void)interval_sec;
    mem->ttl_running = true;
    if (pthread_create(&mem->ttl_thread, NULL, ttl_thread_fn, mem) != 0) {
        mem->ttl_running = false;
        return false;
    }
    pthread_detach(mem->ttl_thread);
    return true;
}

void memory_ttl_stop_thread(memory_t *mem) {
    if (!mem || !mem->ttl_running) return;
    mem->ttl_running = false;
    /* Thread is detached, it will exit on next sleep wake */
}

/* Auto-save thread */
static void *autosave_thread_fn(void *arg) {
    memory_t *mem = (memory_t *)arg;
    while (mem->autosave_running) {
        sleep((unsigned int)mem->autosave_interval_sec);
        if (!mem->autosave_running) break;
        memory_persist(mem);
    }
    return NULL;
}

bool memory_autosave_start_thread(memory_t *mem, int interval_sec) {
    if (!mem || mem->autosave_running) return false;
    mem->autosave_running = true;
    mem->autosave = MEMORY_AUTOSAVE_ON;
    if (interval_sec > 0) mem->autosave_interval_sec = interval_sec;
    if (pthread_create(&mem->autosave_thread, NULL, autosave_thread_fn, mem) != 0) {
        mem->autosave_running = false;
        mem->autosave = MEMORY_AUTOSAVE_OFF;
        return false;
    }
    pthread_detach(mem->autosave_thread);
    return true;
}

void memory_autosave_stop_thread(memory_t *mem) {
    if (!mem || !mem->autosave_running) return;
    mem->autosave_running = false;
    mem->autosave = MEMORY_AUTOSAVE_OFF;
}

bool memory_persist(memory_t *mem) {
    if (!mem) return false;
    pthread_mutex_lock(&mem->lock);
    bool ok = mem->storage.vtable.persist(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
    return ok;
}

bool memory_load(memory_t *mem) {
    if (!mem) return false;
    pthread_mutex_lock(&mem->lock);
    bool ok = mem->storage.vtable.load(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
    return ok;
}

/* ================================================================
 *  Import / Export (P157)
 * ================================================================ */

int memory_import_file(memory_t *mem, const char *path, bool merge) {
    if (!mem || !path) return 0;

    char *err = NULL;
    json_t *doc = json_parse_file(path, &err);
    if (err) { free(err); return 0; }
    if (!doc) return 0;

    int imported = memory_import_json(mem, NULL, merge);
    /* Actually use the parsed doc */
    if (doc->type == JSON_ARRAY) {
        pthread_mutex_lock(&mem->lock);
        imported = mem->storage.vtable.import_json(&mem->storage, doc);
        mem->storage.vtable.persist(&mem->storage);
        pthread_mutex_unlock(&mem->lock);
    } else if (doc->type == JSON_OBJECT) {
        /* Support old format: {key: {content, ...}} */
        /* Convert to array by extracting values */
        size_t n = json_len(doc);
        json_t *arr = json_new_array();
        for (size_t i = 0; i < n; i++) {
            json_t *val = json_get(doc, i);
            if (val) json_append(arr, json_copy(val));
        }

        pthread_mutex_lock(&mem->lock);
        imported = mem->storage.vtable.import_json(&mem->storage, arr);
        mem->storage.vtable.persist(&mem->storage);
        pthread_mutex_unlock(&mem->lock);
        json_free(arr);
    }

    json_free(doc);
    return imported;
}

int memory_import_json(memory_t *mem, const char *json_str, bool merge) {
    if (!mem || !json_str) return 0;

    char *err = NULL;
    json_t *doc = json_parse(json_str, &err);
    if (err) { free(err); return 0; }
    if (!doc) return 0;

    int imported = 0;
    if (doc->type == JSON_ARRAY) {
        if (!merge) memory_clear(mem);
        pthread_mutex_lock(&mem->lock);
        imported = mem->storage.vtable.import_json(&mem->storage, doc);
        mem->storage.vtable.persist(&mem->storage);
        pthread_mutex_unlock(&mem->lock);
    }

    json_free(doc);
    return imported;
}

bool memory_export_file(memory_t *mem, const char *path) {
    if (!mem || !path) return false;

    char *json_str = memory_export_json(mem);
    if (!json_str) return false;

    FILE *f = fopen(path, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return true;
}

char *memory_export_json(memory_t *mem) {
    if (!mem) return NULL;

    pthread_mutex_lock(&mem->lock);
    json_t *arr = mem->storage.vtable.export_json(&mem->storage);
    pthread_mutex_unlock(&mem->lock);

    if (!arr) return strdup("[]");

    char *json_str = json_serialize_pretty(arr, 2);
    json_free(arr);
    return json_str;
}

/* ================================================================
 *  Compression (P158)
 * ================================================================ */

int memory_compress_old(memory_t *mem, time_t before,
                         char *(*compress_cb)(const char *content)) {
    if (!mem || !compress_cb) return 0;

    pthread_mutex_lock(&mem->lock);
    int result = mem->storage.vtable.compress_old(&mem->storage, before, compress_cb);
    mem->storage.vtable.persist(&mem->storage);
    pthread_mutex_unlock(&mem->lock);
    return result;
}

/* ================================================================
 *  Tool handler — backward-compatible memory tool (P151-P158)
 * ================================================================ */

/* Static memory instance for the tool handler */
static memory_t g_memory;
static bool g_memory_initialized = false;
static void *g_plugin_registry = NULL;

/* Set plugin registry for plugin-backed memory */
void memory_set_plugin_registry(void *reg) {
    g_plugin_registry = reg;
}

/* Ensure global memory is initialized */
static memory_t *get_global_memory(void) {
    if (!g_memory_initialized) {
        char path[512];
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) home = "/tmp";

        /* Try plugin-backed memory if registry available */
        if (g_plugin_registry) {
            memory_init(&g_memory, MEMORY_STORAGE_PLUGIN, NULL);
        } else {
            snprintf(path, sizeof(path), "%s/.hermes/memory.json", home);
            memory_init(&g_memory, MEMORY_STORAGE_FILE, path);
        }
        g_memory.dedup_enabled = true;
        g_memory.search_limit = 20;
        g_memory.ttl_days = 30;
        g_memory.autosave = MEMORY_AUTOSAVE_ON;
        g_memory.autosave_interval_sec = 60;
        memory_autosave_start_thread(&g_memory, 60);
        g_memory_initialized = true;
    }
    return &g_memory;
}

char *memory_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse error\"}"); }

    const char *action = json_object_get_string(args, "action", "list");

    memory_t *mem = get_global_memory();
    json_node_t *result = json_new_object();

    if (strcmp(action, "add") == 0) {
        const char *content = json_object_get_string(args, "content", "");
        int priority = (int)json_object_get_number(args, "priority", 0);
        int ttl = (int)json_object_get_number(args, "ttl", 0);
        const char *key = json_object_get_string(args, "key", "");

        memory_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        if (key && key[0]) snprintf(entry.key, sizeof(entry.key), "%s", key);
        snprintf(entry.content, sizeof(entry.content), "%s", content ? content : "");
        entry.priority = priority;
        entry.created_at = time(NULL);
        entry.updated_at = entry.created_at;
        if (ttl > 0) entry.expires_at = entry.created_at + ttl;
        entry.hash = memory_hash_content(entry.content);

        /* Check tags */
        const json_t *tags_arr = json_obj_get(args, "tags");
        if (tags_arr && tags_arr->type == JSON_ARRAY) {
            size_t n = json_len(tags_arr);
            for (size_t i = 0; i < n && entry.tag_count < MEMORY_TAGS_MAX; i++) {
                json_t *tag = json_get(tags_arr, i);
                if (tag && tag->type == JSON_STRING) {
                    snprintf(entry.tags[entry.tag_count], MEMORY_TAG_MAX, "%s", tag->str_val);
                    entry.tag_count++;
                }
            }
        }

        if (memory_store(mem, &entry)) {
            json_object_set(result, "status", json_new_string("saved"));
            json_object_set(result, "key", json_new_string(entry.key));
            json_object_set(result, "priority", json_new_number((double)entry.priority));
        } else {
            json_object_set(result, "error", json_new_string("save failed"));
        }

    } else if (strcmp(action, "get") == 0) {
        const char *key = json_object_get_string(args, "key", "");
        if (!key[0]) {
            json_object_set(result, "error", json_new_string("key required"));
        } else {
            memory_entry_t entry;
            if (memory_get(mem, key, &entry)) {
                json_object_set(result, "found", json_bool(true));
                json_object_set(result, "entry", memory_entry_to_json(&entry));
            } else {
                json_object_set(result, "found", json_bool(false));
            }
        }

    } else if (strcmp(action, "delete") == 0) {
        const char *key = json_object_get_string(args, "key", "");
        if (memory_delete(mem, key)) {
            json_object_set(result, "status", json_new_string("deleted"));
        } else {
            json_object_set(result, "status", json_new_string("not_found"));
        }

    } else if (strcmp(action, "clear") == 0) {
        memory_clear(mem);
        json_object_set(result, "status", json_new_string("cleared"));

    } else if (strcmp(action, "search") == 0) {
        const char *query = json_object_get_string(args, "query", "");
        int limit = (int)json_object_get_number(args, "limit", 10);

        memory_entry_t *entries = memory_search(mem, query, limit);
        /* We need to iterate the returned entries — but the vtable search
         * returns up to `limit` entries as an array. Since we don't know
         * the count from the vtable, we use a simpler approach: list keys
         * and check manually. */
        size_t nkeys = 0;
        char **keys = memory_list_keys(mem, &nkeys);

        json_t *results_arr = json_new_array();
        int count = 0;
        for (size_t i = 0; i < nkeys && count < limit; i++) {
            memory_entry_t e;
            if (memory_get(mem, keys[i], &e)) {
                /* Simple substring search */
                if (strstr(e.content, query) || strstr(e.key, query)) {
                    json_append(results_arr, memory_entry_to_json(&e));
                    count++;
                }
            }
            free(keys[i]);
        }
        free(keys);

        json_object_set(result, "results", results_arr);
        json_object_set(result, "count", json_number((double)count));

        free(entries); /* may be NULL, safe */

    } else if (strcmp(action, "prioritized") == 0) {
        int limit = (int)json_object_get_number(args, "limit", 10);
        size_t count = 0;
        memory_entry_t *entries = memory_get_prioritized(mem, (size_t)limit, &count);

        json_t *arr = json_new_array();
        for (size_t i = 0; i < count; i++) {
            json_append(arr, memory_entry_to_json(&entries[i]));
        }
        json_object_set(result, "entries", arr);
        json_object_set(result, "count", json_number((double)count));
        free(entries);

    } else if (strcmp(action, "ttl_cleanup") == 0) {
        size_t removed = memory_ttl_cleanup(mem);
        json_object_set(result, "removed", json_number((double)removed));

    } else if (strcmp(action, "export") == 0) {
        char *json_out = memory_export_json(mem);
        if (json_out) {
            /* Parse back to JSON to embed */
            char *e = NULL;
            json_t *j = json_parse(json_out, &e);
            if (e) free(e);
            if (j) {
                json_object_set(result, "entries", j);
            }
            free(json_out);
        } else {
            json_object_set(result, "entries", json_new_array());
        }
        json_object_set(result, "count", json_number((double)memory_count(mem)));

    } else if (strcmp(action, "import") == 0) {
        const char *json_str = json_object_get_string(args, "data", "");
        bool merge = json_get_str(args, "merge", "true") || true;
        (void)merge;
        /* Support JSON string data or file path */
        const char *file = json_object_get_string(args, "file", "");
        int imported = 0;
        if (file[0]) {
            imported = memory_import_file(mem, file, true);
        } else if (json_str[0]) {
            imported = memory_import_json(mem, json_str, true);
        }
        json_object_set(result, "imported", json_number((double)imported));

    } else if (strcmp(action, "stats") == 0) {
        size_t count = memory_count(mem);
        json_object_set(result, "count", json_number((double)count));
        json_object_set(result, "storage", json_string(
            mem->storage.type == MEMORY_STORAGE_INMEM ? "inmem" :
            mem->storage.type == MEMORY_STORAGE_FILE ? "file" :
            mem->storage.type == MEMORY_STORAGE_SQLITE ? "sqlite" :
            "plugin"));
        json_object_set(result, "dedup", json_bool(mem->dedup_enabled));
        json_object_set(result, "ttl_days", json_number((double)mem->ttl_days));
        json_object_set(result, "autosave", json_bool(mem->autosave == MEMORY_AUTOSAVE_ON));
        json_object_set(result, "compression", json_bool(mem->compression.enabled));

    } else {
        /* Default: list all entries */
        size_t nkeys = 0;
        char **keys = memory_list_keys(mem, &nkeys);

        json_t *entries_arr = json_new_array();
        for (size_t i = 0; i < nkeys; i++) {
            memory_entry_t e;
            if (memory_get(mem, keys[i], &e)) {
                json_append(entries_arr, memory_entry_to_json(&e));
            }
            free(keys[i]);
        }
        free(keys);

        json_object_set(result, "count", json_number((double)nkeys));
        json_object_set(result, "entries", entries_arr);
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_memory(void) {
    registry_register("memory",
        "Persistent memory system with storage backends. "
        "Actions: add, get, delete, search, clear, prioritized, "
        "ttl_cleanup, export, import, stats, list. "
        "Supports TTL, dedup, priority ordering, auto-save, JSON import/export, "
        "and compression. Data persists across sessions.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"add | get | delete | search | clear | prioritized | ttl_cleanup | export | import | stats | list\",\"default\":\"list\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"Content to save (required for add)\"},"
          "\"key\":{\"type\":\"string\",\"description\":\"Entry key (for get/delete)\"},"
          "\"query\":{\"type\":\"string\",\"description\":\"Search query (for search)\"},"
          "\"limit\":{\"type\":\"integer\",\"description\":\"Max results\",\"default\":10},"
          "\"priority\":{\"type\":\"integer\",\"description\":\"Priority 0-100 (for add)\",\"default\":0},"
          "\"ttl\":{\"type\":\"integer\",\"description\":\"TTL in seconds (for add, 0=never)\",\"default\":0},"
          "\"tags\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Tags (for add)\"},"
          "\"data\":{\"type\":\"string\",\"description\":\"JSON data to import (for import)\"},"
          "\"file\":{\"type\":\"string\",\"description\":\"File path to import from (for import)\"},"
          "\"merge\":{\"type\":\"boolean\",\"description\":\"Merge with existing (for import)\",\"default\":true}"
        "},"
        "\"required\":[]"
        "}", memory_handler);
}
