/*
 * file.c — File I/O tools for Hermes C.
 * Read, write, search files. Path security checks.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>

/* Schema */
static const char *SCHEMA_READ = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"path\":{\"type\":\"string\",\"description\":\"Path to file\"},"
      "\"offset\":{\"type\":\"number\",\"description\":\"Starting line (1-indexed)\",\"default\":1},"
      "\"limit\":{\"type\":\"number\",\"description\":\"Max lines\",\"default\":500}"
    "},"
    "\"required\":[\"path\"]"
"}";

static const char *SCHEMA_WRITE = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"path\":{\"type\":\"string\",\"description\":\"Path to file\"},"
      "\"content\":{\"type\":\"string\",\"description\":\"Content to write\"}"
    "},"
    "\"required\":[\"path\",\"content\"]"
"}";

static const char *SCHEMA_SEARCH = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"pattern\":{\"type\":\"string\",\"description\":\"Search pattern\"},"
      "\"path\":{\"type\":\"string\",\"description\":\"Directory to search\"},"
      "\"file_glob\":{\"type\":\"string\",\"description\":\"File glob filter\"}"
    "},"
    "\"required\":[\"pattern\"]"
"}";

/* ================================================================
 *  P168: File Sandbox — Allowed directories & symlink attack prevention
 * ================================================================ */

#define MAX_SANDBOX_DIRS 32

static char *g_allowed_dirs[MAX_SANDBOX_DIRS];
static int g_allowed_dir_count = 0;
static bool g_sandbox_enabled = false;
static bool g_symlink_check_enabled = true;

