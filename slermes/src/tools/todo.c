/*
 * todo.c — Task tracking tool for Hermes C.
 * Manages session task list. Backed by JSON file.
 * Supports: add, done, list, update, search, merge.
 * Status: pending, in_progress, completed, cancelled.
 * Fields: id, content, status, priority (p0-p3).
 * Always returns full list with summary counts.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *VALID_STATUSES[] = {"pending", "in_progress", "completed", "cancelled"};
static const int NUM_STATUSES = 4;

static bool is_valid_status(const char *s) {
    if (!s) return false;
    for (int i = 0; i < NUM_STATUSES; i++)
        if (strcmp(s, VALID_STATUSES[i]) == 0) return true;
    return false;
}

static const char *VALID_PRIORITIES[] = {"p0", "p1", "p2", "p3"};
static const int NUM_PRIORITIES = 4;

static bool is_valid_priority(const char *s) {
    if (!s) return false;
    for (int i = 0; i < NUM_PRIORITIES; i++)
        if (strcmp(s, VALID_PRIORITIES[i]) == 0) return true;
    return false;
}

static char *get_todo_path(void) {
    static char path[4096];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/.hermes/todo.json", home);
    return path;
}

static json_node_t *load_todos(void) {
    char dir[4096];
    const char *mpath = get_todo_path();
    snprintf(dir, sizeof(dir), "%s", mpath);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }

    char *err = NULL;
    json_node_t *doc = json_parse_file(mpath, &err);
    if (doc) return doc;
    free(err);
    return json_new_array();
}

static bool save_todos(json_node_t *arr) {
    const char *mpath = get_todo_path();
    char *json_str = json_serialize(arr);
    if (!json_str) return false;
    FILE *f = fopen(mpath, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return true;
}

/* Build summary object with count breakdown */
static json_node_t *build_summary(json_node_t *todos) {
    int total = (int)json_len(todos);
    int pending = 0, in_progress = 0, completed = 0, cancelled = 0;
    for (size_t i = 0; i < (size_t)total; i++) {
        json_node_t *item = json_get(todos, i);
        const char *s = json_get_str(item, "status", "");
        if (strcmp(s, "pending") == 0) pending++;
        else if (strcmp(s, "in_progress") == 0) in_progress++;
        else if (strcmp(s, "completed") == 0) completed++;
        else if (strcmp(s, "cancelled") == 0) cancelled++;
    }
    json_node_t *summary = json_new_object();
    json_object_set(summary, "total", json_new_number((double)total));
    json_object_set(summary, "pending", json_new_number((double)pending));
    json_object_set(summary, "in_progress", json_new_number((double)in_progress));
    json_object_set(summary, "completed", json_new_number((double)completed));
    json_object_set(summary, "cancelled", json_new_number((double)cancelled));
    return summary;
}

/* Normalize a single todo item: ensure id, content, status exist */
static json_node_t *normalize_item(json_node_t *item, size_t default_idx) {
    json_node_t *out = json_copy(item);
    /* Ensure id */
    const char *id = json_get_str(out, "id", "");
    if (!id || !*id) {
        char buf[32];
        snprintf(buf, sizeof(buf), "task_%zu", default_idx);
        json_set(out, "id", json_string(buf));
    }
    /* Ensure content */
    const char *content = json_get_str(out, "content", "");
    if (!content || !*content)
        json_set(out, "content", json_string("(no description)"));
    /* Ensure status */
    const char *status = json_get_str(out, "status", "");
    if (!status || !*status || !is_valid_status(status))
        json_set(out, "status", json_string("pending"));
    return out;
}

static json_node_t *build_response(json_node_t *todos) {
    json_node_t *result = json_new_object();
    json_object_set(result, "count", json_new_number((double)json_len(todos)));
    json_object_set(result, "items", json_copy(todos));
    json_object_set(result, "summary", build_summary(todos));
    return result;
}

