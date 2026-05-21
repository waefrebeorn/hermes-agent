/*
 * db.c — File-based JSON session store for C.
 * Each session = one .json file on disk. No SQLite.
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
    /* Sanitize session_id: only allow safe chars */
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

/* Ensure directory exists */
static bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return true;
        /* Exists but not a dir */
        return false;
    }
    /* Try to create */
    return mkdir(path, 0755) == 0 || errno == EEXIST;
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

    FILE *f = fopen(path, "wb");
    if (!f) return false;

    size_t len = strlen(json_data);
    size_t written = fwrite(json_data, 1, len, f);
    fclose(f);

    if (written != len) {
        unlink(path);
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

    FILE *f = fopen(path, "rb");
    if (!f) {
        if (error_msg) *error_msg = strdup("session not found");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) { fclose(f); if (error_msg) *error_msg = strdup("stat failed"); return NULL; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); if (error_msg) *error_msg = strdup("OOM"); return NULL; }

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    return buf;
}

bool db_delete(db_t *db, const char *session_id) {
    if (!db || !session_id) return false;
    char path[4096];
    make_path(db, session_id, path, sizeof(path));
    int rc = unlink(path);
    if (rc == 0) db->dirty = true;
    return rc == 0;
}

bool db_exists(const db_t *db, const char *session_id) {
    if (!db || !session_id) return false;
    char path[4096];
    make_path(db, session_id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

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
        /* Only match *.json files */
        const char *name = entry->d_name;
        size_t name_len = strlen(name);
        if (name_len < 6) continue; /* at least "x.json" */
        if (strcmp(name + name_len - 5, ".json") != 0) continue;

        /* Extract session ID (strip .json) */
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

long long db_storage_size(const db_t *db) {
    if (!db) return 0;
    long long total = 0;
    size_t count = 0;
    char **list = db_list(db, &count);
    if (!list) return 0;

    for (size_t i = 0; i < count; i++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s.json", db->dir_path, list[i]);
        struct stat st;
        if (stat(path, &st) == 0) total += (long long)st.st_size;
        free(list[i]);
    }
    free(list);
    return total;
}

bool db_clear(db_t *db) {
    if (!db) return false;
    size_t count = 0;
    char **list = db_list(db, &count);
    if (!list) return true; /* already empty */

    for (size_t i = 0; i < count; i++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s.json", db->dir_path, list[i]);
        unlink(path);
        free(list[i]);
    }
    free(list);
    db->dirty = true;
    return true;
}

bool db_flush(db_t *db) {
    if (!db || !db->dirty) return true;
    /* File-based store: writes are synchronous, so flush is a no-op.
     * This exists for API compatibility if we add write caching later. */
    db->dirty = false;
    return true;
}
