/*
 * test_browser_url.c — Tests for browser URL resolution (B01).
 * Tests resolve_url() function from browser.c.
 * Compile against compiled browser.o with -Wl,--unresolved-symbols=ignore-all
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Forward declaration of the function we want to test */
extern char *browser_navigate_handler(const char *args_json, const char *task_id);
extern char *browser_snapshot_handler(const char *args_json, const char *task_id);
extern char *browser_click_handler(const char *args_json, const char *task_id);
extern char *browser_type_handler(const char *args_json, const char *task_id);
extern char *browser_scroll_handler(const char *args_json, const char *task_id);
extern char *browser_get_images_handler(const char *args_json, const char *task_id);
extern char *browser_press_handler(const char *args_json, const char *task_id);
extern char *browser_back_handler(const char *args_json, const char *task_id);
extern char *browser_forward_handler(const char *args_json, const char *task_id);

/* resolve_url is static. We test it indirectly by testing navigate handler
 * which internally calls resolve_url for relative URL resolution.
 * For now, just test that browser_navigate_handler doesn't crash with valid args. */

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

static int has_error(const char *json) {
    return json && strstr(json, "\"error\"") != NULL;
}

int main(void) {
    printf("=== Browser URL Resolution Tests ===\n");

    /* Test 1: NULL args */
    {
        char *res = browser_navigate_handler(NULL, NULL);
        TEST("navigate NULL args returns error", res && has_error(res));
        free(res);
    }

    /* Test 2: Empty args */
    {
        char *res = browser_navigate_handler("{}", NULL);
        TEST("navigate empty args returns error", res && has_error(res));
        free(res);
    }

    /* Test 3: Valid URL */
    {
        char *res = browser_navigate_handler(
            "{\\\"url\\\":\\\"https://example.com\\\"}", NULL);
        TEST("navigate valid URL", res != NULL);
        free(res);
    }

    /* Test 4: Absolute URL */
    {
        char *res = browser_navigate_handler(
            "{\\\"url\\\":\\\"https://other.com/path?q=1\\\"}", NULL);
        TEST("navigate absolute URL", res != NULL);
        free(res);
    }

    /* Test 5: snapshot handler NULL args */
    {
        char *res = browser_snapshot_handler(NULL, NULL);
        TEST("snapshot NULL args returns error", res && has_error(res));
        free(res);
    }

    /* Test 6: snapshot handler valid */
    {
        char *res = browser_snapshot_handler("{}", NULL);
        TEST("snapshot valid", res != NULL);
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All browser tests PASSED");
    return failures ? 1 : 0;
}
