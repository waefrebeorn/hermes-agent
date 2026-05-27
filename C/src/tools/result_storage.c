/*
 * result_storage.c — P49-P50: Tool result storage and output limits.
 * Stores large tool results in temp files to avoid filling message buffers.
 * Auto-truncates results exceeding max_result_size.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

/* ================================================================
 *  Result storage — when result exceeds threshold, write to temp file
 * ================================================================ */

#define RESULT_INLINE_MAX 4096  /* Keep small results inline */

typedef struct {
    char *data;          /* inline data (if size <= RESULT_INLINE_MAX) */
    char *file_path;     /* temp file path (if size > RESULT_INLINE_MAX) */
    size_t size;         /* actual size */
    bool stored_in_file; /* true if result is in temp file */
} tool_result_t;

/* Store a tool result. Returns a string suitable for message content.
 * For large results, writes to temp file and returns a reference string.
 * Caller must free the returned string. */
char *tool_result_store(const char *data, size_t size, size_t max_inline) {
    if (!data) return strdup("{\"error\":\"NULL result\"}");

    size_t data_len = (size > 0) ? size : strlen(data);
    size_t max_size = (max_inline > 0) ? max_inline : (size_t)RESULT_INLINE_MAX;

    /* Check if result exceeds max configured size */
    hermes_config_t cfg;
    int max_result = 0;
    if (hermes_config_load(&cfg, NULL)) {
        max_result = cfg.tools.max_result_size;
    }
    if (max_result <= 0) max_result = 50000;

    if (data_len > (size_t)max_result) {
        /* Truncate */
        char *truncated = (char *)malloc((size_t)max_result + 128);
        if (!truncated) return strdup("{\"error\":\"OOM\"}");
        memcpy(truncated, data, (size_t)max_result);
        snprintf(truncated + max_result, 128,
                 "\n\n... [truncated: %zu bytes, max %d]",
                 data_len, max_result);
        return truncated;
    }

    if (data_len <= max_size) {
        /* Inline — return copy */
        return strdup(data);
    }

    /* Large result — write to temp file */
    char tmpdir[HERMES_PATH_MAX];
    const char *tmp_env = getenv("SLERMES_HOME");
    if (tmp_env)
        snprintf(tmpdir, sizeof(tmpdir), "%s/tmp", tmp_env);
    else
        snprintf(tmpdir, sizeof(tmpdir), "/tmp/slermes_results");

    mkdir(tmpdir, 0755);

    char tmp_path[HERMES_PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s/result_%d_%ld.json",
             tmpdir, getpid(), (long)rand());

    FILE *f = fopen(tmp_path, "w");
    if (!f) {
        /* Fall back to truncated inline */
        char *fallback = (char *)malloc(max_size + 64);
        if (!fallback) return strdup("{\"error\":\"OOM\"}");
        memcpy(fallback, data, max_size);
        fallback[max_size] = '\0';
        return fallback;
    }
    fwrite(data, 1, data_len, f);
    fclose(f);

    /* Return file reference */
    char *ref = (char *)malloc(HERMES_PATH_MAX + 64);
    if (!ref) return strdup("{\"error\":\"OOM\"}");
    snprintf(ref, HERMES_PATH_MAX + 64,
             "{\"result_file\":\"%s\",\"size\":%zu}", tmp_path, data_len);
    return ref;
}

/* Clean up temp files older than N seconds */
void tool_result_cleanup(int max_age_seconds) {
    char tmpdir[HERMES_PATH_MAX];
    const char *tmp_env = getenv("SLERMES_HOME");
    if (tmp_env)
        snprintf(tmpdir, sizeof(tmpdir), "%s/tmp", tmp_env);
    else
        snprintf(tmpdir, sizeof(tmpdir), "/tmp/slermes_results");

    DIR *dir = opendir(tmpdir);
    if (!dir) return;

    time_t now = time(NULL);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "result_", 7) != 0) continue;

        char fullpath[HERMES_PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", tmpdir, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (now - st.st_mtime > max_age_seconds)
                unlink(fullpath);
        }
    }
    closedir(dir);
}

/* ================================================================
 *  Preview generation — truncate at last newline within max_chars
 *  Port of Python tools/tool_result_storage.py::generate_preview().
 * ================================================================ */

/* Generate a preview of content, truncating at the last newline within max_chars.
 * Returns a malloc'd preview string. Sets *has_more to true if content was truncated.
 * Caller must free the returned string. */
char *tool_result_generate_preview(const char *content, int max_chars, bool *has_more)
{
    if (!content) {
        if (has_more) *has_more = false;
        return strdup("");
    }

    size_t len = strlen(content);
    if (len <= (size_t)max_chars) {
        if (has_more) *has_more = false;
        return strdup(content);
    }

    /* Truncate at last newline within max_chars */
    int trunc_len = max_chars;
    const char *start = content;
    const char *end = content + max_chars;

    /* Scan backwards from max_chars-1 to find last newline */
    const char *p = end;
    /* Handle max_chars == 0 edge case */
    if (p > start) p--;

    while (p > start && *p != '\n') p--;

    /* Only use newline boundary if it's past the halfway point */
    if (p > start && (size_t)(p - start) > (size_t)max_chars / 2)
        trunc_len = (int)(p - start + 1);  /* include the newline */

    char *preview = (char *)malloc((size_t)trunc_len + 1);
    if (!preview) {
        if (has_more) *has_more = true;
        return strdup(content);  /* fallback */
    }
    memcpy(preview, content, (size_t)trunc_len);
    preview[trunc_len] = '\0';

    if (has_more) *has_more = true;
    return preview;
}
