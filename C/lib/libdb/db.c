/*
 * db.c — File-based JSON session store for C.
 * Each session = one .json file on disk + .meta.json sidecar.
 * MIT License — WuBu Hermes Project
 */

#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

/* ================================================================
 *  Internal
 * ================================================================ */

struct db_t {
    char dir_path[4096];     /* sessions directory */
    size_t dir_len;
    bool dirty;              /* needs flush */
};

/* Build full file path for a session ID (sanitized) */
static void make_path(const db_t *db, const char *session_id,
                      char *out, size_t out_sz) {
    char safe[256];
    size_t j = 0;
    for (const char *p = session_id; *p && j < sizeof(safe) - 1; p++) {
        char c = *p;
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.')
            safe[j++] = c;
        else
            safe[j++] = '_';
    }
    safe[j] = '\0';
    snprintf(out, out_sz, "%s/%s.json", db->dir_path, safe);
}

/* Build metadata file path */
static void make_meta_path(const db_t *db, const char *session_id,
                           char *out, size_t out_sz) {
    char safe[256];
    size_t j = 0;
    for (const char *p = session_id; *p && j < sizeof(safe) - 1; p++) {
        char c = *p;
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.')
            safe[j++] = c;
        else
            safe[j++] = '_';
    }
    safe[j] = '\0';
    snprintf(out, out_sz, "%s/%s.meta.json", db->dir_path, safe);
}

/* Ensure directory exists */
static bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return true;
        return false;
    }
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

/* Read a file into a malloc'd string */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Extract JSON string value by key (simple parser, no deps) */
static char *json_extract_str(const char *json, const char *key) {
    if (!json || !key) return NULL;
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    /* Find closing quote */
    const char *end = p;
    while (*end && *end != '"') {
        if (*end == '\\' && *(end+1)) end++;
        end++;
    }
    if (!*end) return NULL;
    size_t len = (size_t)(end - p);
    char *val = (char *)malloc(len + 1);
    if (!val) return NULL;
    size_t j = 0;
    for (const char *s = p; s < end; s++) {
        if (*s == '\\' && *(s+1)) {
            s++;
            val[j++] = *s;
        } else {
            val[j++] = *s;
        }
    }
    val[j] = '\0';
    return val;
}

static long json_extract_num(const char *json, const char *key) {
    if (!json || !key) return 0;
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ') p++;
    return atol(p);
}

static int json_extract_int(const char *json, const char *key) {
    return (int)json_extract_num(json, key);
}

/* Simple JSON object serialization of session_meta_t. Caller must free. */
static char *meta_to_json(const session_meta_t *meta) {
    if (!meta) return NULL;
    /* Estimate buffer size */
    size_t sz = 1024;
    for (int i = 0; i < meta->tag_count; i++)
        sz += strlen(meta->tags[i]) + 8;
    char *buf = (char *)malloc(sz);
    if (!buf) return NULL;
    
    char time_created[32], time_updated[32];
    struct tm *tm;
    tm = localtime(&meta->created_at);
    strftime(time_created, sizeof(time_created), "%Y-%m-%dT%H:%M:%S", tm);
    tm = localtime(&meta->updated_at);
    strftime(time_updated, sizeof(time_updated), "%Y-%m-%dT%H:%M:%S", tm);
    
    char tags_json[1024] = "";
    if (meta->tag_count > 0) {
        strcat(tags_json, "[");
        for (int i = 0; i < meta->tag_count; i++) {
            if (i > 0) strcat(tags_json, ",");
            strcat(tags_json, "\"");
            /* Escape quotes in tag */
            size_t tlen = strlen(tags_json);
            for (const char *p = meta->tags[i]; *p; p++) {
                if (*p == '"' || *p == '\\') tags_json[tlen++] = '\\';
                tags_json[tlen++] = *p;
            }
            tags_json[tlen] = '"';
            tags_json[tlen+1] = '\0';
        }
        strcat(tags_json, "]");
    } else {
        strcat(tags_json, "[]");
    }
    
    snprintf(buf, sz, 
        "{"
        "\"schema_version\":%d,"
        "\"title\":\"%s\","
        "\"model\":\"%s\","
        "\"token_count\":%d,"
        "\"message_count\":%d,"
        "\"created_at\":%ld,"
        "\"created_at_str\":\"%s\","
        "\"updated_at\":%ld,"
        "\"updated_at_str\":\"%s\","
        "\"tags\":%s,"
        "\"parent_id\":\"%s\","
        "\"branch_point\":%d"
        "}",
        meta->schema_version,
        meta->title,
        meta->model,
        meta->token_count,
        meta->message_count,
        (long)meta->created_at, time_created,
        (long)meta->updated_at, time_updated,
        tags_json,
        meta->parent_id,
        meta->branch_point);
    
    return buf;
}

