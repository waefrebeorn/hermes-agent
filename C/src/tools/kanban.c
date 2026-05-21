/*
 * kanban.c — Kanban tools for Hermes C.
 * File-based JSON storage under ~/.slermes/kanban/.
 * Each task = one JSON file. Task links in links.json.
 * Gated on HERMES_KANBAN_TASK env or config toolset.
 * MIT License — WuBu Hermes Project
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* ================================================================
 *  Constants
 * ================================================================ */

#define MAX_PATH 4096
#define KANBAN_LIST_LIMIT_DEFAULT 50
#define KANBAN_LIST_LIMIT_MAX 200
#define MAX_EVENTS 50

/* ================================================================
 *  Helpers
 * ================================================================ */

static const char *kanban_dir(void) {
    static char dir[MAX_PATH] = {0};
    if (dir[0]) return dir;
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(dir, sizeof(dir), "%s/kanban", home);
    mkdir(dir, 0755);
    return dir;
}

static void links_path(char *buf, size_t sz) {
    snprintf(buf, sz, "%s/links.json", kanban_dir());
}

static void gen_id(char *buf, size_t sz) {
    static bool seeded = false;
    if (!seeded) { srand((unsigned)(time(NULL) ^ (getpid() << 16))); seeded = true; }
    snprintf(buf, sz, "%08x-%04x-%04x-%04x-%08x%04x",
        (unsigned)rand(), (unsigned)(rand() & 0xffff),
        (unsigned)((rand() & 0x0fff) | 0x4000),
        (unsigned)((rand() & 0x3fff) | 0x8000),
        (unsigned)rand(), (unsigned)(rand() & 0xffff));
}

static void now_iso(char *buf, size_t sz) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, sz, "%Y-%m-%dT%H:%M:%S", &tm);
}

static void task_path(const char *tid, char *buf, size_t sz) {
    snprintf(buf, sz, "%s/%s.json", kanban_dir(), tid);
}

static bool kanban_mode(void) {
    if (getenv("HERMES_KANBAN_TASK")) return true;
    return false;
}

static bool kanban_orchestrator(void) {
    if (getenv("HERMES_KANBAN_TASK")) return false;
    return true;
}

static const char *default_task_id(const char *arg) {
    if (arg && *arg) return arg;
    const char *env = getenv("HERMES_KANBAN_TASK");
    return env ? env : NULL;
}

static const char *enforce_ownership(const char *tid) {
    const char *env_tid = getenv("HERMES_KANBAN_TASK");
    if (!env_tid) return NULL;
    if (strcmp(tid, env_tid) != 0) {
        static char err[512];
        snprintf(err, sizeof(err),
            "{\"error\":\"worker is scoped to task %s; refusing to mutate %s\"}",
            env_tid, tid);
        return err;
    }
    return NULL;
}

static const char *require_orchestrator(const char *tool_name) {
    if (getenv("HERMES_KANBAN_TASK")) {
        static char err[512];
        snprintf(err, sizeof(err),
            "{\"error\":\"%s is orchestrator-only\"}", tool_name);
        return err;
    }
    return NULL;
}

