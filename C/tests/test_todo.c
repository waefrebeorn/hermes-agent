/*
 * test_todo.c — Todo tool unit tests (M43).
 *
 * Tests todo CRUD operations via the JSON-backed handler:
 * add, list, complete, error paths.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_todo.c src/tools/todo.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_todo -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   HERMES_HOME=/tmp/hermes_test_todo_home /tmp/hermes_test_todo
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

extern char *todo_handler(const char *args_json, const char *task_id);

static json_node_t *parse_result(const char *json_str) {
    char *err = NULL;
    json_node_t *r = json_parse(json_str, &err);
    if (!r) fprintf(stderr, "  JSON parse error: %s\n", err ? err : "unknown");
    free(err);
    return r;
}

static const char *result_get_str(json_node_t *obj, const char *key, const char *def) {
    return json_object_get_string(obj, key, def);
}

int main(void) {
    printf("=== Todo Tool Tests ===\n");

    /* Set HERMES_HOME to isolate test from user's real todo */
    const char *test_home = "/tmp/hermes_test_todo_home";
    setenv("HERMES_HOME", test_home, 1);

    /* Ensure test home dir exists */
    mkdir(test_home, 0755);

    /* Clean up any leftover from previous runs */
    char todo_path[4096];
    snprintf(todo_path, sizeof(todo_path), "%s/.hermes/todo.json", test_home);
    unlink(todo_path);
    /* Remove .hermes dir too */
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/.hermes", test_home);
    rmdir(dir);

    json_node_t *r;

    /* 1. null args */
    r = parse_result(todo_handler(NULL, NULL));
    TEST("todo null args returns error",
         r && strstr(result_get_str(r, "error", ""), "No args") != NULL);
    json_free(r);

    /* 2. list empty */
    r = parse_result(todo_handler("{\"action\":\"list\"}", NULL));
    TEST("todo list empty returns count 0",
         r && json_object_get_number(r, "count", -1) == 0.0);
    json_free(r);

    /* 3. add first task */
    r = parse_result(todo_handler("{\"action\":\"add\",\"content\":\"First task\"}", NULL));
    TEST("todo add first task returns status added",
         r && strcmp(result_get_str(r, "status", ""), "added") == 0);
    json_free(r);

    /* 4. list after one add */
    r = parse_result(todo_handler("{\"action\":\"list\"}", NULL));
    TEST("todo list after add returns count 1",
         r && json_object_get_number(r, "count", -1) == 1.0);
    json_free(r);

    /* 5. add second task */
    r = parse_result(todo_handler("{\"action\":\"add\",\"content\":\"Second task\"}", NULL));
    TEST("todo add second task returns status added",
         r && strcmp(result_get_str(r, "status", ""), "added") == 0);
    json_free(r);

    /* 6. list after two adds */
    r = parse_result(todo_handler("{\"action\":\"list\"}", NULL));
    TEST("todo list after 2 adds returns count 2",
         r && json_object_get_number(r, "count", -1) == 2.0);
    json_free(r);

    /* 7. add with missing content */
    r = parse_result(todo_handler("{\"action\":\"add\"}", NULL));
    TEST("todo add missing content returns error",
         r && strstr(result_get_str(r, "error", ""), "Missing content") != NULL);
    json_free(r);

    /* 8. complete task_0 (first task) */
    r = parse_result(todo_handler("{\"action\":\"done\",\"id\":\"task_0\"}", NULL));
    TEST("todo done task_0 returns found true",
         r && json_object_get_bool(r, "found", false));
    TEST("todo done task_0 returns status updated",
         r && strcmp(result_get_str(r, "status", ""), "updated") == 0);
    json_free(r);

    /* 9. complete non-existent task */
    r = parse_result(todo_handler("{\"action\":\"done\",\"id\":\"task_999\"}", NULL));
    TEST("todo done non-existent returns found false",
         r && !json_object_get_bool(r, "found", true));
    json_free(r);

    /* 10. complete with no id */
    r = parse_result(todo_handler("{\"action\":\"done\"}", NULL));
    TEST("todo done missing id returns found false",
         r && !json_object_get_bool(r, "found", true));
    json_free(r);

    /* 11. default action (list) */
    r = parse_result(todo_handler("{}", NULL));
    TEST("todo default action lists",
         r && json_object_get_number(r, "count", -1) >= 0);
    json_free(r);

    /* 12. complete using 'complete' alias */
    r = parse_result(todo_handler("{\"action\":\"complete\",\"id\":\"task_1\"}", NULL));
    TEST("todo complete alias works",
         r && json_object_get_bool(r, "found", false));
    json_free(r);

    /* 13. list again to verify state */
    r = parse_result(todo_handler("{\"action\":\"list\"}", NULL));
    TEST("todo list final returns count 2",
         r && json_object_get_number(r, "count", -1) == 2.0);
    json_free(r);

    /* Cleanup */
    unlink(todo_path);
    snprintf(dir, sizeof(dir), "%s/.hermes", test_home);
    rmdir(dir);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
