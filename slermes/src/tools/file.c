/*
 * file.c — File I/O tools for Slermes C.
 * Read, write, search files. Path security checks.
 */

#include "slermes.h"
#include "slermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

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
 *  Path security check
 * ================================================================ */

static bool is_safe_path(const char *path) {
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