static json_t *read_task(const char *tid) {
    char path[MAX_PATH];
    task_path(tid, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    json_t *j = json_parse(buf, NULL);
    free(buf);
    return j;
}

static bool write_task(const char *tid, json_t *j) {
    char path[MAX_PATH];
    task_path(tid, path, sizeof(path));
    char *json_str = json_serialize(j);
    if (!json_str) return false;
    FILE *f = fopen(path, "wb");
    if (!f) { free(json_str); return false; }
    size_t len = strlen(json_str);
    fwrite(json_str, 1, len, f);
    fclose(f);
    free(json_str);
    return true;
}

static json_t *read_links(void) {
    char path[MAX_PATH];
    links_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    json_t *j = json_parse(buf, NULL);
    free(buf);
    return j;
}

static bool write_links(json_t *j) {
    char path[MAX_PATH];
    links_path(path, sizeof(path));
    char *json_str = json_serialize(j);
    if (!json_str) return false;
    FILE *f = fopen(path, "wb");
    if (!f) { free(json_str); return false; }
    size_t len = strlen(json_str);
    fwrite(json_str, 1, len, f);
    fclose(f);
    free(json_str);
    return true;
}

static json_t *get_parent_ids(const char *tid) {
    json_t *links = read_links();
    json_t *parents = json_array();
    if (!links) return parents;
    json_t *link_list = json_obj_get(links, "links");
    if (link_list && link_list->type == JSON_ARRAY) {
        size_t n = json_len(link_list);
        for (size_t i = 0; i < n; i++) {
            json_t *link = json_get(link_list, i);
            const char *child = json_get_str(link, "child", "");
            if (strcmp(child, tid) == 0) {
                const char *parent = json_get_str(link, "parent", "");
                if (parent && *parent)
                    json_append(parents, json_string(parent));
            }
        }
    }
    json_free(links);
    return parents;
}

static json_t *get_child_ids(const char *tid) {
    json_t *links = read_links();
    json_t *children = json_array();
    if (!links) return children;
    json_t *link_list = json_obj_get(links, "links");
    if (link_list && link_list->type == JSON_ARRAY) {
        size_t n = json_len(link_list);
        for (size_t i = 0; i < n; i++) {
            json_t *link = json_get(link_list, i);
            const char *parent = json_get_str(link, "parent", "");
            if (strcmp(parent, tid) == 0) {
                const char *child = json_get_str(link, "child", "");
                if (child && *child)
                    json_append(children, json_string(child));
            }
        }
    }
    json_free(links);
    return children;
}

/* Add event to task, keep last MAX_EVENTS */
static bool add_event(const char *tid, const char *kind,
                      const char *payload, json_t *task) {
    json_t *events = json_obj_get(task, "events");
    if (!events) {
        events = json_array();
        json_set(task, "events", events);
    }
    json_t *ev = json_object();
    json_set(ev, "kind", json_string(kind));
    json_set(ev, "payload", json_string(payload ? payload : ""));
    char ts[64];
    now_iso(ts, sizeof(ts));
    json_set(ev, "created_at", json_string(ts));
    json_append(events, ev);

    /* Cap events to MAX_EVENTS — rebuild array with last N */
    size_t n = json_len(events);
    if (n > MAX_EVENTS) {
        json_t *trimmed = json_array();
        for (size_t i = n - MAX_EVENTS; i < n; i++)
            json_append(trimmed, json_get(events, i));
        json_set(task, "events", trimmed);
    }
    return true;
}

/* Create a task summary dict from task JSON */
static json_t *task_summary(const char *tid, json_t *task) {
    json_t *s = json_object();
    json_set(s, "id", json_string(tid));
    json_set(s, "title", json_string(json_get_str(task, "title", "")));
    json_set(s, "assignee", json_string(json_get_str(task, "assignee", "")));
    json_set(s, "status", json_string(json_get_str(task, "status", "")));
    json_set(s, "priority", json_number(json_get_num(task, "priority", 0)));

    json_t *parents = get_parent_ids(tid);
    json_t *children = get_child_ids(tid);
    json_set(s, "parents", parents);
    json_set(s, "children", children);
    json_set(s, "parent_count", json_number((double)json_len(parents)));
    json_set(s, "child_count", json_number((double)json_len(children)));

    json_set(s, "tenant", json_string(json_get_str(task, "tenant", "")));
    json_set(s, "workspace_kind", json_string(json_get_str(task, "workspace_kind", "")));
    json_set(s, "workspace_path", json_string(json_get_str(task, "workspace_path", "")));
    json_set(s, "created_by", json_string(json_get_str(task, "created_by", "")));
    json_set(s, "created_at", json_string(json_get_str(task, "created_at", "")));
    return s;
}

/* ================================================================
 *  Handlers
 * ================================================================ */

static char *handle_show(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");

    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    json_free(args);
    if (!tid || !*tid)
        return strdup("{\"error\":\"task_id is required (or set HERMES_KANBAN_TASK in the env)\"}");

    json_t *task = read_task(tid);
    if (!task) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"task %s not found\"}", tid);
        return strdup(err);
    }

    json_t *result = json_object();
    json_set(result, "task", task);
    json_set(result, "parents", get_parent_ids(tid));
    json_set(result, "children", get_child_ids(tid));

    json_t *comments = json_obj_get(task, "comments");
    if (!comments) comments = json_array();
    json_set(result, "comments", comments);

    json_t *events = json_obj_get(task, "events");
    if (events) {
        size_t n = json_len(events);
        size_t start = n > 50 ? n - 50 : 0;
        json_t *trimmed = json_array();
        for (size_t i = start; i < n; i++)
            json_append(trimmed, json_get(events, i));
        json_set(result, "events", trimmed);
    } else {
        json_set(result, "events", json_array());
    }

    json_t *runs = json_obj_get(task, "runs");
    if (!runs) runs = json_array();
    json_set(result, "runs", runs);

    char *out = json_serialize(result);
    json_free(result);
    return out ? out : strdup("{\"error\":\"OOM\"}");
}

