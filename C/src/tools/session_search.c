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
#include <dirent.h>
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
            /* Use grep to search session files */
            char escaped[4096];
            shell_escape(query, escaped, sizeof(escaped));

            char cmd[16384];
            snprintf(cmd, sizeof(cmd),
                     "grep -rli -- '%s' '%s' 2>/dev/null | head -%d",
                     escaped, sdir, limit);

            FILE *fp = popen(cmd, "r");
            if (!fp) {
                json_object_set(result, "error", json_new_string("grep failed"));
            } else {
                json_node_t *sessions = json_new_array();
                char line[4096];
                while (fgets(line, sizeof(line), fp)) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                        line[--len] = '\0';

                    /* Extract just the filename (session ID) from path */
                    const char *sname = line;
                    char *slash = strrchr(line, '/');
                    if (slash) sname = slash + 1;
                    /* Strip .json extension */
                    char sname_clean[256];
                    snprintf(sname_clean, sizeof(sname_clean), "%s", sname);
                    char *dot = strrchr(sname_clean, '.');
                    if (dot) *dot = '\0';

                    json_node_t *s = json_new_object();
                    json_object_set(s, "session_id", json_new_string(sname_clean));
                    json_object_set(s, "path", json_new_string(line));

                    /* Get preview (first 200 chars) */
                    char *content = read_file_str(line);
                    if (content) {
                        /* Extract first meaningful content (skip JSON wrapping) */
                        const char *preview = content;
                        if (strlen(content) > 200) {
                            char preview_buf[256];
                            snprintf(preview_buf, sizeof(preview_buf), "%.200s...", content);
                            json_object_set(s, "preview", json_new_string(preview_buf));
                        } else {
                            json_object_set(s, "preview", json_new_string(content));
                        }
                        free(content);
                    }
                    json_array_append(sessions, s);
                }
                pclose(fp);
                json_object_set(result, "query", json_new_string(query));
                json_object_set(result, "count", json_new_number((double)json_array_count(sessions)));
                json_object_set(result, "sessions_dir", json_new_string(sdir));
                json_object_set(result, "results", sessions);
            }
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
