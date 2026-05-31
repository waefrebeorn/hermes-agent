/* test_todo.c — Unit tests for todo handler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

/* Forward declarations from todo.c */
extern char *todo_handler(const char *args_json, const char *task_id);
extern void registry_init_todo(void);

#define TEST(fn) do { \
    printf("  %s ... ", #fn); \
    fn; \
    printf("PASS\n"); \
} while(0)

static int g_test_count = 0;
static void check(int cond, const char *msg, const char *got) {
    g_test_count++;
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        if (got) fprintf(stderr, "  got: %s\n", got);
        exit(1);
    }
}

static void ensure_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s/.hermes", home);
    char parent[4096];
    snprintf(parent, sizeof(parent), "%s", home);
    mkdir(parent, 0755);
    mkdir(buf, 0755);
}

/* Reset todos to empty */
static void reset_todos(void) {
    char *r = todo_handler("{\"action\":\"write\",\"todos\":[]}", NULL);
    free(r);
}

static void test_add(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"add\",\"content\":\"test item\",\"priority\":\"p0\"}", NULL);
    check(r != NULL && strstr(r, "\"summary\"") && strstr(r, "\"count\"") && strstr(r, "\"test item\""), "add basic", r);
    free(r);
}

static void test_add_missing_content(void) {
    char *r = todo_handler("{\"action\":\"add\"}", NULL);
    check(r != NULL && strstr(r, "\"error\"") && strstr(r, "Missing content"), "add missing content", r);
    free(r);
}

static void test_write_replace(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"write\",\"todos\":["
        "{\"id\":\"a\",\"content\":\"task a\",\"status\":\"pending\"},"
        "{\"id\":\"b\",\"content\":\"task b\",\"status\":\"in_progress\"}"
    "]}", NULL);
    check(r != NULL && strstr(r, "\"count\":2") && strstr(r, "\"in_progress\""), "write replace 2 items", r);
    free(r);
}

static void test_merge(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"write\",\"todos\":["
        "{\"id\":\"x\",\"content\":\"keep me\",\"status\":\"pending\"}"
    "]}", NULL);
    free(r);
    r = todo_handler("{\"action\":\"merge\",\"todos\":["
        "{\"id\":\"x\",\"content\":\"keep me updated\",\"status\":\"in_progress\"},"
        "{\"id\":\"y\",\"content\":\"new task\",\"status\":\"pending\"}"
    "]}", NULL);
    check(r != NULL && strstr(r, "\"count\":2") && strstr(r, "\"keep me updated\""), "merge", r);
    free(r);
}

static void test_in_progress_status(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"write\",\"todos\":["
        "{\"id\":\"z\",\"content\":\"active\",\"status\":\"in_progress\"}"
    "]}", NULL);
    check(r != NULL && strstr(r, "\"in_progress\"") && strstr(r, "\"in_progress\":1"), "in_progress status", r);
    free(r);
}

static void test_cancelled_status(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"write\",\"todos\":["
        "{\"id\":\"c\",\"content\":\"cancel this\",\"status\":\"cancelled\"}"
    "]}", NULL);
    check(r != NULL && strstr(r, "\"cancelled\""), "cancelled status", r);
    free(r);
}

static void test_summary_counts(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"write\",\"todos\":["
        "{\"id\":\"p1\",\"content\":\"p1\",\"status\":\"pending\"},"
        "{\"id\":\"p2\",\"content\":\"p2\",\"status\":\"in_progress\"},"
        "{\"id\":\"p3\",\"content\":\"p3\",\"status\":\"completed\"},"
        "{\"id\":\"p4\",\"content\":\"p4\",\"status\":\"cancelled\"}"
    "]}", NULL);
    check(r != NULL && strstr(r, "\"pending\":1") && strstr(r, "\"in_progress\":1")
          && strstr(r, "\"completed\":1") && strstr(r, "\"cancelled\":1") && strstr(r, "\"total\":4"),
          "summary counts", r);
    free(r);
}

