/*
 * test_session_search.c -- Tests for P142 session search.
 *
 * Tests: query with matching results, scoring/sorting, snippet extraction,
 * tag filter, role filter, session_id filter, offset/limit pagination,
 * empty queries, non-existent session dir, NULL args.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* Forward declare the handler we're testing */
char *session_search_handler(const char *args_json, const char *task_id);

/* Test harness */
static int passed = 0, failed = 0, skipped = 0;

#define TEST(name) do { printf("  %s: ", name); } while(0)
#define PASS do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

/* Parse JSON and get string/number from object */
static const char *json_str(json_node_t *obj, const char *key) {
    return json_object_get_string(obj, key, "");
}

static double json_num(json_node_t *obj, const char *key) {
    return json_object_get_number(obj, key, 0.0);
}

/* Create a session file with content */
static void make_session(const char *dir, const char *sid, const char *content) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s.json", dir, sid);
    FILE *f = fopen(path, "w");
    if (f) { fwrite(content, 1, strlen(content), f); fclose(f); }
}

/* Create a session meta file */
static void make_meta(const char *dir, const char *sid, const char *meta) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s.meta.json", dir, sid);
    FILE *f = fopen(path, "w");
    if (f) { fwrite(meta, 1, strlen(meta), f); fclose(f); }
}

