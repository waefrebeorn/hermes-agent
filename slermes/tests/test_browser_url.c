/*
 * test_browser_url.c — Tests for browser tool error paths (B01).
 * Tests NULL/empty args for handlers that work without browser session.
 * Stateful handlers (snapshot, back, forward, get_images) only test NULL.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern char *browser_navigate_handler(const char *args_json, const char *task_id);
extern char *browser_snapshot_handler(const char *args_json, const char *task_id);
extern char *browser_click_handler(const char *args_json, const char *task_id);
extern char *browser_type_handler(const char *args_json, const char *task_id);
extern char *browser_scroll_handler(const char *args_json, const char *task_id);
extern char *browser_get_images_handler(const char *args_json, const char *task_id);
extern char *browser_press_handler(const char *args_json, const char *task_id);
extern char *browser_back_handler(const char *args_json, const char *task_id);
extern char *browser_forward_handler(const char *args_json, const char *task_id);

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

static int has_error(const char *json) {
    return json && strstr(json, "\"error\"") != NULL;
}

int main(void) {
    printf("=== Browser Tool Handler Tests ===\n");

    /* navigate: NULL args → error */
    {
        char *res = browser_navigate_handler(NULL, NULL);
        TEST("browser_navigate NULL args returns error", res && has_error(res));
        free(res);
    }

    /* navigate: empty args → error (needs url) */
    {
        char *res = browser_navigate_handler("{}", NULL);
        TEST("browser_navigate empty args returns error", res && has_error(res));
        free(res);
    }

    /* navigate: valid URL passes through */
    {
        char *res = browser_navigate_handler(
            "{\"url\":\"https://example.com\"}", NULL);
        TEST("browser_navigate valid URL", res != NULL);
        free(res);
    }

    /* navigate: absolute URL passes through */
    {
        char *res = browser_navigate_handler(
            "{\"url\":\"https://other.com/path?q=1\"}", NULL);
        TEST("browser_navigate absolute URL", res != NULL);
        free(res);
    }

    /* navigate: wrong key → error (needs 'url') */
    {
        char *res = browser_navigate_handler(
            "{\"wrong_key\":\"value\"}", NULL);
        TEST("browser_navigate missing url returns error",
             res && has_error(res));
        free(res);
    }

    /* snapshot: NULL args */
    {
        char *res = browser_snapshot_handler(NULL, NULL);
        TEST("browser_snapshot NULL args returns error",
             res && has_error(res));
        free(res);
    }

    /* click: NULL args → error */
    {
        char *res = browser_click_handler(NULL, NULL);
        TEST("browser_click NULL args returns error",
             res && has_error(res));
        free(res);
    }

    /* click: empty args → error (needs target) */
    {
        char *res = browser_click_handler("{}", NULL);
        TEST("browser_click empty args returns error",
             res && has_error(res));
        free(res);
    }

    /* click: missing target → error */
    {
        char *res = browser_click_handler(
            "{\"wrong_key\":\"value\"}", NULL);
        TEST("browser_click missing target returns error",
             res && has_error(res));
        free(res);
    }

    /* type: NULL args → error */
    {
        char *res = browser_type_handler(NULL, NULL);
        TEST("browser_type NULL args returns error",
             res && has_error(res));
        free(res);
    }

    /* type: empty args → error */
    {
        char *res = browser_type_handler("{}", NULL);
        TEST("browser_type empty args returns error",
             res && has_error(res));
        free(res);
    }

    /* type: missing text → error */
    {
        char *res = browser_type_handler(
            "{\"target\":\"#input\"}", NULL);
        TEST("browser_type missing text returns error",
             res && has_error(res));
        free(res);
    }

    /* scroll: NULL args → error */
    {
        char *res = browser_scroll_handler(NULL, NULL);
        TEST("browser_scroll NULL args returns error",
             res && has_error(res));
        free(res);
    }

    /* scroll: empty args → error */
    {
        char *res = browser_scroll_handler("{}", NULL);
        TEST("browser_scroll empty args returns error",
             res && has_error(res));
        free(res);
    }

    /* scroll: missing direction → error */
    {
        char *res = browser_scroll_handler(
            "{\"amount\":100}", NULL);
        TEST("browser_scroll missing direction returns error",
             res && has_error(res));
        free(res);
    }

    /* get_images: NULL args → error */
    {
        char *res = browser_get_images_handler(NULL, NULL);
        TEST("browser_get_images NULL args returns error",
             res && has_error(res));
        free(res);
    }

    /* press: NULL args → error */
    {
        char *res = browser_press_handler(NULL, NULL);
        TEST("browser_press NULL args returns error",
             res && has_error(res));
        free(res);
    }

    /* press: empty args → error (needs key) */
    {
        char *res = browser_press_handler("{}", NULL);
        TEST("browser_press empty args returns error",
             res && has_error(res));
        free(res);
    }

    /* press: invalid JSON → error */
    {
        char *res = browser_press_handler("{bad json}", NULL);
        TEST("browser_press invalid JSON returns error",
             res && has_error(res));
        free(res);
    }

    /* back: NULL args → error */
    {
        char *res = browser_back_handler(NULL, NULL);
        TEST("browser_back NULL args returns error",
             res && has_error(res));
        free(res);
    }

    /* forward: NULL args → error */
    {
        char *res = browser_forward_handler(NULL, NULL);
        TEST("browser_forward NULL args returns error",
             res && has_error(res));
        free(res);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All browser tests PASSED");
    return failures ? 1 : 0;
}
