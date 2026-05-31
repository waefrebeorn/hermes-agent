/*
 * test_tui_slash_worker.c — Tests for TUI Slash Command Worker (T06).
 * Tests registration, dispatch, argument parsing, categories.
 */
#include "tui_slash_worker.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

/* Track calls to test handlers */
static int g_handler_calls = 0;
static char g_last_cmd[64] = {0};

static bool test_handler(const char *line, int argc, char **argv, void *state) {
    (void)state;
    g_handler_calls++;
    if (argc > 0) strncpy(g_last_cmd, argv[0], sizeof(g_last_cmd) - 1);
    return true;
}

static bool test_fail_handler(const char *line, int argc, char **argv, void *state) {
    (void)line; (void)argc; (void)argv; (void)state;
    return false;
}

int main(void) {
    /* Test 1: Init and shutdown */
    {
        bool ok = tui_slash_init(NULL);
        TEST("init succeeds", ok == true);
        tui_slash_shutdown();
    }

    /* Test 2: Register a command */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t cmd = {
            .cmd = "/hello",
            .desc = "Say hello",
            .args = "",
            .category = SLASH_CAT_TUI,
            .handler = test_handler,
            .userdata = NULL
        };
        bool ok = tui_slash_register(&cmd);
        TEST("register /hello", ok == true);
        TEST("count is 1", tui_slash_count() == 1);
        tui_slash_shutdown();
    }

    /* Test 3: Find a registered command */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t cmd = {
            .cmd = "/test", .desc = "Test command", .args = "",
            .category = SLASH_CAT_AGENT, .handler = test_handler
        };
        tui_slash_register(&cmd);
        const slash_cmd_entry_t *found = tui_slash_find("/test");
        TEST("find /test returns non-null", found != NULL);
        TEST("find /test desc matches", found && strcmp(found->desc, "Test command") == 0);
        TEST("find 'test' (no slash) works", tui_slash_find("test") != NULL);
        TEST("find nonexistent returns NULL", tui_slash_find("/nonexistent") == NULL);
        tui_slash_shutdown();
    }

    /* Test 4: Dispatch routes to correct handler */
    {
        tui_slash_init(NULL);
        g_handler_calls = 0;
        slash_cmd_entry_t cmd = {
            .cmd = "/ping", .desc = "Ping", .args = "",
            .category = SLASH_CAT_TUI, .handler = test_handler
        };
        tui_slash_register(&cmd);
        bool handled = tui_slash_dispatch("/ping");
        TEST("dispatch /ping handled", handled == true);
        TEST("handler was called", g_handler_calls == 1);
        tui_slash_shutdown();
    }

    /* Test 5: Dispatch returns false for unregistered commands */
    {
        tui_slash_init(NULL);
        bool handled = tui_slash_dispatch("/unknown_cmd");
        TEST("dispatch unknown returns false", handled == false);
        tui_slash_shutdown();
    }

    /* Test 6: Dispatch returns false for non-slash lines */
    {
        tui_slash_init(NULL);
        bool handled = tui_slash_dispatch("hello");
        TEST("dispatch non-slash returns false", handled == false);
        handled = tui_slash_dispatch(NULL);
        TEST("dispatch NULL returns false", handled == false);
        handled = tui_slash_dispatch("");
        TEST("dispatch empty returns false", handled == false);
        tui_slash_shutdown();
    }

    /* Test 7: Batch registration */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t batch[] = {
            {"/cmd1", "First", "", SLASH_CAT_TUI, test_handler, NULL},
            {"/cmd2", "Second", "", SLASH_CAT_AGENT, test_handler, NULL},
            {"/cmd3", "Third", "", SLASH_CAT_MODAL, test_handler, NULL},
            {NULL, NULL, NULL, 0, NULL, NULL}
        };
        int regd = tui_slash_register_batch(batch);
        TEST("batch registered 3", regd == 3);
        TEST("total count 3", tui_slash_count() == 3);
        tui_slash_shutdown();
    }

    /* Test 8: Prevent duplicate registration */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t cmd = {
            .cmd = "/dup", .desc = "First", .args = "",
            .category = SLASH_CAT_TUI, .handler = test_handler
        };
        tui_slash_register(&cmd);
        bool duplicate = tui_slash_register(&cmd);
        TEST("duplicate registration rejected", duplicate == false);
        TEST("count still 1", tui_slash_count() == 1);
        tui_slash_shutdown();
    }

    /* Test 9: Unregister a command */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t cmd = {
            .cmd = "/temp", .desc = "Temporary", .args = "",
            .category = SLASH_CAT_TUI, .handler = test_handler
        };
        tui_slash_register(&cmd);
        TEST("count before unregister", tui_slash_count() == 1);
        tui_slash_unregister("/temp");
        TEST("count after unregister", tui_slash_count() == 0);
        TEST("find returns null after unregister", tui_slash_find("/temp") == NULL);
        tui_slash_shutdown();
    }

    /* Test 10: Argument parsing */
    {
        tui_slash_init(NULL);
        char *argv[16];
        int argc;

        argc = tui_slash_parse("/greet World", 16, argv);
        TEST("argc > 0", argc > 0);
        if (argc > 0) {
            TEST("argv[0] is command", strcmp(argv[0], "/greet") == 0);
        }
        if (argc > 1) {
            TEST("argv[1] is arg", strcmp(argv[1], "World") == 0);
        }
        tui_slash_free_args(argc, argv);

        /* Test with quoted args */
        argc = tui_slash_parse("/echo \"hello world\" test", 16, argv);
        TEST("quoted parse: argc == 3", argc == 3);
        if (argc > 1) {
            TEST("quoted arg without quotes", strcmp(argv[1], "hello world") == 0);
        }
        if (argc > 2) {
            TEST("third arg is test", strcmp(argv[2], "test") == 0);
        }
        tui_slash_free_args(argc, argv);

        tui_slash_shutdown();
    }

    /* Test 11: Category names */
    {
        TEST("TUI category name", strcmp(tui_slash_category_name(SLASH_CAT_TUI), "TUI Display") == 0);
        TEST("META category name", strcmp(tui_slash_category_name(SLASH_CAT_META), "Meta") == 0);
        TEST("SKILL category name", strcmp(tui_slash_category_name(SLASH_CAT_SKILL), "Skills") == 0);
    }

    /* Test 12: Help text generation */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t batch[] = {
            {"/help",  "Show help", "",   SLASH_CAT_TUI,   test_handler, NULL},
            {"/quit",  "Exit TUI", "",    SLASH_CAT_META,  test_handler, NULL},
            {"/model", "Set model","<m>", SLASH_CAT_AGENT, test_handler, NULL},
            {NULL, NULL, NULL, 0, NULL, NULL}
        };
        tui_slash_register_batch(batch);

        char *help = tui_slash_help_for_category(SLASH_CAT_TUI);
        TEST("help text non-null", help != NULL);
        if (help) {
            TEST("help contains TUI Display header", strstr(help, "TUI Display") != NULL);
            TEST("help contains /help", strstr(help, "/help") != NULL);
            free(help);
        }

        char *meta_help = tui_slash_help_for_category(SLASH_CAT_META);
        TEST("meta help non-null", meta_help != NULL);
        if (meta_help) {
            TEST("meta help contains /quit", strstr(meta_help, "/quit") != NULL);
            free(meta_help);
        }

        tui_slash_shutdown();
    }

    /* Test 13: Get all commands */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t batch[] = {
            {"/a", "Alpha", "", SLASH_CAT_TUI, test_handler, NULL},
            {"/b", "Beta",  "", SLASH_CAT_TUI, test_handler, NULL},
            {NULL, NULL, NULL, 0, NULL, NULL}
        };
        tui_slash_register_batch(batch);

        const slash_cmd_entry_t **all = tui_slash_get_all();
        TEST("get_all returns non-null", all != NULL);
        TEST("first cmd is /a", all[0] != NULL && strcmp(all[0]->cmd, "/a") == 0);
        TEST("second cmd is /b", all[1] != NULL && strcmp(all[1]->cmd, "/b") == 0);
        TEST("third entry is NULL (terminator)", all[2] == NULL);
        tui_slash_shutdown();
    }

    /* Test 14: Handler returning false */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t cmd = {
            .cmd = "/fail", .desc = "Fails", .args = "",
            .category = SLASH_CAT_TUI, .handler = test_fail_handler
        };
        tui_slash_register(&cmd);
        bool handled = tui_slash_dispatch("/fail");
        TEST("fail handler returns false", handled == false);
        tui_slash_shutdown();
    }

    /* Test 15: Registration with NULL handler rejected */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t cmd = {
            .cmd = "/nocb", .desc = "No handler", .args = "",
            .category = SLASH_CAT_TUI, .handler = NULL
        };
        bool ok = tui_slash_register(&cmd);
        TEST("registration with NULL handler rejected", ok == false);
        tui_slash_shutdown();
    }

    /* Test 16: Shutdown resets state */
    {
        tui_slash_init(NULL);
        slash_cmd_entry_t cmd = {
            .cmd = "/test", .desc = "Test", .args = "",
            .category = SLASH_CAT_AGENT, .handler = test_handler
        };
        tui_slash_register(&cmd);
        tui_slash_shutdown();
        TEST("count is 0 after shutdown", tui_slash_count() == 0);
        /* Re-init should work */
        tui_slash_init(NULL);
        TEST("re-init works", tui_slash_count() == 0);
        tui_slash_shutdown();
    }

    /* Test 17: Edge case — empty line parse */
    {
        char *argv[4];
        int argc = tui_slash_parse("", 4, argv);
        TEST("empty line parse returns 0", argc == 0);
        tui_slash_free_args(argc, argv);
    }

    /* Test 18: Edge case — whitespace-only line */
    {
        char *argv[4];
        int argc = tui_slash_parse("   \t  ", 4, argv);
        TEST("whitespace line returns 0", argc == 0);
        tui_slash_free_args(argc, argv);
    }

    /* Test 19: Many arguments */
    {
        char *argv[32];
        int argc = tui_slash_parse("/cmd a b c d e f g", 32, argv);
        TEST("8 args parsed", argc == 8);
        if (argc > 7) {
            TEST("argv[7] is 'g'", strcmp(argv[7], "g") == 0);
        }
        tui_slash_free_args(argc, argv);
    }

    /* Test 20: Max args limit */
    {
        char *argv[4];
        int argc = tui_slash_parse("/cmd a b c d e", 4, argv);
        TEST("max args capped at 4", argc == 4);
        tui_slash_free_args(argc, argv);
    }

    printf("\n");
    if (failures > 0)
        printf("  %d TESTS FAILED\n", failures);
    else
        printf("  ALL TESTS PASSED\n");
    return failures > 0 ? 1 : 0;
}
