/* Test kanban plugin via plugin API */
#include "plugin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;

#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

int main(void) {
    printf("=== Plugin Kanban (In-Memory Board) Tests ===\n");

    /* Load the plugin */
    const char *plugin_path = PLUGIN_DIR "/plugin_kanban.so";
    plugin_t *p = plugin_load(plugin_path);
    TEST("plugin loads", p != NULL);
    if (!p) {
        printf("  Error: %s (path: %s)\n", plugin_error(), plugin_path);
        printf("\n=== %d/%d passed ===\n", pass, pass + fail);
        return 1;
    }

    /* Check metadata */
    TEST("name = kanban-board", strcmp(plugin_name(p), "kanban-board") == 0);
    TEST("type = KANBAN", plugin_type(p) == PLUGIN_KANBAN);
    const char *desc = plugin_description(p);
    TEST("description not null", desc != NULL);
    TEST("description mentions kanban", desc && strstr(desc, "kanban") != NULL);

    /* Check version */
    const plugin_version_t *ver = plugin_version(p);
    TEST("version not null", ver != NULL);
    TEST("major == 1", ver->major == 1);

    /* Check deps */
    int dep_count;
    const plugin_dep_t *deps = plugin_deps(p, &dep_count);
    TEST("deps accessible", deps != NULL);
    TEST("no dependencies", dep_count == 0);

    /* Init */
    int init_rc = -1;
    {
        typedef int (*init_fn_t)(void);
        init_fn_t init_fn = (init_fn_t)plugin_symbol(p, "plugin_init");
        TEST("plugin_init symbol found", init_fn != NULL);
        if (init_fn) {
            init_rc = init_fn();
            TEST("init returns 0", init_rc == 0);
        }
    }

    /* Get interface */
    plugin_interface_t *iface = NULL;
    {
        void *(*get_iface)(void) = (void *(*)(void))plugin_symbol(p, "plugin_get_interface");
        TEST("plugin_get_interface found", get_iface != NULL);
        if (get_iface) {
            iface = (plugin_interface_t *)get_iface();
            TEST("interface not null", iface != NULL);
            TEST("interface type = KANBAN", iface != NULL && iface->type == PLUGIN_KANBAN);
        }
    }

    if (iface && iface->kanban_create_board && iface->kanban_add_task &&
        iface->kanban_get_board && iface->kanban_list_boards) {
        char *r;

        /* Create board */
        r = iface->kanban_create_board("test-board", "{\"project\":\"demo\"}");
        TEST("create board returns result", r != NULL);
        TEST("create board succeeds", r && strstr(r, "\"status\":\"ok\"") != NULL);
        TEST("create board has board_id 0", r && strstr(r, "\"board_id\":0") != NULL);
        free(r);

        /* List boards — should have 1 */
        r = iface->kanban_list_boards();
        TEST("list boards returns result", r != NULL);
        TEST("list boards has count 1", r && strstr(r, "\"count\":1") != NULL);
        TEST("list boards has test-board", r && strstr(r, "test-board") != NULL);
        free(r);

        /* Add task to board 0 */
        r = iface->kanban_add_task("0", "{\"description\":\"Implement feature X\",\"column\":\"todo\",\"priority\":4}");
        TEST("add task returns result", r != NULL);
        TEST("add task succeeds", r && strstr(r, "\"status\":\"ok\"") != NULL);
        TEST("add task has task_id", r && strstr(r, "\"task_id\":\"b0-") != NULL);
        free(r);

        /* Add second task to different column */
        r = iface->kanban_add_task("0", "{\"description\":\"Fix bug Y\",\"column\":\"in_progress\",\"assignee\":\"worker1\",\"priority\":5}");
        TEST("add task #2 returns result", r != NULL);
        TEST("add task #2 succeeds", r && strstr(r, "\"status\":\"ok\"") != NULL);
        TEST("add task #2 is in_progress", r && strstr(r, "in_progress") != NULL);
        free(r);

        /* Add third task with sticky */
        r = iface->kanban_add_task("0", "{\"description\":\"Critical blocker\",\"column\":\"blocked\",\"sticky\":true}");
        TEST("add task #3 (sticky) returns result", r != NULL);
        TEST("add task #3 succeeds", r && strstr(r, "\"status\":\"ok\"") != NULL);
        free(r);

        /* Get board 0 — should have 3 tasks */
        r = iface->kanban_get_board("0");
        TEST("get board returns result", r != NULL);
        TEST("get board has name test-board", r && strstr(r, "test-board") != NULL);
        TEST("get board has 3 tasks", r && strstr(r, "\"tasks\":[") != NULL);
        TEST("get board contains feature X", r && strstr(r, "Implement feature X") != NULL);
        TEST("get board contains bug Y", r && strstr(r, "Fix bug Y") != NULL);
        TEST("get board contains critical blocker", r && strstr(r, "Critical blocker") != NULL);
        free(r);

        /* Error: invalid board */
        r = iface->kanban_get_board("99");
        TEST("invalid board get returns error", r != NULL);
        TEST("invalid board error message", r && strstr(r, "\"error\"") != NULL);
        free(r);

        /* Error: add task to invalid board */
        r = iface->kanban_add_task("99", "{\"description\":\"should fail\"}");
        TEST("invalid board add returns error", r != NULL);
        TEST("invalid board add error message", r && strstr(r, "\"error\"") != NULL);
        free(r);

        /* Error: null name */
        r = iface->kanban_create_board(NULL, NULL);
        TEST("null name board returns error", r != NULL);
        TEST("null name error message", r && strstr(r, "\"error\"") != NULL);
        free(r);

        /* List boards after all ops */
        r = iface->kanban_list_boards();
        TEST("list boards after adds", r != NULL);
        TEST("list boards count 1", r && strstr(r, "\"count\":1") != NULL);
        TEST("list shows task_count 3", r && strstr(r, "\"task_count\":3") != NULL);
        free(r);
    } else {
        printf("  FAIL: interface functions missing\n");
        fail++;
    }

    /* Cleanup */
    {
        typedef int (*cleanup_fn_t)(void);
        cleanup_fn_t cleanup_fn = (cleanup_fn_t)plugin_symbol(p, "plugin_cleanup");
        TEST("plugin_cleanup found", cleanup_fn != NULL);
        if (cleanup_fn) {
            int rc = cleanup_fn();
            TEST("cleanup returns 0", rc == 0);
        }
    }

    plugin_unload(p);

    printf("\n=== %d/%d passed, %d failed ===\n", pass, pass + fail, fail);
    return fail > 0 ? 1 : 0;
}
