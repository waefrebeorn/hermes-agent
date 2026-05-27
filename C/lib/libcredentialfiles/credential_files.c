/*
 * credential_files.c — File passthrough registry for remote terminal backends.
 * Port of Python tools/credential_files.py.
 *
 * Remote backends (Docker, Modal, SSH) create sandboxes with no host files.
 * This module ensures that credential files, skill directories, and host-side
 * cache directories are mounted or synced into those sandboxes.
 */

#include "credential_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ── Forward declarations for internal helpers ── */
static bool _path_is_absolute(const char *path);
static bool _contains_path_traversal(const char *path);
static bool _file_exists(const char *path);
static char *_str_trim(char *s);
static void _normalize_path(char *out, size_t out_size, const char *base, const char *rel);

/* ── Singleton registry ── */
static credfiles_registry_t g_registry = { .count = 0 };

credfiles_registry_t *credfiles_get_registry(void)
{
    return &g_registry;
}

/* ── Internal helpers ── */

static bool _path_is_absolute(const char *path)
{
    return path && path[0] == '/';
}

static bool _contains_path_traversal(const char *path)
{
    if (!path) return false;
    /* Check for ".." as a path component */
    const char *p = path;
    while ((p = strstr(p, "..")) != NULL) {
        /* Must be a standalone path component "/../" or starts with "../" or ends with "/.." */
        if ((p == path || p[-1] == '/') &&
            (p[2] == '\0' || p[2] == '/')) {
            return true;
        }
        p += 2;
    }
    return false;
}

static bool _file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static bool _dir_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static char *_str_trim(char *s)
{
    if (!s) return NULL;
    /* Skip leading whitespace */
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    if (*s == '\0') return s;
    /* Trim trailing whitespace */
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        end--;
    *(end + 1) = '\0';
    return s;
}

static void _normalize_path(char *out, size_t out_size,
                            const char *base, const char *rel)
{
    /* Remove trailing slashes from base */
    size_t base_len = strlen(base);
    while (base_len > 0 && base[base_len - 1] == '/')
        base_len--;

    /* Remove leading slashes from rel */
    while (*rel == '/')
        rel++;

    int written = snprintf(out, out_size, "%.*s/%s", (int)base_len, base, rel);
    if (written < 0 || (size_t)written >= out_size) {
        /* Truncation: still null-terminated */
        out[out_size - 1] = '\0';
    }
}

/* ── Public API ── */

bool credfiles_register_file(const char *relative_path,
                              const char *container_base)
{
    if (!relative_path || !*relative_path)
        return false;

    const char *base = container_base ? container_base : "/root/.hermes";

    /* Reject absolute paths */
    if (_path_is_absolute(relative_path))
        return false;

    /* Reject path traversal */
    if (_contains_path_traversal(relative_path))
        return false;

    /* Resolve HERMES_HOME */
    const char *hermes_home = credfiles_get_hermes_home();
    if (!hermes_home)
        return false;

    /* Build host path: HERMES_HOME/relative_path */
    char host_path[CREDFILES_MAX_PATH];
    _normalize_path(host_path, sizeof(host_path), hermes_home, relative_path);

    /* Check file exists */
    if (!_file_exists(host_path))
        return false;

    /* Build container path */
    char container_path[CREDFILES_MAX_PATH];
    _normalize_path(container_path, sizeof(container_path), base, relative_path);

    /* Store in registry */
    credfiles_registry_t *r = credfiles_get_registry();
    if (r->count >= CREDFILES_MAX_PAIRS)
        return false;

    strncpy(r->pairs[r->count].host_path, host_path, CREDFILES_MAX_PATH - 1);
    r->pairs[r->count].host_path[CREDFILES_MAX_PATH - 1] = '\0';
    strncpy(r->pairs[r->count].container_path, container_path, CREDFILES_MAX_PATH - 1);
    r->pairs[r->count].container_path[CREDFILES_MAX_PATH - 1] = '\0';
    r->count++;

    return true;
}

int credfiles_register_files(const char *entries[], int entry_count,
                              const char *container_base,
                              char missing[][CREDFILES_MAX_PATH],
                              int missing_size)
{
    int missing_count = 0;
    for (int i = 0; i < entry_count; i++) {
        if (!entries[i] || !*entries[i])
            continue;

        /* Each entry is a simple path string (no dict support in C version) */
        if (!credfiles_register_file(entries[i], container_base)) {
            if (missing && missing_count < missing_size) {
                strncpy(missing[missing_count], entries[i], CREDFILES_MAX_PATH - 1);
                missing[missing_count][CREDFILES_MAX_PATH - 1] = '\0';
                missing_count++;
            }
        }
    }
    return missing_count;
}

void credfiles_clear(void)
{
    g_registry.count = 0;
}

