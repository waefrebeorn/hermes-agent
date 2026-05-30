/*
 * test_budget_config.c — Tests for budget_config module.
 * Tests: defaults, overrides, resolve threshold, pinned tools.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "budget_config.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

int main(void)
{
    printf("=== Budget Config Tests ===\n\n");

    /* ── Defaults ── */
    printf("-- Defaults --\n");
    {
        const budget_config_t *def = budget_config_default();
        TEST("default_result_size", def->default_result_size == BUDGET_DEFAULT_RESULT_SIZE_CHARS);
        TEST("turn_budget", def->turn_budget == BUDGET_DEFAULT_TURN_BUDGET_CHARS);
        TEST("preview_size", def->preview_size == BUDGET_DEFAULT_PREVIEW_SIZE_CHARS);
        TEST("no overrides", def->overrides == NULL);
    }

    /* ── Init ── */
    printf("\n-- Init --\n");
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        TEST("init default_result_size", cfg.default_result_size == BUDGET_DEFAULT_RESULT_SIZE_CHARS);
        TEST("init turn_budget", cfg.turn_budget == BUDGET_DEFAULT_TURN_BUDGET_CHARS);
        TEST("init preview_size", cfg.preview_size == BUDGET_DEFAULT_PREVIEW_SIZE_CHARS);
        TEST("init overrides null", cfg.overrides == NULL);
    }

    /* ── Resolve threshold: pinned read_file ── */
    printf("\n-- Pinned Threshold --\n");
    {
        budget_config_t cfg;
        budget_config_init(&cfg);

        int t = budget_config_resolve_threshold(&cfg, "read_file");
        TEST("read_file pinned infinite", t == BUDGET_PINNED_READ_FILE_THRESHOLD);

        t = budget_config_resolve_threshold(&cfg, "write_file");
        TEST("write_file default", t == BUDGET_DEFAULT_RESULT_SIZE_CHARS);
    }

    /* ── Tool overrides ── */
    printf("\n-- Tool Overrides --\n");
    {
        budget_config_t cfg;
        budget_config_init(&cfg);

        budget_config_set_override(&cfg, "terminal", 50000);
        budget_config_set_override(&cfg, "browser", 200000);

        int t = budget_config_resolve_threshold(&cfg, "terminal");
        TEST("terminal override 50000", t == 50000);

        t = budget_config_resolve_threshold(&cfg, "browser");
        TEST("browser override 200000", t == 200000);

        t = budget_config_resolve_threshold(&cfg, "web");
        TEST("web (no override) default", t == BUDGET_DEFAULT_RESULT_SIZE_CHARS);
    }

    /* ── Override update (same tool, new value) ── */
    printf("\n-- Override Update --\n");
    {
        budget_config_t cfg;
        budget_config_init(&cfg);

        budget_config_set_override(&cfg, "terminal", 50000);
        budget_config_set_override(&cfg, "terminal", 75000);

        int t = budget_config_resolve_threshold(&cfg, "terminal");
        TEST("terminal updated 75000", t == 75000);
    }

    /* ── read_file still pinned after override ── */
    printf("\n-- Pinned + Override --\n");
    {
        budget_config_t cfg;
        budget_config_init(&cfg);

        /* Even if someone sets an override for read_file, pinned still wins */
        budget_config_set_override(&cfg, "read_file", 100);

        int t = budget_config_resolve_threshold(&cfg, "read_file");
        TEST("read_file pinned beats override", t == BUDGET_PINNED_READ_FILE_THRESHOLD);

        t = budget_config_resolve_threshold(&cfg, "other");
        TEST("other_tool uses default", t == BUDGET_DEFAULT_RESULT_SIZE_CHARS);
    }

    /* ── Null safety ── */
    printf("\n-- Null Safety --\n");
    {
        int t = budget_config_resolve_threshold(NULL, "tool");
        TEST("null config returns default", t == BUDGET_DEFAULT_RESULT_SIZE_CHARS);

        t = budget_config_resolve_threshold(NULL, NULL);
        TEST("null both returns default", t == BUDGET_DEFAULT_RESULT_SIZE_CHARS);
    }

    /* ── Cleanup ── */
    {
        budget_config_t cfg;
        budget_config_init(&cfg);
        budget_config_set_override(&cfg, "a", 1);
        budget_config_set_override(&cfg, "b", 2);
        budget_config_cleanup(&cfg);
        TEST("cleanup frees overrides", cfg.overrides == NULL);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