static char *handle_list(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *guard = require_orchestrator("kanban_list");
    if (guard) return strdup(guard);

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");

    const char *assignee = json_get_str(args, "assignee", "");
    const char *status = json_get_str(args, "status", "");
    const char *tenant = json_get_str(args, "tenant", "");
    bool include_archived = json_get_bool(args, "include_archived", false);
    int limit = (int)json_get_num(args, "limit", KANBAN_LIST_LIMIT_DEFAULT);
    json_free(args);

    if (limit < 1) limit = 1;
    if (limit > KANBAN_LIST_LIMIT_MAX) limit = KANBAN_LIST_LIMIT_MAX;

    json_t *tasks = json_array();
    DIR *dir = opendir(kanban_dir());
    int count = 0;
    bool truncated = false;

    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && !truncated) {
            const char *name = entry->d_name;
            size_t nlen = strlen(name);
            if (nlen < 6) continue;
            if (strcmp(name + nlen - 5, ".json") != 0) continue;
            if (strcmp(name, "links.json") == 0) continue;

            char tid[256];
            snprintf(tid, sizeof(tid), "%.*s", (int)(nlen - 5), name);

            json_t *task = read_task(tid);
            if (!task) continue;

            const char *t_status = json_get_str(task, "status", "");
            const char *t_assignee = json_get_str(task, "assignee", "");
            const char *t_tenant = json_get_str(task, "tenant", "");

            if (*status && strcmp(t_status, status) != 0) {
                json_free(task); continue;
            }
            if (*assignee && strcmp(t_assignee, assignee) != 0) {
                json_free(task); continue;
            }
            if (*tenant && strcmp(t_tenant, tenant) != 0) {
                json_free(task); continue;
            }
            if (!include_archived && strcmp(t_status, "archived") == 0) {
                json_free(task); continue;
            }

            if (count >= limit) {
                truncated = true;
                json_free(task);
                break;
            }

            json_append(tasks, task_summary(tid, task));
            count++;
            json_free(task);
        }
        closedir(dir);
    }

    json_t *result = json_object();
    json_set(result, "tasks", tasks);
    json_set(result, "count", json_number((double)count));
    json_set(result, "limit", json_number((double)limit));
    json_set(result, "truncated", json_bool(truncated));
    json_set(result, "promoted", json_number(0));
    if (truncated && limit < KANBAN_LIST_LIMIT_MAX) {
        int next = limit * 2;
        if (next > KANBAN_LIST_LIMIT_MAX) next = KANBAN_LIST_LIMIT_MAX;
        json_set(result, "next_limit", json_number((double)next));
    }

    char *out = json_serialize(result);
    json_free(result);
    return out ? out : strdup("{\"error\":\"OOM\"}");
}

