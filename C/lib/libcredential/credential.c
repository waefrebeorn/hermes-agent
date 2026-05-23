/*
 * credential.c — File passthrough registry for remote terminal backends.
 *
 * Port of Python tools/credential_files.py.
 *
 * Manages a session-scoped registry of credential files to mount into
 * remote sandboxes (Docker, Modal, SSH). Also provides skills directory
 * mounts and cache directory mounts.
 *
 * Security: paths are validated against HERMES_HOME to prevent traversal
 * attacks. Symlinks in skills dirs are stripped via sanitized temp copies.
 */

#define _GNU_SOURCE
#include "credential.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>

/* ─── Internal helpers ───────────────────────────────────────────────── */

/* Resolve HERMES_HOME. Returns pointer to static buffer. */
static const char *credential_get_home(void)
{
    static char buf[CREDENTIAL_MAX_PATH];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sizeof(buf), "%s", home);
    /* Strip trailing slash */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '/')
        buf[len - 1] = '\0';
    return buf;
}

/* Check if a path contains traversal components */
static bool has_traversal(const char *path)
{
    if (!path) return true;
    /* Check for ".." */
    const char *p = path;
    while ((p = strstr(p, "..")) != NULL) {
        /* Must be a standalone component (surrounded by / or at start/end) */
        if ((p == path || p[-1] == '/') &&
            (p[2] == '\0' || p[2] == '/'))
            return true;
        p++;
    }
    return false;
}

/* Check that resolved path stays within base dir */
static bool within_dir(const char *resolved_path, const char *base_dir)
{
    size_t base_len = strlen(base_dir);
    /* Path must start with base directory */
    if (strncmp(resolved_path, base_dir, base_len) != 0)
        return false;
    /* And must be followed by '/' or '\0' (not a partial prefix match) */
    if (resolved_path[base_len] != '\0' && resolved_path[base_len] != '/')
        return false;
    return true;
}

/* Resolve a HERMES_HOME-relative path to absolute, with containment check.
 * Returns pointer to static buffer, or NULL if invalid. */
static const char *resolve_host_path(const char *relative_path)
{
    static char buf[CREDENTIAL_MAX_PATH];

    if (!relative_path || !relative_path[0])
        return NULL;

    if (relative_path[0] == '/') {
        /* Absolute paths rejected — must be relative to HERMES_HOME */
        return NULL;
    }

    if (has_traversal(relative_path)) {
        return NULL;  /* Path traversal detected */
    }

    const char *home = credential_get_home();
    snprintf(buf, sizeof(buf), "%s/%s", home, relative_path);

    /* Realpath resolves symlinks AND normalizes */
    char resolved[CREDENTIAL_MAX_PATH];
    if (!realpath(buf, resolved)) {
        /* File doesn't exist — that's OK for existence check later */
        return NULL;
    }

    /* Containment check: resolved path must be inside HERMES_HOME */
    if (!within_dir(resolved, home)) {
        return NULL;
    }

    /* Copy to static buffer for return */
    memcpy(buf, resolved, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

/* Check if a file exists and is a regular file */
static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Check if a directory exists */
static bool dir_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Check if a path has any symlink components */
static bool has_symlinks(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) return false;

    struct dirent *entry;
    char full[CREDENTIAL_MAX_PATH];
    bool found = false;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(full, sizeof(full), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (lstat(full, &st) == 0 && S_ISLNK(st.st_mode)) {
            found = true;
            break;
        }
    }
    closedir(dir);
    return found;
}

/* Recursively copy files from src to dst (skip symlinks, dirs only) */
static bool copy_dir_tree(const char *src, const char *dst)
{
    DIR *dir = opendir(src);
    if (!dir) return false;

    /* Create dst if needed */
    mkdir(dst, 0755);

    struct dirent *entry;
    char src_path[CREDENTIAL_MAX_PATH];
    char dst_path[CREDENTIAL_MAX_PATH];
    bool ok = true;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

        struct stat st;
        if (lstat(src_path, &st) != 0) continue;

        if (S_ISLNK(st.st_mode)) {
            /* Skip symlinks */
            continue;
        } else if (S_ISDIR(st.st_mode)) {
            mkdir(dst_path, 0755);
            if (!copy_dir_tree(src_path, dst_path)) {
                ok = false;
                break;
            }
        } else if (S_ISREG(st.st_mode)) {
            /* Simple file copy */
            FILE *sf = fopen(src_path, "rb");
            if (!sf) { ok = false; break; }
            FILE *df = fopen(dst_path, "wb");
            if (!df) { fclose(sf); ok = false; break; }

            char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), sf)) > 0) {
                if (fwrite(buf, 1, n, df) != n) {
                    ok = false;
                    break;
                }
            }
            fclose(sf);
            fclose(df);
            if (!ok) break;
        }
    }
    closedir(dir);
    return ok;
}

/* ─── Session-scoped registry ────────────────────────────────────────── */

static credential_mount_t g_registry[CREDENTIAL_MAX_FILES];
static int g_registry_count = 0;

/* Temp dir for symlink-safe skills copies */
static char g_safe_tempdir[CREDENTIAL_MAX_PATH] = "";

