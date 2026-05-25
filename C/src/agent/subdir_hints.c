/*
 * hermes_subdir_hints.c — Progressive subdirectory hint discovery.
 * Port of Python agent/subdirectory_hints.py.
 *
 * When the agent calls tools that reference file paths (read_file, terminal,
 * search_files, patch, etc.), this module checks if the paths belong to
 * unvisited directories. If so, it looks for context files (AGENTS.md,
 * CLAUDE.md, .cursorrules) in those directories and returns formatted hint
 * text that gets appended to the tool result.
 *
 * MIT License — WuBu Hermes Project
 */

#include "hermes_subdir_hints.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>

/* ================================================================
 *  State
 * ================================================================ */

/* Tracked directories */
static char g_loaded_dirs[SUBDIR_HINTS_MAX_DIRS][PATH_MAX];
static int g_loaded_count = 0;

/* Working directory */
static char g_working_dir[PATH_MAX] = {0};

/* Thread safety */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* Tool argument keys that typically contain file paths */
static const char *PATH_ARG_KEYS[] = {"path", "file_path", "workdir", NULL};

/* Tool names that take shell commands where we extract paths */
/* (currently handled inline in subdir_hints_check)
static const char *COMMAND_TOOLS[] = {"terminal", NULL};
*/

/* Hint filenames in priority order */
static const char *HINT_FILENAMES[] = {
    "AGENTS.md", "agents.md",
    "CLAUDE.md", "claude.md",
    ".cursorrules",
    NULL
};

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static bool is_dir_loaded(const char *dir) {
    for (int i = 0; i < g_loaded_count; i++) {
        if (strcmp(g_loaded_dirs[i], dir) == 0)
            return true;
    }
    return false;
}

static bool add_loaded_dir(const char *dir) {
    if (g_loaded_count >= SUBDIR_HINTS_MAX_DIRS) return false;
    if (is_dir_loaded(dir)) return false;
    snprintf(g_loaded_dirs[g_loaded_count], PATH_MAX, "%s", dir);
    g_loaded_count++;
    return true;
}

static bool is_regular_file(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static bool is_directory(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static bool has_suffix(const char *path) {
    /* Check if path has an extension (contains '.') in its last component */
    const char *base = strrchr(path, '/');
    if (base) base++; else base = path;
    return strchr(base, '.') != NULL;
}

/* Resolve a path, handling absolute vs relative, ~ expansion */
static char *resolve_path(const char *raw_path) {
    if (!raw_path || !raw_path[0]) return NULL;

    char buf[PATH_MAX];

    /* Handle ~ expansion */
    if (raw_path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) return NULL;
        const char *rest = raw_path + 1;
        if (rest[0] == '/' || rest[0] == '\0') {
            snprintf(buf, sizeof(buf), "%s%s", home, rest);
        } else {
            /* ~user expansion: look up user's home directory via getpwnam */
            const char *slash = strchr(rest, '/');
            size_t user_len = slash ? (size_t)(slash - rest) : strlen(rest);
            if (user_len >= 256) return NULL; /* sanity */
            char username[256];
            memcpy(username, rest, user_len);
            username[user_len] = '\0';
            struct passwd *pw = getpwnam(username);
            if (!pw) return NULL; /* user not found */
            const char *subpath = slash ? slash : "";
            snprintf(buf, sizeof(buf), "%s%s", pw->pw_dir, subpath);
        }
    } else if (raw_path[0] != '/') {
        /* Relative path — prepend working dir */
        if (!g_working_dir[0]) return NULL;
        snprintf(buf, sizeof(buf), "%s/%s", g_working_dir, raw_path);
    } else {
        snprintf(buf, sizeof(buf), "%s", raw_path);
    }

    /* Resolve symlinks with realpath */
    char *resolved = realpath(buf, NULL);
    if (!resolved) {
        /* realpath failed (path may not exist yet), use normalized version */
        resolved = strdup(buf);
    }

    return resolved;
}

/* Read entire file into malloc'd string. Returns NULL on failure. */
static char *read_file_content(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0 || len > SUBDIR_HINTS_MAX_CHARS * 2) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char *content = (char *)malloc((size_t)len + 1);
    if (!content) { fclose(f); return NULL; }

    size_t n = fread(content, 1, (size_t)len, f);
    fclose(f);
    content[n] = '\0';
    return content;
}

