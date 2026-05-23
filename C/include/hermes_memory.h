/**
 * @defgroup hermes_memory Memory
 * @brief In-memory and plugin-backed memory store.
 *
 *
Interface for memory_store/search/clear operations.
Backed by the honcho plugin (in-memory array store).
 *
 * @{
 */
#ifndef HERMES_MEMORY_H
#define HERMES_MEMORY_H

/*
 * hermes_memory.h — Memory system depth for Hermes C.
 * P151-P158: Storage abstraction, TTL, prioritization, dedup, search,
 *            auto-save, import/export, compression.
 *
 * Provides a unified memory abstraction supporting multiple backends:
 *   - In-memory (volatile, fast)
 *   - File-based (JSON file, backward-compatible with existing memory.json)
 *   - SQLite (persistent, indexed)
 *   - Plugin (delegates to a PLUGIN_MEMORY plugin via plugin_interface_t)
 *
 * MIT License — WuBu Hermes Project
 */

#include "hermes.h"
#include "hermes_json.h"
#include "plugin.h"
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Memory Entry
 * ================================================================ */

#define MEMORY_KEY_MAX      256
#define MEMORY_CONTENT_MAX  65536
#define MEMORY_TAG_MAX      64
#define MEMORY_TAGS_MAX     16

typedef struct {
    char     key[MEMORY_KEY_MAX];
    char     content[MEMORY_CONTENT_MAX];
    time_t   created_at;
    time_t   updated_at;
    time_t   expires_at;       /* 0 = never expires (TTL) */
    int      priority;          /* 0-100, higher = more important */
    uint64_t hash;              /* content hash for dedup */
    char     tags[MEMORY_TAGS_MAX][MEMORY_TAG_MAX];
    int      tag_count;
    bool     compressed;        /* true if content was LLM-summarized */
    int      access_count;      /* how many times accessed */
    time_t   last_accessed;     /* LRU tracking */
} memory_entry_t;

/* ================================================================
 *  Storage Backend Types (P151)
 * ================================================================ */

typedef enum {
    MEMORY_STORAGE_INMEM   = 0,   /* In-memory hash map */
    MEMORY_STORAGE_FILE    = 1,   /* JSON file (backward compat) */
    MEMORY_STORAGE_SQLITE  = 2,   /* SQLite database */
    MEMORY_STORAGE_PLUGIN  = 3,   /* Plugin delegate */
} memory_storage_type_t;

/* Forward declarations */
typedef struct memory_storage_t memory_storage_t;
typedef struct memory_t memory_t;

/* Compression callback: takes content, returns malloc'd compressed string */
typedef char *(*memory_compress_fn_t)(const char *content);

/* Storage backend vtable — implement this for custom backends */
typedef struct {
    /* Name of this backend */
    const char *name;

    /* Open/initialize the backend. Returns true on success. */
    bool (*open)(memory_storage_t *st, const char *uri);

    /* Close/shutdown the backend */
    void (*close)(memory_storage_t *st);

    /* Store an entry. If key exists in entry, update by key.
     * If key is empty, auto-generate. Returns true on success. */
    bool (*store)(memory_storage_t *st, memory_entry_t *entry);

    /* Get entry by key. Returns true if found. */
    bool (*get)(memory_storage_t *st, const char *key, memory_entry_t *entry);

    /* Delete entry by key. Returns true if existed. */
    bool (*delete)(memory_storage_t *st, const char *key);

    /* Clear all entries */
    void (*clear)(memory_storage_t *st);

    /* Count entries */
    size_t (*count)(memory_storage_t *st);

    /* List all keys. Keys is malloc'd array of strings, *count receives count.
     * Caller must free each key string and the array. */
    char **(*list_keys)(memory_storage_t *st, size_t *count);

    /* Search by keyword in content. Returns malloc'd entries array.
     * *count receives result count. Caller must free each entry and the array. */
    memory_entry_t *(*search)(memory_storage_t *st, const char *query, int limit);

    /* Import entries from a JSON array of entry objects.
     * Returns number of entries imported. */
    int (*import_json)(memory_storage_t *st, const json_t *entries);

    /* Export all entries as a JSON array of entry objects.
     * Returns a new json_t, caller must free. */
    json_t *(*export_json)(memory_storage_t *st);

    /* Get entry by hash (for dedup checking). Returns true if found. */
    bool (*get_by_hash)(memory_storage_t *st, uint64_t hash, memory_entry_t *entry);

    /* Save to persistent storage (no-op for in-memory) */
    bool (*persist)(memory_storage_t *st);

    /* Load from persistent storage (no-op for in-memory) */
    bool (*load)(memory_storage_t *st);

    /* Compact/compress old entries via LLM summarization callback. */
    int (*compress_old)(memory_storage_t *st, time_t before, memory_compress_fn_t compress_cb);

    /* Get entries ordered by priority (highest first). */
    memory_entry_t *(*get_prioritized)(memory_storage_t *st, size_t limit, size_t *count);
} memory_storage_vtable_t;