static void test_done_with_id(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"add\",\"content\":\"do me\"}", NULL);
    free(r);
    r = todo_handler("{\"action\":\"list\"}", NULL);
    check(r != NULL, "list for done test", r);
    const char *id_start = strstr(r, "\"id\":\"");
    check(id_start != NULL, "found id in list", NULL);
    id_start += 6;
    const char *id_end = strchr(id_start, '"');
    check(id_end != NULL, "found id end", NULL);
    char done_args[256];
    snprintf(done_args, sizeof(done_args), "{\"action\":\"done\",\"id\":\"%.*s\"}", (int)(id_end - id_start), id_start);
    char *r2 = todo_handler(done_args, NULL);
    check(r2 != NULL && strstr(r2, "\"completed\""), "done with id", r2);
    free(r2);
    free(r);
}

static void test_add_with_deadline(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"add\",\"content\":\"deadline task\",\"deadline\":\"2026-06-15\"}", NULL);
    check(r != NULL && strstr(r, "\"deadline\":\"2026-06-15\""), "add with deadline", r);
    free(r);
    r = todo_handler("{\"action\":\"list\"}", NULL);
    check(r != NULL && strstr(r, "\"deadline\":\"2026-06-15\"") && strstr(r, "\"deadline task\""), "deadline persists", r);
    free(r);
}

static void test_update_deadline(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"add\",\"content\":\"update deadline\"}", NULL);
    free(r);
    r = todo_handler("{\"action\":\"list\"}", NULL);
    const char *id_start = strstr(r, "\"id\":\"");
    check(id_start != NULL, "found id for update deadline", NULL);
    id_start += 6;
    const char *id_end = strchr(id_start, '"');
    check(id_end != NULL, "found id end for update deadline", NULL);
    char update_args[256];
    snprintf(update_args, sizeof(update_args), "{\"action\":\"update\",\"id\":\"%.*s\",\"deadline\":\"2026-07-01\"}", (int)(id_end - id_start), id_start);
    char *r2 = todo_handler(update_args, NULL);
    check(r2 != NULL && strstr(r2, "\"deadline\":\"2026-07-01\""), "update deadline", r2);
    free(r2);
    free(r);
}

/* ── New edge cases ── */

static void test_null_args(void) {
    char *r = todo_handler(NULL, NULL);
    check(r != NULL && strstr(r, "No args"), "NULL args returns No args error", r);
    free(r);
}

static void test_invalid_json(void) {
    char *r = todo_handler("not json at all", NULL);
    check(r != NULL && strstr(r, "JSON parse"), "invalid JSON returns JSON parse error", r);
    free(r);
}

static void test_list_empty(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"list\"}", NULL);
    check(r != NULL && strstr(r, "\"count\":0") && strstr(r, "\"items\":["), "list empty with count=0", r);
    free(r);
}

static void test_add_all_priorities(void) {
    reset_todos();
    const char *prios[] = {"p0", "p1", "p3"};
    for (int i = 0; i < 3; i++) {
        char args[256];
        snprintf(args, sizeof(args), "{\"action\":\"add\",\"content\":\"prio_%s\",\"priority\":\"%s\"}", prios[i], prios[i]);
        char *r = todo_handler(args, NULL);
        check(r != NULL && strstr(r, prios[i]), "add with priority", r);
        free(r);
    }
    char *r = todo_handler("{\"action\":\"list\"}", NULL);
    check(r != NULL && strstr(r, "\"count\":3"), "3 items after priority adds", r);
    free(r);
}

static void test_add_invalid_priority(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"add\",\"content\":\"bad prio\",\"priority\":\"p99\"}", NULL);
    /* Invalid priority defaults to p2 */
    check(r != NULL && strstr(r, "\"p2\""), "invalid priority defaults to p2", r);
    free(r);
}

static void test_done_not_found(void) {
    reset_todos();
    char *r = todo_handler("{\"action\":\"done\",\"id\":\"nonexistent\"}", NULL);
    check(r != NULL && strstr(r, "Task not found"), "done nonexistent returns error", r);
    free(r);
}

static void test_update_missing_id(void) {
    char *r = todo_handler("{\"action\":\"update\",\"content\":\"no id\"}", NULL);
    check(r != NULL && strstr(r, "Missing id"), "update without id returns Missing id error", r);
    free(r);
}

