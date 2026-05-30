/*
 * db.c — File-based session store for Hermes C.
 * Stores sessions as individual JSON files.
 * No SQLite dependency.
 */

#define _GNU_SOURCE
#include "hermes_db.h"
#include "hermes_json.h"
#include "hermes_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* ================================================================
 *  Internal
 * ================================================================ */

static void *xmalloc(size_t sz) {
    void *p = malloc(sz ? sz : 1);
    if (!p) { fprintf(stderr, "db: OOM\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t sz) {
    p = realloc(p, sz ? sz : 1);
    if (!p) { fprintf(stderr, "db: OOM\n"); exit(1); }
    return p;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *d = (char *)xmalloc(n + 1);
    memcpy(d, s, n + 1);
    return d;
}

struct db_t {
    char dir[4096];
};

/* ================================================================
 *  File path helpers
 * ================================================================ */

static void session_path(db_t *db, const char *session_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/%s.json", db->dir, session_id);
}

/* ================================================================
 *  File path helpers
 * ================================================================ */

static bool ensure_dir(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) return true;
    return mkdir(dir, 0755) == 0 || (errno == EEXIST && stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
}

/* ================================================================
 *  Public API
 * ================================================================ */

db_t *db_open(const char *dir_path) {
    db_t *db = (db_t *)xmalloc(sizeof(db_t));
    snprintf(db->dir, sizeof(db->dir), "%s", dir_path);
    ensure_dir(db->dir);
    return db;
}

void db_close(db_t *db) {
    free(db);
}

/* Generate session ID like "YYYYMMDD_HHMMSS_xxxx" */
char *db_gen_session_id(db_t *db_unused) {
    (void)db_unused;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    char buf[64];
    snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Add random suffix to avoid collisions */
    unsigned char rnd[4];
    { /* pragma: GCC always has crypto_random_bytes since it's linked */
        rnd[0] = (unsigned char)(clock() & 0xFF);
        rnd[1] = (unsigned char)(getpid() & 0xFF);
        rnd[2] = (unsigned char)(t & 0xFF);
        rnd[3] = (unsigned char)((t >> 8) & 0xFF);
    }

    char sid[96];
    snprintf(sid, sizeof(sid), "%s_%02x%02x%02x%02x",
             buf, rnd[0], rnd[1], rnd[2], rnd[3]);

    return xstrdup(sid);
}

bool db_save_message(db_t *db, const char *session_id,
                     const char *role, const char *content)
{
    char path[4096];
    session_path(db, session_id, path, sizeof(path));

    json_node_t *root = NULL;
    json_node_t *msgs = NULL;

    /* Try to load existing session file */
    char *err = NULL;
    root = json_parse_file(path, &err);
    if (root) {
        msgs = json_object_get(root, "messages");
    }

    if (!msgs) {
        /* Create new session file */
        if (root) json_free(root);
        root = json_new_object();
        json_object_set(root, "session_id", json_new_string(session_id));
        msgs = json_new_array();
        json_object_set(root, "messages", msgs);
    }

    /* Add message */
    json_node_t *msg = json_new_object();
    json_object_set(msg, "role", json_new_string(role));
    json_object_set(msg, "content", json_new_string(content ? content : ""));
    json_array_append(msgs, msg);

    /* Write to file */
    char *serialized = json_serialize_pretty(root, 0);
    FILE *f = fopen(path, "w");
    if (!f) { free(serialized); json_free(root); return false; }
    fputs(serialized, f);
    fclose(f);
    free(serialized);
    json_free(root);
    return true;
}

char **db_load_session(db_t *db, const char *session_id, size_t *count,
                       const char **roles_out)
{
    char path[4096];
    session_path(db, session_id, path, sizeof(path));

    char *err = NULL;
    json_node_t *root = json_parse_file(path, &err);
    if (!root) {
        *count = 0;
        return NULL;
    }

    json_node_t *msgs = json_object_get(root, "messages");
    if (!msgs || msgs->type != JSON_ARRAY) {
        *count = 0;
        json_free(root);
        return NULL;
    }

    size_t n = json_array_count(msgs);
    char **result = (char **)xmalloc((n + 1) * sizeof(char *));
    if (roles_out) {
        /* Allocate roles array - caller must free */
        char **roles = (char **)xmalloc((n + 1) * sizeof(char *));
        *(char ***)roles_out = roles;
    }

    for (size_t i = 0; i < n; i++) {
        json_node_t *msg = json_array_get(msgs, i);
        const char *content = json_object_get_string(msg, "content", "");
        result[i] = xstrdup(content);
        if (roles_out) {
            char **roles = *(char ***)roles_out;
            const char *role = json_object_get_string(msg, "role", "unknown");
            roles[i] = xstrdup(role);
        }
    }
    result[n] = NULL;
    if (roles_out) {
        char **roles = *(char ***)roles_out;
        roles[n] = NULL;
    }

    *count = n;
    json_free(root);
    return result;
}

char **db_list_sessions(db_t *db, size_t *count, int limit) {
    DIR *dir = opendir(db->dir);
    if (!dir) { *count = 0; return NULL; }

    size_t cap = 64, n = 0;
    char **sessions = (char **)xmalloc(cap * sizeof(char *));

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip non-JSON and index */
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".json") != 0)
            continue;
        if (strcmp(entry->d_name, "_index.json") == 0)
            continue;

        /* Strip .json */
        entry->d_name[len - 5] = '\0';

        if (n >= cap) { cap *= 2; sessions = (char **)xrealloc(sessions, cap * sizeof(char *)); }
        sessions[n++] = xstrdup(entry->d_name);

        if (limit > 0 && (int)n >= limit) break;
    }
    closedir(dir);

    sessions = (char **)xrealloc(sessions, (n + 1) * sizeof(char *));
    sessions[n] = NULL;
    *count = n;
    return sessions;
}

char **db_search(db_t *db, const char *query, size_t *count) {
    /* Simple grep-based search */
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "grep -rl '%s' '%s'/*.json 2>/dev/null | head -50",
             query, db->dir);

    FILE *fp = popen(cmd, "r");
    if (!fp) { *count = 0; return NULL; }

    size_t cap = 32, n = 0;
    char **results = (char **)xmalloc(cap * sizeof(char *));
    char line[4096];

    while (fgets(line, sizeof(line), fp)) {
        /* Strip newline and .json extension */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

        /* Extract session ID from path */
        char *slash = strrchr(line, '/');
        char *fname = slash ? slash + 1 : line;
        len = strlen(fname);
        if (len > 5 && strcmp(fname + len - 5, ".json") == 0) fname[len - 5] = '\0';

        if (n >= cap) { cap *= 2; results = (char **)xrealloc(results, cap * sizeof(char *)); }
        results[n++] = xstrdup(fname);
    }

    pclose(fp);
    results = (char **)xrealloc(results, (n + 1) * sizeof(char *));
    results[n] = NULL;
    *count = n;
    return results;
}

bool db_delete_session(db_t *db, const char *session_id) {
    char path[4096];
    session_path(db, session_id, path, sizeof(path));
    return unlink(path) == 0;
}
