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
#include <fnmatch.h>
#include <ctype.h>
#include "difflib.h"

/* WSL path translation — convert Windows to /mnt/ form (static buf, single-thread) */
static const char *wsl_translate_path(const char *path) {
    static char buf[4096];
    if (!path) return NULL;
    if (path[0] == '/') return NULL;
    if (strlen(path) >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        char drive = tolower((unsigned char)path[0]);
        if (drive >= 'a' && drive <= 'z') {
            snprintf(buf, sizeof(buf), "/mnt/%c%s", drive, path + 2);
            for (char *p = buf; *p; p++) if (*p == '\\') *p = '/';
            return buf;
        }
    }
    return NULL;
}

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

    static const char *SCHEMA_DIFF = "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"path_a\":{\"type\":\"string\",\"description\":\"First file path\"},"
          "\"path_b\":{\"type\":\"string\",\"description\":\"Second file path\"},"
          "\"context_lines\":{\"type\":\"number\",\"description\":\"Context lines (default: 3)\"}"
        "},"
        "\"required\":[\"path_a\",\"path_b\"]"
    "}";

    static const char *SCHEMA_PERMS = "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"path\":{\"type\":\"string\",\"description\":\"File path\"},"
          "\"mode\":{\"type\":\"string\",\"description\":\"New permissions (e.g. 644, 755). Omit to just stat.\"}"
        "},"
        "\"required\":[\"path\"]"
    "}";

    static const char *SCHEMA_HEX = "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"path\":{\"type\":\"string\",\"description\":\"File path\"},"
          "\"offset\":{\"type\":\"number\",\"description\":\"Byte offset to start (default: 0)\"},"
          "\"limit\":{\"type\":\"number\",\"description\":\"Max bytes to show (default: 256, max: 4096)\"}"
        "},"
        "\"required\":[\"path\"]"
    "}";

    static const char *SCHEMA_SYNTAX = "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"path\":{\"type\":\"string\",\"description\":\"File path to check\"},"
          "\"type\":{\"type\":\"string\",\"description\":\"File type override: python, sh, json, yaml, c. Auto-detected from extension if omitted.\"}"
        "},"
        "\"required\":[\"path\"]"
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

    /* S04: Null byte injection bypass check */
    if (path && strlen(path) != strnlen(path, 4096)) return false;

    /* S04: URL-encoded traversal detection (before realpath resolves it) */
    const char *encoded_checks[] = {
        "%2e",  /* %2e = '.' in URL encoding */
        "%2E",  /* uppercase variant */
        "..",   /* literal traversal */
        NULL
    };
    bool has_dotdot = false;
    for (int chk = 0; encoded_checks[chk]; chk++) {
        const char *p = path;
        while ((p = strstr(p, encoded_checks[chk])) != NULL) {
            /* For literal '..', verify it's a path component boundary */
            if (encoded_checks[chk][0] == '.' && encoded_checks[chk][1] == '.') {
                if ((p == path || p[-1] == '/') &&
                    (p[2] == '/' || p[2] == '\0'))
                    has_dotdot = true;
            } else {
                has_dotdot = true;
            }
            p++;
        }
    }
    if (has_dotdot) return false;

    /* S04: Resolve real path for symlink bypass detection */
    char *resolved = realpath(path, NULL);
    if (resolved) {
        /* Resolved path must be under HOME, /tmp/, or /dev/ */
        const char *home = getenv("HOME");
        bool under_home = home && strncmp(resolved, home, strlen(home)) == 0;
        bool under_tmp = strncmp(resolved, "/tmp/", 5) == 0;
        bool under_dev = strncmp(resolved, "/dev/", 5) == 0;
        bool under_proc_self = strncmp(resolved, "/proc/self/", 11) == 0;
        free(resolved);
        if (!under_home && !under_tmp && !under_dev && !under_proc_self)
            return false;
        return true;  /* resolved path passes all checks */
    }

    /* Path doesn't exist yet (write/create) — fallback to static checks */
    /* Block absolute paths outside home */
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
    {
        const char *wsl = wsl_translate_path(path);
        if (wsl) path = wsl;
    }
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
    if (fread(content, 1, (size_t)fsize, f) == 0 && fsize > 0) { /* suppress warn_unused_result */ }
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

    /* Atomic write: write to temp file, then rename */
    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.XXXXXX", path);
    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        json_free(args);
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"Cannot create temp file: %s\"}", strerror(errno));
        return strdup(buf);
    }

    /* Write content to temp file */
    ssize_t written = write(fd, content, strlen(content));
    if (written < 0 || (size_t)written != strlen(content)) {
        close(fd);
        unlink(tmp_path);
        json_free(args);
        return strdup("{\"error\":\"Write to temp file failed\"}");
    }

    /* Flush and sync to ensure data is on disk */
    fsync(fd);
    close(fd);

    /* Atomic rename — replaces target atomically on POSIX */
    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        json_free(args);
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"Atomic rename failed: %s\"}", strerror(errno));
        return strdup(buf);
    }

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

    /* Build grep command — F14: with glob support via find for ** patterns */
    char cmd[16384];
    if (file_glob && file_glob[0]) {
        /* F14: glob with path prefix support (e.g. src/glob/foo*.c) */
        char find_glob[1024];
        const char *last_slash = strrchr(file_glob, '/');
        if (last_slash) {
            /* Glob has path component: extract dir + filename pattern */
            size_t dir_len = (size_t)(last_slash - file_glob);
            char glob_dir[1024];
            snprintf(glob_dir, sizeof(glob_dir), "%.*s", (int)dir_len, file_glob);
            snprintf(find_glob, sizeof(find_glob), "%s", last_slash + 1);

            /* Resolve glob_dir relative to search path */
            char resolved_dir[2048];
            snprintf(resolved_dir, sizeof(resolved_dir), "%s/%s", path, glob_dir);
            /* Remove trailing / and * characters */
            char *pp = resolved_dir + strlen(resolved_dir) - 1;
            while (pp > resolved_dir && (*pp == '/' || *pp == '*')) pp--;
            pp[1] = '\0';

            snprintf(cmd, sizeof(cmd),
                     "find '%s' -name '%s' -type f 2>/dev/null | "
                     "xargs -r grep -rn '%s' 2>/dev/null | head -50",
                     resolved_dir, find_glob, pattern);
        } else {
            /* Simple glob pattern — find recurses naturally */
            snprintf(cmd, sizeof(cmd),
                     "find '%s' -name '%s' -type f 2>/dev/null | "
                     "xargs -r grep -rn '%s' 2>/dev/null | head -50",
                     path, file_glob, pattern);
        }
    } else
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

