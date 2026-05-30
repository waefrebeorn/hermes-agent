/*
 * test_think_scrubber.c — Quick smoke test for think_scrubber.
 *
 * Tests basic scenarios:
 *   1. Closed <think>...</think> pair stripped
 *   2. Unterminated open at block boundary stripped
 *   3. Open tag mid-line NOT stripped (prose mention)
 *   4. Multiple tag variants
 *   5. Streaming with partial tag split across deltas
 *   6. Orphan close tag stripped
 *   7. flush() of held-back content
 */

#include "hermes_think_scrubber.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int passed = 0, failed = 0;

#define TEST(name) do { \
    if (test_##name()) { \
        passed++; printf("  PASS: %s\n", #name); \
    } else { \
        failed++; printf("  FAIL: %s\n", #name); \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { printf("    FAIL: %s\n", msg); return 0; } \
} while(0)

#define ASSERT_STR_EQ(a, b, msg) do { \
    const char *_a = (a) ? (a) : "(null)"; \
    const char *_b = (b) ? (b) : "(null)"; \
    if (strcmp(_a, _b) != 0) { \
        printf("    FAIL: %s — expected \"%s\", got \"%s\"\n", msg, _b, _a); \
        return 0; \
    } \
} while(0)

static int test_closed_pair(void) {
    think_scrubber_t *sc = think_scrubber_new();
    const char *r = think_scrubber_feed(sc, "Hello <think>internal thought</think> world");
    ASSERT_STR_EQ(r, "Hello  world", "closed <think> pair");
    r = think_scrubber_flush(sc);
    ASSERT(r == NULL, "flush empty");
    think_scrubber_free(sc);
    return 1;
}

static int test_unterminated_open(void) {
    think_scrubber_t *sc = think_scrubber_new();
    const char *r = think_scrubber_feed(sc, "Let me reason\n<reasoning>deep thoughts\nmore thoughts\n");
    ASSERT_STR_EQ(r, "Let me reason\n", "before open tag at boundary");
    r = think_scrubber_feed(sc, "still thinking</reasoning> visible output");
    ASSERT_STR_EQ(r, " visible output", "after close tag in same feed");
    r = think_scrubber_feed(sc, " after close");
    ASSERT_STR_EQ(r, " after close", "after close tag");
    think_scrubber_free(sc);
    return 1;
}

static int test_prose_mention(void) {
    think_scrubber_t *sc = think_scrubber_new();
    /* Mid-line <think> is prose, not a block boundary */
    const char *r = think_scrubber_feed(sc, "Use <think> tags like this");
    ASSERT_STR_EQ(r, "Use <think> tags like this", "prose mention preserved");
    think_scrubber_free(sc);
    return 1;
}

static int test_multiple_variants(void) {
    think_scrubber_t *sc = think_scrubber_new();
    const char *r = think_scrubber_feed(sc, "a <thinking>hidden</thinking> b <thought>secret</thought> c");
    ASSERT_STR_EQ(r, "a  b  c", "multiple variants stripped");
    think_scrubber_free(sc);
    return 1;
}

static int test_streaming_split(void) {
    think_scrubber_t *sc = think_scrubber_new();
    const char *r = think_scrubber_feed(sc, "start <thi");
    ASSERT_STR_EQ(r, "start ", "before partial open");
    r = think_scrubber_feed(sc, "nk>hidden</think> visible");
    ASSERT_STR_EQ(r, " visible", "after close with visible in same feed");
    r = think_scrubber_feed(sc, " end");
    ASSERT_STR_EQ(r, " end", "more text");
    think_scrubber_free(sc);
    return 1;
}

static int test_orphan_close(void) {
    think_scrubber_t *sc = think_scrubber_new();
    /* Orphan close tag with no matching open */
    const char *r = think_scrubber_feed(sc, "hello</think> world");
    ASSERT_STR_EQ(r, "helloworld", "orphan close + trailing ws stripped");
    think_scrubber_free(sc);
    return 1;
}

static int test_flush_tail(void) {
    think_scrubber_t *sc = think_scrubber_new();
    const char *r = think_scrubber_feed(sc, "hello");
    ASSERT_STR_EQ(r, "hello", "normal text");
    r = think_scrubber_flush(sc);
    ASSERT(r == NULL, "no held-back content");
    think_scrubber_free(sc);
    return 1;
}

static int test_reset(void) {
    think_scrubber_t *sc = think_scrubber_new();
    think_scrubber_feed(sc, "<think>unterminated");
    think_scrubber_reset(sc);
    const char *r = think_scrubber_feed(sc, "fresh start");
    ASSERT_STR_EQ(r, "fresh start", "after reset");
    think_scrubber_free(sc);
    return 1;
}

static int test_empty_input(void) {
    think_scrubber_t *sc = think_scrubber_new();
    const char *r = think_scrubber_feed(sc, "");
    ASSERT(r == NULL, "empty input returns NULL");
    r = think_scrubber_feed(sc, NULL);
    ASSERT(r == NULL, "NULL input returns NULL");
    think_scrubber_free(sc);
    return 1;
}

static int test_reasoning_scratchpad(void) {
    think_scrubber_t *sc = think_scrubber_new();
    const char *r = think_scrubber_feed(sc, "a <REASONING_SCRATCHPAD>deep</REASONING_SCRATCHPAD> b");
    ASSERT_STR_EQ(r, "a  b", "REASONING_SCRATCHPAD stripped");
    think_scrubber_free(sc);
    return 1;
}

int main(void) {
    printf("=== Think Scrubber Tests ===\n\n");
    TEST(closed_pair);
    TEST(unterminated_open);
    TEST(prose_mention);
    TEST(multiple_variants);
    TEST(streaming_split);
    TEST(orphan_close);
    TEST(flush_tail);
    TEST(reset);
    TEST(empty_input);
    TEST(reasoning_scratchpad);
    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           passed, failed, passed + failed);
    return failed > 0 ? 1 : 0;
}