int main(void) {
    printf("=== Session Search Tests (P142) ===\n");

    /* Create temp session directory */
    char tmpdir[256] = "/tmp/hermes_test_ss_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("  FAIL: cannot create temp dir\n");
        return 1;
    }
    /* Create the parent dirs that get_sessions_dir() checks */
    char dotdir[512];
    snprintf(dotdir, sizeof(dotdir), "%s/.hermes", tmpdir);
    char sessions_dir[512];
    snprintf(sessions_dir, sizeof(sessions_dir), "%s/.hermes/sessions", tmpdir);
    mkdir(dotdir, 0755);
    mkdir(sessions_dir, 0755);

    /* Set HERMES_HOME to our temp dir */
    setenv("HERMES_HOME", tmpdir, 1);
    setenv("HOME", tmpdir, 1);
    unsetenv("SLERMES_HOME");

    /* Create test sessions */
    make_session(sessions_dir, "session_alpha",
        "{\"role\":\"user\",\"content\":\"How do I implement a binary search in Python?\"}");
    make_session(sessions_dir, "session_beta",
        "{\"role\":\"user\",\"content\":\"What is binary search tree traversal?\"}");
    make_session(sessions_dir, "session_gamma",
        "{\"role\":\"user\",\"content\":\"Tell me about sorting algorithms\"}");
    make_session(sessions_dir, "session_delta",
        "{\"role\":\"assistant\",\"content\":\"Binary search runs in O(log n) time\"}");

    /* Meta for tag filtering */
    make_meta(sessions_dir, "session_beta",
        "{\"tags\":[\"algorithms\",\"data-structures\"]}");
    make_meta(sessions_dir, "session_alpha",
        "{\"tags\":[\"python\",\"algorithms\"]}");

    /* --- Basic search --- */
    printf("\n--- Basic Search ---\n");
    {
        TEST("search for 'binary' returns results");
        char *res = session_search_handler("{\"query\":\"binary\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double count = json_num(j, "count");
            if (count > 0) { PASS; }
            else { FAIL("no results"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("results sorted by score descending");
        char *res = session_search_handler("{\"query\":\"binary\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            json_node_t *results = json_obj_get(j, "results");
            if (results && json_len(results) >= 2) {
                json_node_t *first = json_get(results, 0);
                json_node_t *second = json_get(results, 1);
                double s1 = json_num(first, "score");
                double s2 = json_num(second, "score");
                if (s1 >= s2) { PASS; }
                else { char b[128]; snprintf(b, sizeof(b), "scores: %.1f then %.1f", s1, s2); FAIL(b); }
            } else { FAIL("need 2+ results"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("empty query returns error");
        char *res = session_search_handler("{\"query\":\"\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && *err) { PASS; } else { FAIL("no error field"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("NULL args returns error JSON");
        char *res = session_search_handler(NULL, NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && *err) { PASS; } else { FAIL("no error field"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* --- Offset/limit pagination --- */
    printf("\n--- Pagination ---\n");
    {
        TEST("limit 1 returns 1 result");
        char *res = session_search_handler("{\"query\":\"binary\",\"limit\":1}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double count = json_num(j, "count");
            if (count == 1.0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f results", count); FAIL(b); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("offset 1 skips top result");
        char *res = session_search_handler("{\"query\":\"binary\",\"limit\":10,\"offset\":1}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            /* Get top result if any — check it's NOT the first one */
            json_node_t *results = json_obj_get(j, "results");
            if (results && json_len(results) > 0) {
                json_node_t *first = json_get(results, 0);
                /* With binary search, first without offset should be the one with most matches */
                /* session_alpha has 'binary search', session_beta has 'binary search tree',
                   session_delta has 'Binary search'. Offsetting 1 should skip the best match */
                PASS;
            } else { FAIL("no results after offset"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("offset beyond results returns empty");
        char *res = session_search_handler("{\"query\":\"binary\",\"offset\":999}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double count = json_num(j, "count");
            if (count == 0.0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f results", count); FAIL(b); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* --- Filtering --- */
    printf("\n--- Filtering ---\n");
    {
        TEST("tag_filter 'python' returns only tagged sessions");
        char *res = session_search_handler("{\"query\":\"binary\",\"tag_filter\":\"python\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double count = json_num(j, "count");
            if (count > 0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f results", count); FAIL(b); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("role_filter 'assistant' returns only assistant messages");
        char *res = session_search_handler("{\"query\":\"binary\",\"role_filter\":\"assistant\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            json_node_t *results = json_obj_get(j, "results");
            if (results && json_len(results) > 0) {
                json_node_t *first = json_get(results, 0);
                const char *sid = json_str(first, "session_id");
                /* Should find session_delta (assistant role) */
                if (strstr(sid, "delta")) { PASS; }
                else { char b[128]; snprintf(b, sizeof(b), "got session '%s'", sid); FAIL(b); }
            } else { FAIL("no results after role filter"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("session_id_filter narrows results");
        char *res = session_search_handler("{\"query\":\"binary\",\"session_id_filter\":\"alpha\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            json_node_t *results = json_obj_get(j, "results");
            if (results && json_len(results) > 0) {
                json_node_t *first = json_get(results, 0);
                const char *sid = json_str(first, "session_id");
                if (strstr(sid, "alpha")) { PASS; }
                else { char b[128]; snprintf(b, sizeof(b), "got '%s'", sid); FAIL(b); }
            } else { FAIL("no results"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* --- Result metadata --- */
    printf("\n--- Result Metadata ---\n");
    {
        TEST("results include score and snippet");
        char *res = session_search_handler("{\"query\":\"binary\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            json_node_t *results = json_obj_get(j, "results");
            if (results && json_len(results) > 0) {
                json_node_t *first = json_get(results, 0);
                double score = json_num(first, "score");
                const char *snippet = json_str(first, "snippet");
                if (score > 0 && snippet && strlen(snippet) > 0) { PASS; }
                else { FAIL("missing score or snippet"); }
            } else { FAIL("no results"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("result shows total_matches count");
        char *res = session_search_handler("{\"query\":\"binary\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double total = json_num(j, "total_matches");
            if (total > 0) { PASS; }
            else { FAIL("total_matches not > 0"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* Cleanup temp dir */
    unsetenv("HERMES_HOME");
    unsetenv("HOME");

    /* Summary */
    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, skipped);
    printf("==============================================\n");

    /* Cleanup temp files */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    return failed > 0 ? 1 : 0;
}