/* File diff — unified diff between two files */
static char *handle_diff(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"No args\"}");
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *path_a = json_object_get_string(args, "path_a", NULL);
    const char *path_b = json_object_get_string(args, "path_b", NULL);
    double ctx = json_object_get_number(args, "context_lines", -1.0);
    int context_lines = (ctx >= 0) ? (int)ctx : 3;

    if (!path_a || !path_b) {
        json_free(args);
        return strdup("{\"error\":\"Missing path_a or path_b\"}");
    }

    /* Read file A */
    FILE *fa = fopen(path_a, "rb");
    if (!fa) {
        json_free(args);
        char buf[1024];
        snprintf(buf, sizeof(buf), "{\"error\":\"Cannot open %s: %s\"}", path_a, strerror(errno));
        return strdup(buf);
    }
    fseek(fa, 0, SEEK_END);
    long size_a = ftell(fa);
    if (size_a > 10485760) { fclose(fa); json_free(args); return strdup("{\"error\":\"File too large (>10MB)\"}"); }
    rewind(fa);
    char *content_a = (char *)malloc(size_a + 1);
    if (!content_a) { fclose(fa); json_free(args); return strdup("{\"error\":\"OOM\"}"); }
    size_t read_a = fread(content_a, 1, size_a, fa);
    content_a[read_a] = '\0';
    fclose(fa);

    /* Read file B */
    FILE *fb = fopen(path_b, "rb");
    if (!fb) {
        free(content_a);
        json_free(args);
        char buf[1024];
        snprintf(buf, sizeof(buf), "{\"error\":\"Cannot open %s: %s\"}", path_b, strerror(errno));
        return strdup(buf);
    }
    fseek(fb, 0, SEEK_END);
    long size_b = ftell(fb);
    if (size_b > 10485760) { fclose(fb); free(content_a); json_free(args); return strdup("{\"error\":\"File too large (>10MB)\"}"); }
    rewind(fb);
    char *content_b = (char *)malloc(size_b + 1);
    if (!content_b) { fclose(fb); free(content_a); json_free(args); return strdup("{\"error\":\"OOM\"}"); }
    size_t read_b = fread(content_b, 1, size_b, fb);
    content_b[read_b] = '\0';
    fclose(fb);

    /* Generate diff */
    char *diff_str = difflib_unified_diff(content_a, content_b, context_lines);
    if (!diff_str) {
        free(content_a); free(content_b); json_free(args);
        return strdup("{\"error\":\"Diff generation failed\"}");
    }

    /* Compute similarity ratio */
    double ratio = difflib_ratio(content_a, content_b);

    json_node_t *result = json_new_object();
    json_object_set(result, "path_a", json_new_string(path_a));
    json_object_set(result, "path_b", json_new_string(path_b));
    json_object_set(result, "diff", json_new_string(diff_str));
    json_object_set(result, "similarity", json_new_number(ratio));
    json_object_set(result, "size_a", json_new_number((double)read_a));
    json_object_set(result, "size_b", json_new_number((double)read_b));

    char *json_out = json_serialize(result);
    free(diff_str);
    free(content_a);
    free(content_b);
    json_free(result);
    json_free(args);
    return json_out;
}

