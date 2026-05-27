/*
 * file_batch.c — F15: Batch file operations for Hermes C.
 * Multi-file copy, move, delete, chmod, touch, stat, hash with progress reporting.
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
#include <time.h>
#include <glob.h>
#include "hash.h"

/* Forward: sandbox from file.c */
bool sandbox_check_path(const char *path);

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"batch operation: copy | move | delete | chmod | touch | stat | hash | rename\"},"
      "\"files\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Source file paths\"},"
      "\"dest\":{\"type\":\"string\",\"description\":\"Destination path (for copy/move)\"},"
      "\"mode\":{\"type\":\"string\",\"description\":\"File permission mode (octal, e.g. '755', '0644'). Required for chmod action.\"},"
      "\"pattern\":{\"type\":\"string\",\"description\":\"Glob pattern for batch rename (e.g. '*.txt'). Used with rename action instead of files array.\"},"
      "\"dest_pattern\":{\"type\":\"string\",\"description\":\"Rename pattern. Use * to carry matched portion from glob (e.g. 'backup_*.md'). Required for rename action with pattern.\"}"
    "},"
    "\"required\":[\"action\"]"
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

/* Stat a file and return JSON object with size, mode, mtime. */
static json_t *stat_file_json(const char *path) {

    struct stat st;
    if (stat(path, &st) != 0) return NULL;

    json_t *j = json_object();
    json_set(j, "size", json_number((double)st.st_size));
    char mode_str[16];
    snprintf(mode_str, sizeof(mode_str), "%o", (unsigned)(st.st_mode & 07777));
    json_set(j, "mode", json_string(mode_str));
    json_set(j, "mtime", json_number((double)st.st_mtime));
    json_set(j, "is_dir", json_bool(S_ISDIR(st.st_mode) ? 1 : 0));
    return j;
}

/* Hash a file and return JSON with sha256 hex. */
static json_t *hash_file_json(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* Hash via libhash */
    size_t n;
    /* Read whole file into memory. 100MB cap for safety. */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize < 0 || fsize > 100 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    unsigned char *data = (unsigned char *)malloc((size_t)fsize);
    if (!data) { fclose(f); return NULL; }
    n = fread(data, 1, (size_t)fsize, f);
    fclose(f);
    if (n != (size_t)fsize) { free(data); return NULL; }

    char *hex = hash_sha256_hex(data, n);
    free(data);
    if (!hex) return NULL;

    json_t *j = json_object();
    json_set(j, "sha256", json_string(hex));
    free(hex);
    return j;
}

/* Glob wildcard helpers for batch rename */
/* Extract the portion of filename that matched * in glob pattern.
   E.g. pattern "prefix_*.txt", filename "prefix_foo.txt" -> "foo".
   Returns strdup'd string or NULL on mismatch. */
static char *extract_wildcard(const char *pattern, const char *filename) {
    const char *star = strchr(pattern, '*');
    if (!star) return strdup(filename);  /* no wildcard -> whole name is match */

    /* Split pattern around * */
    size_t prefix_len = (size_t)(star - pattern);
    const char *suffix = star + 1;
    size_t suffix_len = strlen(suffix);
    size_t name_len = strlen(filename);

    /* Check prefix match */
    if (name_len < prefix_len) return NULL;
    if (strncmp(filename, pattern, prefix_len) != 0) return NULL;

    /* Check suffix match */
    if (name_len < suffix_len) return NULL;
    if (strcmp(filename + name_len - suffix_len, suffix) != 0) return NULL;

    /* Extract the middle portion */
    size_t mid_len = name_len - prefix_len - suffix_len;
    char *portion = (char *)malloc(mid_len + 1);
    if (!portion) return NULL;
    memcpy(portion, filename + prefix_len, mid_len);
    portion[mid_len] = '\0';
    return portion;
}

/* Substitute * in dest_pattern with wildcard_value.
   E.g. dest_pattern "renamed_*.md", value "foo" -> "renamed_foo.md".
   Returns strdup'd string. */
static char *substitute_wildcard(const char *dest_pattern, const char *wildcard_value) {
    const char *star = strchr(dest_pattern, '*');
    if (!star) return strdup(dest_pattern);

    size_t prefix_len = (size_t)(star - dest_pattern);
    const char *suffix = star + 1;

    size_t total_len = prefix_len + strlen(wildcard_value) + strlen(suffix) + 1;
    char *result = (char *)malloc(total_len);
    if (!result) return NULL;

    memcpy(result, dest_pattern, prefix_len);
    result[prefix_len] = '\0';
    strcat(result, wildcard_value);
    strcat(result, suffix);
    return result;
}

/* Rename a single file using pattern-based renaming.
   pattern: glob pattern (e.g. "*.txt")
   dest_pattern: rename template (e.g. "backup_*.md")
   filename: actual matched filename
   Returns new path or NULL on failure/extraction mismatch. */