/* Storage backend instance */
struct memory_storage_t {
    memory_storage_type_t   type;
    memory_storage_vtable_t vtable;
    void                   *data;     /* backend-specific data */
    char                    uri[512]; /* connection URI / file path */
    plugin_t               *plugin_plug;   /* loaded plugin handle (plugin backend) */
    plugin_interface_t     *plugin_iface;  /* cached interface pointer */
};

/* ================================================================
 *  Memory Manager
 * ================================================================ */

/* Auto-save state */
typedef enum {
    MEMORY_AUTOSAVE_OFF  = 0,
    MEMORY_AUTOSAVE_ON   = 1,
} memory_autosave_t;

/* Compression config for P158 */
typedef struct {
    bool    enabled;
    int     min_entries;       /* minimum entries before compression triggers */
    int     max_batch;         /* max entries to compress in one pass */
    time_t  older_than_days;   /* only compress entries older than N days */
} memory_compress_config_t;

/* The main memory manager */
struct memory_t {
    memory_storage_t     storage;
    memory_autosave_t    autosave;
    int                  autosave_interval_sec; /* default 60 */
    int                  ttl_days;              /* default 30 */
    bool                 dedup_enabled;
    int                  search_limit;          /* max search results */

    /* TTL cleanup thread */
    pthread_t            ttl_thread;
    volatile bool        ttl_running;

    /* Auto-save thread */
    pthread_t            autosave_thread;
    volatile bool        autosave_running;

    /* Compression config */
    memory_compress_config_t compression;

    /* Mutex for thread safety */
    pthread_mutex_t      lock;

    /* Config reference (optional, for access from handler) */
    const memory_config_t *config;
};

/* ================================================================
 *  API Functions
 * ================================================================ */

/* Initialize memory manager with storage backend.
 * type: storage backend type
 * uri: path/connection string (e.g. "/path/to/memory.json", "memory.db", or "" for inmem)
 * Returns true on success. */
bool memory_init(memory_t *mem, memory_storage_type_t type, const char *uri);

/* Initialize memory from config struct */
bool memory_init_from_config(memory_t *mem, const memory_config_t *cfg);

/* Shutdown memory manager — stops threads, closes storage, frees resources */
void memory_cleanup(memory_t *mem);

/* Store a memory entry.
 * If entry->key is empty, auto-generates one.
 * If dedup is enabled and duplicate hash found, updates existing entry.
 * Returns true on success. */
bool memory_store(memory_t *mem, memory_entry_t *entry);

/* Store with convenience parameters.
 * Auto-generates key, sets created_at/updated_at to now. */
bool memory_store_simple(memory_t *mem, const char *content, int priority, int ttl_seconds);

/* Get entry by key. Returns true if found and not expired. */
bool memory_get(memory_t *mem, const char *key, memory_entry_t *entry);

