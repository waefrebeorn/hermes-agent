/*
 * plugin_kanban.c — Real kanban board plugin (P133).
 * In-memory board storage with task CRUD, column tracking,
 * timestamps, and status lifecycle.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_kanban.c -o plugin_kanban.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "kanban-board";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "kanban";
}

const char *plugin_meta_description(void) {
    return "Multi-agent kanban board — in-memory task storage with "
           "columns, status tracking, timestamps, and lifecycle";
}

/* No dependencies */
int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ================================================================
 *  Internal data structures
 * ================================================================ */

#define MAX_BOARDS 8
#define MAX_TASKS_PER_BOARD 256
#define MAX_COLUMNS 8
#define MAX_COMMENTS 32

/* Column definitions */
#define COLUMN_NAMES {"todo", "in_progress", "done", "blocked"}
#define DEFAULT_COLUMNS 4

/* A single task comment */
typedef struct {
    char author[64];
    char body[1024];
    time_t created_at;
} task_comment_t;

/* A single task */
typedef struct {
    char id[64];           /* unique ID */
    char description[2048];
    char column[32];       /* which column/lane */
    time_t created_at;
    time_t updated_at;
    int priority;          /* 1-5, 5=highest */
    char assignee[128];    /* optional worker assignment */
    char block_reason[512];/* block reason if column=blocked */
    bool sticky;           /* auto-promote skips sticky */
    int  comment_count;
    task_comment_t comments[MAX_COMMENTS];
    time_t heartbeat_ts;   /* last heartbeat for in_progress */
} kanban_task_t;

/* A single board */
typedef struct {
    char name[128];
    char config_json[2048];
    bool active;
    int  task_count;
    kanban_task_t tasks[MAX_TASKS_PER_BOARD];
    char columns[MAX_COLUMNS][32];
    int  column_count;
} kanban_board_t;

/* Static storage */
static kanban_board_t boards[MAX_BOARDS];
static int board_count = 0;
static int g_initialized = 0;
static int g_task_counter = 0;

/* Generate unique task ID */
static void gen_task_id(char *buf, size_t sz, int board_idx) {
    snprintf(buf, sz, "b%d-t%d-%ld",
             board_idx, g_task_counter++, (long)time(NULL));
}

/* Find first column index that matches a name */
static int find_column(int board_idx, const char *col_name) {
    if (!col_name || !col_name[0]) return 0; /* default: todo */
    for (int i = 0; i < boards[board_idx].column_count; i++) {
        if (strcmp(boards[board_idx].columns[i], col_name) == 0)
            return i;
    }
    return 0; /* default to first column */
}

/* Escape string for JSON embedding */
static void json_escape(const char *src, char *dst, size_t dst_sz) {
    size_t pos = 0;
    for (const char *s = src; *s && pos < dst_sz - 2; s++) {
        switch (*s) {
            case '"':  if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = '"'; } break;
            case '\\': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = '\\'; } break;
            case '\n': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = 'n'; } break;
            case '\t': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = 't'; } break;
            default:   dst[pos++] = *s; break;
        }
    }
    dst[pos] = '\0';
}

/* Append a task's JSON representation to a buffer */
static size_t append_task_json(char *buf, size_t buf_sz, size_t pos,
                                const kanban_task_t *t) {
    char desc_esc[4096], assign_esc[256], block_esc[1024];
    json_escape(t->description, desc_esc, sizeof(desc_esc));
    json_escape(t->assignee, assign_esc, sizeof(assign_esc));
    json_escape(t->block_reason, block_esc, sizeof(block_esc));

    pos += snprintf(buf + pos, buf_sz - pos,
        "{\"id\":\"%s\",\"description\":\"%s\",\"column\":\"%s\","
        "\"priority\":%d,\"assignee\":\"%s\",\"sticky\":%s,"
        "\"block_reason\":\"%s\",\"comment_count\":%d,"
        "\"created_at\":%ld,\"updated_at\":%ld}",
        t->id, desc_esc, t->column,
        t->priority, assign_esc, t->sticky ? "true" : "false",
        block_esc, t->comment_count,
        (long)t->created_at, (long)t->updated_at);
    return pos;
}

