/* test_manual_compression_feedback.c — Tests for compression feedback. */

#include "manual_compression_feedback.h"
#include <stdio.h>
#include <string.h>

static int pass = 0, fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\\n", name, __LINE__); \
        fail++; \
    } else { \
        pass++; \
    } \
} while(0)

#define TEST_STR(name, actual, expected) do { \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (strcmp(_a, _e) != 0) { \
        fprintf(stderr, "FAIL: %s (line %d): got '%s', expected '%s'\\n", \
                name, __LINE__, _a, _e); \
        fail++; \
    } else { pass++; } \
} while(0)

static void test_noop(void) {
    compression_feedback_t fb;
    summarize_manual_compression(42, 42, 15000, 15000, &fb);
    TEST("noop: true", fb.noop);
    TEST_STR("noop: headline contains", fb.headline, "No changes from compression: 42 messages");
    TEST_STR("noop: token unchanged", fb.token_line, "Approx request size: ~15000 tokens (unchanged)");
    TEST("noop: no note", fb.note[0] == '\0');
}

static void test_noop_token_change(void) {
    compression_feedback_t fb;
    summarize_manual_compression(42, 42, 15000, 12000, &fb);
    TEST("noop+tokens: true", fb.noop);
    TEST("noop+tokens: arrow in token line",
         strstr(fb.token_line, "\xe2\x86\x92") != NULL);
}

static void test_compressed(void) {
    compression_feedback_t fb;
    summarize_manual_compression(42, 27, 15000, 5000, &fb);
    TEST("compressed: not noop", !fb.noop);
    TEST("compressed: headline has arrow",
         strstr(fb.headline, "\xe2\x86\x92") != NULL);
    TEST("compressed: note empty (tokens decreased)",
         fb.note[0] == '\0');
}

static void test_counterintuitive_note(void) {
    compression_feedback_t fb;
    /* Fewer messages but MORE tokens */
    summarize_manual_compression(42, 27, 15000, 25000, &fb);
    TEST("counter: not noop", !fb.noop);
    TEST("counter: note present", fb.note[0] != '\0');
    TEST("counter: note contains 'fewer messages'",
         strstr(fb.note, "fewer messages") != NULL);
}

static void test_null_out(void) {
    /* Should not crash */
    summarize_manual_compression(10, 10, 100, 100, NULL);
    pass++; /* survived */
}

static void test_negative_tokens(void) {
    compression_feedback_t fb;
    summarize_manual_compression(10, 10, -1, -1, &fb);
    TEST("negative: noop", fb.noop);
    TEST("negative: no crash", fb.headline[0] != '\0');
}

int main(void) {
    test_noop();
    test_noop_token_change();
    test_compressed();
    test_counterintuitive_note();
    test_null_out();
    test_negative_tokens();

    fprintf(stderr, "manual_compression_feedback: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
