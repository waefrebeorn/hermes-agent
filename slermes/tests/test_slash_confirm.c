/*
 * test_slash_confirm.c — Tests for slash_confirm module.
 * Tests: register, get, clear, staleness, resolve.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "slash_confirm.h"

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

/* Test handler: returns an allocated string describing the choice */
static char *test_handler(const char *choice)
{
    if (!choice) return NULL;
    char *result = malloc(128);
    if (result) snprintf(result, 128, "Handled: %s", choice);
    return result;
}

int main(void)
{
    printf("=== Slash Confirm Tests ===\n\n");

    /* ── Basic register/get ── */
    printf("-- Register/Get --\n");
    {
        slashconfirm_clear_all();

        /* Register a confirm */
        slashconfirm_register("session1", "cid123", "reload-mcp", test_handler);
        slashconfirm_entry_t *e = slashconfirm_get_pending("session1");
        TEST("get pending returns entry", e != NULL);
        if (e) {
            TEST("session key matches", strcmp(e->session_key, "session1") == 0);
            TEST("confirm id matches", strcmp(e->confirm_id, "cid123") == 0);
            TEST("command matches", strcmp(e->command, "reload-mcp") == 0);
            TEST("handler not null", e->handler != NULL);
            TEST("created_at > 0", e->created_at > 0);
            slashconfirm_free_entry(e);
        }

        /* Non-existent session */
        e = slashconfirm_get_pending("nonexistent");
        TEST("nonexistent session returns NULL", e == NULL);
    }

    /* ── Overwrite ── */
    printf("\n-- Overwrite --\n");
    {
        slashconfirm_register("session2", "first", "cmd1", test_handler);
        slashconfirm_register("session2", "second", "cmd2", test_handler);

        slashconfirm_entry_t *e = slashconfirm_get_pending("session2");
        TEST("overwritten session returns latest", e != NULL);
        if (e) {
            TEST("confirm_id is 'second'", strcmp(e->confirm_id, "second") == 0);
            TEST("command is 'cmd2'", strcmp(e->command, "cmd2") == 0);
            slashconfirm_free_entry(e);
        }
        slashconfirm_clear("session2");
    }

    /* ── Clear ── */
    printf("\n-- Clear --\n");
    {
        slashconfirm_register("session3", "cid", "cmd", test_handler);
        TEST("exists before clear",
             slashconfirm_get_pending("session3") != NULL);

        slashconfirm_clear("session3");
        TEST("gone after clear",
             slashconfirm_get_pending("session3") == NULL);

        /* Clear non-existent is safe */
        slashconfirm_clear("nonexistent");
        TEST("clear nonexistent is safe", 1);
    }

    /* ── Staleness ── */
    printf("\n-- Staleness --\n");
    {
        slashconfirm_register("session4", "cid", "cmd", test_handler);

        /* Use very short timeout — entry should be fresh still */
        bool dropped = slashconfirm_clear_if_stale("session4", 999999.0);
        TEST("fresh entry not dropped with long timeout", !dropped);
        TEST("still exists after fresh check",
             slashconfirm_get_pending("session4") != NULL);

        /* Use negative timeout — forces stale */
        dropped = slashconfirm_clear_if_stale("session4", -1.0);
        TEST("stale entry dropped with negative timeout", dropped);
        TEST("gone after stale check",
             slashconfirm_get_pending("session4") == NULL);

        /* Stale on nonexistent */
        dropped = slashconfirm_clear_if_stale("nonexistent", 0.0);
        TEST("stale check on nonexistent returns false", !dropped);
    }

    /* ── Resolve ── */
    printf("\n-- Resolve --\n");
    {
        slashconfirm_register("session5", "cid", "reload-mcp", test_handler);
        char *result = slashconfirm_resolve("session5", "cid", "once",
                                             SLASHCONFIRM_DEFAULT_TIMEOUT_SECONDS);
        TEST("resolve returns result", result != NULL);
        if (result) {
            TEST("result contains 'Handled: once'",
                 strcmp(result, "Handled: once") == 0);
            free(result);
        }
        TEST("resolved entry cleared",
             slashconfirm_get_pending("session5") == NULL);
    }

    /* ── Resolve wrong confirm_id ── */
    printf("\n-- Resolve wrong confirm_id --\n");
    {
        slashconfirm_register("session6", "real_id", "cmd", test_handler);
        char *result = slashconfirm_resolve("session6", "wrong_id", "once",
                                             SLASHCONFIRM_DEFAULT_TIMEOUT_SECONDS);
        TEST("wrong confirm_id returns NULL", result == NULL);

        /* Entry should still exist */
        TEST("entry still exists after wrong confirm_id",
             slashconfirm_get_pending("session6") != NULL);

        slashconfirm_clear("session6");
    }

    /* ── Resolve stale ── */
    printf("\n-- Resolve stale --\n");
    {
        slashconfirm_register("session7", "cid", "cmd", test_handler);
        char *result = slashconfirm_resolve("session7", "cid", "once", -1.0);
        TEST("stale resolve returns NULL", result == NULL);
        TEST("stale resolved entry cleared",
             slashconfirm_get_pending("session7") == NULL);
    }

    /* ── Clear all ── */
    printf("\n-- Clear all --\n");
    {
        slashconfirm_register("s1", "c1", "cmd", test_handler);
        slashconfirm_register("s2", "c2", "cmd", test_handler);
        TEST("2 entries exist before clear all",
             slashconfirm_get_pending("s1") != NULL &&
             slashconfirm_get_pending("s2") != NULL);

        slashconfirm_clear_all();
        TEST("s1 cleared after clear_all",
             slashconfirm_get_pending("s1") == NULL);
        TEST("s2 cleared after clear_all",
             slashconfirm_get_pending("s2") == NULL);
    }

    /* ── Summary ── */
    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