/* ================================================================
 *  Interface functions
 * ================================================================ */

static char *kanban_create_board(const char *name, const char *config_json) {
    if (!name || !name[0])
        return strdup("{\"error\":\"board name required\"}");
    if (board_count >= MAX_BOARDS)
        return strdup("{\"error\":\"max boards reached\"}");

    kanban_board_t *b = &boards[board_count];

    snprintf(b->name, sizeof(b->name), "%s", name);
    if (config_json)
        snprintf(b->config_json, sizeof(b->config_json), "%s", config_json);
    else
        b->config_json[0] = '\0';

    /* Set up default columns */
    const char *col_names[] = COLUMN_NAMES;
    b->column_count = DEFAULT_COLUMNS;
    for (int i = 0; i < b->column_count && i < MAX_COLUMNS; i++)
        snprintf(b->columns[i], sizeof(b->columns[i]), "%s", col_names[i]);

    b->active = true;
    b->task_count = 0;
    memset(b->tasks, 0, sizeof(b->tasks));

    char result[512];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"board_id\":%d,\"name\":\"%s\","
             "\"columns\":%d}",
             board_count, name, b->column_count);
    board_count++;
    return strdup(result);
}

static char *kanban_add_task(const char *board_id, const char *task_json) {
    int id = board_id ? atoi(board_id) : -1;
    if (id < 0 || id >= board_count)
        return strdup("{\"error\":\"invalid board_id\"}");
    if (boards[id].task_count >= MAX_TASKS_PER_BOARD)
        return strdup("{\"error\":\"board full\"}");

    kanban_board_t *b = &boards[id];
    kanban_task_t *t = &b->tasks[b->task_count];

    /* Generate ID */
    gen_task_id(t->id, sizeof(t->id), id);

    /* Parse task_json for fields */
    if (task_json && task_json[0]) {
        /* Extract description (required) */
        const char *desc_start = strstr(task_json, "\"description\"");
        const char *desc = NULL;
        if (desc_start) {
            desc_start = strchr(desc_start + 13, '"');
            if (desc_start) {
                desc_start++;
                const char *desc_end = strchr(desc_start, '"');
                if (desc_end) {
                    size_t dlen = (size_t)(desc_end - desc_start);
                    if (dlen >= sizeof(t->description)) dlen = sizeof(t->description) - 1;
                    memcpy(t->description, desc_start, dlen);
                    t->description[dlen] = '\0';
                    desc = t->description;
                }
            }
        }

        /* Extract column */
        const char *col_start = strstr(task_json, "\"column\"");
        if (col_start) {
            col_start = strchr(col_start + 8, '"');
            if (col_start) {
                col_start++;
                const char *col_end = strchr(col_start, '"');
                if (col_end) {
                    size_t clen = (size_t)(col_end - col_start);
                    if (clen >= sizeof(t->column)) clen = sizeof(t->column) - 1;
                    memcpy(t->column, col_start, clen);
                    t->column[clen] = '\0';
                }
            }
        }
        if (!t->column[0]) snprintf(t->column, sizeof(t->column), "todo");

        /* Extract priority */
        const char *prio_start = strstr(task_json, "\"priority\"");
        if (prio_start) {
            prio_start = strchr(prio_start + 10, ':');
            if (prio_start) {
                prio_start++;
                t->priority = atoi(prio_start);
                if (t->priority < 1) t->priority = 3;
                if (t->priority > 5) t->priority = 5;
            }
        } else {
            t->priority = 3;
        }

        /* Extract assignee */
        const char *as_start = strstr(task_json, "\"assignee\"");
        if (as_start) {
            as_start = strchr(as_start + 10, '"');
            if (as_start) {
                as_start++;
                const char *as_end = strchr(as_start, '"');
                if (as_end) {
                    size_t alen = (size_t)(as_end - as_start);
                    if (alen >= sizeof(t->assignee)) alen = sizeof(t->assignee) - 1;
                    memcpy(t->assignee, as_start, alen);
                    t->assignee[alen] = '\0';
                }
            }
        }

        /* Extract sticky */
        if (strstr(task_json, "\"sticky\":true"))
            t->sticky = true;

        /* If no description was found, use raw task_json as fallback */
        if (!desc || !desc[0]) {
            snprintf(t->description, sizeof(t->description), "%s", task_json);
        }
    } else {
        snprintf(t->description, sizeof(t->description), "untitled task");
        snprintf(t->column, sizeof(t->column), "todo");
        t->priority = 3;
    }

    t->created_at = time(NULL);
    t->updated_at = t->created_at;
    t->comment_count = 0;
    t->heartbeat_ts = 0;

    char desc_esc[4096];
    json_escape(t->description, desc_esc, sizeof(desc_esc));

    char result[1024];
    snprintf(result, sizeof(result),
             "{\"status\":\"ok\",\"task_id\":\"%s\",\"board\":\"%s\","
             "\"column\":\"%s\",\"priority\":%d}",
             t->id, b->name, t->column, t->priority);
    b->task_count++;
    return strdup(result);
}