/* File permissions — stat file or change mode */
static char *handle_perms(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"No args\"}");
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *path = json_object_get_string(args, "path", NULL);
    if (!path || !*path) {
        json_free(args);
        return strdup("{\"error\":\"Missing path\"}");
    }

    const char *mode_str = json_object_get_string(args, "mode", "");

    /* If mode provided, try chmod */
    if (mode_str && *mode_str) {
        long mode_val = strtol(mode_str, NULL, 8);
        if (mode_val <= 0) {
            json_free(args);
            return strdup("{\"error\":\"Invalid mode (use octal, e.g. 644)\"}");
        }
        if (chmod(path, (mode_t)mode_val) != 0) {
            json_free(args);
            char buf[512];
            snprintf(buf, sizeof(buf), "{\"error\":\"chmod failed: %s\"}", strerror(errno));
            return strdup(buf);
        }
    }

    /* Stat the file */
    struct stat st;
    if (stat(path, &st) != 0) {
        json_free(args);
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\":\"Cannot stat %s: %s\"}", path, strerror(errno));
        return strdup(buf);
    }

    /* Format mode as octal string */
    char mode_oct[16];
    snprintf(mode_oct, sizeof(mode_oct), "%03o", (unsigned)(st.st_mode & 0777));

    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));
    json_object_set(result, "mode", json_new_string(mode_oct));
    json_object_set(result, "size", json_new_number((double)st.st_size));
    json_object_set(result, "uid", json_new_number((double)st.st_uid));
    json_object_set(result, "gid", json_new_number((double)st.st_gid));
    json_object_set(result, "is_dir", json_new_bool(S_ISDIR(st.st_mode)));
    json_object_set(result, "is_file", json_new_bool(S_ISREG(st.st_mode)));
    json_object_set(result, "is_link", json_new_bool(S_ISLNK(st.st_mode)));

    /* Add human-readable type */
    const char *ftype = "other";
    if (S_ISDIR(st.st_mode)) ftype = "directory";
    else if (S_ISREG(st.st_mode)) ftype = "file";
    else if (S_ISLNK(st.st_mode)) ftype = "symlink";
    else if (S_ISCHR(st.st_mode)) ftype = "char_device";
    else if (S_ISBLK(st.st_mode)) ftype = "block_device";
    else if (S_ISFIFO(st.st_mode)) ftype = "fifo";
    else if (S_ISSOCK(st.st_mode)) ftype = "socket";
    json_object_set(result, "type", json_new_string(ftype));

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

