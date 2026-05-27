/*
 * file_batch.c — F15: Batch file operations for Hermes C.
 * Multi-file copy, move, delete, chmod, touch with progress reporting.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/* Forward: sandbox from file.c */
bool sandbox_check_path(const char *path);

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"batch operation: copy | move | delete | chmod | touch\"},"
      "\"files\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Source file paths\"},"
      "\"dest\":{\"type\":\"string\",\"description\":\"Destination path (for copy/move)\"},"
      "\"mode\":{\"type\":\"string\",\"description\":\"File permission mode (octal, e.g. '755', '0644'). Required for chmod action.\"}"
    "},"
    "\"required\":[\"action\",\"files\"]"
"}" ;

/* Buffer for file copy */
#define COPY_BUF_SIZE 65536

static bool copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;

    /* Ensure parent dir exists */
    char dstdir[4096];
    snprintf(dstdir, sizeof(dstdir), "%s", dst);
    char *slash = strrchr(dstdir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dstdir, 0755);
    }

    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }

    char buf[COPY_BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in); fclose(out); unlink(dst);
            return false;
        }
    }

    fclose(in);
    fclose(out);
    return true;
}

static bool move_file(const char *src, const char *dst) {
    if (rename(src, dst) == 0) return true;
    /* Cross-filesystem: copy + unlink */
    if (!copy_file(src, dst)) return false;
    unlink(src);
    return true;
}

static bool delete_file(const char *path) {
    return unlink(path) == 0;
}

/* Build target path from dest (dir or full path) and source basename */
static void build_dest_path(const char *dest, const char *src,
                            char *out, size_t out_size) {
    struct stat st;
    if (stat(dest, &st) == 0 && S_ISDIR(st.st_mode)) {
        /* dest is a directory: append source basename */
        const char *base = strrchr(src, '/');
        base = base ? base + 1 : src;
        snprintf(out, out_size, "%s/%s", dest, base);
    } else {
        snprintf(out, out_size, "%s", dest);
    }
}

/* Parse octal mode string (e.g. "755", "0644") to mode_t.
 * Returns (mode_t)-1 on parse failure. */
static mode_t parse_mode_string(const char *mode_str) {
    if (!mode_str || !*mode_str) return (mode_t)-1;
    char *end = NULL;
    long val = strtol(mode_str, &end, 8);
    if (*end != '\0' || val < 0 || val > 07777) return (mode_t)-1;
    return (mode_t)val;
}

static bool chmod_file(const char *path, mode_t mode) {
    return chmod(path, mode) == 0;
}

static bool touch_file(const char *path) {
    /* Update timestamps. If file doesn't exist, create it. */
    if (utimensat(AT_FDCWD, path, NULL, 0) == 0)
        return true;
    /* Create empty file */
    FILE *f = fopen(path, "a");
    if (!f) return false;
    fclose(f);
    return true;
}

char *file_batch_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *action = json_get_str(args, "action", "copy");
    json_t *files_node = json_obj_get(args, "files");
    const char *dest = json_get_str(args, "dest", NULL);
    const char *mode_str = json_get_str(args, "mode", NULL);

    /* Validate action early */
    if (strcmp(action, "copy") != 0 && strcmp(action, "move") != 0 &&
        strcmp(action, "delete") != 0 && strcmp(action, "chmod") != 0 &&
        strcmp(action, "touch") != 0) {
        json_free(args);
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"Unknown action: %s\"}", action);
        return strdup(buf);
    }

    if (!files_node || files_node->type != JSON_ARRAY) {
        json_free(args);
        return strdup("{\"error\":\"'files' must be a non-empty array\"}");
    }

    int n_files = (int)json_len(files_node);
    if (n_files == 0) {
        json_free(args);
        return strdup("{\"error\":\"No files specified\"}");
    }

    json_t *result = json_object();
    json_t *results_arr = json_array();
    int success_count = 0;
    int fail_count = 0;

    for (int i = 0; i < n_files; i++) {
        json_t *file_node = json_array_get(files_node, i);
        if (!file_node || file_node->type != JSON_STRING) continue;

        const char *src = file_node->str_val;
        json_t *entry = json_object();
        json_set(entry, "source", json_string(src));

        if (strcmp(action, "delete") == 0) {
            if (!sandbox_check_path(src) || !delete_file(src)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string(strerror(errno)));
                fail_count++;
            } else {
                json_set(entry, "status", json_string("deleted"));
                success_count++;
            }

        } else if (strcmp(action, "copy") == 0) {
            if (!dest) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Missing dest for copy"));
                fail_count++;
            } else if (!sandbox_check_path(src)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Source path not allowed"));
                fail_count++;
            } else {
                char dst[4096];
                build_dest_path(dest, src, dst, sizeof(dst));
                if (!sandbox_check_path(dst)) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string("Dest path not allowed"));
                    fail_count++;
                } else if (!copy_file(src, dst)) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string(strerror(errno)));
                    fail_count++;
                } else {
                    json_set(entry, "status", json_string("copied"));
                    json_set(entry, "dest", json_string(dst));
                    success_count++;
                }
            }

        } else if (strcmp(action, "move") == 0) {
            if (!dest) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Missing dest for move"));
                fail_count++;
            } else if (!sandbox_check_path(src)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Source path not allowed"));
                fail_count++;
            } else {
                char dst[4096];
                build_dest_path(dest, src, dst, sizeof(dst));
                if (!sandbox_check_path(dst)) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string("Dest path not allowed"));
                    fail_count++;
                } else if (!move_file(src, dst)) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string(strerror(errno)));
                    fail_count++;
                } else {
                    json_set(entry, "status", json_string("moved"));
                    json_set(entry, "dest", json_string(dst));
                    success_count++;
                }
            }

        } else if (strcmp(action, "chmod") == 0) {
            mode_t mode = parse_mode_string(mode_str);
            if (mode == (mode_t)-1) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error",
                    json_string("Invalid mode string. Use octal e.g. '755', '0644'"));
                fail_count++;
            } else if (!sandbox_check_path(src)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Source path not allowed"));
                fail_count++;
            } else if (!chmod_file(src, mode)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string(strerror(errno)));
                fail_count++;
            } else {
                json_set(entry, "status", json_string("chmod"));
                json_set(entry, "mode", json_string(mode_str));
                success_count++;
            }

        } else if (strcmp(action, "touch") == 0) {
            if (!sandbox_check_path(src)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Source path not allowed"));
                fail_count++;
            } else if (!touch_file(src)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string(strerror(errno)));
                fail_count++;
            } else {
                json_set(entry, "status", json_string("touched"));
                success_count++;
            }

        } else {
            /* copy — handled in the copy/move/delete branches above */
            json_set(entry, "status", json_string("failed"));
            json_set(entry, "error", json_string("Internal: unknown action"));
            fail_count++;
        }

        json_append(results_arr, entry);
    }

    json_set(result, "results", results_arr);
    json_set(result, "success_count", json_number(success_count));
    json_set(result, "fail_count", json_number(fail_count));
    json_set(result, "action", json_string(action));

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_file_batch(void) {
    registry_register("file_batch",
        "Batch file operations. Actions: copy, move, delete, chmod, touch. "
        "Accepts array of source paths, optional destination (copy/move), "
        "and optional mode string (chmod, e.g. '755'). "
        "Returns per-file result with success/failure counts.",
        SCHEMA, file_batch_handler);
}
