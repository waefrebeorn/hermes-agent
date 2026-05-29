/*
 * session_search.c — Enhanced session search tool for Hermes C.
 * P142: FTS5-like search with ranked results, relevance scoring, snippet generation.
 * P146: Tag filtering support.
 * P147: role_filter, session_id filter, window scroll support.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"query\":{\"type\":\"string\",\"description\":\"Search terms or phrase (omit for browse mode)\"},"
      "\"session_id\":{\"type\":\"string\",\"description\":\"Session ID for scroll mode\"},"
      "\"around_message_id\":{\"type\":\"number\",\"description\":\"Message ID to center scroll window on\"},"
      "\"window\":{\"type\":\"number\",\"description\":\"Messages each side of anchor (scroll mode)\",\"default\":5},"
      "\"limit\":{\"type\":\"number\",\"description\":\"Max results\",\"default\":10},"
      "\"offset\":{\"type\":\"number\",\"description\":\"Result offset for pagination\",\"default\":0},"
      "\"tag_filter\":{\"type\":\"string\",\"description\":\"Filter by tag (substring match)\"},"
      "\"role_filter\":{\"type\":\"string\",\"description\":\"Filter by message role (user/assistant/tool/system)\"},"
      "\"session_id_filter\":{\"type\":\"string\",\"description\":\"Filter by session ID (substring match)\"},"
      "\"min_score\":{\"type\":\"number\",\"description\":\"Minimum relevance score\",\"default\":0}"
    "},"
    "\"required\":[]"
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

    snprintf(buf, sizeof(buf), "%s/.hermes/sessions", home);
    return buf;
}

/* Read a file into a malloc'd string */
static char *read_file_str(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > 1024 * 1024) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Escape single quotes for shell */
__attribute__((unused))
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

/* Simple case-insensitive substring count */
static int count_occurrences(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 0;
    int count = 0;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        if (strncasecmp(p, needle, nlen) == 0) {
            count++;
            p += nlen - 1;
        }
    }
    return count;
}

/* Compute TF-based relevance score for a file.
 * Score = match_count + (match_count / max(1, file_size/1000)) + title_bonus
 * Higher is better. */
static double compute_score(const char *content, const char *query, size_t file_size) {
    if (!content || !query) return 0.0;

    int match_count = count_occurrences(content, query);
    if (match_count == 0) return 0.0;

    /* TF component: normalized by file size */
    double tf = (double)match_count / (1.0 + (double)file_size / 1000.0);

    /* Bonus for matches in first 500 chars (likely title/early content) */
    size_t qlen = strlen(query);
    double title_bonus = 0.0;
    if (strncasecmp(content, query, qlen) == 0)
        title_bonus = 2.0;
    else {
        const char *early = content;
        int early_matches = count_occurrences(early, query);
        title_bonus = (double)early_matches * 0.5;
    }

    return (double)match_count * 2.0 + tf * 10.0 + title_bonus;
}

/* Extract snippet around first match position */
static void extract_snippet(const char *content, const char *query,
                            char *snippet_out, size_t snippet_sz) {
    if (!content || !query || !snippet_out) return;

    size_t qlen = strlen(query);
    const char *match_pos = NULL;

    for (const char *p = content; *p && !match_pos; p++) {
        if (strncasecmp(p, query, qlen) == 0)
            match_pos = p;
    }

    if (match_pos) {
        size_t offset = (size_t)(match_pos - content);
        size_t ctx_start = offset > 60 ? offset - 60 : 0;
        size_t content_len = strlen(content);
        size_t ctx_end = offset + qlen + 120;
        if (ctx_end > content_len) ctx_end = content_len;

        size_t pos = 0;
        if (ctx_start > 0)
            pos += snprintf(snippet_out + pos, snippet_sz - pos, "...");
        size_t copy_len = ctx_end - ctx_start;
        if (copy_len > snippet_sz - pos - 4)
            copy_len = snippet_sz - pos - 4;
        memcpy(snippet_out + pos, content + ctx_start, copy_len);
        pos += copy_len;
        if (ctx_start + copy_len < content_len)
            pos += snprintf(snippet_out + pos, snippet_sz - pos, "...");
        snippet_out[pos] = '\0';
    } else {
        /* Fallback: first 200 chars */
        snprintf(snippet_out, snippet_sz, "%.200s", content);
    }
}

/* Load metadata for a session. Returns true if meta file exists. */
static bool load_session_meta(const char *dir, const char *session_id,
                              char *meta_out, size_t meta_sz) {
    char meta_path[4096];
    snprintf(meta_path, sizeof(meta_path), "%s/%s.meta.json", dir, session_id);
    struct stat st;
    if (stat(meta_path, &st) != 0) return false;

    char *content = read_file_str(meta_path);
    if (!content) return false;
    snprintf(meta_out, meta_sz, "%s", content);
    free(content);
    return true;
}

