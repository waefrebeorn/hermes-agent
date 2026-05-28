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

static void ensure_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    /* Create full path including HERMES_HOME if needed */
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s/.hermes", home);
    /* First ensure parent exists */
    char parent[4096];
    snprintf(parent, sizeof(parent), "%s", home);
    mkdir(parent, 0755);
    mkdir(buf, 0755);
}

static void test_add(void) {
    char *r = todo_handler("{\"action\":\"add\",\"content\":\"test item\",\"priority\":\"p0\"}", NULL);
    assert(r != NULL);
    /* Should create and return with summary */
    assert(strstr(r, "\"summary\"") != NULL);
    assert(strstr(r, "\"count\"") != NULL);
    assert(strstr(r, "\"pending\"") != NULL);
    assert(strstr(r, "\"test item\"") != NULL);
    free(r);
}

static void test_add_missing_content(void) {
    char *r = todo_handler("{\"action\":\"add\"}", NULL);
    assert(r != NULL);
    assert(strstr(r, "\"error\"") != NULL);
    assert(strstr(r, "Missing content") != NULL);
    free(r);
}

static void test_write_replace(void) {
    char *r = todo_handler(
        "{\"action\":\"write\",\"todos\":["
        "{\"id\":\"a\",\"content\":\"task a\",\"status\":\"pending\"},"
        "{\"id\":\"b\",\"content\":\"task b\",\"status\":\"in_progress\"}"
        "]}", NULL);
    assert(r != NULL);
    assert(strstr(r, "\"count\":2") != NULL);
    assert(strstr(r, "\"in_progress\"") != NULL);
    free(r);
}

static void test_merge(void) {
    /* Write initial state */
    char *r = todo_handler(
        "{\"action\":\"write\",\"todos\":["
        "{\"id\":\"x\",\"content\":\"keep me\",\"status\":\"pending\"}"
        "]}", NULL);
    assert(r != NULL);
    free(r);

    /* Merge: update existing + add new */
    r = todo_handler(
        "{\"action\":\"merge\",\"todos\":["
        "{\"id\":\"x\",\"content\":\"keep me updated\",\"status\":\"in_progress\"},"
        "{\"id\":\"y\",\"content\":\"new task\",\"status\":\"pending\"}"
        "]}", NULL);
    assert(r != NULL);
    assert(strstr(r, "\"count\":2") != NULL);
    assert(strstr(r, "\"keep me updated\"") != NULL);
    free(r);
}

static void test_in_progress_status(void) {
    char *r = todo_handler(
        "{\"action\":\"write\",\"todos\":["
        "{\"id\":\"z\",\"content\":\"active\",\"status\":\"in_progress\"}"
        "]}", NULL);
    assert(r != NULL);
    assert(strstr(r, "\"in_progress\"") != NULL);
    /* Check summary */
    assert(strstr(r, "\"in_progress\":1") != NULL);
    free(r);
}

static void test_cancelled_status(void) {
    char *r = todo_handler(
        "{\"action\":\"write\",\"todos\":["
        "{\"id\":\"c\",\"content\":\"cancel this\",\"status\":\"cancelled\"}"
        "]}", NULL);
    assert(r != NULL);
    assert(strstr(r, "\"cancelled\"") != NULL);
    free(r);
}

static void test_summary_counts(void) {
    char *r = todo_handler(
        "{\"action\":\"write\",\"todos\":["
        "{\"id\":\"p1\",\"content\":\"p1\",\"status\":\"pending\"},"
        "{\"id\":\"p2\",\"content\":\"p2\",\"status\":\"in_progress\"},"
        "{\"id\":\"p3\",\"content\":\"p3\",\"status\":\"completed\"},"
        "{\"id\":\"p4\",\"content\":\"p4\",\"status\":\"cancelled\"}"
        "]}", NULL);
    assert(r != NULL);
    assert(strstr(r, "\"pending\":1") != NULL);
    assert(strstr(r, "\"in_progress\":1") != NULL);
    assert(strstr(r, "\"completed\":1") != NULL);
    assert(strstr(r, "\"cancelled\":1") != NULL);
    assert(strstr(r, "\"total\":4") != NULL);
    free(r);
}

static void test_done_with_id(void) {
    char *r = todo_handler("{\"action\":\"write\",\"todos\":[]}", NULL);
    free(r);

    r = todo_handler("{\"action\":\"add\",\"content\":\"do me\"}", NULL);
    assert(r != NULL);
    free(r);

    /* List to find the id */
    r = todo_handler("{\"action\":\"list\"}", NULL);
    assert(r != NULL);
    const char *id_start = strstr(r, "\"id\":\"");
    assert(id_start != NULL);
    id_start += 6;
    const char *id_end = strchr(id_start, '"');
    assert(id_end != NULL);

    char done_args[256];
    snprintf(done_args, sizeof(done_args), "{\"action\":\"done\",\"id\":\"%.*s\"}",
             (int)(id_end - id_start), id_start);
    char *r2 = todo_handler(done_args, NULL);
    assert(r2 != NULL);
    assert(strstr(r2, "\"completed\"") != NULL);
    free(r2);
    free(r);
}

int main(void) {
    ensure_home();

    /* Clean slate */
    char *r = todo_handler("{\"action\":\"write\",\"todos\":[]}", NULL);
    free(r);

    printf("todo tests:\n");
    TEST(test_add());
    TEST(test_add_missing_content());
    TEST(test_write_replace());
    TEST(test_merge());
    TEST(test_summary_counts());
    TEST(test_in_progress_status());
    TEST(test_cancelled_status());
    TEST(test_done_with_id());

    printf("All todo tests passed.\n");
    return 0;
}