char *todo_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", "list");
    json_node_t *todos = load_todos();
    json_node_t *result = NULL;

    if (strcmp(action, "add") == 0) {
        const char *content = json_object_get_string(args, "content", NULL);
        if (!content) {
            result = json_new_object();
            json_object_set(result, "error", json_new_string("Missing content"));
        } else {
            json_node_t *item = json_new_object();
            json_object_set(item, "content", json_new_string(content));
            json_object_set(item, "status", json_new_string("pending"));
            const char *priority = json_object_get_string(args, "priority", NULL);
            if (!priority || !is_valid_priority(priority))
                priority = "p2";
            json_object_set(item, "priority", json_new_string(priority));
            const char *deadline = json_object_get_string(args, "deadline", NULL);
            if (deadline) json_object_set(item, "deadline", json_new_string(deadline));
            char id[32];
            snprintf(id, sizeof(id), "task_%zu", json_len(todos));
            json_object_set(item, "id", json_new_string(id));
            json_append(todos, item);
            if (save_todos(todos))
                result = build_response(todos);
            else {
                result = json_new_object();
                json_object_set(result, "error", json_new_string("save failed"));
            }
        }

    } else if (strcmp(action, "done") == 0 || strcmp(action, "complete") == 0) {
        const char *id = json_object_get_string(args, "id", NULL);
        bool found = false;
        for (size_t i = 0; i < json_len(todos); i++) {
            json_node_t *item = json_get(todos, i);
            const char *item_id = json_get_str(item, "id", "");
            if (id && strcmp(item_id, id) == 0) {
                json_set(item, "status", json_string("completed"));
                found = true;
                break;
            }
        }
        if (found && save_todos(todos))
            result = build_response(todos);
        else {
            result = json_new_object();
            if (!found)
                json_object_set(result, "error", json_new_string("Task not found"));
            else
                json_object_set(result, "error", json_new_string("save failed"));
        }

    } else if (strcmp(action, "update") == 0) {
        const char *id = json_object_get_string(args, "id", NULL);
        if (!id) {
            result = json_new_object();
            json_object_set(result, "error", json_new_string("Missing id"));
        } else {
            bool found = false;
            for (size_t i = 0; i < json_len(todos); i++) {
                json_node_t *item = json_get(todos, i);
                const char *item_id = json_get_str(item, "id", "");
                if (strcmp(item_id, id) == 0) {
                    const char *content = json_object_get_string(args, "content", NULL);
                    if (content) json_set(item, "content", json_string(content));
                    const char *status = json_object_get_string(args, "status", NULL);
                    if (status && is_valid_status(status))
                        json_set(item, "status", json_string(status));
                    const char *priority = json_object_get_string(args, "priority", NULL);
                    if (priority && is_valid_priority(priority))
                        json_set(item, "priority", json_string(priority));
                    const char *deadline = json_object_get_string(args, "deadline", NULL);
                    if (deadline) json_set(item, "deadline", json_string(deadline));
                    found = true;
                    break;
                }
            }
            if (found && save_todos(todos))
                result = build_response(todos);
            else {
                result = json_new_object();
                if (!found)
                    json_object_set(result, "error", json_new_string("Task not found"));
                else
                    json_object_set(result, "error", json_new_string("save failed"));
            }
        }

    } else if (strcmp(action, "search") == 0) {
        const char *q = json_object_get_string(args, "query", NULL);
        const char *status_filter = json_object_get_string(args, "status", NULL);
        const char *priority_filter = json_object_get_string(args, "priority", NULL);

        json_node_t *matches = json_new_array();
        for (size_t i = 0; i < json_len(todos); i++) {
            json_node_t *item = json_get(todos, i);
            const char *item_content = json_get_str(item, "content", "");
            const char *item_status = json_get_str(item, "status", "");
            const char *item_pri = json_get_str(item, "priority", "");

            if (status_filter && strcmp(item_status, status_filter) != 0) continue;
            if (priority_filter && strcmp(item_pri, priority_filter) != 0) continue;
            if (q && strstr(item_content, q) == NULL) continue;

            json_append(matches, json_copy(item));
        }
        result = json_new_object();
        json_object_set(result, "count", json_new_number((double)json_len(matches)));
        json_object_set(result, "items", matches);
        json_object_set(result, "total", json_new_number((double)json_len(todos)));

    } else if (strcmp(action, "write") == 0 || strcmp(action, "merge") == 0) {
        /* Write/merge mode with todo items array — like Python's write() */
        json_node_t *provided = json_object_get(args, "todos");
        bool merge_mode = (strcmp(action, "merge") == 0)
            || json_object_get_bool(args, "merge", false);

        if (merge_mode) {
            /* Merge: update existing by id, append new ones */
            for (size_t i = 0; i < json_len(provided); i++) {
                json_node_t *incoming = json_get(provided, i);
                const char *incoming_id = json_get_str(incoming, "id", "");
                bool found = false;
                for (size_t j = 0; j < json_len(todos); j++) {
                    json_node_t *existing = json_get(todos, j);
                    const char *existing_id = json_get_str(existing, "id", "");
                    if (strcmp(incoming_id, existing_id) == 0) {
                        /* Update fields that were provided */
                        const char *content = json_get_str(incoming, "content", "");
                        if (*content) json_set(existing, "content", json_string(content));
                        const char *status = json_get_str(incoming, "status", "");
                        if (*status && is_valid_status(status))
                            json_set(existing, "status", json_string(status));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* New item — normalize and append */
                    json_node_t *normalized = normalize_item(incoming, json_len(todos));
                    json_append(todos, normalized);
                }
            }
        } else {
            /* Replace: clear and re-add */
            json_node_t *new_todos = json_new_array();
            for (size_t i = 0; i < json_len(provided); i++) {
                json_node_t *normalized = normalize_item(json_get(provided, i), i);
                json_append(new_todos, normalized);
            }
            json_free(todos);
            todos = new_todos;
        }

        if (save_todos(todos))
            result = build_response(todos);
        else {
            result = json_new_object();
            json_object_set(result, "error", json_new_string("save failed"));
        }

    } else {
        /* Default: list all */
        result = build_response(todos);
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(todos);
    json_free(args);
    return json_out;
}

void registry_init_todo(void) {
    registry_register("todo",
        "Manage your task list for the current session. "
        "Actions: add (create with priority p0-p3), done/complete (mark completed), "
        "update (edit content/status/priority), "
        "write (replace entire list with 'todos' array), "
        "merge (update existing items by id, append new ones), "
        "search (filter by query, status, priority), list (show all). "
        "Always returns full list with summary counts. "
        "Backed by JSON file at HERMES_HOME/todo.json.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"add\",\"done\",\"complete\",\"update\",\"search\",\"list\",\"write\",\"merge\"],\"description\":\"Action to perform\",\"default\":\"list\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"Task description (required for add)\"},"
          "\"id\":{\"type\":\"string\",\"description\":\"Task ID\"},"
          "\"priority\":{\"type\":\"string\",\"enum\":[\"p0\",\"p1\",\"p2\",\"p3\"],\"description\":\"Priority level: p0=critical, p1=high, p2=normal, p3=low\",\"default\":\"p2\"},"
          "\"status\":{\"type\":\"string\",\"enum\":[\"pending\",\"in_progress\",\"completed\",\"cancelled\"],\"description\":\"Task status (for update/search filter)\"},"
          "\"query\":{\"type\":\"string\",\"description\":\"Search query substring (for search action)\"},"
          "\"todos\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"},\"status\":{\"type\":\"string\",\"enum\":[\"pending\",\"in_progress\",\"completed\",\"cancelled\"]}},\"required\":[\"id\",\"content\",\"status\"]},\"description\":\"Task items array (for write/merge actions)\"},"
          "\"merge\":{\"type\":\"boolean\",\"description\":\"When action=write, true=update existing by id, false=replace entire list\",\"default\":false}"
        "},"
        "\"required\":[]"
        "}", todo_handler);
}