/* ─── Implementation: Registration ───────────────────────────────────── */

bool credential_register_file(const char *relative_path,
                              const char *container_base)
{
    if (!relative_path || !relative_path[0])
        return false;

    const char *resolved = resolve_host_path(relative_path);
    if (!resolved) {
        return false;  /* Path invalid, traversal, or doesn't exist */
    }

    /* Verify it's a real file */
    if (!file_exists(resolved)) {
        return false;
    }

    if (!container_base || !container_base[0])
        container_base = CREDENTIAL_DEFAULT_BASE;

    /* Build container path */
    char container_path[CREDENTIAL_MAX_PATH];
    snprintf(container_path, sizeof(container_path),
             "%s/%s", container_base, relative_path);

    /* Check if already registered */
    for (int i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].container_path, container_path) == 0)
            return true;
    }

    if (g_registry_count >= CREDENTIAL_MAX_FILES)
        return false;

    snprintf(g_registry[g_registry_count].host_path,
             sizeof(g_registry[g_registry_count].host_path),
             "%s", resolved);
    snprintf(g_registry[g_registry_count].container_path,
             sizeof(g_registry[g_registry_count].container_path),
             "%s", container_path);
    g_registry_count++;
    return true;
}

int credential_register_files(const char **paths, int count,
                              const char *container_base,
                              char **missing_out, int missing_cap)
{
    if (!paths || count <= 0) return 0;

    int missing_count = 0;
    for (int i = 0; i < count; i++) {
        if (!credential_register_file(paths[i], container_base)) {
            if (missing_out && missing_count < missing_cap) {
                missing_out[missing_count] = strdup(paths[i]);
            }
            missing_count++;
        }
    }
    return missing_count;
}

/* ─── Implementation: Query ──────────────────────────────────────────── */

int credential_get_mounts(credential_mount_t *mounts, int cap)
{
    if (!mounts || cap <= 0) return 0;

    int written = 0;
    for (int i = 0; i < g_registry_count && written < cap; i++) {
        /* Re-check existence */
        if (file_exists(g_registry[i].host_path)) {
            memcpy(&mounts[written], &g_registry[i], sizeof(credential_mount_t));
            written++;
        }
    }
    return written;
}

/* ─── Implementation: Skills directory ───────────────────────────────── */