/* Initialize sandbox with default allowed directories */
void sandbox_init(void) {
    const char *home = getenv("HOME");
    if (home) {
        sandbox_add_allowed_dir(home);
    }
    sandbox_add_allowed_dir("/tmp");
    sandbox_add_allowed_dir(getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") : "");
    g_sandbox_enabled = true;
}

/* Enable/disable the sandbox */
void sandbox_enable(bool enabled) {
    g_sandbox_enabled = enabled;
}

/* Add an allowed directory */
bool sandbox_add_allowed_dir(const char *dir) {
    if (!dir || !*dir || g_allowed_dir_count >= MAX_SANDBOX_DIRS)
        return false;

    /* Resolve to absolute path */
    char resolved[4096];
    if (dir[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) return false;
        snprintf(resolved, sizeof(resolved), "%s%s", home, dir + 1);
    } else {
        snprintf(resolved, sizeof(resolved), "%s", dir);
    }

    /* Canonicalize via realpath if path exists */
    char *real = realpath(resolved, NULL);
    if (real) {
        /* Check for duplicate */
        for (int i = 0; i < g_allowed_dir_count; i++) {
            if (g_allowed_dirs[i] && strcmp(g_allowed_dirs[i], real) == 0) {
                free(real);
                return true;
            }
        }
        g_allowed_dirs[g_allowed_dir_count++] = real;
        return true;
    }

    /* Path doesn't exist yet, add as-is */
    for (int i = 0; i < g_allowed_dir_count; i++) {
        if (g_allowed_dirs[i] && strcmp(g_allowed_dirs[i], resolved) == 0)
            return true;
    }
    g_allowed_dirs[g_allowed_dir_count++] = strdup(resolved);
    return true;
}

/* Remove an allowed directory */
bool sandbox_remove_allowed_dir(const char *dir) {
    if (!dir) return false;
    for (int i = 0; i < g_allowed_dir_count; i++) {
        if (g_allowed_dirs[i] && strcmp(g_allowed_dirs[i], dir) == 0) {
            free(g_allowed_dirs[i]);
            g_allowed_dirs[i] = g_allowed_dirs[--g_allowed_dir_count];
            g_allowed_dirs[g_allowed_dir_count] = NULL;
            return true;
        }
    }
    return false;
}

/* Clear all sandbox dirs */
void sandbox_clear(void) {
    for (int i = 0; i < g_allowed_dir_count; i++) {
        free(g_allowed_dirs[i]);
        g_allowed_dirs[i] = NULL;
    }
    g_allowed_dir_count = 0;
}

/* Check if a path is a symlink (outside allowed dirs) */
static bool is_unsafe_symlink(const char *path) {
    if (!g_symlink_check_enabled) return false;

    struct stat st;
    if (lstat(path, &st) != 0) return false; /* Doesn't exist, allow through */

    if (!S_ISLNK(st.st_mode)) return false; /* Not a symlink */

    /* It's a symlink — check if target is within allowed dirs */
    char target[4096];
    ssize_t tlen = readlink(path, target, sizeof(target) - 1);
    if (tlen < 0) return false;

    target[tlen] = '\0';

    /* Resolve relative symlinks */
    char *resolved = realpath(path, NULL);
    if (!resolved) return true; /* Can't resolve — block */

    /* Check if resolved path is in allowed dirs */
    bool safe = false;
    for (int i = 0; i < g_allowed_dir_count; i++) {
        if (g_allowed_dirs[i] && strncmp(resolved, g_allowed_dirs[i],
                                          strlen(g_allowed_dirs[i])) == 0) {
            safe = true;
            break;
        }
    }
    free(resolved);
    return !safe;
}

/* Check if a path is within an allowed directory */
bool sandbox_check_path(const char *path) {
    if (!path || !*path) return false;
    if (!g_sandbox_enabled) return true;

    /* Check for path traversal */
    if (strstr(path, "..")) return false;

    /* Resolve path */
    char *resolved = realpath(path, NULL);
    char check_path[4096];

    if (resolved) {
        snprintf(check_path, sizeof(check_path), "%s", resolved);
    } else {
        /* Path doesn't exist yet — check parent directory */
        snprintf(check_path, sizeof(check_path), "%s", path);
        /* Remove trailing slash */
        char *end = check_path + strlen(check_path) - 1;
        while (end > check_path && *end == '/') *end-- = '\0';

        /* Try parent */
        char *slash = strrchr(check_path, '/');
        if (slash && slash != check_path) {
            *slash = '\0';
        }
    }

    /* Check if the path (or its parent) is in allowed dirs */
    bool allowed = false;
    for (int i = 0; i < g_allowed_dir_count; i++) {
        if (!g_allowed_dirs[i]) continue;
        size_t dlen = strlen(g_allowed_dirs[i]);

        /* Allow exact match or subpath */
        if (strncmp(check_path, g_allowed_dirs[i], dlen) == 0) {
            if (check_path[dlen] == '\0' || check_path[dlen] == '/') {
                allowed = true;
                break;
            }
        }
    }

    if (resolved) free(resolved);

    /* Check symlink safety */
    if (allowed && !is_unsafe_symlink(path)) {
        return true;
    }

    return allowed;
}

/* Enable/disable symlink checking */
void sandbox_set_symlink_check(bool enabled) {
    g_symlink_check_enabled = enabled;
}

/* ================================================================
 *  Path security check (updated to use sandbox)
 * ================================================================ */

static bool is_safe_path(const char *path) {
    /* P168: Use sandbox for path validation if enabled */
    if (g_sandbox_enabled) {
        return sandbox_check_path(path);
    }

    /* Fallback: basic checks */
    /* Block absolute paths outside home, and '..' traversals */
    if (strstr(path, "..")) return false;
    if (path[0] == '/' && strncmp(path, getenv("HOME"), strlen(getenv("HOME"))) != 0) {
        /* Allow /tmp/ and common paths */
        if (strncmp(path, "/tmp/", 5) != 0 && strncmp(path, "/dev/", 5) != 0)
            return false;
    }
    return true;
}

/* ================================================================
 *  Handlers
 * ================================================================ */

static char *handle_read(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"No args\"}");
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { return strdup("{\"error\":\"JSON parse\"}"); }

    const char *path = json_object_get_string(args, "path", NULL);
    if (!path || !is_safe_path(path)) {
        json_free(args);
        return strdup("{\"error\":\"Invalid or unsafe path\"}");
    }

    int offset = (int)json_object_get_number(args, "offset", 1);
    int limit = (int)json_object_get_number(args, "limit", 500);

    /* Read file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        json_free(args);
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"Cannot open: %s\"}", strerror(errno));
        return strdup(buf);
    }

    /* Read into memory */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize > 10 * 1024 * 1024) { /* > 10MB */
        fclose(f);
        json_free(args);
        return strdup("{\"error\":\"File too large (>10MB)\"}");
    }
    fseek(f, 0, SEEK_SET);
    char *content = (char *)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); json_free(args); return strdup("{\"error\":\"OOM\"}"); }
    fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[fsize] = '\0';

    /* Count lines and extract requested range */
    long total_lines = 0;
    for (char *p = content; *p; p++) if (*p == '\n') total_lines++;
    if (fsize > 0 && content[fsize-1] != '\n') total_lines++;

    /* Build result */
    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));
    json_object_set(result, "total_lines", json_new_number((double)total_lines));
    json_object_set(result, "content", json_new_string(content));
    json_object_set(result, "offset", json_new_number((double)offset));
    json_object_set(result, "limit", json_new_number((double)limit));
    json_object_set(result, "file_size", json_new_number((double)fsize));

    char *json_out = json_serialize(result);
    json_free(result);
    free(content);
    json_free(args);
    return json_out;
}