/* Parse JSON into session_meta_t. Returns true on success. */
static bool json_to_meta(const char *json_str, session_meta_t *meta) {
    if (!json_str || !meta) return false;
    memset(meta, 0, sizeof(*meta));
    
    meta->schema_version = json_extract_int(json_str, "schema_version");
    if (meta->schema_version == 0) meta->schema_version = SESSION_SCHEMA_VERSION;
    
    char *val;
    val = json_extract_str(json_str, "title");
    if (val) { snprintf(meta->title, sizeof(meta->title), "%s", val); free(val); }
    
    val = json_extract_str(json_str, "model");
    if (val) { snprintf(meta->model, sizeof(meta->model), "%s", val); free(val); }
    
    meta->token_count = json_extract_int(json_str, "token_count");
    meta->message_count = json_extract_int(json_str, "message_count");
    meta->created_at = (time_t)json_extract_num(json_str, "created_at");
    meta->updated_at = (time_t)json_extract_num(json_str, "updated_at");
    
    val = json_extract_str(json_str, "parent_id");
    if (val) { snprintf(meta->parent_id, sizeof(meta->parent_id), "%s", val); free(val); }
    
    meta->branch_point = json_extract_int(json_str, "branch_point");
    
    /* Parse tags array */
    const char *tags_start = strstr(json_str, "\"tags\":");
    if (tags_start) {
        tags_start += 7;
        while (*tags_start == ' ') tags_start++;
        if (*tags_start == '[') {
            tags_start++;
            while (*tags_start == ' ') tags_start++;
            if (*tags_start != ']') {
                const char *p = tags_start;
                while (*p && *p != ']' && meta->tag_count < SESSION_TAGS_MAX) {
                    while (*p == ' ' || *p == ',') p++;
                    if (*p == ']' || !*p) break;
                    if (*p == '"') {
                        p++;
                        size_t ti = 0;
                        while (*p && *p != '"' && ti < SESSION_TAG_LEN - 1) {
                            if (*p == '\\' && *(p+1)) { p++; }
                            meta->tags[meta->tag_count][ti++] = *p;
                            p++;
                        }
                        meta->tags[meta->tag_count][ti] = '\0';
                        meta->tag_count++;
                        if (*p == '"') p++;
                    } else break;
                }
            }
        }
    }
    
    return true;
}

/* ================================================================
 *  Public API
 * ================================================================ */

db_t *db_open(const char *dir_path, char **error_msg) {
    if (!dir_path || !*dir_path) {
        if (error_msg) *error_msg = strdup("NULL or empty directory path");
        return NULL;
    }

    if (!ensure_dir(dir_path)) {
        if (error_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "cannot create directory '%s'", dir_path);
            *error_msg = strdup(buf);
        }
        return NULL;
    }

    db_t *db = (db_t *)calloc(1, sizeof(db_t));
    if (!db) { if (error_msg) *error_msg = strdup("OOM"); return NULL; }

    strncpy(db->dir_path, dir_path, sizeof(db->dir_path) - 1);
    db->dir_len = strlen(db->dir_path);
    /* Strip trailing slash */
    while (db->dir_len > 0 && db->dir_path[db->dir_len - 1] == '/')
        db->dir_path[--db->dir_len] = '\0';

    return db;
}

