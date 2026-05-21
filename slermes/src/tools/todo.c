/*
 * todo.c — Task tracking tool for Slermes C.
 * Manages session task list. Backed by memory.
 */

#include "slermes.h"
#include "slermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *get_todo_path(void) {
    static char path[4096];
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/.slermes/todo.json", home);
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
            json_t *item = json_get(todos, i);
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
        "Manage session task list. Actions: add (create todo), done/complete (mark done), list (show all). "
        "Backed by JSON file at SLERMES_HOME/todo.json.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"add | done | list\",\"default\":\"list\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"Task description (required for add)\"},"
          "\"id\":{\"type\":\"string\",\"description\":\"Task ID to mark done\"}"
        "},"
        "\"required\":[]"
        "}", todo_handler);
}
