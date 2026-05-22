/*
 * plugin_kanban.c — Example kanban plugin (P133).
 * Stub showing typed plugin API usage pattern for multi-agent task boards.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_kanban.c -o plugin_kanban.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "kanban-board";
}

const char *plugin_meta_version(void) {
    return "0.2.0";
}

const char *plugin_meta_type(void) {
    return "kanban";
}

const char *plugin_meta_description(void) {
    return "Multi-agent kanban board dispatcher and worker coordinator";
}

/* No dependencies */
int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ================================================================
 *  Interface function pointers
 * ================================================================ */

/* In-memory board storage (stub) */
#define MAX_BOARDS 8
#define MAX_TASKS  64

static struct {
    char name[128];
    char config_json[2048];
    bool active;
    int  task_count;
} boards[MAX_BOARDS];

static int board_count = 0;

static char *kanban_create_board(const char *name, const char *config_json) {
    (void)config_json;
    if (board_count >= MAX_BOARDS)
        return strdup("{\"error\":\"max boards reached\"}");

    snprintf(boards[board_count].name, sizeof(boards[board_count].name), "%s", name);
    if (config_json)
        snprintf(boards[board_count].config_json, sizeof(boards[board_count].config_json), "%s", config_json);
    boards[board_count].active = true;
    boards[board_count].task_count = 0;
    board_count++;

    char result[256];
    snprintf(result, sizeof(result), "{\"status\":\"ok\",\"board_id\":%d,\"name\":\"%s\"}",
             board_count - 1, name);
    return strdup(result);
}

static char *kanban_add_task(const char *board_id, const char *task_json) {
    int id = board_id ? atoi(board_id) : -1;
    if (id < 0 || id >= board_count)
        return strdup("{\"error\":\"invalid board_id\"}");

    boards[id].task_count++;
    (void)task_json;

    char result[256];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"task_id\":%d,\"board\":\"%s\"}",
             boards[id].task_count, boards[id].name);
    return strdup(result);
}

static char *kanban_get_board(const char *board_id) {
    int id = board_id ? atoi(board_id) : -1;
    if (id < 0 || id >= board_count)
        return strdup("{\"error\":\"invalid board_id\"}");

    char result[512];
    snprintf(result, sizeof(result),
             "{\"board\":{\"id\":%d,\"name\":\"%s\",\"tasks\":%d}}",
             id, boards[id].name, boards[id].task_count);
    return strdup(result);
}

static char *kanban_list_boards(void) {
    char result[2048] = "{\"boards\":[";
    for (int i = 0; i < board_count; i++) {
        char entry[256];
        snprintf(entry, sizeof(entry), "%s{\"id\":%d,\"name\":\"%s\",\"tasks\":%d}",
                 (i > 0) ? "," : "", i, boards[i].name, boards[i].task_count);
        strncat(result, entry, sizeof(result) - strlen(result) - 1);
    }
    strncat(result, "]}", sizeof(result) - strlen(result) - 1);
    return strdup(result);
}

static plugin_interface_t interface = {
    .type = PLUGIN_KANBAN,
    .kanban_create_board = kanban_create_board,
    .kanban_add_task     = kanban_add_task,
    .kanban_get_board    = kanban_get_board,
    .kanban_list_boards  = kanban_list_boards,
};

void *plugin_get_interface(void) {
    return &interface;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    fprintf(stderr, "[kanban-board] initializing...\n");
    board_count = 0;
    memset(boards, 0, sizeof(boards));
    fprintf(stderr, "[kanban-board] ready (max %d boards)\n", MAX_BOARDS);
    return 0;
}

int plugin_cleanup(void) {
    fprintf(stderr, "[kanban-board] shutting down...\n");
    board_count = 0;
    return 0;
}

int plugin_configure(const char *config_json) {
    fprintf(stderr, "[kanban-board] configure: %s\n", config_json);
    return 0;
}