void db_close(db_t *db) {
    if (!db) return;
    db_flush(db);
    free(db);
}

bool db_save(db_t *db, const char *session_id, const char *json_data) {
    if (!db || !session_id || !json_data) return false;

    char path[4096];
    make_path(db, session_id, path, sizeof(path));

    /* Write to temporary file first, then rename for atomicity */
    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "wb");
    if (!f) return false;

    size_t len = strlen(json_data);
    size_t written = fwrite(json_data, 1, len, f);
    fclose(f);

    if (written != len) {
        unlink(tmp_path);
        return false;
    }

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return false;
    }

    db->dirty = true;
    return true;
}

char *db_load(const db_t *db, const char *session_id, char **error_msg) {
    if (!db || !session_id) {
        if (error_msg) *error_msg = strdup("NULL args");
        return NULL;
    }

    char path[4096];
    make_path(db, session_id, path, sizeof(path));

    char *buf = read_file(path);
    if (!buf) {
        if (error_msg) *error_msg = strdup("session not found");
        return NULL;
    }

    return buf;
}

bool db_delete(db_t *db, const char *session_id) {
    if (!db || !session_id) return false;
    char path[4096];
    make_path(db, session_id, path, sizeof(path));
    int rc = unlink(path);
    /* Also delete metadata */
    char meta_path[4096];
    make_meta_path(db, session_id, meta_path, sizeof(meta_path));
    unlink(meta_path);
    if (rc == 0) db->dirty = true;
    return rc == 0;
}