/* Extract path values from JSON args object */
static void extract_path_args(const char *args_json, char paths[][PATH_MAX], int *count, int max) {
    *count = 0;
    if (!args_json || !args_json[0]) return;

    json_node_t *root = json_parse(args_json, NULL);
    if (!root) return;

    for (int k = 0; PATH_ARG_KEYS[k] && *count < max; k++) {
        const char *val = json_object_get_string(root, PATH_ARG_KEYS[k], NULL);
        if (val && val[0]) {
            snprintf(paths[*count], PATH_MAX, "%s", val);
            (*count)++;
        }
    }

    json_free(root);
}

/* Check if tool_name is a command-containing tool */
/* (no longer used — terminal paths extracted inline in subdir_hints_check)
static bool is_command_tool(const char *tool_name) {
    for (int i = 0; COMMAND_TOOLS[i]; i++) {
        if (strcmp(tool_name, COMMAND_TOOLS[i]) == 0)
            return true;
    }
    return false;
}
*/

/* Extract coarse path tokens from a shell command (simple split) */
static void extract_cmd_paths(const char *cmd, char paths[][PATH_MAX], int *count, int max) {
    *count = 0;
    if (!cmd || !cmd[0]) return;

    /* Simple whitespace tokenization */
    char buf[16384];
    snprintf(buf, sizeof(buf), "%s", cmd);

    int idx = 0;
    char *save;
    char *tok = strtok_r(buf, " \t\n\r", &save);

    while (tok && *count < max) {
        /* Skip flags, URLs */
        if (tok[0] != '-' &&
            strncmp(tok, "http://", 7) != 0 &&
            strncmp(tok, "https://", 8) != 0 &&
            strncmp(tok, "git@", 4) != 0) {
            /* Must look like a path (contains / or .) */
            if (strchr(tok, '/') || strchr(tok, '.')) {
                snprintf(paths[*count], PATH_MAX, "%s", tok);
                (*count)++;
            }
        }
        tok = strtok_r(NULL, " \t\n\r", &save);
        idx++;
    }
}

/* ================================================================
 *  Public API
 * ================================================================ */

void subdir_hints_init(const char *working_dir) {
    pthread_mutex_lock(&g_lock);

    g_loaded_count = 0;

    if (working_dir && working_dir[0]) {
        char *resolved = realpath(working_dir, NULL);
        if (resolved) {
            snprintf(g_working_dir, sizeof(g_working_dir), "%s", resolved);
            /* Mark working dir as already-loaded (startup context handles it) */
            add_loaded_dir(resolved);
            free(resolved);
        }
    } else {
        /* Use CWD */
        if (getcwd(g_working_dir, sizeof(g_working_dir))) {
            add_loaded_dir(g_working_dir);
        }
    }

    pthread_mutex_unlock(&g_lock);
}

void subdir_hints_cleanup(void) {
    pthread_mutex_lock(&g_lock);
    g_loaded_count = 0;
    g_working_dir[0] = '\0';
    pthread_mutex_unlock(&g_lock);
}