static char *handle_complete(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");

    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    if (!tid || !*tid) { json_free(args); return strdup("{\"error\":\"task_id is required\"}"); }
    const char *ownership = enforce_ownership(tid);
    if (ownership) { json_free(args); return strdup(ownership); }

    const char *summary = json_get_str(args, "summary", "");
    const char *result_str = json_get_str(args, "result", "");
    json_free(args);

    if (!*summary && !*result_str)
        return strdup("{\"error\":\"provide at least one of: summary or result\"}");

    json_t *task = read_task(tid);
    if (!task) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"task %s not found\"}", tid);
        return strdup(err);
    }

    json_set(task, "status", json_string("done"));
    char ts[64]; now_iso(ts, sizeof(ts));
    json_set(task, "completed_at", json_string(ts));
    if (*summary) json_set(task, "summary", json_string(summary));
    if (*result_str) json_set(task, "result", json_string(result_str));

    json_t *runs = json_obj_get(task, "runs");
    if (!runs) {
        runs = json_array();
        json_set(task, "runs", runs);
    }
    json_t *run = json_object();
    json_set(run, "id", json_string("run-1"));
    json_set(run, "status", json_string("completed"));
    json_set(run, "outcome", json_string("success"));
    json_set(run, "summary", json_string(summary));
    json_set(run, "started_at", json_string(ts));
    json_set(run, "ended_at", json_string(ts));
    json_append(runs, run);

    add_event(tid, "completed", summary, task);
    write_task(tid, task);
    json_free(task);
    return strdup("{\"ok\":true}");
}

static char *handle_block(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");

    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    const char *reason = json_get_str(args, "reason", "");
    json_free(args);
    if (!tid || !*tid) return strdup("{\"error\":\"task_id is required\"}");

    const char *ownership = enforce_ownership(tid);
    if (ownership) return strdup(ownership);

    json_t *task = read_task(tid);
    if (!task) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"task %s not found\"}", tid);
        return strdup(err);
    }
    json_set(task, "status", json_string("blocked"));
    add_event(tid, "blocked", *reason ? reason : "blocked without reason", task);
    write_task(tid, task);
    json_free(task);
    return strdup("{\"ok\":true,\"status\":\"blocked\"}");
}

static char *handle_heartbeat(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    json_free(args);
    if (!tid || !*tid) return strdup("{\"error\":\"task_id is required\"}");

    const char *ownership = enforce_ownership(tid);
    if (ownership) return strdup(ownership);

    json_t *task = read_task(tid);
    if (!task) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"task %s not found\"}", tid);
        return strdup(err);
    }
    char ts[64]; now_iso(ts, sizeof(ts));
    add_event(tid, "heartbeat", ts, task);
    write_task(tid, task);
    json_free(task);
    return strdup("{\"ok\":true}");
}

static char *handle_comment(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = default_task_id(json_get_str(args, "task_id", ""));
    const char *body = json_get_str(args, "body", "");
    json_free(args);
    if (!tid || !*tid) return strdup("{\"error\":\"task_id is required\"}");
    if (!*body) return strdup("{\"error\":\"body is required\"}");

    json_t *task = read_task(tid);
    if (!task) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"task %s not found\"}", tid);
        return strdup(err);
    }
    json_t *comments = json_obj_get(task, "comments");
    if (!comments) {
        comments = json_array();
        json_set(task, "comments", comments);
    }
    json_t *comment = json_object();
    json_set(comment, "author", json_string("worker"));
    json_set(comment, "body", json_string(body));
    char ts[64]; now_iso(ts, sizeof(ts));
    json_set(comment, "created_at", json_string(ts));
    json_append(comments, comment);
    add_event(tid, "comment", body, task);
    write_task(tid, task);
    json_free(task);
    return strdup("{\"ok\":true}");
}

