/*
 * file_sync.c — Environment file sync for remote execution backends
 *
 * Port of Python tools/environments/file_sync.py (402 LOC).
 * Core functions: collect files, build commands, upload.
 *
 * Thread-safe: uses thread-local static for hermes_home.
 * No external dependencies beyond libc.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include "file_sync.h"
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ── Internal helpers ────────────────────────────────────────────── */

static pthread_once_t g_home_once = PTHREAD_ONCE_INIT;
static char g_hermes_home[1024] = "";

static void default_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (home && home[0])
        snprintf(g_hermes_home, sizeof(g_hermes_home), "%s", home);
    else {
        const char *user = getenv("HOME");
        if (user)
            snprintf(g_hermes_home, sizeof(g_hermes_home), "%s/.hermes", user);
        else
            snprintf(g_hermes_home, sizeof(g_hermes_home), "/root/.hermes");
    }
}

void file_sync_set_home(const char *home) {
    if (home)
        snprintf(g_hermes_home, sizeof(g_hermes_home), "%s", home);
}

static const char *get_home(void) {
    pthread_once(&g_home_once, default_home);
    return g_hermes_home;
}

/* ── File list management ────────────────────────────────────────── */

static bool list_add(file_sync_list_t *list, const char *host, const char *remote) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity ? list->capacity * 2 : 64;
        file_sync_entry_t *tmp = realloc(list->entries, new_cap * sizeof(file_sync_entry_t));
        if (!tmp) return false;
        list->entries = tmp;
        list->capacity = new_cap;
    }
    snprintf(list->entries[list->count].host_path, sizeof(list->entries[list->count].host_path), "%s", host);
    snprintf(list->entries[list->count].remote_path, sizeof(list->entries[list->count].remote_path), "%s", remote);
    list->count++;
    return true;
}

/* ── Recursive directory enumeration ─────────────────────────────── */

static bool collect_dir(file_sync_list_t *list, const char *host_dir,
                        const char *remote_base, const char *host_base) {
    DIR *d = opendir(host_dir);
    if (!d) return false;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* Build full paths */
        char host_path[1024];
        snprintf(host_path, sizeof(host_path), "%s/%s", host_dir, entry->d_name);

        struct stat st;
        if (stat(host_path, &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            /* Compute remote path: replace host_base with remote_base */
            char remote_path[1024];
            size_t base_len = strlen(host_base);
            if (strncmp(host_path, host_base, base_len) == 0) {
                snprintf(remote_path, sizeof(remote_path), "%s%s",
                         remote_base, host_path + base_len);
            } else {
                /* Fallback: just use host path */
                snprintf(remote_path, sizeof(remote_path), "%s", host_path);
            }
            list_add(list, host_path, remote_path);
        } else if (S_ISDIR(st.st_mode)) {
            collect_dir(list, host_path, remote_base, host_base);
        }
    }
    closedir(d);
    return true;
}

/* ── Collect credential files ────────────────────────────────────── */

static void collect_credentials(file_sync_list_t *list, const char *hermes_home,
                                const char *container_base) {
    /* Scan for .provider_* and creds directories in hermes_home */
    const char *cred_dirs[] = {
        "credentials",
        "data/skills",
        NULL
    };

    char cred_path[1024];
    for (int i = 0; cred_dirs[i]; i++) {
        (void)snprintf(cred_path, sizeof(cred_path), "%s/%s", hermes_home, cred_dirs[i]);
        struct stat st;
        if (stat(cred_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            collect_dir(list, cred_path, container_base, hermes_home);
        }
    }

    /* Also include config and .env files */
    const char *top_files[] = {
        "config.yaml",
        ".env",
        NULL
    };
    for (int i = 0; top_files[i]; i++) {
        char host_path[1024], remote_path[1024];
        snprintf(host_path, sizeof(host_path), "%s/%s", hermes_home, top_files[i]);
        struct stat st;
        if (stat(host_path, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(remote_path, sizeof(remote_path), "%s/%s",
                     container_base, top_files[i]);
            list_add(list, host_path, remote_path);
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

file_sync_list_t *file_sync_collect(const char *container_base) {
    file_sync_list_t *list = calloc(1, sizeof(file_sync_list_t));
    if (!list) return NULL;

    const char *home = get_home();

    collect_credentials(list, home, container_base);

    /* Also scan skills directory */
    char skills_path[1024];
    snprintf(skills_path, sizeof(skills_path), "%s/skills", home);
    struct stat st;
    if (stat(skills_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        collect_dir(list, skills_path, container_base, home);
    }

    return list;
}

void file_sync_list_free(file_sync_list_t *list) {
    if (!list) return;
    free(list->entries);
    free(list);
}

char *file_sync_mkdir_cmd(file_sync_list_t *files) {
    if (!files || files->count == 0) return strdup("");

    /* Collect unique parent dirs */
    char **dirs = NULL;
    int dir_count = 0, dir_cap = 0;

    for (int i = 0; i < files->count; i++) {
        /* Find last '/' in remote_path as parent dir */
        const char *slash = strrchr(files->entries[i].remote_path, '/');
        if (!slash) continue;

        char parent[1024];
        size_t len = slash - files->entries[i].remote_path;
        if (len >= sizeof(parent)) len = sizeof(parent) - 1;
        memcpy(parent, files->entries[i].remote_path, len);
        parent[len] = '\0';

        /* Check for duplicates */
        int found = 0;
        for (int j = 0; j < dir_count; j++) {
            if (strcmp(dirs[j], parent) == 0) { found = 1; break; }
        }
        if (found) continue;

        /* Add */
        if (dir_count >= dir_cap) {
            int new_cap = dir_cap ? dir_cap * 2 : 16;
            char **tmp = realloc(dirs, new_cap * sizeof(char *));
            if (!tmp) { /* leak but proceed */ break; }
            dirs = tmp;
            dir_cap = new_cap;
        }
        dirs[dir_count] = strdup(parent);
        if (dirs[dir_count]) dir_count++;
    }

    /* Build "mkdir -p <dir1> <dir2> ..." */
    size_t total = 10; /* "mkdir -p " */
    for (int i = 0; i < dir_count; i++)
        total += strlen(dirs[i]) + 2; /* space + shell_quote approx */

    char *cmd = malloc(total + 10);
    if (!cmd) {
        for (int i = 0; i < dir_count; i++) free(dirs[i]);
        free(dirs);
        return NULL;
    }

    size_t pos = 0;
    pos += snprintf(cmd + pos, total - pos, "mkdir -p");
    for (int i = 0; i < dir_count; i++) {
        pos += snprintf(cmd + pos, total - pos, " '%s'", dirs[i]);
        free(dirs[i]);
    }
    free(dirs);
    return cmd;
}

bool file_sync_upload_all(file_sync_list_t *files,
                           file_sync_upload_fn upload, void *upload_ctx) {
    if (!files || !upload) return false;

    for (int i = 0; i < files->count; i++) {
        if (!upload(files->entries[i].host_path,
                     files->entries[i].remote_path,
                     upload_ctx))
            return false;
    }
    return true;
}
#pragma GCC diagnostic pop
