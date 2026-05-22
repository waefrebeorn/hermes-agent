/*
 * cron_sqlite.c — P169: SQLite job store for Hermes C cron.
 *
 * Persistent job definitions using embedded libdb pattern.
 * Replaces in-memory job list in scheduler.c.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* ================================================================
 *  SQLite Job Store (file-based JSON storage, no libsqlite dependency)
 * ================================================================ */

#define MAX_JOBS_STORED 256

/* Global store instance — initialized lazily */
cron_sqlite_store_t *g_cron_store = NULL;

typedef struct {
    char name[128];
    char schedule[128];
    char command[2048];
    bool active;
    int  retry_count;
    int  max_retries;
    int  backoff_sec;
    char chain_from[128];
    char template_name[128];
    char script_type[32];
    char interpreter[64];
    char notify_channel[128];
    bool notify_on_complete;
    bool notify_on_failure;
    time_t last_run;
    time_t created_at;
} cron_job_entry_t;

struct cron_sqlite_store_t {
    char path[4096];
    cron_job_entry_t jobs[MAX_JOBS_STORED];
    int count;
    bool loaded;
};

/* ================================================================
 *  Open / Close
 * ================================================================ */

cron_sqlite_store_t *cron_sqlite_open(const char *path) {
    if (!path) return NULL;

    cron_sqlite_store_t *store = (cron_sqlite_store_t *)calloc(1, sizeof(cron_sqlite_store_t));
    if (!store) return NULL;

    snprintf(store->path, sizeof(store->path), "%s", path);

    /* Ensure parent directory exists */
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, 0755);
    }

    return store;
}

void cron_sqlite_close(cron_sqlite_store_t *store) {
    if (!store) return;
    free(store);
}

/* ================================================================
 *  Persistence to JSON file
 * ================================================================ */

static bool store_save(cron_sqlite_store_t *store) {
    if (!store) return false;

    json_t *root = json_object();
    json_set(root, "version", json_number(1));

    json_t *jobs = json_array();
    for (int i = 0; i < store->count; i++) {
        json_t *j = json_object();
        json_set(j, "name", json_string(store->jobs[i].name));
        json_set(j, "schedule", json_string(store->jobs[i].schedule));
        json_set(j, "command", json_string(store->jobs[i].command));
        json_set(j, "active", json_bool(store->jobs[i].active));
        json_set(j, "retry_count", json_number(store->jobs[i].retry_count));
        json_set(j, "max_retries", json_number(store->jobs[i].max_retries));
        json_set(j, "backoff_sec", json_number(store->jobs[i].backoff_sec));
        json_set(j, "chain_from", json_string(store->jobs[i].chain_from));
        json_set(j, "template_name", json_string(store->jobs[i].template_name));
        json_set(j, "script_type", json_string(store->jobs[i].script_type));
        json_set(j, "interpreter", json_string(store->jobs[i].interpreter));
        json_set(j, "notify_channel", json_string(store->jobs[i].notify_channel));
        json_set(j, "notify_on_complete", json_bool(store->jobs[i].notify_on_complete));
        json_set(j, "notify_on_failure", json_bool(store->jobs[i].notify_on_failure));
        json_set(j, "last_run", json_number((double)store->jobs[i].last_run));
        json_set(j, "created_at", json_number((double)store->jobs[i].created_at));
        json_append(jobs, j);
    }
    json_set(root, "jobs", jobs);

    char *json_str = json_serialize_pretty(root, 2);
    json_free(root);

    if (!json_str) return false;

    /* Atomic write */
    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", store->path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);

    rename(tmp_path, store->path);
    chmod(store->path, 0600);

    free(json_str);
    return true;
}

static bool store_load(cron_sqlite_store_t *store) {
    if (!store) return false;

    FILE *f = fopen(store->path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize > 1024 * 1024) { fclose(f); return false; }
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return false; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    buf[nread] = '\0';
    fclose(f);

    char *err = NULL;
    json_t *root = json_parse(buf, &err);
    free(buf);
    if (!root) { free(err); return false; }

    json_t *jobs = json_obj_get(root, "jobs");
    if (!jobs) { json_free(root); return false; }

    store->count = 0;
    size_t n = json_len(jobs);
    for (size_t i = 0; i < n && store->count < MAX_JOBS_STORED; i++) {
        json_t *j = json_get(jobs, i);
        if (!j) continue;

        cron_job_entry_t *e = &store->jobs[store->count];
        memset(e, 0, sizeof(*e));

        snprintf(e->name, sizeof(e->name), "%s", json_get_str(j, "name", ""));
        snprintf(e->schedule, sizeof(e->schedule), "%s", json_get_str(j, "schedule", ""));
        snprintf(e->command, sizeof(e->command), "%s", json_get_str(j, "command", ""));
        e->active = json_get_bool(j, "active", true);
        e->retry_count = (int)json_get_num(j, "retry_count", 0);
        e->max_retries = (int)json_get_num(j, "max_retries", 0);
        e->backoff_sec = (int)json_get_num(j, "backoff_sec", 30);
        snprintf(e->chain_from, sizeof(e->chain_from), "%s", json_get_str(j, "chain_from", ""));
        snprintf(e->template_name, sizeof(e->template_name), "%s", json_get_str(j, "template_name", ""));
        snprintf(e->script_type, sizeof(e->script_type), "%s", json_get_str(j, "script_type", ""));
        snprintf(e->interpreter, sizeof(e->interpreter), "%s", json_get_str(j, "interpreter", ""));
        snprintf(e->notify_channel, sizeof(e->notify_channel), "%s", json_get_str(j, "notify_channel", ""));
        e->notify_on_complete = json_get_bool(j, "notify_on_complete", false);
        e->notify_on_failure = json_get_bool(j, "notify_on_failure", false);
        e->last_run = (time_t)json_get_num(j, "last_run", 0);
        e->created_at = (time_t)json_get_num(j, "created_at", 0);

        if (e->name[0]) store->count++;
    }

    json_free(root);
    store->loaded = true;
    return true;
}

