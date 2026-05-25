/* lmstudio_reasoning test — 21 test cases covering all effort paths. */

#include "lmstudio_reasoning.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    if (!_a && !_e) { pass++; } \
    else if (!_a || !_e || strcmp(_a, _e) != 0) { \
        fprintf(stderr, "FAIL: %s (line %d): got '%s', expected '%s'\\n", \
                name, __LINE__, _a ? _a : "(null)", _e ? _e : "(null)"); \
        fail++; \
    } else { pass++; } \
} while(0)

/* ── lmstudio_is_valid_effort ──────────────────────────────── */

static void test_valid_efforts(void) {
    TEST("valid: none",     lmstudio_is_valid_effort("none"));
    TEST("valid: minimal",  lmstudio_is_valid_effort("minimal"));
    TEST("valid: low",      lmstudio_is_valid_effort("low"));
    TEST("valid: medium",   lmstudio_is_valid_effort("medium"));
    TEST("valid: high",     lmstudio_is_valid_effort("high"));
    TEST("valid: xhigh",    lmstudio_is_valid_effort("xhigh"));
    TEST("invalid: off",    !lmstudio_is_valid_effort("off"));
    TEST("invalid: on",     !lmstudio_is_valid_effort("on"));
    TEST("invalid: empty",  !lmstudio_is_valid_effort(""));
    TEST("invalid: null",   !lmstudio_is_valid_effort(NULL));
    TEST("invalid: bogus",  !lmstudio_is_valid_effort("bogus"));
}

/* ── lmstudio_map_effort_alias ─────────────────────────────── */

static void test_aliases(void) {
    TEST_STR("alias off→none",   lmstudio_map_effort_alias("off"),   "none");
    TEST_STR("alias on→medium",  lmstudio_map_effort_alias("on"),    "medium");
    TEST_STR("alias medium→medium", lmstudio_map_effort_alias("medium"), "medium");
    TEST_STR("alias none→none",  lmstudio_map_effort_alias("none"),  "none");
    TEST_STR("alias null→null",  lmstudio_map_effort_alias(NULL),    NULL);
}

/* ── resolve_lmstudio_effort ───────────────────────────────── */

static void test_resolve_default(void) {
    /* No config, no allowed_options → default medium */
    TEST_STR("default medium", resolve_lmstudio_effort(true, NULL, NULL), "medium");
}

static void test_resolve_disabled(void) {
    /* enabled=false → none */
    TEST_STR("disabled → none", resolve_lmstudio_effort(false, NULL, NULL), "none");
}

static void test_resolve_effort(void) {
    /* Explicit effort */
    TEST_STR("effort=low",    resolve_lmstudio_effort(true, "low",    NULL), "low");
    TEST_STR("effort=high",   resolve_lmstudio_effort(true, "high",   NULL), "high");
    TEST_STR("effort=none",   resolve_lmstudio_effort(true, "none",   NULL), "none");

    /* Alias mapping */
    TEST_STR("alias off→none",   resolve_lmstudio_effort(true, "off",  NULL), "none");
    TEST_STR("alias on→medium",  resolve_lmstudio_effort(true, "on",   NULL), "medium");
}

static void test_resolve_case_insensitive(void) {
    TEST_STR("effort=HIGH", resolve_lmstudio_effort(true, "HIGH", NULL), "high");
    TEST_STR("effort=Medium", resolve_lmstudio_effort(true, "Medium", NULL), "medium");
}

static void test_resolve_with_allowed(void) {
    /* Effort in allowed set → resolved */
    const char *allowed1[] = {"off", "on", NULL};
    TEST_STR("on → medium (in allowed)",
             resolve_lmstudio_effort(true, "medium", allowed1), "medium");

    /* Effort NOT in allowed set → NULL (omit field) */
    const char *allowed2[] = {"off", NULL};
    TEST("high not in allowed {off} → NULL",
         resolve_lmstudio_effort(true, "high", allowed2) == NULL);

    /* Multiple allowed options */
    const char *allowed3[] = {"off", "minimal", "low", NULL};
    TEST_STR("low in allowed {off,minimal,low} → low",
             resolve_lmstudio_effort(true, "low", allowed3), "low");
    TEST("high not in allowed {off,minimal,low} → NULL",
         resolve_lmstudio_effort(true, "high", allowed3) == NULL);
}

static void test_resolve_disabled_with_allowed(void) {
    const char *allowed[] = {"off", "on", NULL};
    TEST_STR("disabled (none) in allowed → none",
             resolve_lmstudio_effort(false, NULL, allowed), "none");
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    test_valid_efforts();
    test_aliases();
    test_resolve_default();
    test_resolve_disabled();
    test_resolve_effort();
    test_resolve_case_insensitive();
    test_resolve_with_allowed();
    test_resolve_disabled_with_allowed();

    fprintf(stderr, "lmstudio_reasoning: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