int credential_get_skills_mount(const char *container_base,
                                credential_mount_t *mounts, int cap)
{
    if (!mounts || cap <= 0) return 0;

    const char *home = credential_get_home();
    char skills_dir[CREDENTIAL_MAX_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.hermes/skills", home);

    if (!dir_exists(skills_dir))
        return 0;

    if (!container_base || !container_base[0])
        container_base = CREDENTIAL_DEFAULT_BASE;

    /* Check for symlinks — if found, use safe copy */
    const char *mount_path;
    char safe_path[CREDENTIAL_MAX_PATH];

    if (has_symlinks(skills_dir)) {
        /* Create a symlink-safe copy */
        if (g_safe_tempdir[0] == '\0') {
            snprintf(g_safe_tempdir, sizeof(g_safe_tempdir),
                     "/tmp/hermes-credential-skills-XXXXXX");
            if (!mkdtemp(g_safe_tempdir)) {
                return 0;
            }
        }
        snprintf(safe_path, sizeof(safe_path), "%s/skills", g_safe_tempdir);
        /* Only create if not already done */
        if (!dir_exists(safe_path)) {
            mkdir(safe_path, 0755);
            if (!copy_dir_tree(skills_dir, safe_path)) {
                return 0;
            }
        }
        mount_path = safe_path;
    } else {
        mount_path = skills_dir;
    }

    snprintf(mounts[0].host_path, sizeof(mounts[0].host_path),
             "%s", mount_path);
    snprintf(mounts[0].container_path, sizeof(mounts[0].container_path),
             "%s/skills", container_base);
    return 1;
}

/* ─── Directory traversal helper (collect-first) ──────────────── */

/* Recursive helper: collect regular files into mounts array.
 * Returns number of files written.
 * dir_path: current directory to scan.
 * cont_prefix: container path prefix for this directory level.
 * mounts/cap/written: in/out buffer state.
 */
static int iter_dir_files(const char *dir_path, const char *cont_prefix,
                          credential_mount_t *mounts, int cap, int *written)
{
    if (*written >= cap) return 0;

    DIR *sd = opendir(dir_path);
    if (!sd) return 0;

    struct dirent *entry;
    while ((entry = readdir(sd)) != NULL && *written < cap) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full[CREDENTIAL_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (lstat(full, &st) != 0) continue;

        if (S_ISLNK(st.st_mode))
            continue;

        if (S_ISDIR(st.st_mode)) {
            char child_prefix[CREDENTIAL_MAX_PATH];
            snprintf(child_prefix, sizeof(child_prefix), "%s/%s", cont_prefix, entry->d_name);
            iter_dir_files(full, child_prefix, mounts, cap, written);
        } else if (S_ISREG(st.st_mode)) {
            snprintf(mounts[*written].host_path,
                     sizeof(mounts[*written].host_path), "%s", full);
            snprintf(mounts[*written].container_path,
                     sizeof(mounts[*written].container_path),
                     "%s/%s", cont_prefix, entry->d_name);
            (*written)++;
        }
    }
    closedir(sd);
    return *written;
}

int credential_iter_skills_files(const char *container_base,
                                 credential_mount_t *mounts, int cap)
{
    if (!mounts || cap <= 0) return 0;

    const char *home = credential_get_home();
    char skills_dir[CREDENTIAL_MAX_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.hermes/skills", home);

    if (!dir_exists(skills_dir))
        return 0;

    if (!container_base || !container_base[0])
        container_base = CREDENTIAL_DEFAULT_BASE;

    char cont_prefix[CREDENTIAL_MAX_PATH];
    snprintf(cont_prefix, sizeof(cont_prefix), "%s/skills", container_base);

    int written = 0;
    iter_dir_files(skills_dir, cont_prefix, mounts, cap, &written);
    return written;
}

/* ─── Implementation: Cache directories ──────────────────────────────── */

typedef struct {
    const char *subpath;
    const char *old_name;
} cache_dir_t;

static const cache_dir_t CACHE_DIRS[] = {
    {"cache/documents",    "document_cache"},
    {"cache/images",       "image_cache"},
    {"cache/audio",        "audio_cache"},
    {"cache/screenshots",  "browser_screenshots"},
};

/* Resolve a cache dir path. Returns pointer to static buffer. */
static const char *get_cache_dir_path(const cache_dir_t *cd)
{
    static char buf[CREDENTIAL_MAX_PATH];
    (void)cd->old_name; /* placeholder for future backward compat */

    const char *home = credential_get_home();
    snprintf(buf, sizeof(buf), "%s/.hermes/%s", home, cd->subpath);
    return buf;
}

int credential_get_cache_mounts(const char *container_base,
                                credential_mount_t *mounts, int cap)
{
    if (!mounts || cap <= 0) return 0;

    if (!container_base || !container_base[0])
        container_base = CREDENTIAL_DEFAULT_BASE;

    int written = 0;
    int count = sizeof(CACHE_DIRS) / sizeof(CACHE_DIRS[0]);

    for (int i = 0; i < count && written < cap; i++) {
        const char *host_dir = get_cache_dir_path(&CACHE_DIRS[i]);
        if (dir_exists(host_dir)) {
            snprintf(mounts[written].host_path,
                     sizeof(mounts[written].host_path), "%s", host_dir);
            snprintf(mounts[written].container_path,
                     sizeof(mounts[written].container_path),
                     "%s/%s", container_base, CACHE_DIRS[i].subpath);
            written++;
        }
    }
    return written;
}

char *credential_to_visible_cache_path(const char *host_path,
                                       const char *container_base)
{
    if (!host_path || !host_path[0])
        return NULL;

    if (!container_base || !container_base[0])
        container_base = CREDENTIAL_DEFAULT_BASE;

    credential_mount_t mounts[CREDENTIAL_CACHE_DIRS];
    int count = credential_get_cache_mounts(container_base, mounts,
                                            CREDENTIAL_CACHE_DIRS);

    for (int i = 0; i < count; i++) {
        size_t host_len = strlen(mounts[i].host_path);
        if (strncmp(host_path, mounts[i].host_path, host_len) == 0) {
            /* Path is under this cache dir */
            if (host_path[host_len] == '/' || host_path[host_len] == '\0') {
                /* Compute relative suffix */
                const char *suffix = host_path + host_len;
                char result[CREDENTIAL_MAX_PATH];
                snprintf(result, sizeof(result), "%s%s",
                         mounts[i].container_path, suffix);
                return strdup(result);
            }
        }
    }
    return NULL;
}

int credential_iter_cache_files(const char *container_base,
                                credential_mount_t *mounts, int cap)
{
    if (!mounts || cap <= 0) return 0;

    if (!container_base || !container_base[0])
        container_base = CREDENTIAL_DEFAULT_BASE;

    int written = 0;
    int count = sizeof(CACHE_DIRS) / sizeof(CACHE_DIRS[0]);

    for (int d = 0; d < count && written < cap; d++) {
        const char *host_dir = get_cache_dir_path(&CACHE_DIRS[d]);
        if (!dir_exists(host_dir))
            continue;

        char container_root[CREDENTIAL_MAX_PATH];
        snprintf(container_root, sizeof(container_root),
                 "%s/%s", container_base, CACHE_DIRS[d].subpath);

        iter_dir_files(host_dir, container_root, mounts, cap, &written);
    }
    return written;
}

/* ─── Implementation: Lifecycle ──────────────────────────────────────── */

void credential_clear(void)
{
    g_registry_count = 0;
}

void credential_shutdown(void)
{
    credential_clear();
    if (g_safe_tempdir[0] != '\0') {
        /* Remove the safe temp dir recursively */
        char cmd[CREDENTIAL_MAX_PATH + 32];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", g_safe_tempdir);
        system(cmd);
        g_safe_tempdir[0] = '\0';
    }
}
