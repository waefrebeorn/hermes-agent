/*
 * session_search.c — Session search tool for Hermes C.
 * Searches past conversation sessions using grep over session files.
 * Replaces Python's FTS5-based session search.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"query\":{\"type\":\"string\",\"description\":\"Search terms or phrase\"},"
      "\"limit\":{\"type\":\"number\",\"description\":\"Max results\",\"default\":5}"
    "},"
    "\"required\":[\"query\"]"
"}";

static char *get_sessions_dir(void) {
    static char buf[4096];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sizeof(buf), "%s/.hermes/sessions", home);

    struct stat st;
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return buf;

    /* Try flat sessions directory */
    snprintf(buf, sizeof(buf), "%s/sessions", home);
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return buf;

    /* Try SLERMES_HOME sessions */
    const char *shome = getenv("SLERMES_HOME");
    if (!shome) {
        snprintf(buf, sizeof(buf), "%s/.slermes/sessions", home);
    } else {
        snprintf(buf, sizeof(buf), "%s/sessions", shome);
    }
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return buf;

    /* Return hermes dir as default */
    snprintf(buf, sizeof(buf), "%s/.hermes/sessions", home);
    return buf;
}

/* Read a file into a malloc'd string */
static char *read_file_str(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > 1024 * 1024) { fclose(f); return NULL; } /* > 1MB skip */
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Escape single quotes for shell */
static void shell_escape(const char *in, char *out, size_t out_sz) {
    size_t pos = 0;
    for (const char *p = in; *p && pos < out_sz - 4; p++) {
        if (*p == '\'') {
            if (pos + 4 < out_sz) {
                out[pos++] = '\'';
                out[pos++] = '\\';
                out[pos++] = '\'';
                out[pos++] = '\'';
            }
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
}

char *session_search_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *query = json_object_get_string(args, "query", NULL);
    int limit = (int)json_object_get_number(args, "limit", 5);

    json_node_t *result = json_new_object();

    if (!query || !*query) {
        json_object_set(result, "error", json_new_string("Missing query"));
    } else {
        const char *sdir = get_sessions_dir();
        struct stat st;
        if (stat(sdir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            json_object_set(result, "error", json_new_string("Session dir not found"));
            json_object_set(result, "sessions_dir", json_new_string(sdir));
        } else {
            /* Use grep with match counts and context for ranking + snippets */
            char escaped[4096];
            shell_escape(query, escaped, sizeof(escaped));

            json_node_t *sessions = json_new_array();

            /* Collect matching files with match counts, ranked by match frequency */
#define MAX_MATCH_FILES 256
            char match_files[MAX_MATCH_FILES][4096];
            int  match_counts[MAX_MATCH_FILES];
            int  match_file_count = 0;

            char cmd[16384];
            snprintf(cmd, sizeof(cmd),
                     "grep -rlc -- '%s' '%s' 2>/dev/null | while IFS= read -r f; do "
                     "  c=$(grep -oc -- '%s' \"$f\" 2>/dev/null || echo 0); "
                     "  echo \"$c $f\"; "
                     "done | sort -rn | head -%d",
                     escaped, sdir, escaped, limit);

            FILE *fp = popen(cmd, "r");
            if (!fp) {
                json_object_set(result, "error", json_new_string("grep failed"));
            } else {
                char line[16384];
                while (fgets(line, sizeof(line), fp) && match_file_count < MAX_MATCH_FILES) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                        line[--len] = '\0';

                    /* Parse "count path" */
                    char *space = strchr(line, ' ');
                    if (!space) continue;
                    *space = '\0';
                    int count = atoi(line);
                    const char *path = space + 1;

                    snprintf(match_files[match_file_count], sizeof(match_files[0]), "%s", path);
                    match_counts[match_file_count] = count;
                    match_file_count++;
                }
                pclose(fp);

                /* Build results with ranked, snippet-rich output */
                for (int i = 0; i < match_file_count; i++) {
                    /* Extract session ID from path */
                    const char *sname = match_files[i];
                    const char *last_slash = strrchr(match_files[i], '/');
                    if (last_slash) sname = last_slash + 1;

                    char sname_clean[256];
                    snprintf(sname_clean, sizeof(sname_clean), "%s", sname);
                    char *dot = strrchr(sname_clean, '.');
                    if (dot) *dot = '\0';

                    json_node_t *s = json_new_object();
                    json_object_set(s, "session_id", json_new_string(sname_clean));
                    json_object_set(s, "path", json_new_string(match_files[i]));
                    json_object_set(s, "score", json_new_number((double)match_counts[i]));

                    /* Get snippet with match context */
                    char *content = read_file_str(match_files[i]);
                    if (content) {
                        /* Find first match position in content (case-insensitive) */
                        char *match_pos = NULL;
                        size_t qlen = strlen(query);
                        for (char *p = content; *p && !match_pos; p++) {
                            if (strncasecmp(p, query, qlen) == 0)
                                match_pos = p;
                        }

                        if (match_pos) {
                            /* Extract context window: 60 chars before, 120 chars after */
                            size_t offset = (size_t)(match_pos - content);
                            size_t ctx_start = offset > 60 ? offset - 60 : 0;
                            size_t ctx_end = offset + qlen + 120;
                            if (ctx_end > strlen(content)) ctx_end = strlen(content);

                            char snippet[512];
                            size_t snippet_len = ctx_end - ctx_start;
                            if (snippet_len > 480) snippet_len = 480;

                            size_t pos = 0;
                            if (ctx_start > 0)
                                pos += snprintf(snippet + pos, sizeof(snippet) - pos, "...");
                            size_t copy_len = snippet_len;
                            if (copy_len > sizeof(snippet) - pos - 4)
                                copy_len = sizeof(snippet) - pos - 4;
                            memcpy(snippet + pos, content + ctx_start, copy_len);
                            pos += copy_len;
                            if (ctx_start + copy_len < strlen(content))
                                pos += snprintf(snippet + pos, sizeof(snippet) - pos, "...");
                            snippet[pos] = '\0';

                            json_object_set(s, "snippet", json_new_string(snippet));
                        } else {
                            /* Fallback: first 200 chars */
                            char preview[256];
                            snprintf(preview, sizeof(preview), "%.200s",
                                     strlen(content) > 200 ? content : content);
                            json_object_set(s, "snippet", json_new_string(preview));
                        }
                        free(content);
                    }

                    json_array_append(sessions, s);
                }
            }

            json_object_set(result, "query", json_new_string(query));
            json_object_set(result, "count", json_new_number((double)json_array_count(sessions)));
            json_object_set(result, "sessions_dir", json_new_string(sdir));
            json_object_set(result, "ranked_by", json_new_string("match_count"));
            json_object_set(result, "results", sessions);
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_session_search(void) {
    registry_register("session_search",
        "Search past conversation sessions. Uses grep over session files. "
        "Returns matching session IDs with previews.",
        SCHEMA, session_search_handler);
}