/* Delete entry by key */
bool memory_delete(memory_t *mem, const char *key);

/* Clear all entries */
void memory_clear(memory_t *mem);

/* Get total entry count */
size_t memory_count(memory_t *mem);

/* List all keys. Returns malloc'd array (caller must free each key + array). */
char **memory_list_keys(memory_t *mem, size_t *count);

/* Search memory entries by keyword in content (P155).
 * Returns malloc'd array of entries, *count receives result count.
 * Caller must free each entry and the array. */
memory_entry_t *memory_search(memory_t *mem, const char *query, int limit);

/* Priority-ordered retrieval (P153).
 * Returns top-N entries sorted by priority (highest first).
 * Caller must free each entry and the array. */
memory_entry_t *memory_get_prioritized(memory_t *mem, size_t limit, size_t *count);

/* Run TTL cleanup manually (P152).
 * Removes all expired entries. Returns number removed. */
size_t memory_ttl_cleanup(memory_t *mem);

/* Start TTL cleanup thread. Runs every 'interval_sec' seconds. */
bool memory_ttl_start_thread(memory_t *mem, int interval_sec);

/* Stop TTL cleanup thread */
void memory_ttl_stop_thread(memory_t *mem);

/* Start auto-save thread. Persists every 'interval_sec' seconds. */
bool memory_autosave_start_thread(memory_t *mem, int interval_sec);

/* Stop auto-save thread */
void memory_autosave_stop_thread(memory_t *mem);

/* Persist to storage */
bool memory_persist(memory_t *mem);

/* Load from storage */
bool memory_load(memory_t *mem);

/* Import entries from a JSON file (P157).
 * path: path to JSON file containing an array of entry objects.
 * merge: if true, merge with existing entries; if false, replace.
 * Returns number of entries imported. */
int memory_import_file(memory_t *mem, const char *path, bool merge);

/* Import entries from a JSON string.
 * Returns number of entries imported. */
int memory_import_json(memory_t *mem, const char *json_str, bool merge);

/* Export all entries to a JSON file (P157).
 * Returns true on success. */
bool memory_export_file(memory_t *mem, const char *path);

/* Export all entries as a JSON string. Caller must free. */
char *memory_export_json(memory_t *mem);

/* Compress old entries via LLM summarization (P158).
 * before: only compress entries older than this timestamp.
 * compress_cb: callback that takes content and returns compressed string.
 * Returns number of entries compressed. */
int memory_compress_old(memory_t *mem, time_t before,
                        memory_compress_fn_t compress_cb);

/* Compute 64-bit FNV-1a hash of a string (for dedup) */
uint64_t memory_hash_content(const char *content);

/* Check if an entry is expired */
bool memory_entry_expired(const memory_entry_t *entry);

/* Create a memory entry from JSON object (for import) */
bool memory_entry_from_json(memory_entry_t *entry, const json_t *obj);

/* Convert entry to JSON object. Returns new json_t, caller must free. */
json_t *memory_entry_to_json(const memory_entry_t *entry);

/* ================================================================
 *  Built-in Storage Backend Constructors
 * ================================================================ */

/* Initialize in-memory storage */
bool memory_storage_inmem_init(memory_storage_t *st);

/* Initialize file-based storage (JSON file) */
bool memory_storage_file_init(memory_storage_t *st, const char *path);

/* Initialize SQLite storage (if SQLite support compiled in) */
bool memory_storage_sqlite_init(memory_storage_t *st, const char *path);

/* Initialize plugin-delegate storage.
 * plugin_reg: the plugin registry to search for a PLUGIN_MEMORY plugin.
 * plugin_name: name of the memory plugin to use (or NULL for first found). */
bool memory_storage_plugin_init(memory_storage_t *st, void *plugin_reg, const char *plugin_name);

/* Set global plugin registry for plugin-backed memory */
void memory_set_plugin_registry(void *reg);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of hermes_memory group */
#endif /* HERMES_MEMORY_H */