/* Hex view — dump file bytes as hex with ASCII side panel */
static char *handle_hex(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"No args\"}");
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *path = json_object_get_string(args, "path", NULL);
    if (!path || !*path) {
        json_free(args);
        return strdup("{\"error\":\"Missing path\"}");
    }

    double off_val = json_object_get_number(args, "offset", 0.0);
    long start_off = (off_val >= 0) ? (long)off_val : 0;
    double lim_val = json_object_get_number(args, "limit", 256.0);
    long max_bytes = (lim_val > 0) ? (long)lim_val : 256;
    if (max_bytes > 4096) max_bytes = 4096;

    FILE *f = fopen(path, "rb");
    if (!f) {
        json_free(args);
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\":\"Cannot open %s: %s\"}", path, strerror(errno));
        return strdup(buf);
    }

    /* Seek to offset */
    if (start_off > 0 && fseek(f, start_off, SEEK_SET) != 0) {
        fclose(f);
        json_free(args);
        return strdup("{\"error\":\"Seek failed\"}");
    }

    /* Read bytes */
    unsigned char buf[4096];
    long to_read = max_bytes < 4096 ? max_bytes : 4096;
    size_t nread = fread(buf, 1, to_read, f);
    fclose(f);

    if (nread == 0) {
        /* Get file size for reporting */
        FILE *fs = fopen(path, "rb");
        long fsize = 0;
        if (fs) { fseek(fs, 0, SEEK_END); fsize = ftell(fs); fclose(fs); }
        json_free(args);
        json_node_t *r = json_new_object();
        json_object_set(r, "path", json_new_string(path));
        json_object_set(r, "offset", json_new_number((double)start_off));
        json_object_set(r, "bytes_read", json_new_number(0.0));
        json_object_set(r, "file_size", json_new_number((double)fsize));
        json_object_set(r, "hex", json_new_string(""));
        char *out = json_serialize(r);
        json_free(r);
        return out;
    }

    /* Build hex output */
    char hex[16384];
    int hex_len = 0;
    char ascii[17];
    ascii[16] = '\0';

    int written = 0;
    for (size_t i = 0; i < nread; i += 16) {
        /* Offset */
        hex_len += snprintf(hex + hex_len, sizeof(hex) - hex_len,
            "%08lx  ", start_off + i);

        /* Hex bytes */
        int ascii_idx = 0;
        for (int j = 0; j < 16; j++) {
            if (i + j < nread) {
                hex_len += snprintf(hex + hex_len, sizeof(hex) - hex_len,
                    "%02x ", buf[i + j]);
                ascii[ascii_idx++] = (buf[i + j] >= 32 && buf[i + j] < 127) ? buf[i + j] : '.';
            } else {
                hex_len += snprintf(hex + hex_len, sizeof(hex) - hex_len, "   ");
                ascii[ascii_idx++] = ' ';
            }
        }
        ascii[ascii_idx] = '\0';

        /* ASCII panel */
        hex_len += snprintf(hex + hex_len, sizeof(hex) - hex_len, " |%s|\n", ascii);
        written += 16;

        if (hex_len > (int)sizeof(hex) - 256) break;
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));
    json_object_set(result, "offset", json_new_number((double)start_off));
    json_object_set(result, "bytes_read", json_new_number((double)nread));
    json_object_set(result, "hex", json_new_string(hex));

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