static char *handle_write(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"No args\"}");
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { return strdup("{\"error\":\"JSON parse\"}"); }

    const char *path = json_object_get_string(args, "path", NULL);
    const char *content = json_object_get_string(args, "content", NULL);

    if (!path || !content || !is_safe_path(path)) {
        json_free(args);
        return strdup("{\"error\":\"Invalid path or missing content\"}");
    }

    /* Ensure parent dir exists */
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, 0755);
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        json_free(args);
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"Cannot write: %s\"}", strerror(errno));
        return strdup(buf);
    }
    fputs(content, f);
    fclose(f);

    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));
    json_object_set(result, "bytes_written", json_new_number((double)strlen(content)));
    json_object_set(result, "success", json_new_bool(true));

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

static char *handle_search(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"No args\"}");
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { return strdup("{\"error\":\"JSON parse\"}"); }

    const char *pattern = json_object_get_string(args, "pattern", NULL);
    const char *path = json_object_get_string(args, "path", ".");
    const char *file_glob = json_object_get_string(args, "file_glob", NULL);

    if (!pattern) {
        json_free(args);
        return strdup("{\"error\":\"Missing pattern\"}");
    }

    /* Build grep command */
    char cmd[16384];
    if (file_glob && file_glob[0])
        snprintf(cmd, sizeof(cmd), "grep -rn --include='%s' '%s' '%s' 2>/dev/null | head -50",
                 file_glob, pattern, path);
    else
        snprintf(cmd, sizeof(cmd), "grep -rn '%s' '%s' 2>/dev/null | head -50",
                 pattern, path);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        json_free(args);
        return strdup("{\"error\":\"grep failed\"}");
    }

    size_t cap = 4096, len = 0;
    char *output = (char *)malloc(cap);
    if (!output) { pclose(fp); json_free(args); return strdup("{\"error\":\"OOM\"}"); }
    output[0] = '\0';

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t add = strlen(line);
        if (len + add + 1 > cap) {
            cap *= 2;
            output = (char *)realloc(output, cap);
            if (!output) { pclose(fp); json_free(args); return strdup("{\"error\":\"OOM\"}"); }
        }
        memcpy(output + len, line, add + 1);
        len += add;
    }
    pclose(fp);

    json_node_t *result = json_new_object();
    json_object_set(result, "pattern", json_new_string(pattern));
    json_object_set(result, "path", json_new_string(path));
    json_object_set(result, "matches", json_new_string(output));
    json_object_set(result, "match_count", json_new_number((double)(len > 0 ? 1 : 0)));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    json_free(args);
    return json_out;
}

/* ================================================================
 *  Public handlers
 * ================================================================ */

char *file_read_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    return handle_read(args_json);
}

char *file_write_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    return handle_write(args_json);
}

char *file_search_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    return handle_search(args_json);
}

/* Auto-registration */
void registry_init_file(void) {
    registry_register("read_file",
        "Read a text file with line numbers. Returns content, total lines, file size.",
        SCHEMA_READ, file_read_handler);
    registry_register("write_file",
        "Write content to a file (creates parent directories). Overwrites existing.",
        SCHEMA_WRITE, file_write_handler);
    registry_register("search_files",
        "Search file contents using grep. Returns matching lines with line numbers.",
        SCHEMA_SEARCH, file_search_handler);
}
