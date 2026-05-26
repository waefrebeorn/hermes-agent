/*
 * todo.c — Task tracking tool for Hermes C.
 * Manages session task list. Backed by JSON file.
 * Actions: add, done, list, update, search.
 * Fields: id, content, status, priority (p0-p3).
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

char *todo_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", "list");
    json_node_t *todos = load_todos();
    json_node_t *result = json_new_object();

    if (strcmp(action, "add") == 0) {
        const char *content = json_object_get_string(args, "content", NULL);
        if (!content) {
            json_object_set(result, "error", json_new_string("Missing content"));
        } else {
            json_node_t *item = json_new_object();
            json_object_set(item, "content", json_new_string(content));
            json_object_set(item, "status", json_new_string("pending"));
            const char *priority = json_object_get_string(args, "priority", NULL);
            if (!priority || (strcmp(priority, "p0") != 0 && strcmp(priority, "p1") != 0
                && strcmp(priority, "p2") != 0 && strcmp(priority, "p3") != 0)) {
                priority = "p2";
            }
            json_object_set(item, "priority", json_new_string(priority));
            char id[32];
            snprintf(id, sizeof(id), "task_%zu", json_len(todos));
            json_object_set(item, "id", json_new_string(id));
            json_append(todos, item);
            if (save_todos(todos))
                json_object_set(result, "status", json_new_string("added"));
            else
                json_object_set(result, "error", json_new_string("save failed"));
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
        json_object_set(result, "found", json_new_bool(found));
        if (found && save_todos(todos))
            json_object_set(result, "status", json_new_string("updated"));
        else if (!found)
            json_object_set(result, "error", json_new_string("Task not found"));

    } else if (strcmp(action, "update") == 0) {
        const char *id = json_object_get_string(args, "id", NULL);
        if (!id) {
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
                    if (status) json_set(item, "status", json_string(status));
                    const char *priority = json_object_get_string(args, "priority", NULL);
                    if (priority && (strcmp(priority, "p0") == 0 || strcmp(priority, "p1") == 0
                        || strcmp(priority, "p2") == 0 || strcmp(priority, "p3") == 0))
                        json_set(item, "priority", json_string(priority));
                    found = true;
                    break;
                }
            }
            json_object_set(result, "found", json_new_bool(found));
            if (found && save_todos(todos))
                json_object_set(result, "status", json_new_string("updated"));
            else if (!found)
                json_object_set(result, "error", json_new_string("Task not found"));
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

            /* Apply filters */
            if (status_filter && strcmp(item_status, status_filter) != 0) continue;
            if (priority_filter && strcmp(item_pri, priority_filter) != 0) continue;
            if (q && strstr(item_content, q) == NULL) continue;

            json_append(matches, json_copy(item));
        }
        json_object_set(result, "count", json_new_number((double)json_len(matches)));
        json_object_set(result, "items", matches);
        json_object_set(result, "total", json_new_number((double)json_len(todos)));

    } else {
        /* List all */
        json_object_set(result, "count", json_new_number((double)json_len(todos)));
        json_object_set(result, "items", json_copy(todos));
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(todos);
    json_free(args);
    return json_out;
}

void registry_init_todo(void) {
    registry_register("todo",
        "Manage session task list. Actions: add (create with priority p0-p3), "
        "done/complete (mark done), update (edit content/status/priority), "
        "search (filter by query, status, priority), list (show all). "
        "Backed by JSON file at HERMES_HOME/todo.json.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"add\",\"done\",\"complete\",\"update\",\"search\",\"list\"],\"description\":\"Action to perform\",\"default\":\"list\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"Task description (required for add)\"},"
          "\"id\":{\"type\":\"string\",\"description\":\"Task ID\"},"
          "\"priority\":{\"type\":\"string\",\"enum\":[\"p0\",\"p1\",\"p2\",\"p3\"],\"description\":\"Priority level: p0=critical, p1=high, p2=normal, p3=low\",\"default\":\"p2\"},"
          "\"status\":{\"type\":\"string\",\"enum\":[\"pending\",\"completed\"],\"description\":\"Task status (for update/search filter)\"},"
          "\"query\":{\"type\":\"string\",\"description\":\"Search query substring (for search action)\"}"
        "},"
        "\"required\":[]"
        "}", todo_handler);
}
