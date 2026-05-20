/*
 * memory.c — Memory (persistent storage) tool for Hermes C.
 * Key-value store backed by JSON file in HERMES_HOME.
 * Replaces Python's memory tool.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *get_memory_path(void) {
    static char path[4096];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/.hermes/memory.json", home);
    return path;
}

/* Load all memory entries as JSON object */
static json_node_t *load_memory(void) {
    const char *mpath = get_memory_path();

    /* Ensure directory exists */
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", mpath);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }

    char *err = NULL;
    json_node_t *doc = json_parse_file(mpath, &err);
    if (doc) return doc;
    free(err);
    return json_new_object();
}

/* Save memory JSON object */
static bool save_memory(json_node_t *obj) {
    const char *mpath = get_memory_path();
    char *json_str = json_serialize(obj);
    if (!json_str) return false;

    FILE *f = fopen(mpath, "w");
    if (!f) { free(json_str); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return true;
}

char *memory_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", "list");
    const char *content = json_object_get_string(args, "content", NULL);

    json_node_t *mem = load_memory();

    json_node_t *result = json_new_object();

    if (strcmp(action, "add") == 0 && content) {
        /* Add a new entry with incremental key */
        char key[32];
        int idx = (int)json_array_count(mem);
        snprintf(key, sizeof(key), "entry_%d", idx);
        json_object_set(mem, key, json_new_string(content));

        if (save_memory(mem)) {
            json_object_set(result, "status", json_new_string("saved"));
            json_object_set(result, "key", json_new_string(key));
        } else {
            json_object_set(result, "error", json_new_string("save failed"));
        }
    } else if (strcmp(action, "read") == 0) {
        /* Return entire memory */
        json_object_set(result, "entries", json_copy(mem));
    } else if (strcmp(action, "clear") == 0) {
        json_node_t *empty = json_new_object();
        if (save_memory(empty)) {
            json_object_set(result, "status", json_new_string("cleared"));
        }
        json_free(empty);
    } else {
        /* List by default */
        size_t count = json_len(mem);
        json_object_set(result, "count", json_new_number((double)count));
        json_object_set(result, "entries", json_copy(mem));
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(mem);
    json_free(args);
    return json_out;
}

void registry_init_memory(void) {
    registry_register("memory",
        "Persistent key-value memory. Actions: add (save a fact), read (dump all), clear. "
        "Data persists across sessions in memory.json.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
          "\"action\":{\"type\":\"string\",\"description\":\"add | read | clear\",\"default\":\"read\"},"
          "\"content\":{\"type\":\"string\",\"description\":\"Content to save (required for add)\"}"
        "},"
        "\"required\":[]"
        "}", memory_handler);
}
