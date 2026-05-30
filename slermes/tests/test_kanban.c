/*
 * test_kanban.c — Kanban tool unit tests (M41).
 *
 * Tests all 9 kanban tools via registry dispatch:
 * create, show, list, complete, block, unblock, comment, heartbeat, link.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_kanban.c src/tools/kanban.c src/tools/registry.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_kanban -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   SLERMES_HOME=/tmp/hermes_test_kanban_home /tmp/hermes_test_kanban
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Kanban registry init — registers all 9 static handlers */
extern void registry_init_kanban(void);
/* Registry dispatch — find tool by name and call handler */
extern char *registry_dispatch(const char *name, const char *args_json,
                                const char *task_id);

static json_node_t *parse_result(const char *json_str) {
    char *err = NULL;
    json_node_t *r = json_parse(json_str, &err);
    if (!r) fprintf(stderr, "  JSON parse error: %s\n", err ? err : "unknown");
    free(err);
    return r;
}

static void cleanup_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    char path[4096];
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        unlink(path);
    }
    closedir(d);
    rmdir(dir);
}

static void mkdir_p(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

int main(void) {
    printf("=== Kanban Tool Tests ===\n");

    /* Isolate test from user's real kanban data */
    const char *test_home = "/tmp/hermes_test_kanban_home";
    setenv("SLERMES_HOME", test_home, 1);
    /* Unset KANBAN_TASK so orchestrator guards (list, unblock) pass */
    unsetenv("HERMES_KANBAN_TASK");

    /* Clean any previous run */
    mkdir_p(test_home);
    char kanban_dir[8192];
    snprintf(kanban_dir, sizeof(kanban_dir), "%s/kanban", test_home);
    cleanup_dir(kanban_dir);
    char links_path[8192];
    snprintf(links_path, sizeof(links_path), "%s/links.json", kanban_dir);
    unlink(links_path);

    /* Register all kanban tools */
    registry_init_kanban();

    json_node_t *r;
    const char *s;

    /* ===== 1. Create task — missing title ===== */
    r = parse_result(registry_dispatch("kanban_create",
        "{\"assignee\":\"worker\"}", NULL));
    s = json_object_get_string(r, "error", "");
    TEST("create missing title returns error", strlen(s) > 0);
    json_free(r);

    /* ===== 2. Create task — missing assignee ===== */
    r = parse_result(registry_dispatch("kanban_create",
        "{\"title\":\"My task\"}", NULL));
    s = json_object_get_string(r, "error", "");
    TEST("create missing assignee returns error", strlen(s) > 0);
    json_free(r);

    /* ===== 3. Create first task ===== */
    r = parse_result(registry_dispatch("kanban_create",
        "{\"title\":\"Fix login bug\",\"assignee\":\"worker\","
        "\"body\":\"The login form crashes on empty password\",\"priority\":1}", NULL));
    TEST("create task 1 returns ok", json_object_get_bool(r, "ok", false));
    TEST("create task 1 status is running",
         strcmp(json_object_get_string(r, "status", ""), "running") == 0);
    const char *tid1 = strdup(json_object_get_string(r, "task_id", ""));
    json_free(r);

    /* ===== 4. Create second task ===== */
    r = parse_result(registry_dispatch("kanban_create",
        "{\"title\":\"Write unit tests\",\"assignee\":\"worker\","
        "\"body\":\"Add test coverage for all modules\","
        "\"priority\":2,\"tenant\":\"test\"}", NULL));
    TEST("create task 2 returns ok", json_object_get_bool(r, "ok", false));
    TEST("create task 2 has task_id",
         strlen(json_object_get_string(r, "task_id", "")) > 0);
    const char *tid2 = strdup(json_object_get_string(r, "task_id", ""));
    json_free(r);
    TEST("tid1 != tid2", strcmp(tid1, tid2) != 0);

    /* ===== 5. Show task 1 ===== */
    char show_args[256];
    snprintf(show_args, sizeof(show_args), "{\"task_id\":\"%s\"}", tid1);
    r = parse_result(registry_dispatch("kanban_show", show_args, NULL));
    json_node_t *task = json_object_get(r, "task");
    TEST("show task 1 has task key", task != NULL);
    if (task) {
        s = json_object_get_string(task, "title", "");
        TEST("show task 1 title matches", strcmp(s, "Fix login bug") == 0);
        s = json_object_get_string(task, "assignee", "");
        TEST("show task 1 assignee matches", strcmp(s, "worker") == 0);
        s = json_object_get_string(task, "status", "");
        TEST("show task 1 status is running", strcmp(s, "running") == 0);
        s = json_object_get_string(task, "body", "");
        TEST("show task 1 body matches",
             strcmp(s, "The login form crashes on empty password") == 0);
        double pri = json_object_get_number(task, "priority", -1);
        TEST("show task 1 priority is 1", pri == 1.0);
    }
    json_free(r);

    /* ===== 6. Show non-existent task ===== */
    r = parse_result(registry_dispatch("kanban_show",
        "{\"task_id\":\"nonexistent-id\"}", NULL));
    s = json_object_get_string(r, "error", "");
    TEST("show non-existent returns error", strlen(s) > 0);
    json_free(r);

    /* ===== 7. List tasks ===== */
    r = parse_result(registry_dispatch("kanban_list", "{}", NULL));
    double count = json_object_get_number(r, "count", -1);
    TEST("list returns count >= 2", count >= 2.0);
    json_free(r);

    /* ===== 8. Block task 1 ===== */
    snprintf(show_args, sizeof(show_args),
             "{\"task_id\":\"%s\",\"reason\":\"Need database access\"}", tid1);
    r = parse_result(registry_dispatch("kanban_block", show_args, NULL));
    TEST("block task 1 returns ok", json_object_get_bool(r, "ok", false));
    s = json_object_get_string(r, "status", "");
    TEST("block task 1 status is blocked", strcmp(s, "blocked") == 0);
    json_free(r);

    /* ===== 9. Show blocked task ===== */
    snprintf(show_args, sizeof(show_args), "{\"task_id\":\"%s\"}", tid1);
    r = parse_result(registry_dispatch("kanban_show", show_args, NULL));
    task = json_object_get(r, "task");
    if (task) {
        s = json_object_get_string(task, "status", "");
        TEST("show blocked task status is blocked", strcmp(s, "blocked") == 0);
    }
    json_free(r);

    /* ===== 10. Unblock task 1 ===== */
    snprintf(show_args, sizeof(show_args), "{\"task_id\":\"%s\"}", tid1);
    r = parse_result(registry_dispatch("kanban_unblock", show_args, NULL));
    TEST("unblock task 1 returns ok", json_object_get_bool(r, "ok", false));
    s = json_object_get_string(r, "status", "");
    TEST("unblock task 1 status is ready", strcmp(s, "ready") == 0);
    json_free(r);

    /* ===== 11. Show unblocked task ===== */
    snprintf(show_args, sizeof(show_args), "{\"task_id\":\"%s\"}", tid1);
    r = parse_result(registry_dispatch("kanban_show", show_args, NULL));
    task = json_object_get(r, "task");
    if (task) {
        s = json_object_get_string(task, "status", "");
        TEST("show unblocked task status is ready", strcmp(s, "ready") == 0);
    }
    json_free(r);

    /* ===== 12. Complete task 1 ===== */
    snprintf(show_args, sizeof(show_args),
             "{\"task_id\":\"%s\",\"summary\":\"Fixed login validation\"}",
             tid1);
    r = parse_result(registry_dispatch("kanban_complete", show_args, NULL));
    TEST("complete task 1 returns ok", json_object_get_bool(r, "ok", false));
    s = json_object_get_string(r, "status", "");
    TEST("complete task 1 status is done", strcmp(s, "done") == 0);
    json_free(r);

    /* ===== 13. Show completed task ===== */
    snprintf(show_args, sizeof(show_args), "{\"task_id\":\"%s\"}", tid1);
    r = parse_result(registry_dispatch("kanban_show", show_args, NULL));
    task = json_object_get(r, "task");
    if (task) {
        s = json_object_get_string(task, "status", "");
        TEST("show completed task status is done", strcmp(s, "done") == 0);
    }
    json_free(r);

    /* ===== 14. Comment on task 2 ===== */
    snprintf(show_args, sizeof(show_args),
             "{\"task_id\":\"%s\",\"body\":\"Looking good, keep it up\"}", tid2);
    r = parse_result(registry_dispatch("kanban_comment", show_args, NULL));
    TEST("comment returns ok", json_object_get_bool(r, "ok", false));
    json_free(r);

    /* ===== 15. Show task 2 after comment ===== */
    snprintf(show_args, sizeof(show_args), "{\"task_id\":\"%s\"}", tid2);
    r = parse_result(registry_dispatch("kanban_show", show_args, NULL));
    /* Verify comment was recorded */
    json_node_t *comments = json_object_get(r, "comments");
    TEST("task 2 has comments",
         comments && comments->type == JSON_ARRAY);
    if (comments) {
        size_t n = json_len(comments);
        TEST("task 2 has at least 1 comment", n >= 1);
    }
    json_free(r);

    /* ===== 16. Heartbeat on task 2 ===== */
    snprintf(show_args, sizeof(show_args), "{\"task_id\":\"%s\"}", tid2);
    r = parse_result(registry_dispatch("kanban_heartbeat", show_args, NULL));
    TEST("heartbeat returns ok", json_object_get_bool(r, "ok", false));
    json_free(r);

    /* ===== 17. Link task 1 → task 2 ===== */
    char link_args[512];
    snprintf(link_args, sizeof(link_args),
             "{\"parent_id\":\"%s\",\"child_id\":\"%s\"}", tid1, tid2);
    r = parse_result(registry_dispatch("kanban_link", link_args, NULL));
    TEST("link task1->task2 returns ok", json_object_get_bool(r, "ok", false));
    s = json_object_get_string(r, "parent_id", "");
    TEST("link parent_id matches tid1", strcmp(s, tid1) == 0);
    s = json_object_get_string(r, "child_id", "");
    TEST("link child_id matches tid2", strcmp(s, tid2) == 0);
    json_free(r);

    /* ===== 18. Self-link should fail ===== */
    snprintf(link_args, sizeof(link_args),
             "{\"parent_id\":\"%s\",\"child_id\":\"%s\"}", tid1, tid1);
    r = parse_result(registry_dispatch("kanban_link", link_args, NULL));
    s = json_object_get_string(r, "error", "");
    TEST("self-link returns error", strlen(s) > 0);
    json_free(r);

    /* ===== 19. Comment with no body ===== */
    snprintf(show_args, sizeof(show_args), "{\"task_id\":\"%s\"}", tid2);
    r = parse_result(registry_dispatch("kanban_comment", show_args, NULL));
    s = json_object_get_string(r, "error", "");
    TEST("comment missing body returns error", strlen(s) > 0);
    json_free(r);

    /* ===== 20. Block non-existent task ===== */
    r = parse_result(registry_dispatch("kanban_block",
        "{\"task_id\":\"nonexistent\",\"reason\":\"test\"}", NULL));
    s = json_object_get_string(r, "error", "");
    TEST("block non-existent returns error", strlen(s) > 0);
    json_free(r);

    /* ===== 21. Complete with no task_id (defaults to env) ===== */
    /* HERMES_KANBAN_TASK is unset, so should error */
    r = parse_result(registry_dispatch("kanban_complete",
        "{\"summary\":\"test\"}", NULL));
    s = json_object_get_string(r, "error", "");
    TEST("complete no task_id without env returns error", strlen(s) > 0);
    json_free(r);

    /* ===== 22. List with status filter ===== */
    r = parse_result(registry_dispatch("kanban_list",
        "{\"status\":\"done\"}", NULL));
    count = json_object_get_number(r, "count", -1);
    TEST("list done tasks returns count >= 1", count >= 1.0);
    json_free(r);

    /* ===== 23. List with assignee filter ===== */
    r = parse_result(registry_dispatch("kanban_list",
        "{\"assignee\":\"worker\"}", NULL));
    count = json_object_get_number(r, "count", -1);
    TEST("list assignee worker returns count >= 2", count >= 2.0);
    json_free(r);

    /* ===== 24. Show task with default task_id (no env set) ===== */
    r = parse_result(registry_dispatch("kanban_show", "{}", NULL));
    s = json_object_get_string(r, "error", "");
    TEST("show no task_id without env returns error", strlen(s) > 0);
    json_free(r);

    /* Clean up */
    cleanup_dir(kanban_dir);
    unlink(links_path);
    rmdir(test_home);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
