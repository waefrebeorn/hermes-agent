/*
 * test_skill_provenance.c — Tests for skill provenance tracking.
 */

#include "skill_provenance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

static void test_default_origin(void)
{
    TEST("default origin is foreground",
         strcmp(skill_provenance_get(), "foreground") == 0);
    TEST("default is not background_review",
         !skill_provenance_is_background_review());
}

static void test_set_background_review(void)
{
    skill_provenance_token_t token = skill_provenance_set("background_review");
    TEST("origin after set to background_review",
         strcmp(skill_provenance_get(), "background_review") == 0);
    TEST("is_background_review true",
         skill_provenance_is_background_review());

    skill_provenance_reset(token);
    TEST("origin after reset",
         strcmp(skill_provenance_get(), "foreground") == 0);
    TEST("is_background_review false after reset",
         !skill_provenance_is_background_review());
}

static void test_set_foreground(void)
{
    /* Verify explicit foreground works */
    skill_provenance_set("background_review");
    TEST("set to background_review works",
         skill_provenance_is_background_review());

    skill_provenance_token_t token = skill_provenance_set("foreground");
    TEST("after explicit foreground",
         strcmp(skill_provenance_get(), "foreground") == 0);
    TEST("is_background_review false",
         !skill_provenance_is_background_review());

    /* Restore so others don't see "foreground" as the saved token value */
    skill_provenance_reset(token);
    (void)skill_provenance_set("foreground");
}

static void test_nested_set_reset(void)
{
    skill_provenance_set("foreground");

    skill_provenance_token_t t1 = skill_provenance_set("background_review");
    TEST("nested: level 1 is background_review",
         skill_provenance_is_background_review());

    skill_provenance_token_t t2 = skill_provenance_set("foreground");
    TEST("nested: level 2 is foreground",
         strcmp(skill_provenance_get(), "foreground") == 0);

    skill_provenance_reset(t2);
    TEST("nested: reset to background_review",
         skill_provenance_is_background_review());

    skill_provenance_reset(t1);
    TEST("nested: reset to foreground",
         strcmp(skill_provenance_get(), "foreground") == 0);
}

static void test_null_origin(void)
{
    skill_provenance_set("foreground");
    skill_provenance_token_t token = skill_provenance_set(NULL);
    TEST("NULL origin treated as foreground",
         strcmp(skill_provenance_get(), "foreground") == 0);

    skill_provenance_reset(token);
    TEST("null-set reset restores foreground",
         strcmp(skill_provenance_get(), "foreground") == 0);
}

static void test_empty_origin(void)
{
    skill_provenance_set("foreground");
    skill_provenance_token_t token = skill_provenance_set("");
    TEST("empty origin treated as foreground",
         strcmp(skill_provenance_get(), "foreground") == 0);

    skill_provenance_reset(token);
}

static void test_roundtrip_via_get(void)
{
    /* Verify we can set, get matches, reset, get matches saved */
    skill_provenance_set("foreground");
    const char *before = skill_provenance_get();

    skill_provenance_token_t token = skill_provenance_set("background_review");
    TEST("get returns background_review after set",
         strcmp(skill_provenance_get(), "background_review") == 0);

    skill_provenance_reset(token);
    TEST("get returns foreground after reset",
         strcmp(skill_provenance_get(), before) == 0);
}

int main(void)
{
    printf("=== Skill Provenance Library Tests ===\n");

    test_default_origin();
    test_set_background_review();
    test_set_foreground();
    test_nested_set_reset();
    test_null_origin();
    test_empty_origin();
    test_roundtrip_via_get();

    printf("\nResults: %d passed, %d failed, %d total\n",
           passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