char *subdir_hints_check(const char *tool_name, const char *args_json) {
    if (!tool_name || !args_json) return NULL;

    pthread_mutex_lock(&g_lock);

    if (!g_working_dir[0]) {
        /* Auto-init with CWD */
        if (getcwd(g_working_dir, sizeof(g_working_dir))) {
            add_loaded_dir(g_working_dir);
        } else {
            pthread_mutex_unlock(&g_lock);
            return NULL;
        }
    }

    /* Collect candidate paths from tool args */
    char raw_paths[16][PATH_MAX];
    int n_raw = 0;

    /* Direct path arguments */
    if (strcmp(tool_name, "read_file") == 0 ||
        strcmp(tool_name, "search_files") == 0 ||
        strcmp(tool_name, "patch") == 0 ||
        strcmp(tool_name, "write_file") == 0) {
        extract_path_args(args_json, raw_paths, &n_raw, 16);
    } else if (strcmp(tool_name, "terminal") == 0) {
        /* Extract command arg and look for paths in it */
        json_node_t *root = json_parse(args_json, NULL);
        if (root) {
            const char *cmd = json_object_get_string(root, "command", NULL);
            if (cmd) {
                extract_cmd_paths(cmd, raw_paths, &n_raw, 16);
            }
            json_free(root);
        }
    } else if (strcmp(tool_name, "execute_code") == 0) {
        /* Extract workdir */
        extract_path_args(args_json, raw_paths, &n_raw, 16);
    }

    if (n_raw == 0) {
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    /* Resolve each path and discover unvisited directories */
    char new_dirs[SUBDIR_HINTS_MAX_DIRS][PATH_MAX];
    int n_new = 0;

    for (int i = 0; i < n_raw && n_new < SUBDIR_HINTS_MAX_DIRS; i++) {
        char *resolved = resolve_path(raw_paths[i]);
        if (!resolved) continue;

        /* If it's a file path (has extension or exists as file), use parent dir */
        char dir[PATH_MAX];
        if (has_suffix(resolved) || is_regular_file(resolved)) {
            char *slash = strrchr(resolved, '/');
            if (slash) {
                size_t dlen = (size_t)(slash - resolved);
                if (dlen > 0) {
                    memcpy(dir, resolved, dlen);
                    dir[dlen] = '\0';
                } else {
                    snprintf(dir, sizeof(dir), "/");
                }
            } else {
                snprintf(dir, sizeof(dir), "%s", resolved);
            }
        } else {
            snprintf(dir, sizeof(dir), "%s", resolved);
        }
        free(resolved);

        /* Walk up ancestors looking for unvisited dirs */
        char current[PATH_MAX];
        snprintf(current, sizeof(current), "%s", dir);

        for (int w = 0; w < SUBDIR_HINTS_MAX_ANCESTOR_WALK && n_new < SUBDIR_HINTS_MAX_DIRS; w++) {
            if (is_dir_loaded(current)) break;

            if (is_directory(current)) {
                snprintf(new_dirs[n_new], PATH_MAX, "%s", current);
                n_new++;
            }

            /* Walk up to parent */
            if (strcmp(current, "/") == 0) break;
            char *slash = strrchr(current, '/');
            if (slash == current) {
                snprintf(current, sizeof(current), "/");
            } else if (slash) {
                *slash = '\0';
            } else {
                break;
            }
        }
    }

    if (n_new == 0) {
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    /* For each new dir, try to load hint files */
    char result[32768];
    int pos = 0;
    result[0] = '\0';

    for (int i = 0; i < n_new; i++) {
        if (!is_directory(new_dirs[i])) continue;
        if (!add_loaded_dir(new_dirs[i])) continue;

        /* Try each hint filename in priority order */
        char *hint_content = NULL;
        char hint_path[PATH_MAX];

        for (int h = 0; HINT_FILENAMES[h]; h++) {
            snprintf(hint_path, sizeof(hint_path), "%s/%s", new_dirs[i], HINT_FILENAMES[h]);
            if (is_regular_file(hint_path)) {
                char *content = read_file_content(hint_path);
                if (content && content[0]) {
                    if (strlen(content) > SUBDIR_HINTS_MAX_CHARS) {
                        /* Truncate */
                        content[SUBDIR_HINTS_MAX_CHARS] = '\0';
                    }
                    hint_content = content;
                } else {
                    free(content);
                }
                break; /* First match wins per directory */
            }
        }

        if (!hint_content) continue;

        /* Format hint text */
        int remaining = (int)(sizeof(result) - (size_t)pos - 1);
        if (remaining < 64) break;

        /* Compute relative path for display */
        const char *display_path = hint_path;
        if (g_working_dir[0]) {
            size_t wd_len = strlen(g_working_dir);
            if (strncmp(hint_path, g_working_dir, wd_len) == 0 &&
                hint_path[wd_len] == '/') {
                display_path = hint_path + wd_len + 1;
            } else {
                /* Try home-relative */
                const char *home = getenv("HOME");
                if (home) {
                    size_t home_len = strlen(home);
                    if (strncmp(hint_path, home, home_len) == 0 &&
                        hint_path[home_len] == '/') {
                        /* We'll use the full path since we can't easily prefix ~ */
                        display_path = hint_path + home_len + 1;
                    }
                }
            }
        }

        int n = snprintf(result + pos, (size_t)remaining,
            "\n\n[Subdirectory context discovered: %s]\n%s",
            display_path, hint_content);

        if (n > 0 && n < remaining) {
            pos += n;
        }

        free(hint_content);
    }

    pthread_mutex_unlock(&g_lock);

    if (pos == 0) return NULL;
    return strdup(result);
}