credfiles_path_list_t credfiles_get_mounts(void)
{
    credfiles_path_list_t result;
    result.count = 0;

    /* Start with skill-registered files */
    credfiles_registry_t *r = credfiles_get_registry();
    for (int i = 0; i < r->count; i++) {
        /* Re-check existence (file may have been deleted since registration) */
        if (_file_exists(r->pairs[i].host_path)) {
            result.entries[result.count] = r->pairs[i];
            result.count++;
        }
    }

    /* Add config-based files that aren't already registered */
    credfiles_path_list_t config_entries = credfiles_load_config_mounts();
    for (int i = 0; i < config_entries.count && result.count < CREDFILES_MAX_PAIRS; i++) {
        /* Check for duplicate container path */
        bool found = false;
        for (int j = 0; j < result.count; j++) {
            if (strcmp(result.entries[j].container_path,
                       config_entries.entries[i].container_path) == 0) {
                found = true;
                break;
            }
        }
        if (!found && _file_exists(config_entries.entries[i].host_path)) {
            result.entries[result.count] = config_entries.entries[i];
            result.count++;
        }
    }

    return result;
}

credfiles_path_list_t credfiles_load_config_mounts(void)
{
    credfiles_path_list_t result;
    result.count = 0;

    /* In C, config is loaded from YAML at agent startup and stored as
     * environment variables or config structs. For now, check HERMES_CREDENTIAL_FILES
     * env var as a fallback (colon-separated relative paths). */
    const char *env = getenv("HERMES_CREDENTIAL_FILES");
    if (!env || !*env)
        return result;

    const char *hermes_home = credfiles_get_hermes_home();
    if (!hermes_home)
        return result;

    /* Parse colon-separated paths */
    char buf[CREDFILES_MAX_PATH];
    strncpy(buf, env, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ":");
    while (token && result.count < CREDFILES_MAX_ENTRIES) {
        char *trimmed = _str_trim(token);
        if (*trimmed && !_path_is_absolute(trimmed) && !_contains_path_traversal(trimmed)) {
            char host_path[CREDFILES_MAX_PATH];
            _normalize_path(host_path, sizeof(host_path), hermes_home, trimmed);

            if (_file_exists(host_path)) {
                char container_path[CREDFILES_MAX_PATH];
                _normalize_path(container_path, sizeof(container_path), "/root/.hermes", trimmed);

                strncpy(result.entries[result.count].host_path, host_path, CREDFILES_MAX_PATH - 1);
                result.entries[result.count].host_path[CREDFILES_MAX_PATH - 1] = '\0';
                strncpy(result.entries[result.count].container_path, container_path, CREDFILES_MAX_PATH - 1);
                result.entries[result.count].container_path[CREDFILES_MAX_PATH - 1] = '\0';
                result.count++;
            }
        }
        token = strtok(NULL, ":");
    }

    return result;
}

credfiles_path_list_t credfiles_get_skills_mount(const char *container_base)
{
    credfiles_path_list_t result;
    result.count = 0;

    const char *base = container_base ? container_base : "/root/.hermes";
    const char *hermes_home = credfiles_get_hermes_home();
    if (!hermes_home)
        return result;

    /* Build skills dir path */
    char skills_dir[CREDFILES_MAX_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", hermes_home);

    if (!_dir_exists(skills_dir))
        return result;

    char container_path[CREDFILES_MAX_PATH];
    _normalize_path(container_path, sizeof(container_path), base, "skills");

    strncpy(result.entries[0].host_path, skills_dir, CREDFILES_MAX_PATH - 1);
    result.entries[0].host_path[CREDFILES_MAX_PATH - 1] = '\0';
    strncpy(result.entries[0].container_path, container_path, CREDFILES_MAX_PATH - 1);
    result.entries[0].container_path[CREDFILES_MAX_PATH - 1] = '\0';
    result.count = 1;

    return result;
}

credfiles_path_list_t credfiles_get_cache_mounts(const char *container_base)
{
    credfiles_path_list_t result;
    result.count = 0;

    const char *base = container_base ? container_base : "/root/.hermes";
    const char *hermes_home = credfiles_get_hermes_home();
    if (!hermes_home)
        return result;

    /* Cache subdirectories (matching Python's _CACHE_DIRS) */
    static const char *cache_dirs[] = {
        "cache/documents",
        "cache/images",
        "cache/audio",
        "cache/screenshots"
    };
    int num_cache_dirs = sizeof(cache_dirs) / sizeof(cache_dirs[0]);

    for (int i = 0; i < num_cache_dirs && result.count < CREDFILES_MAX_CACHE_DIRS; i++) {
        char host_dir[CREDFILES_MAX_PATH];
        snprintf(host_dir, sizeof(host_dir), "%s/%s", hermes_home, cache_dirs[i]);

        if (_dir_exists(host_dir)) {
            char container_path[CREDFILES_MAX_PATH];
            _normalize_path(container_path, sizeof(container_path), base, cache_dirs[i]);

            strncpy(result.entries[result.count].host_path, host_dir, CREDFILES_MAX_PATH - 1);
            result.entries[result.count].host_path[CREDFILES_MAX_PATH - 1] = '\0';
            strncpy(result.entries[result.count].container_path, container_path, CREDFILES_MAX_PATH - 1);
            result.entries[result.count].container_path[CREDFILES_MAX_PATH - 1] = '\0';
            result.count++;
        }
    }

    return result;
}

const char *credfiles_get_hermes_home(void)
{
    const char *home = getenv("HERMES_HOME");
    if (home && *home)
        return home;

    /* Fallback: ~/.hermes */
    home = getenv("HOME");
    if (!home)
        return NULL;

    static char buf[CREDFILES_MAX_PATH];
    snprintf(buf, sizeof(buf), "%s/.hermes", home);
    return buf;
}