static char *kanban_get_board(const char *board_id) {
    int id = board_id ? atoi(board_id) : -1;
    if (id < 0 || id >= board_count)
        return strdup("{\"error\":\"invalid board_id\"}");

    kanban_board_t *b = &boards[id];

    /* Estimate buffer: board header + per-task JSON */
    size_t buf_sz = 4096 + (size_t)b->task_count * 1024;
    char *result = (char *)malloc(buf_sz);
    if (!result)
        return strdup("{\"error\":\"out of memory\"}");

    size_t pos = 0;
    pos += snprintf(result + pos, buf_sz - pos,
        "{\"board\":{\"id\":%d,\"name\":\"%s\",\"active\":%s,"
        "\"columns\":[",
        id, b->name, b->active ? "true" : "false");

    for (int c = 0; c < b->column_count; c++) {
        if (c > 0) result[pos++] = ',';
        pos += snprintf(result + pos, buf_sz - pos, "\"%s\"", b->columns[c]);
    }

    pos += snprintf(result + pos, buf_sz - pos, "],\"tasks\":[");

    for (int i = 0; i < b->task_count; i++) {
        if (i > 0) result[pos++] = ',';
        pos = append_task_json(result, buf_sz, pos, &b->tasks[i]);
    }

    pos += snprintf(result + pos, buf_sz - pos, "]}}");
    return result;
}

static char *kanban_list_boards(void) {
    /* Estimate buffer */
    size_t buf_sz = 2048 + (size_t)board_count * 512;
    char *result = (char *)malloc(buf_sz);
    if (!result)
        return strdup("{\"boards\":[]}");

    size_t pos = 0;
    pos += snprintf(result + pos, buf_sz - pos,
        "{\"count\":%d,\"boards\":[", board_count);

    for (int i = 0; i < board_count; i++) {
        if (i > 0) result[pos++] = ',';
        pos += snprintf(result + pos, buf_sz - pos,
            "{\"id\":%d,\"name\":\"%s\",\"active\":%s,\"task_count\":%d}",
            i, boards[i].name,
            boards[i].active ? "true" : "false",
            boards[i].task_count);
    }

    pos += snprintf(result + pos, buf_sz - pos, "]}");
    return result;
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
    if (g_initialized) return 0;
    fprintf(stderr, "[kanban-board] initializing...\n");
    board_count = 0;
    g_task_counter = 0;
    memset(boards, 0, sizeof(boards));
    g_initialized = 1;
    fprintf(stderr, "[kanban-board] ready (%d boards x %d tasks per board)\n",
            MAX_BOARDS, MAX_TASKS_PER_BOARD);
    return 0;
}

int plugin_cleanup(void) {
    fprintf(stderr, "[kanban-board] shutting down...\n");
    board_count = 0;
    g_task_counter = 0;
    g_initialized = 0;
    return 0;
}

int plugin_configure(const char *config_json) {
    fprintf(stderr, "[kanban-board] configure: %s\n", config_json);
    /* Accept initial columns config */
    (void)config_json;
    return 0;
}