/* Syntax check — verify file syntax via external tool */
static char *handle_syntax(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"No args\"}");
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *path = json_object_get_string(args, "path", NULL);
    if (!path || !*path) {
        json_free(args);
        return strdup("{\"error\":\"Missing path\"}");
    }

    const char *type_override = json_object_get_string(args, "type", "");

    /* Detect type from extension */
    const char *ftype = type_override;
    if (!ftype || !*ftype) {
        const char *dot = strrchr(path, '.');
        if (dot) {
            dot++;
            if (strcmp(dot, "py") == 0) ftype = "python";
            else if (strcmp(dot, "sh") == 0) ftype = "sh";
            else if (strcmp(dot, "json") == 0) ftype = "json";
            else if (strcmp(dot, "yaml") == 0 || strcmp(dot, "yml") == 0) ftype = "yaml";
            else if (strcmp(dot, "c") == 0 || strcmp(dot, "h") == 0) ftype = "c";
        }
    }

    if (!ftype || !*ftype) {
        json_free(args);
        return strdup("{\"error\":\"Could not detect file type. Specify 'type' parameter.\"}");
    }

    /* Build the appropriate check command */
    char cmd[4096];
    (void)cmd;

    if (strcmp(ftype, "python") == 0) {
        snprintf(cmd, sizeof(cmd),
            "python3 -c \"compile(open('%s').read() + '\\\\n', '%s', 'exec')\" 2>&1",
            path, path);
    } else if (strcmp(ftype, "sh") == 0) {
        snprintf(cmd, sizeof(cmd), "bash -n '%s' 2>&1", path);
    } else if (strcmp(ftype, "json") == 0) {
        snprintf(cmd, sizeof(cmd),
            "python3 -c \"import json; json.load(open('%s'))\" 2>&1", path);
    } else if (strcmp(ftype, "yaml") == 0) {
        snprintf(cmd, sizeof(cmd),
            "python3 -c \"import yaml; yaml.safe_load(open('%s'))\" 2>&1", path);
    } else if (strcmp(ftype, "c") == 0) {
        snprintf(cmd, sizeof(cmd),
            "gcc -fsyntax-only -x c '%s' 2>&1", path);
    } else {
        json_free(args);
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\":\"Unknown file type: %s\"}", ftype);
        return strdup(buf);
    }

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        json_free(args);
        return strdup("{\"error\":\"Failed to run syntax check\"}");
    }

    char output[8192];
    output[0] = '\0';
    size_t total = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp) && total < sizeof(output) - 1) {
        size_t add = strlen(line);
        if (total + add < sizeof(output)) {
            memcpy(output + total, line, add + 1);
            total += add;
        }
    }
    int exit_code = pclose(fp);

    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));
    json_object_set(result, "type", json_new_string(ftype));
    json_object_set(result, "valid", json_new_bool(exit_code == 0));

    size_t out_len = strlen(output);
    if (out_len > 0) {
        /* Remove trailing newline */
        if (out_len > 0 && output[out_len - 1] == '\n') output[out_len - 1] = '\0';
        json_object_set(result, "error", json_new_string(output));
    }

    char *json_out = json_serialize(result);
    json_free(result);
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

char *file_diff_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    return handle_diff(args_json);
}

char *file_perms_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    return handle_perms(args_json);
}

char *file_hex_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    return handle_hex(args_json);
}

char *file_syntax_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    return handle_syntax(args_json);
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
    registry_register("file_diff",
        "Generate unified diff between two files. Shows added/removed/changed lines. "
        "Optional: context_lines (default: 3).",
        SCHEMA_DIFF, file_diff_handler);
    registry_register("file_permissions",
        "Get or set file permissions. Without 'mode', shows current permissions, "
        "type, size, owner. With 'mode' (octal, e.g. 644), changes permissions.",
        SCHEMA_PERMS, file_perms_handler);
    registry_register("file_hex",
        "View file contents as hexadecimal dump with ASCII side panel. "
        "Optional: offset (byte offset), limit (max bytes, default 256, max 4096).",
        SCHEMA_HEX, file_hex_handler);
    registry_register("file_syntax",
        "Check file syntax. Auto-detects type from extension (python, sh, json, yaml, c). "
        "Optional: type override. Returns valid=true/false and error messages.",
        SCHEMA_SYNTAX, file_syntax_handler);
}