static char *handle_create(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");

    const char *title = json_get_str(args, "title", "");
    const char *assignee = json_get_str(args, "assignee", "");
    if (!*title || !*assignee) {
        json_free(args);
        return strdup("{\"error\":\"title and assignee are required\"}");
    }

    char new_id[64];
    gen_id(new_id, sizeof(new_id));

    json_t *task = json_object();
    json_set(task, "title", json_string(title));
    json_set(task, "assignee", json_string(assignee));

    const char *body = json_get_str(args, "body", "");
    if (*body) json_set(task, "body", json_string(body));

    int priority = (int)json_get_num(args, "priority", 0);
    json_set(task, "priority", json_number((double)priority));

    const char *tenant = json_get_str(args, "tenant", "");
    json_set(task, "tenant", json_string(tenant));
    const char *wk = json_get_str(args, "workspace_kind", "");
    if (*wk) json_set(task, "workspace_kind", json_string(wk));
    const char *wp = json_get_str(args, "workspace_path", "");
    if (*wp) json_set(task, "workspace_path", json_string(wp));

    const char *initial_status = json_get_str(args, "initial_status", "");
    bool triage = json_get_bool(args, "triage", false);
    json_free(args);

    const char *status = "running";
    if (*initial_status) status = initial_status;
    else if (triage) status = "triage";
    json_set(task, "status", json_string(status));

    const char *created_by = getenv("HERMES_PROFILE");
    if (!created_by) created_by = "worker";
    json_set(task, "created_by", json_string(created_by));

    char ts[64]; now_iso(ts, sizeof(ts));
    json_set(task, "created_at", json_string(ts));
    json_set(task, "started_at", json_string(ts));
    json_set(task, "events", json_array());
    json_set(task, "comments", json_array());
    json_set(task, "runs", json_array());
    add_event(new_id, "created", title, task);
    write_task(new_id, task);
    json_free(task);

    char out[256];
    snprintf(out, sizeof(out),
             "{\"ok\":true,\"task_id\":\"%s\",\"status\":\"%s\"}", new_id, status);
    return strdup(out);
}

static char *handle_unblock(const char *args_json, const char *task_id) {
    (void)task_id;
    const char *guard = require_orchestrator("kanban_unblock");
    if (guard) return strdup(guard);
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *tid = json_get_str(args, "task_id", "");
    json_free(args);
    if (!*tid) return strdup("{\"error\":\"task_id is required\"}");

    json_t *task = read_task(tid);
    if (!task) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"task %s not found\"}", tid);
        return strdup(err);
    }
    json_set(task, "status", json_string("ready"));
    add_event(tid, "unblocked", "unblocked by orchestrator", task);
    write_task(tid, task);
    json_free(task);
    char out[256];
    snprintf(out, sizeof(out),
             "{\"ok\":true,\"task_id\":\"%s\",\"status\":\"ready\"}", tid);
    return strdup(out);
}

static char *handle_link(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON\"}");
    const char *parent_id = json_get_str(args, "parent_id", "");
    const char *child_id = json_get_str(args, "child_id", "");
    json_free(args);
    if (!*parent_id || !*child_id)
        return strdup("{\"error\":\"both parent_id and child_id are required\"}");
    if (strcmp(parent_id, child_id) == 0)
        return strdup("{\"error\":\"self-links are not allowed\"}");

    json_t *p = read_task(parent_id);
    json_t *c = read_task(child_id);
    if (!p) {
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"parent task %s not found\"}", parent_id);
        return strdup(err);
    }
    if (!c) {
        json_free(p);
        char err[256];
        snprintf(err, sizeof(err), "{\"error\":\"child task %s not found\"}", child_id);
        return strdup(err);
    }
    json_free(p); json_free(c);

    json_t *links = read_links();
    if (!links) {
        links = json_object();
        json_set(links, "links", json_array());
    }
    json_t *link_list = json_obj_get(links, "links");
    if (!link_list) {
        link_list = json_array();
        json_set(links, "links", link_list);
    }

    /* Check duplicate */
    size_t n = json_len(link_list);
    for (size_t i = 0; i < n; i++) {
        json_t *l = json_get(link_list, i);
        const char *p = json_get_str(l, "parent", "");
        const char *c = json_get_str(l, "child", "");
        if (strcmp(p, parent_id) == 0 && strcmp(c, child_id) == 0) {
            json_free(links);
            return strdup("{\"ok\":true}");
        }
    }

    /* Check cycle: does child already have parent_id as descendant? */
    json_t *child_children = get_child_ids(child_id);
    if (child_children) {
        size_t cn = json_len(child_children);
        for (size_t i = 0; i < cn; i++) {
            json_t *elem = json_get(child_children, i);
            if (elem->type == JSON_STRING && elem->str_val &&
                strcmp(elem->str_val, parent_id) == 0) {
                json_free(child_children);
                json_free(links);
                return strdup("{\"error\":\"cycle detected\"}");
            }
        }
        json_free(child_children);
    }

    json_t *link = json_object();
    json_set(link, "parent", json_string(parent_id));
    json_set(link, "child", json_string(child_id));
    json_append(link_list, link);
    write_links(links);
    json_free(links);

    char out[256];
    snprintf(out, sizeof(out),
             "{\"ok\":true,\"parent_id\":\"%s\",\"child_id\":\"%s\"}", parent_id, child_id);
    return strdup(out);
}