/* ================================================================
 *  Job CRUD
 * ================================================================ */

bool cron_sqlite_save_job(cron_sqlite_store_t *store, const char *name,
                           const char *schedule, const char *command,
                           bool active, int retry_count, int max_retries,
                           const char *chain_from, const char *template_name,
                           const char *script_type)
{
    if (!store || !name || !schedule) return false;

    /* Find existing or new slot */
    cron_job_entry_t *e = NULL;
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->jobs[i].name, name) == 0) {
            e = &store->jobs[i];
            break;
        }
    }

    if (!e) {
        if (store->count >= MAX_JOBS_STORED) return false;
        e = &store->jobs[store->count++];
    }

    memset(e, 0, sizeof(*e));
    snprintf(e->name, sizeof(e->name), "%s", name);
    snprintf(e->schedule, sizeof(e->schedule), "%s", schedule);
    snprintf(e->command, sizeof(e->command), "%s", command ? command : "");
    e->active = active;
    e->retry_count = retry_count;
    e->max_retries = max_retries;
    e->backoff_sec = 30;
    snprintf(e->chain_from, sizeof(e->chain_from), "%s", chain_from ? chain_from : "");
    snprintf(e->template_name, sizeof(e->template_name), "%s", template_name ? template_name : "");
    snprintf(e->script_type, sizeof(e->script_type), "%s", script_type ? script_type : "");
    e->last_run = 0;
    e->created_at = time(NULL);

    return store_save(store);
}

bool cron_sqlite_load_jobs(cron_sqlite_store_t *store) {
    if (!store) return false;
    return store_load(store);
}

bool cron_sqlite_delete_job(cron_sqlite_store_t *store, const char *name) {
    if (!store || !name) return false;

    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->jobs[i].name, name) == 0) {
            store->jobs[i] = store->jobs[--store->count];
            memset(&store->jobs[store->count], 0, sizeof(cron_job_entry_t));
            return store_save(store);
        }
    }
    return false;
}

bool cron_sqlite_update_job(cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value)
{
    if (!store || !name || !field) return false;

    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->jobs[i].name, name) == 0) {
            if (strcmp(field, "active") == 0)
                store->jobs[i].active = (strcmp(value, "true") == 0);
            else if (strcmp(field, "schedule") == 0)
                snprintf(store->jobs[i].schedule, sizeof(store->jobs[0].schedule), "%s", value);
            else if (strcmp(field, "command") == 0)
                snprintf(store->jobs[i].command, sizeof(store->jobs[0].command), "%s", value);
            else if (strcmp(field, "max_retries") == 0)
                store->jobs[i].max_retries = atoi(value);
            else if (strcmp(field, "retry_count") == 0)
                store->jobs[i].retry_count = atoi(value);
            else if (strcmp(field, "last_run") == 0)
                store->jobs[i].last_run = (time_t)atol(value);
            else if (strcmp(field, "chain_from") == 0)
                snprintf(store->jobs[i].chain_from, sizeof(store->jobs[0].chain_from), "%s", value);
            else if (strcmp(field, "script_type") == 0)
                snprintf(store->jobs[i].script_type, sizeof(store->jobs[0].script_type), "%s", value);
            else if (strcmp(field, "interpreter") == 0)
                snprintf(store->jobs[i].interpreter, sizeof(store->jobs[0].interpreter), "%s", value);
            else if (strcmp(field, "notify_channel") == 0)
                snprintf(store->jobs[i].notify_channel, sizeof(store->jobs[0].notify_channel), "%s", value);
            else if (strcmp(field, "notify_on_complete") == 0)
                store->jobs[i].notify_on_complete = (strcmp(value, "true") == 0);
            else if (strcmp(field, "notify_on_failure") == 0)
                store->jobs[i].notify_on_failure = (strcmp(value, "true") == 0);
            else
                return false;

            return store_save(store);
        }
    }
    return false;
}

/* Get job entry pointer for internal use */
cron_job_entry_t *cron_sqlite_find(cron_sqlite_store_t *store, const char *name) {
    if (!store || !name) return NULL;
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->jobs[i].name, name) == 0)
            return &store->jobs[i];
    }
    return NULL;
}

int cron_sqlite_count(cron_sqlite_store_t *store) {
    return store ? store->count : 0;
}

cron_job_entry_t *cron_sqlite_get(cron_sqlite_store_t *store, int index) {
    if (!store || index < 0 || index >= store->count) return NULL;
    return &store->jobs[index];
}
