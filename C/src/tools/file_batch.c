/*
 * file_batch.c — F15: Batch file operations for Hermes C.
 * Multi-file copy, move, delete with progress reporting.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

/* Forward: sandbox from file.c */
bool sandbox_check_path(const char *path);

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"batch operation: copy | move | delete\"},"
      "\"files\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Source file paths\"},"
      "\"dest\":{\"type\":\"string\",\"description\":\"Destination path (for copy/move)\"}"
    "},"
    "\"required\":[\"action\",\"files\"]"
"}";

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

char *file_batch_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *action = json_get_str(args, "action", "copy");
    json_t *files_node = json_obj_get(args, "files");
    const char *dest = json_get_str(args, "dest", NULL);

    /* Validate action early */
    if (strcmp(action, "copy") != 0 && strcmp(action, "move") != 0 &&
        strcmp(action, "delete") != 0) {
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
        "Batch file operations. Actions: copy, move, delete. "
        "Accepts array of source paths and optional destination. "
        "Returns per-file result with success/failure counts.",
        SCHEMA, file_batch_handler);
}