bool db_exists(const db_t *db, const char *session_id) {
    if (!db || !session_id) return false;
    char path[4096];
    make_path(db, session_id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

/* ================================================================
 *  P141: Metadata operations
 * ================================================================ */

void db_meta_init(session_meta_t *meta) {
    if (!meta) return;
    memset(meta, 0, sizeof(*meta));
    meta->schema_version = SESSION_SCHEMA_VERSION;
    meta->created_at = time(NULL);
    meta->updated_at = meta->created_at;
    meta->branch_point = -1;
}

bool db_save_meta(db_t *db, const char *session_id, const session_meta_t *meta) {
    if (!db || !session_id || !meta) return false;
    
    char *json = meta_to_json(meta);
    if (!json) return false;
    
    char meta_path[4096];
    make_meta_path(db, session_id, meta_path, sizeof(meta_path));
    
    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", meta_path);
    
    FILE *f = fopen(tmp_path, "wb");
    if (!f) { free(json); return false; }
    
    size_t len = strlen(json);
    size_t written = fwrite(json, 1, len, f);
    fclose(f);
    
    if (written != len) {
        unlink(tmp_path);
        free(json);
        return false;
    }
    
    if (rename(tmp_path, meta_path) != 0) {
        unlink(tmp_path);
        free(json);
        return false;
    }
    
    free(json);
    db->dirty = true;
    return true;
}

bool db_load_meta(const db_t *db, const char *session_id, session_meta_t *meta) {
    if (!db || !session_id || !meta) return false;
    
    char meta_path[4096];
    make_meta_path(db, session_id, meta_path, sizeof(meta_path));
    
    char *json = read_file(meta_path);
    if (!json) {
        /* Return default metadata */
        db_meta_init(meta);
        snprintf(meta->title, sizeof(meta->title), "%s", session_id);
        return false;
    }
    
    bool ok = json_to_meta(json, meta);
    free(json);
    return ok;
}

/* ================================================================
 *  Listing
 * ================================================================ */

char **db_list(const db_t *db, size_t *count) {
    if (!db || !count) return NULL;
    *count = 0;

    DIR *dir = opendir(db->dir_path);
    if (!dir) return NULL;

    size_t cap = 64, idx = 0;
    char **sessions = (char **)calloc(cap, sizeof(char *));
    if (!sessions) { closedir(dir); return NULL; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t name_len = strlen(name);
        /* Only match *.json files, NOT *.meta.json */
        if (name_len < 6) continue;
        if (strcmp(name + name_len - 5, ".json") != 0) continue;
        /* Skip metadata files */
        if (name_len > 10 && strcmp(name + name_len - 10, ".meta.json") == 0) continue;

        char *sid = strndup(name, name_len - 5);
        if (!sid) continue;

        if (idx >= cap) {
            cap *= 2;
            char **nb = (char **)realloc(sessions, cap * sizeof(char *));
            if (!nb) {
                for (size_t i = 0; i < idx; i++) free(sessions[i]);
                free(sessions); closedir(dir); return NULL;
            }
            sessions = nb;
        }
        sessions[idx++] = sid;
    }

    closedir(dir);
    *count = idx;
    return sessions;
}

size_t db_count(const db_t *db) {
    size_t count = 0;
    char **list = db_list(db, &count);
    if (list) {
        for (size_t i = 0; i < count; i++) free(list[i]);
        free(list);
    }
    return count;
}

/* ================================================================
 *  P143: List with metadata
 * ================================================================ */

db_session_entry_t *db_list_with_meta(const db_t *db, size_t *count) {
    if (!db || !count) return NULL;
    *count = 0;

    char **ids = db_list(db, count);
    if (!ids || *count == 0) {
        free(ids);
        return NULL;
    }

    size_t n = *count;
    db_session_entry_t *entries = (db_session_entry_t *)calloc(n, sizeof(db_session_entry_t));
    if (!entries) {
        for (size_t i = 0; i < n; i++) free(ids[i]);
        free(ids);
        *count = 0;
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        snprintf(entries[i].id, sizeof(entries[i].id), "%s", ids[i]);
        db_load_meta(db, ids[i], &entries[i].meta);
        free(ids[i]);
    }
    free(ids);

    return entries;
}

/* ================================================================
 *  P145: Prune
 * ================================================================ */

int db_prune_by_age(db_t *db, int retention_days) {
    if (!db || retention_days <= 0) return 0;

    size_t count = 0;
    char **ids = db_list(db, &count);
    if (!ids) return 0;

    time_t now = time(NULL);
    time_t cutoff = now - (time_t)retention_days * 86400;
    int removed = 0;

    for (size_t i = 0; i < count; i++) {
        session_meta_t meta;
        bool has_meta = db_load_meta(db, ids[i], &meta);
        
        bool should_prune = false;
        if (has_meta && meta.updated_at > 0) {
            if (meta.updated_at < cutoff)
                should_prune = true;
        } else {
            /* No metadata — check file modification time */
            char path[4096];
            make_path(db, ids[i], path, sizeof(path));
            struct stat st;
            if (stat(path, &st) == 0 && st.st_mtime < cutoff)
                should_prune = true;
        }

        if (should_prune) {
            db_delete(db, ids[i]);
            removed++;
        }
        free(ids[i]);
    }
    free(ids);

    if (removed > 0) db->dirty = true;
    return removed;
}

/* ================================================================
 *  P148: Export
 * ================================================================ */

char *db_export_json(db_t *db, const char *session_id) {
    if (!db || !session_id) return NULL;

    char *data = db_load(db, session_id, NULL);
    if (!data) return NULL;

    session_meta_t meta;
    db_load_meta(db, session_id, &meta);
    char *meta_json = meta_to_json(&meta);
    
    char *result = NULL;
    if (meta_json) {
        size_t sz = strlen(data) + strlen(meta_json) + 128;
        result = (char *)malloc(sz);
        if (result) {
            snprintf(result, sz,
                "{\"session_id\":\"%s\",\"metadata\":%s,\"messages\":%s}",
                session_id, meta_json, data);
        }
        free(meta_json);
    } else {
        result = strdup(data);
    }
    free(data);
    return result;
}

char *db_export_markdown(db_t *db, const char *session_id) {
    if (!db || !session_id) return NULL;

    char *data = db_load(db, session_id, NULL);
    if (!data) return NULL;

    session_meta_t meta;
    db_load_meta(db, session_id, &meta);

    /* Build markdown by parsing JSON message array */
    char *result = (char *)malloc(65536);
    if (!result) { free(data); return NULL; }
    
    char *pos = result;
    char *end = result + 65536;
    
    pos += snprintf(pos, (size_t)(end - pos),
        "# Session: %s\n\n", session_id);
    
    if (meta.title[0])
        pos += snprintf(pos, (size_t)(end - pos), "**Title:** %s  \n", meta.title);
    if (meta.model[0])
        pos += snprintf(pos, (size_t)(end - pos), "**Model:** %s  \n", meta.model);
    pos += snprintf(pos, (size_t)(end - pos), "**Messages:** %d  \n", meta.message_count);
    pos += snprintf(pos, (size_t)(end - pos), "**Tokens:** %d  \n\n", meta.token_count);
    pos += snprintf(pos, (size_t)(end - pos), "---\n\n");

    /* Parse messages array from JSON */
    const char *arr_start = strchr(data, '[');
    if (arr_start) {
        const char *p = arr_start;
        int msg_idx = 0;
        while (*p && p < data + strlen(data)) {
            /* Find next object */
            const char *obj_start = strchr(p, '{');
            if (!obj_start) break;
            const char *obj_end = strchr(obj_start + 1, '}');
            if (!obj_end) break;
            
            /* Extract role and content */
            size_t obj_len = (size_t)(obj_end - obj_start + 1);
            char *obj = strndup(obj_start, obj_len);
            
            char *role = json_extract_str(obj, "role");
            char *content = json_extract_str(obj, "content");
            
            if (role && content) {
                msg_idx++;
                pos += snprintf(pos, (size_t)(end - pos),
                    "### Message %d (%s)\n\n%s\n\n",
                    msg_idx, role, content);
            } else if (role) {
                msg_idx++;
                pos += snprintf(pos, (size_t)(end - pos),
                    "### Message %d (%s)\n\n*(no content)*\n\n",
                    msg_idx, role);
            }
            
            free(role);
            free(content);
            free(obj);
            
            p = obj_end + 1;
        }
    }

    free(data);
    return result;
}

/* ================================================================
 *  P149: Branch
 * ================================================================ */

char *db_branch(db_t *db, const char *source_id, const char *new_id, int branch_point) {
    if (!db || !source_id || !new_id) return NULL;

    char *data = db_load(db, source_id, NULL);
    if (!data) return NULL;

    session_meta_t src_meta;
    db_load_meta(db, source_id, &src_meta);

    /* Parse the JSON message array and keep only first `branch_point + 1` messages.
     * We do simple brace counting to find the right number of message objects. */
    const char *arr_start = strchr(data, '[');
    if (!arr_start || branch_point < 0) {
        free(data);
        return NULL;
    }

    /* Find the end of the (branch_point)th message object */
    const char *p = arr_start + 1;
    int depth = 0;
    int msg_count = 0;
    const char *cut_point = NULL;

    while (*p) {
        if (*p == '{') {
            if (depth == 0) msg_count++;
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth == 0 && msg_count == branch_point + 1) {
                cut_point = p + 1;
                break;
            }
        }
        p++;
    }

    if (!cut_point) {
        /* branch_point >= total messages, copy all */
        cut_point = data + strlen(data) - 1;
        /* Find closing bracket */
        while (cut_point > data && *cut_point != ']') cut_point--;
    }

    /* Build new data: [ ...messages up to branch_point... ] */
    size_t prefix_len = (size_t)(cut_point - arr_start);
    size_t new_data_sz = prefix_len + 4;
    char *new_data = (char *)malloc(new_data_sz);
    if (!new_data) { free(data); return NULL; }

    new_data[0] = '[';
    if (prefix_len > 1) {
        memcpy(new_data + 1, arr_start + 1, prefix_len - 1);
    }
    new_data[prefix_len] = ']';
    new_data[prefix_len + 1] = '\0';

    bool ok = db_save(db, new_id, new_data);
    free(data);

    if (!ok) {
        free(new_data);
        return NULL;
    }

    /* Create metadata for branched session */
    session_meta_t new_meta;
    db_meta_init(&new_meta);
    snprintf(new_meta.title, sizeof(new_meta.title), "%s (branch)", src_meta.title[0] ? src_meta.title : source_id);
    snprintf(new_meta.model, sizeof(new_meta.model), "%s", src_meta.model);
    snprintf(new_meta.parent_id, sizeof(new_meta.parent_id), "%s", source_id);
    new_meta.branch_point = branch_point;
    new_meta.message_count = branch_point + 1;
    db_save_meta(db, new_id, &new_meta);

    return new_data;
}

/* ================================================================
 *  P150: Migration
 * ================================================================ */

int db_migrate(db_t *db) {
    if (!db) return 0;

    size_t count = 0;
    char **ids = db_list(db, &count);
    if (!ids) return 0;

    int migrated = 0;
    for (size_t i = 0; i < count; i++) {
        session_meta_t meta;
        bool has_meta = db_load_meta(db, ids[i], &meta);

        if (!has_meta) {
            /* Create metadata for sessions without it */
            db_meta_init(&meta);
            snprintf(meta.title, sizeof(meta.title), "%s", ids[i]);

            /* Count messages from session data */
            char *data = db_load(db, ids[i], NULL);
            if (data) {
                int msg_count = 0;
                const char *p = data;
                int depth = 0;
                while (*p) {
                    if (*p == '{') { if (depth == 0) msg_count++; depth++; }
                    else if (*p == '}') depth--;
                    p++;
                }
                meta.message_count = msg_count;
                free(data);
            }

            db_save_meta(db, ids[i], &meta);
            migrated++;
        } else if (meta.schema_version < SESSION_SCHEMA_VERSION) {
            /* Upgrade schema if needed */
            meta.schema_version = SESSION_SCHEMA_VERSION;
            db_save_meta(db, ids[i], &meta);
            migrated++;
        }
    }

    for (size_t i = 0; i < count; i++) free(ids[i]);
    free(ids);

    if (migrated > 0) db->dirty = true;
    return migrated;
}

/* ================================================================
 *  Maintenance
 * ================================================================ */

long long db_storage_size(const db_t *db) {
    if (!db) return 0;
    long long total = 0;
    size_t count = 0;
    char **list = db_list(db, &count);
    if (!list) return 0;

    for (size_t i = 0; i < count; i++) {
        char path[4096];
        make_path(db, list[i], path, sizeof(path));
        struct stat st;
        if (stat(path, &st) == 0) total += (long long)st.st_size;
        /* Also check meta file */
        char meta_path[4096];
        make_meta_path(db, list[i], meta_path, sizeof(meta_path));
        if (stat(meta_path, &st) == 0) total += (long long)st.st_size;
        free(list[i]);
    }
    free(list);
    return total;
}

bool db_clear(db_t *db) {
    if (!db) return false;
    size_t count = 0;
    char **list = db_list(db, &count);
    if (!list) return true;

    for (size_t i = 0; i < count; i++) {
        db_delete(db, list[i]);
        free(list[i]);
    }
    free(list);
    db->dirty = true;
    return true;
}

bool db_flush(db_t *db) {
    if (!db || !db->dirty) return true;
    db->dirty = false;
    return true;
}