static void test_update_not_found(void) {
    char *r = todo_handler("{\"action\":\"update\",\"id\":\"ghost\",\"content\":\"x\"}", NULL);
    check(r != NULL && strstr(r, "Task not found"), "update nonexistent returns Task not found", r);
    free(r);
}

static void test_search_basic(void) {
    reset_todos();
    todo_handler("{\"action\":\"add\",\"content\":\"apple\"}", NULL);
    todo_handler("{\"action\":\"add\",\"content\":\"banana\"}", NULL);
    todo_handler("{\"action\":\"add\",\"content\":\"application\"}", NULL);
    char *r = todo_handler("{\"action\":\"search\",\"query\":\"app\"}", NULL);
    check(r != NULL && strstr(r, "\"count\":2"), "search app finds 2 items (apple, application)", r);
    free(r);
}

static void test_search_no_match(void) {
    reset_todos();
    todo_handler("{\"action\":\"add\",\"content\":\"apple\"}", NULL);
    char *r = todo_handler("{\"action\":\"search\",\"query\":\"zebra\"}", NULL);
    check(r != NULL && strstr(r, "\"count\":0"), "search no match returns count=0", r);
    free(r);
}

static void test_search_status_filter(void) {
    reset_todos();
    todo_handler("{\"action\":\"add\",\"content\":\"important\"}", NULL);
    /* Mark first item as in_progress via done+id approach or just use write */
    todo_handler("{\"action\":\"write\",\"todos\":["
        "{\"id\":\"t1\",\"content\":\"important\",\"status\":\"in_progress\"},"
        "{\"id\":\"t2\",\"content\":\"later\",\"status\":\"pending\"}"
    "]}", NULL);
    char *r = todo_handler("{\"action\":\"search\",\"status\":\"in_progress\"}", NULL);
    check(r != NULL && strstr(r, "\"count\":1") && strstr(r, "important"), "search by status", r);
    free(r);
}

static void test_complete_alias(void) {
    reset_todos();
    todo_handler("{\"action\":\"add\",\"content\":\"alias test\"}", NULL);
    char *r = todo_handler("{\"action\":\"list\"}", NULL);
    const char *id_start = strstr(r, "\"id\":\"");
    if (!id_start) { free(r); check(0, "found id for alias test", NULL); return; }
    id_start += 6;
    const char *id_end = strchr(id_start, '"');
    if (!id_end) { free(r); check(0, "found id end", NULL); return; }
    char args[256];
    snprintf(args, sizeof(args), "{\"action\":\"complete\",\"id\":\"%.*s\"}", (int)(id_end - id_start), id_start);
    char *r2 = todo_handler(args, NULL);
    check(r2 != NULL && strstr(r2, "\"completed\""), "complete alias for done", r2);
    free(r2);
    free(r);
}

static void test_add_long_content(void) {
    reset_todos();
    char long_content[2001];
    memset(long_content, 'B', sizeof(long_content) - 1);
    long_content[sizeof(long_content) - 1] = '\0';
    char args[4200];
    snprintf(args, sizeof(args), "{\"action\":\"add\",\"content\":\"%s\"}", long_content);
    char *r = todo_handler(args, NULL);
    check(r != NULL && strstr(r, "\"count\":1"), "add with 2000-char content", r);
    free(r);
}

int main(void) {
    ensure_home();
    reset_todos();

    printf("todo tests:\n");
    TEST(test_add());
    TEST(test_add_missing_content());
    TEST(test_write_replace());
    TEST(test_merge());
    TEST(test_summary_counts());
    TEST(test_in_progress_status());
    TEST(test_cancelled_status());
    TEST(test_done_with_id());
    TEST(test_add_with_deadline());
    TEST(test_update_deadline());

    printf("--- Edge Cases ---\n");
    TEST(test_null_args());
    TEST(test_invalid_json());
    TEST(test_list_empty());
    TEST(test_add_all_priorities());
    TEST(test_add_invalid_priority());
    TEST(test_done_not_found());
    TEST(test_update_missing_id());
    TEST(test_update_not_found());
    TEST(test_search_basic());
    TEST(test_search_no_match());
    TEST(test_search_status_filter());
    TEST(test_complete_alias());
    TEST(test_add_long_content());

    printf("\nAll todo tests passed.\n");
    return 0;
}