/* ================================================================
 *  Registration
 * ================================================================ */

void registry_init_kanban(void) {
    registry_register("kanban_show",
        "Read a task's full state — title, body, assignee, parent task handoffs, your prior attempts on this task if any, comments, and recent events. Use this to (re)orient yourself before starting work, especially on retries.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. If omitted, defaults to HERMES_KANBAN_TASK from the env.\"}},\"required\":[]}",
        handle_show);

    registry_register("kanban_list",
        "List Kanban task summaries so an orchestrator profile can discover work to route. Supports assignee, status, tenant, include_archived, and limit filters. Orchestrator-only.",
        "{\"type\":\"object\",\"properties\":{\"assignee\":{\"type\":\"string\",\"description\":\"Optional assignee filter.\"},\"status\":{\"type\":\"string\",\"enum\":[\"triage\",\"todo\",\"ready\",\"running\",\"blocked\",\"done\",\"archived\"],\"description\":\"Optional status filter.\"},\"tenant\":{\"type\":\"string\",\"description\":\"Optional tenant filter.\"},\"include_archived\":{\"type\":\"boolean\"},\"limit\":{\"type\":\"integer\"}},\"required\":[]}",
        handle_list);

    registry_register("kanban_complete",
        "Mark your current task done with a structured handoff. At least one of summary or result is required.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\",\"description\":\"Task id. Defaults to HERMES_KANBAN_TASK.\"},\"summary\":{\"type\":\"string\"},\"result\":{\"type\":\"string\"},\"metadata\":{\"type\":\"object\"},\"created_cards\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},\"artifacts\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},\"required\":[]}",
        handle_complete);

    registry_register("kanban_block",
        "Mark your current task as blocked. Provide a reason.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"},\"reason\":{\"type\":\"string\"}},\"required\":[]}",
        handle_block);

    registry_register("kanban_heartbeat",
        "Extend the liveness window for this task.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"}},\"required\":[]}",
        handle_heartbeat);

    registry_register("kanban_comment",
        "Add a structured comment to a Kanban task.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"}},\"required\":[\"body\"]}",
        handle_comment);

    registry_register("kanban_create",
        "Create a new Kanban task. title and assignee are required.",
        "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"},\"assignee\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"},\"priority\":{\"type\":\"integer\"},\"tenant\":{\"type\":\"string\"},\"workspace_kind\":{\"type\":\"string\",\"enum\":[\"scratch\",\"dir\",\"worktree\"]},\"workspace_path\":{\"type\":\"string\"},\"triage\":{\"type\":\"boolean\"},\"initial_status\":{\"type\":\"string\",\"enum\":[\"running\",\"blocked\"]}},\"required\":[\"title\",\"assignee\"]}",
        handle_create);

    registry_register("kanban_unblock",
        "Move a blocked Kanban task back to ready. Orchestrator-only.",
        "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"string\"}},\"required\":[\"task_id\"]}",
        handle_unblock);

    registry_register("kanban_link",
        "Add a parent->child dependency edge. Cycles and self-links rejected.",
        "{\"type\":\"object\",\"properties\":{\"parent_id\":{\"type\":\"string\"},\"child_id\":{\"type\":\"string\"}},\"required\":[\"parent_id\",\"child_id\"]}",
        handle_link);
}
