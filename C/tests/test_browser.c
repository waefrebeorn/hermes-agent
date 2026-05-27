/* test_browser.c — Tests for browser tool error paths.
 * Tests null/invalid argument handling and edge cases without
 * requiring a live browser or CDP server.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Declare browser functions */
extern void browser_init(void);
extern void browser_cleanup(void);
extern char *browser_navigate_handler(const char *args_json, const char *task_id);
extern char *browser_snapshot_handler(const char *args_json, const char *task_id);
extern char *browser_click_handler(const char *args_json, const char *task_id);
extern char *browser_type_handler(const char *args_json, const char *task_id);
extern char *browser_scroll_handler(const char *args_json, const char *task_id);
extern char *browser_back_handler(const char *args_json, const char *task_id);
extern char *browser_forward_handler(const char *args_json, const char *task_id);
extern char *browser_press_handler(const char *args_json, const char *task_id);
extern char *browser_get_images_handler(const char *args_json, const char *task_id);

static int pass = 0, fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        pass++; \
    } \
} while(0)

static int is_valid_json(const char *s) {
    json_t *j = json_parse(s, NULL);
    if (j) { json_free(j); return 1; }
    return 0;
}

/* All handlers should return valid JSON on null args */
static void test_null_args(void) {
    char *r;

    r = browser_navigate_handler(NULL, NULL);
    TEST("navigate null: not null", r != NULL);
    TEST("navigate null: valid JSON", is_valid_json(r));
    TEST("navigate null: contains error", strstr(r, "error") != NULL);
    free(r);

    r = browser_snapshot_handler(NULL, NULL);
    TEST("snapshot null: not null", r != NULL);
    TEST("snapshot null: valid JSON", is_valid_json(r));
    free(r);

    r = browser_click_handler(NULL, NULL);
    TEST("click null: not null", r != NULL);
    TEST("click null: valid JSON", is_valid_json(r));
    free(r);

    r = browser_type_handler(NULL, NULL);
    TEST("type null: not null", r != NULL);
    TEST("type null: valid JSON", is_valid_json(r));
    free(r);

    r = browser_scroll_handler(NULL, NULL);
    TEST("scroll null: not null", r != NULL);
    TEST("scroll null: valid JSON", is_valid_json(r));
    free(r);

    r = browser_back_handler(NULL, NULL);
    TEST("back null: not null", r != NULL);
    TEST("back null: valid JSON", is_valid_json(r));
    free(r);

    r = browser_forward_handler(NULL, NULL);
    TEST("forward null: not null", r != NULL);
    TEST("forward null: valid JSON", is_valid_json(r));
    free(r);

    r = browser_press_handler(NULL, NULL);
    TEST("press null: not null", r != NULL);
    TEST("press null: valid JSON", is_valid_json(r));
    free(r);

    r = browser_get_images_handler(NULL, NULL);
    TEST("get_images null: not null", r != NULL);
    TEST("get_images null: valid JSON", is_valid_json(r));
    free(r);
}

/* Handlers with empty/invalid JSON */
static void test_invalid_json(void) {
    const char *bad = "{bad json}";
    char *r;

    r = browser_navigate_handler(bad, NULL);
    TEST("navigate bad json: not null", r != NULL);
    TEST("navigate bad json: valid JSON", is_valid_json(r));
    free(r);

    r = browser_snapshot_handler(bad, NULL);
    TEST("snapshot bad json: not null", r != NULL);
    TEST("snapshot bad json: valid JSON", is_valid_json(r));
    free(r);

    r = browser_click_handler(bad, NULL);
    TEST("click bad json: not null", r != NULL);
    TEST("click bad json: valid JSON", is_valid_json(r));
    free(r);
}

/* Navigate with empty URL — should produce error about missing url */
static void test_navigate_empty_url(void) {
    const char *args = "{}";
    char *r = browser_navigate_handler(args, NULL);
    TEST("navigate empty url: not null", r != NULL);
    TEST("navigate empty url: valid JSON", is_valid_json(r));
    json_t *j = json_parse(r, NULL);
    if (j) {
        const char *err = json_get_str(j, "error", "");
        TEST("navigate empty url: mentions missing or url",
             strstr(err, "url") != NULL || strstr(err, "URL") != NULL);
        json_free(j);
    }
    free(r);
}

/* Snapshot without navigation — should handle empty state */
static void test_snapshot_no_page(void) {
    const char *args = "{}";
    char *r = browser_snapshot_handler(args, NULL);
    TEST("snapshot no page: not null", r != NULL);
    TEST("snapshot no page: valid JSON", is_valid_json(r));
    free(r);
}

/* Press without navigation — should handle no focused element */
static void test_press_no_navigation(void) {
    const char *args = "{\"key\":\"Enter\"}";
    char *r = browser_press_handler(args, NULL);
    TEST("press no nav: not null", r != NULL);
    TEST("press no nav: valid JSON", is_valid_json(r));
    free(r);
}

int main(void) {
    browser_init();

    test_null_args();
    test_invalid_json();
    test_navigate_empty_url();
    test_snapshot_no_page();
    test_press_no_navigation();

    browser_cleanup();

    fprintf(stderr, "browser: %d/%d pass\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