static char *apply_rename_pattern(const char *pattern, const char *dest_pattern,
                                   const char *filename) {
    char *wildcard = extract_wildcard(pattern, filename);
    if (!wildcard) return NULL;
    char *new_path = substitute_wildcard(dest_pattern, wildcard);
    free(wildcard);
    return new_path;
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
    if (strcmp(action, "copy") != 0 && strcmp(action, "move") != 0 && strcmp(action, "rename") != 0 &&
        strcmp(action, "delete") != 0 && strcmp(action, "chmod") != 0 &&
        strcmp(action, "touch") != 0 && strcmp(action, "stat") != 0 && strcmp(action, "hash") != 0) {
        json_free(args);
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"Unknown action: %s\"}", action);
        return strdup(buf);
    }

        /* Early return: rename with pattern uses glob, not files array */
    if (strcmp(action, "rename") == 0) {
        const char *pattern = json_get_str(args, "pattern", NULL);
        const char *dest_pattern = json_get_str(args, "dest_pattern", NULL);
        if (pattern && dest_pattern) {
            json_t *result = json_object();
            json_t *results_arr = json_array();
            int success_count = 0, fail_count = 0;

            glob_t globbuf;
            int ret = glob(pattern, 0, NULL, &globbuf);
            if (ret != 0) {
                json_free(args);
                json_free(result);
                return strdup("{\"error\":\"Glob pattern failed\"}");
            }

            for (size_t g = 0; g < globbuf.gl_pathc; g++) {
                const char *fpath = globbuf.gl_pathv[g];
                json_t *entry = json_object();
                json_set(entry, "source", json_string(fpath));

                if (!sandbox_check_path(fpath)) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string("Path not allowed"));
                    fail_count++;
                } else {
                    char *new_path = apply_rename_pattern(pattern, dest_pattern, fpath);
                    if (!new_path) {
                        json_set(entry, "status", json_string("failed"));
                        json_set(entry, "error", json_string("Pattern mismatch"));
                        fail_count++;
                    } else if (!sandbox_check_path(new_path)) {
                        json_set(entry, "status", json_string("failed"));
                        json_set(entry, "error", json_string("Dest path not allowed"));
                        free(new_path);
                        fail_count++;
                    } else if (!move_file(fpath, new_path)) {
                        json_set(entry, "status", json_string("failed"));
                        json_set(entry, "error", json_string(strerror(errno)));
                        free(new_path);
                        fail_count++;
                    } else {
                        json_set(entry, "status", json_string("renamed"));
                        json_set(entry, "dest", json_string(new_path));
                        free(new_path);
                        success_count++;
                    }
                }
                json_append(results_arr, entry);
            }
            globfree(&globbuf);

            json_set(result, "results", results_arr);
            json_set(result, "success_count", json_number(success_count));
            json_set(result, "fail_count", json_number(fail_count));
            json_set(result, "action", json_string(action));

            char *json_out = json_serialize(result);
            json_free(result);
            json_free(args);
            return json_out;
        }
        /* else: fall through to files-array rename in main loop */
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

                } else if (strcmp(action, "rename") == 0) {
            if (!dest) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Missing dest for rename"));
                fail_count++;
            } else {
                char dst[4096];
                build_dest_path(dest, src, dst, sizeof(dst));
                if (!sandbox_check_path(src)) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string("Source path not allowed"));
                    fail_count++;
                } else if (!sandbox_check_path(dst)) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string("Dest path not allowed"));
                    fail_count++;
                } else if (!move_file(src, dst)) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string(strerror(errno)));
                    fail_count++;
                } else {
                    json_set(entry, "status", json_string("renamed"));
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

        } else if (strcmp(action, "stat") == 0) {
            if (!sandbox_check_path(src)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Source path not allowed"));
                fail_count++;
            } else {
                json_t *stat_j = stat_file_json(src);
                if (!stat_j) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string(strerror(errno)));
                    fail_count++;
                } else {
                    json_set(entry, "status", json_string("ok"));
                    json_set(entry, "file_info", stat_j);
                    success_count++;
                }
            }

        } else if (strcmp(action, "hash") == 0) {
            if (!sandbox_check_path(src)) {
                json_set(entry, "status", json_string("failed"));
                json_set(entry, "error", json_string("Source path not allowed"));
                fail_count++;
            } else {
                json_t *hash_j = hash_file_json(src);
                if (!hash_j) {
                    json_set(entry, "status", json_string("failed"));
                    json_set(entry, "error", json_string(strerror(errno)));
                    fail_count++;
                } else {
                    json_set(entry, "status", json_string("ok"));
                    json_set(entry, "file_hash", hash_j);
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
        "Batch file operations. Actions: copy, move, delete, chmod, touch, stat, hash, rename. "
        "Accepts array of source paths, optional destination (copy/move), "
        "and optional mode string (chmod, e.g. '755'). "
        "Returns per-file result with success/failure counts.",
        SCHEMA, file_batch_handler);
}