/* Check if session has a tag (substring match on tags array in metadata) */
static bool session_has_tag(const char *meta_content, const char *tag_filter) {
    if (!meta_content || !tag_filter || !*tag_filter) return true;
    /* Parse tags from metadata JSON */
    const char *tags_field = strstr(meta_content, "\"tags\":");
    if (!tags_field) return false;
    tags_field += 7;
    while (*tags_field == ' ') tags_field++;
    if (*tags_field != '[') return false;
    /* Check if tag_filter appears in the tags array */
    return strstr(meta_content, tag_filter) != NULL;
}

char *session_search_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *query = json_object_get_string(args, "query", NULL);
    int limit = (int)json_object_get_number(args, "limit", 10);
    int offset = (int)json_object_get_number(args, "offset", 0);
    const char *tag_filter = json_object_get_string(args, "tag_filter", NULL);
    const char *role_filter = json_object_get_string(args, "role_filter", NULL);
    const char *session_id_filter = json_object_get_string(args, "session_id_filter", NULL);
    double min_score = json_object_get_number(args, "min_score", 0.0);
    const char *scroll_session = json_object_get_string(args, "session_id", NULL);
    int around_id = (int)json_object_get_number(args, "around_message_id", 0);
    int scroll_window = (int)json_object_get_number(args, "window", 5);

    json_node_t *result = json_new_object();

    /* Three modes inferred from args (single-shape API) */
    if (scroll_session && scroll_session[0] && around_id > 0) {
        /* SCROLL mode: return window around a message in a specific session */
        json_object_set(result, "mode", json_new_string("scroll"));
        json_object_set(result, "session_id", json_new_string(scroll_session));
        json_object_set(result, "around_message_id", json_new_number((double)around_id));

        /* Read session file */
        const char *sdir = get_sessions_dir();
        char session_path[4096];
        snprintf(session_path, sizeof(session_path), "%s/%s.json", sdir, scroll_session);
        FILE *f = fopen(session_path, "rb");
        if (!f) {
            json_object_set(result, "error", json_new_string("Session file not found"));
            json_free(args);
            char *out = json_serialize(result);
            json_free(result);
            return out;
        }
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *fcontent = (char *)malloc((size_t)fsize + 1);
        if (!fcontent) { fclose(f); return strdup("{\"error\":\"OOM\"}"); }
        size_t nread = fread(fcontent, 1, (size_t)fsize, f);
        fclose(f);
        fcontent[nread] = '\0';

        json_node_t *session_data = json_parse(fcontent, &err);
        free(fcontent);
        if (!session_data) {
            free(err);
            json_object_set(result, "error", json_new_string("Session file corrupt"));
            json_free(args);
            char *out = json_serialize(result);
            json_free(result);
            return out;
        }
        free(err);

        /* Find messages array */
        json_node_t *msgs = json_object_get(session_data, "messages");
        if (!msgs || msgs->type != JSON_ARRAY) {
            json_object_set(result, "error", json_new_string("No messages in session"));
            json_free(session_data);
            json_free(args);
            char *out = json_serialize(result);
            json_free(result);
            return out;
        }

        int total_msgs = (int)json_len(msgs);
        int start = around_id - scroll_window;
        if (start < 0) start = 0;
        int end = around_id + scroll_window + 1;
        if (end > total_msgs) end = total_msgs;

        json_node_t *window_arr = json_new_array();
        for (int i = start; i < end; i++) {
            json_node_t *msg = json_copy(json_get(msgs, (size_t)i));
            if (msg) json_array_append(window_arr, msg);
        }

        json_object_set(result, "messages", window_arr);
        json_object_set(result, "messages_before", json_new_number((double)(around_id - start)));
        json_object_set(result, "messages_after", json_new_number((double)(end - around_id - 1)));
        json_object_set(result, "total_messages", json_new_number((double)total_msgs));
        json_free(session_data);
    } else {
        const char *sdir = get_sessions_dir();
        struct stat st;
        if (stat(sdir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            json_object_set(result, "error", json_new_string("Session dir not found"));
            json_object_set(result, "sessions_dir", json_new_string(sdir));
        } else {
            /* BROWSE mode placeholder: empty query returns 0 results */
            if (!query || !*query) {
                json_object_set(result, "count", json_new_number(0));
                json_object_set(result, "mode", json_new_string("browse"));
                json_object_set(result, "results", json_new_array());
            } else {
            /* Read all session files and compute scores */
#define MAX_MATCH_FILES 512
            typedef struct {
                char path[4096];
                char session_id[256];
                double score;
                char snippet[512];
                char meta[2048];
            } match_result_t;

            match_result_t matches[MAX_MATCH_FILES];
            int match_count = 0;

            /* List .json files (not .meta.json) */
            DIR *dir = opendir(sdir);
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL && match_count < MAX_MATCH_FILES) {
                    const char *name = entry->d_name;
                    size_t name_len = strlen(name);
                    if (name_len < 6) continue;
                    if (strcmp(name + name_len - 5, ".json") != 0) continue;
                    /* Skip metadata files */
                    if (name_len > 10 && strcmp(name + name_len - 10, ".meta.json") == 0) continue;

                    /* Extract session ID */
                    char sname_clean[256];
                    snprintf(sname_clean, sizeof(sname_clean), "%s", name);
                    char *dot = strrchr(sname_clean, '.');
                    if (dot) *dot = '\0';

                    /* Apply session_id filter */
                    if (session_id_filter && *session_id_filter) {
                        if (!strstr(sname_clean, session_id_filter))
                            continue;
                    }

                    char full_path[4096];
                    snprintf(full_path, sizeof(full_path), "%s/%s", sdir, name);

                    struct stat fst;
                    if (stat(full_path, &fst) != 0) continue;

                    /* Load content */
                    char *content = read_file_str(full_path);
                    if (!content) continue;

                    /* Apply role_filter on the content */
                    if (role_filter && *role_filter) {
                        /* Search for role string in JSON */
                        char role_search[64];
                        snprintf(role_search, sizeof(role_search), "\"role\":\"%s\"", role_filter);
                        if (!strstr(content, role_search)) {
                            free(content);
                            continue;
                        }
                    }

                    /* Compute relevance score */
                    double score = compute_score(content, query, (size_t)fst.st_size);

                    if (score < min_score) {
                        free(content);
                        continue;
                    }

                    /* Load metadata for tag filtering */
                    char meta_content[2048] = "";
                    load_session_meta(sdir, sname_clean, meta_content, sizeof(meta_content));

                    /* Apply tag filter */
                    if (tag_filter && *tag_filter) {
                        if (!session_has_tag(meta_content, tag_filter)) {
                            free(content);
                            continue;
                        }
                    }

                    /* Insert into sorted position (sort by score descending) */
                    int insert_idx = match_count;
                    for (int i = 0; i < match_count; i++) {
                        if (score > matches[i].score) {
                            insert_idx = i;
                            break;
                        }
                    }

                    /* Shift if inserting in middle */
                    if (insert_idx < match_count) {
                        memmove(&matches[insert_idx + 1], &matches[insert_idx],
                                (size_t)(match_count - insert_idx) * sizeof(match_result_t));
                    }

                    snprintf(matches[insert_idx].path, sizeof(matches[insert_idx].path), "%s", full_path);
                    snprintf(matches[insert_idx].session_id, sizeof(matches[insert_idx].session_id), "%s", sname_clean);
                    matches[insert_idx].score = score;
                    extract_snippet(content, query, matches[insert_idx].snippet, sizeof(matches[insert_idx].snippet));
                    snprintf(matches[insert_idx].meta, sizeof(matches[insert_idx].meta), "%s", meta_content);
                    match_count++;
                    free(content);
                }
                closedir(dir);
            }

            /* Build results with offset/limit pagination */
            json_node_t *sessions = json_new_array();

            for (int i = offset; i < match_count && json_array_count(sessions) < (size_t)limit; i++) {
                json_node_t *s = json_new_object();
                json_object_set(s, "session_id", json_new_string(matches[i].session_id));
                json_object_set(s, "path", json_new_string(matches[i].path));
                json_object_set(s, "score", json_new_number(matches[i].score));
                json_object_set(s, "snippet", json_new_string(matches[i].snippet));

                /* Add metadata fields if available */
                if (matches[i].meta[0]) {
                    char *err2 = NULL;
                    json_node_t *meta_json = json_parse(matches[i].meta, &err2);
                    if (meta_json) {
                        json_object_set(s, "metadata", meta_json);
                    }
                    free(err2);
                }

                json_array_append(sessions, s);
            }

            json_object_set(result, "query", json_new_string(query));
            json_object_set(result, "count", json_new_number((double)json_array_count(sessions)));
            json_object_set(result, "total_matches", json_new_number((double)match_count));
            json_object_set(result, "offset", json_new_number((double)offset));
            json_object_set(result, "limit", json_new_number((double)limit));
            json_object_set(result, "sessions_dir", json_new_string(sdir));
            json_object_set(result, "ranked_by", json_new_string("tf_relevance_score"));
            json_object_set(result, "results", sessions);

            /* Include applied filters in result */
            if (tag_filter && *tag_filter)
                json_object_set(result, "tag_filter", json_new_string(tag_filter));
            if (role_filter && *role_filter)
                json_object_set(result, "role_filter", json_new_string(role_filter));
            if (session_id_filter && *session_id_filter)
                json_object_set(result, "session_id_filter", json_new_string(session_id_filter));
            }  /* close inner else (discovery) */
        }  /* close stat-ok else */
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_session_search(void) {
    registry_register("session_search",
        "Search past conversation sessions with ranked results, snippets, and filters. "
        "Supports tag_filter, role_filter, session_id_filter, and offset/limit pagination.",
        SCHEMA, session_search_handler);
}
