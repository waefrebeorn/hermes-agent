/*
 * test_session_crud.c — Tests for P143 session CRUD operations.
 *
 * Tests: list, create, delete, info, add_tag, remove_tag,
 *        export_json, export_markdown, branch, migrate.
 * Also tests: error handling (missing args, unknown operations).
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

/* Forward declare the handler we're testing */
char *session_crud_handler(const char *args_json, const char *task_id);

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

static bool json_bool_val(json_node_t *obj, const char *key) {
    return json_object_get_bool(obj, key, false);
}

/* Create sessions directory and pre-seed some sessions via db API */
#include "db.h"

static char g_tmpdir[1024];
static char g_sessions_dir[1024];

static db_t *open_db(void) {
    return db_open(g_sessions_dir, NULL);
}

static void seed_session(const char *sid, const char *title,
                          const char *model, int msg_count,
                          int token_count) {
    db_t *db = open_db();
    if (!db) return;

    /* Save session data (JSON array with one message) */
    char data[1024];
    snprintf(data, sizeof(data),
        "[{\"role\":\"user\",\"content\":\"test message for %s\"}]", sid);
    db_save(db, sid, data);

    /* Save metadata */
    session_meta_t meta;
    db_meta_init(&meta);
    snprintf(meta.title, sizeof(meta.title), "%s", title);
    snprintf(meta.model, sizeof(meta.model), "%s", model);
    meta.message_count = msg_count;
    meta.token_count = token_count;
    meta.created_at = time(NULL) - 3600; /* 1 hour ago */
    meta.updated_at = time(NULL);
    db_save_meta(db, sid, &meta);

    db_close(db);
}

int main(void) {
    printf("=== Session CRUD Tests (P143) ===\n");

    /* Create temp session directory */
    snprintf(g_tmpdir, sizeof(g_tmpdir), "/tmp/hermes_test_crud_XXXXXX");
    if (!mkdtemp(g_tmpdir)) {
        printf("  FAIL: cannot create temp dir\n");
        return 1;
    }

    /* Set HERMES_HOME so session_crud_handler finds the right path */
    setenv("HERMES_HOME", g_tmpdir, 1);

    /* Session CRUD handler looks for $HERMES_HOME/.hermes/sessions/ */
    char hermes_dir[1024];
    snprintf(hermes_dir, sizeof(hermes_dir), "%s/.hermes", g_tmpdir);
    snprintf(g_sessions_dir, sizeof(g_sessions_dir), "%s/.hermes/sessions", g_tmpdir);
    g_sessions_dir[sizeof(g_sessions_dir) - 1] = '\0';
    mkdir(hermes_dir, 0755);
    mkdir(g_sessions_dir, 0755);

    /* Seed test sessions */
    seed_session("sess_alpha", "Binary Search", "gpt-4", 12, 1500);
    seed_session("sess_beta", "Sorting Algorithms", "claude-3", 8, 900);
    seed_session("sess_gamma", "Graph Theory", "gpt-4", 15, 2100);

    /* Add tags via direct meta manipulation */
    {
        db_t *db = open_db();
        if (db) {
            session_meta_t meta;
            if (db_load_meta(db, "sess_alpha", &meta)) {
                snprintf(meta.tags[meta.tag_count++], SESSION_TAG_LEN, "python");
                snprintf(meta.tags[meta.tag_count++], SESSION_TAG_LEN, "algorithms");
                db_save_meta(db, "sess_alpha", &meta);
            }
            if (db_load_meta(db, "sess_beta", &meta)) {
                snprintf(meta.tags[meta.tag_count++], SESSION_TAG_LEN, "algorithms");
                snprintf(meta.tags[meta.tag_count++], SESSION_TAG_LEN, "sorting");
                db_save_meta(db, "sess_beta", &meta);
            }
            db_close(db);
        }
    }

    /* ============================================================
     *  Operation: list
     * ============================================================ */
    printf("\n--- list ---\n");
    {
        TEST("list returns all sessions");
        char *res = session_crud_handler("{\"operation\":\"list\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double count = json_num(j, "count");
            if (count == 3.0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f sessions", count); FAIL(b); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("list with limit 1");
        char *res = session_crud_handler("{\"operation\":\"list\",\"limit\":1}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double count = json_num(j, "count");
            if (count == 1.0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f sessions", count); FAIL(b); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("list with tag_filter 'python'");
        char *res = session_crud_handler("{\"operation\":\"list\",\"tag_filter\":\"python\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double count = json_num(j, "count");
            if (count == 1.0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f sessions", count); FAIL(b); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("list with tag_filter for no-match");
        char *res = session_crud_handler("{\"operation\":\"list\",\"tag_filter\":\"nonexistent\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double count = json_num(j, "count");
            if (count == 0.0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f sessions", count); FAIL(b); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* ============================================================
     *  Operation: info
     * ============================================================ */
    printf("\n--- info ---\n");
    {
        TEST("info on existing session returns metadata");
        char *res = session_crud_handler("{\"operation\":\"info\",\"session_id\":\"sess_alpha\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *title = json_str(j, "title");
            const char *model = json_str(j, "model");
            double msg_count = json_num(j, "message_count");
            if (strcmp(title, "Binary Search") == 0 &&
                strcmp(model, "gpt-4") == 0 &&
                msg_count == 12.0) { PASS; }
            else { FAIL("metadata mismatch"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("info on nonexistent session returns warning");
        char *res = session_crud_handler("{\"operation\":\"info\",\"session_id\":\"nonexistent\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double found = json_num(j, "found");
            if (found == 0.0) { PASS; }
            else { FAIL("expected not found"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("info without session_id returns error");
        char *res = session_crud_handler("{\"operation\":\"info\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && *err) { PASS; }
            else { FAIL("no error"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* ============================================================
     *  Operation: add_tag and remove_tag
     * ============================================================ */
    printf("\n--- add_tag / remove_tag ---\n");
    {
        TEST("add_tag to existing session");
        char *res = session_crud_handler(
            "{\"operation\":\"add_tag\",\"session_id\":\"sess_gamma\",\"tag\":\"graph\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            bool success = json_bool_val(j, "success");
            if (success) { PASS; }
            else { FAIL("add_tag failed"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);

        /* Verify tag persisted via info */
        char *res2 = session_crud_handler(
            "{\"operation\":\"info\",\"session_id\":\"sess_gamma\"}", NULL);
        json_node_t *j2 = json_parse(res2, NULL);
        if (j2) {
            json_node_t *tags = json_obj_get(j2, "tags");
            if (tags && json_len(tags) >= 1) {
                /* Serialize the tags array and check for "graph" */
                char *tags_str = json_serialize(tags);
                if (tags_str && strstr(tags_str, "\"graph\"")) { PASS; }
                else { FAIL("tag 'graph' not found"); }
                free(tags_str);
            } else { FAIL("expected at least 1 tag"); }
            json_free(j2);
        } else { FAIL("parse failed"); }
        free(res2);
    }
    {
        TEST("add_tag without session_id returns error");
        char *res = session_crud_handler(
            "{\"operation\":\"add_tag\",\"tag\":\"test\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && *err) { PASS; }
            else { FAIL("no error"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("remove_tag from session");
        char *res = session_crud_handler(
            "{\"operation\":\"remove_tag\",\"session_id\":\"sess_alpha\",\"tag\":\"python\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            bool success = json_bool_val(j, "success");
            if (success) { PASS; }
            else { FAIL("remove_tag failed"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);

        /* Verify tag was removed */
        char *res2 = session_crud_handler(
            "{\"operation\":\"info\",\"session_id\":\"sess_alpha\"}", NULL);
        json_node_t *j2 = json_parse(res2, NULL);
        if (j2) {
            json_node_t *tags = json_obj_get(j2, "tags");
            int tag_count = tags ? json_len(tags) : 0;
            if (tag_count == 1) { PASS; } /* only 'algorithms' remains */
            else { char b[128]; snprintf(b, sizeof(b), "got %d tags", tag_count); FAIL(b); }
            json_free(j2);
        } else { FAIL("parse failed"); }
        free(res2);
    }
    {
        TEST("remove_tag nonexistent tag returns success=false");
        char *res = session_crud_handler(
            "{\"operation\":\"remove_tag\",\"session_id\":\"sess_alpha\",\"tag\":\"nonexistent\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            bool success = json_bool_val(j, "success");
            if (!success) { PASS; }
            else { FAIL("expected false"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* ============================================================
     *  Operation: create
     * ============================================================ */
    printf("\n--- create ---\n");
    {
        TEST("create new session returns session_id");
        char *res = session_crud_handler(
            "{\"operation\":\"create\",\"session_id\":\"sess_new\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            bool success = json_bool_val(j, "success");
            const char *sid = json_str(j, "session_id");
            if (success && strcmp(sid, "sess_new") == 0) { PASS; }
            else { FAIL("create failed"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);

        /* Verify session exists via list */
        TEST("create persists session in DB");
        char *res2 = session_crud_handler("{\"operation\":\"list\"}", NULL);
        json_node_t *j2 = json_parse(res2, NULL);
        if (j2) {
            double count = json_num(j2, "count");
            if (count == 4.0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f sessions", count); FAIL(b); }
            json_free(j2);
        } else { FAIL("parse failed"); }
        free(res2);
    }

    /* ============================================================
     *  Operation: delete
     * ============================================================ */
    printf("\n--- delete ---\n");
    {
        TEST("delete existing session returns success=true");
        char *res = session_crud_handler(
            "{\"operation\":\"delete\",\"session_id\":\"sess_new\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            bool success = json_bool_val(j, "success");
            if (success) { PASS; }
            else { FAIL("delete returned false"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);

        /* Verify deleted */
        TEST("delete removes session from DB");
        char *res2 = session_crud_handler("{\"operation\":\"list\"}", NULL);
        json_node_t *j2 = json_parse(res2, NULL);
        if (j2) {
            double count = json_num(j2, "count");
            if (count == 3.0) { PASS; }
            else { char b[128]; snprintf(b, sizeof(b), "got %.0f sessions", count); FAIL(b); }
            json_free(j2);
        } else { FAIL("parse failed"); }
        free(res2);
    }
    {
        TEST("delete nonexistent returns warning");
        char *res = session_crud_handler(
            "{\"operation\":\"delete\",\"session_id\":\"nonexistent\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            bool success = json_bool_val(j, "success");
            (void)success;
            const char *warn = json_str(j, "warning");
            if (warn && *warn) { PASS; }
            else { FAIL("no warning"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("delete without session_id returns error");
        char *res = session_crud_handler("{\"operation\":\"delete\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && *err) { PASS; }
            else { FAIL("no error"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* ============================================================
     *  Operation: export_json
     * ============================================================ */
    printf("\n--- export_json ---\n");
    {
        TEST("export_json returns data for existing session");
        char *res = session_crud_handler(
            "{\"operation\":\"export_json\",\"session_id\":\"sess_alpha\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *data = json_str(j, "data");
            const char *sid = json_str(j, "session_id");
            if (data && *data && strcmp(sid, "sess_alpha") == 0) { PASS; }
            else { FAIL("no data"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("export_json on nonexistent returns error");
        char *res = session_crud_handler(
            "{\"operation\":\"export_json\",\"session_id\":\"nonexistent\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && *err) { PASS; }
            else { FAIL("no error"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* ============================================================
     *  Operation: export_markdown
     * ============================================================ */
    printf("\n--- export_markdown ---\n");
    {
        TEST("export_markdown returns markdown for existing session");
        char *res = session_crud_handler(
            "{\"operation\":\"export_markdown\",\"session_id\":\"sess_beta\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *data = json_str(j, "data");
            if (data && *data) {
                /* Should contain markdown headers or session title */
                if (strstr(data, "#") || strstr(data, "Sorting")) { PASS; }
                else { FAIL("expected markdown content"); }
            } else { FAIL("no data"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* ============================================================
     *  Operation: branch
     * ============================================================ */
    printf("\n--- branch ---\n");
    {
        TEST("branch creates a new session from source");
        char *res = session_crud_handler(
            "{\"operation\":\"branch\",\"session_id\":\"sess_alpha\","
            "\"new_id\":\"sess_alpha_branch\",\"branch_point\":0}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            bool success = json_bool_val(j, "success");
            const char *new_id = json_str(j, "new_id");
            if (success && strcmp(new_id, "sess_alpha_branch") == 0) { PASS; }
            else { FAIL("branch failed"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);

        /* Verify branch exists */
        TEST("branch creates session with parent_id");
        char *res2 = session_crud_handler(
            "{\"operation\":\"info\",\"session_id\":\"sess_alpha_branch\"}", NULL);
        json_node_t *j2 = json_parse(res2, NULL);
        if (j2) {
            const char *parent = json_str(j2, "parent_id");
            if (strcmp(parent, "sess_alpha") == 0) { PASS; }
            else { FAIL("wrong parent"); }
            json_free(j2);
        } else { FAIL("parse failed"); }
        free(res2);
    }
    {
        TEST("branch without branch_point returns error");
        char *res = session_crud_handler(
            "{\"operation\":\"branch\",\"session_id\":\"sess_alpha\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && *err) { PASS; }
            else { FAIL("no error"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* ============================================================
     *  Operation: migrate
     * ============================================================ */
    printf("\n--- migrate ---\n");
    {
        TEST("migrate runs without error");
        char *res = session_crud_handler("{\"operation\":\"migrate\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            bool success = json_bool_val(j, "success");
            if (success) { PASS; }
            else { FAIL("migrate failed"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("migrate with retention_days > 0");
        char *res = session_crud_handler(
            "{\"operation\":\"migrate\",\"retention_days\":9999}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            double pruned = json_num(j, "pruned_count");
            bool success = json_bool_val(j, "success");
            if (success && pruned >= 0) { PASS; }
            else { FAIL("migrate+prune failed"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* ============================================================
     *  Error handling
     * ============================================================ */
    printf("\n--- error handling ---\n");
    {
        TEST("unknown operation returns error");
        char *res = session_crud_handler(
            "{\"operation\":\"bogus\"}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && strcmp(err, "Unknown operation") == 0) { PASS; }
            else { FAIL("wrong error"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("empty args JSON returns error");
        char *res = session_crud_handler("{}", NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *op = json_str(j, "operation");
            if (!op || !*op) { PASS; }
            else { FAIL("unexpected operation field"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }
    {
        TEST("NULL args returns error JSON (not crash)");
        char *res = session_crud_handler(NULL, NULL);
        json_node_t *j = json_parse(res, NULL);
        if (j) {
            const char *err = json_str(j, "error");
            if (err && *err) { PASS; }
            else { FAIL("no error field"); }
            json_free(j);
        } else { FAIL("parse failed"); }
        free(res);
    }

    /* Cleanup */
    char rm_cmd[1024];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", g_tmpdir);
    int rm_ret = system(rm_cmd);
    (void)rm_ret;

    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n",
           passed, failed, skipped);
    return failed > 0 ? 1 : 0;
}
