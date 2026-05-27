/*
 * path_security.c — Path security checking tool for Hermes C.
 * Checks for path traversal, containment, and normalization.
 */
#include "hermes.h"
#include "hermes_json.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"path\":{\"type\":\"string\",\"description\":\"Path to check\"},"
      "\"root\":{\"type\":\"string\",\"description\":\"Root directory for containment check (optional)\"}"
    "},"
    "\"required\":[\"path\"]"
"}";

static char *path_security_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *path = json_object_get_string(args, "path", NULL);
    const char *root = json_object_get_string(args, "root", NULL);

    if (!path) {
        json_free(args);
        return strdup("{\"error\":\"Missing path\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));

    /* Check traversal */
    bool has_traversal = path_has_traversal(path);
    json_object_set(result, "has_traversal", json_new_bool(has_traversal));

    /* Normalize */
    char *normalized = path_normalize(path);
    json_object_set(result, "normalized", json_new_string(normalized ? normalized : path));
    free(normalized);

    /* Check if absolute */
    json_object_set(result, "is_absolute", json_new_bool(path_is_absolute(path)));

    /* Exists? */
    json_object_set(result, "exists", json_new_bool(path_exists(path)));

    /* Is file or dir? */
    if (path_exists(path)) {
        json_object_set(result, "is_file", json_new_bool(path_is_file(path)));
        json_object_set(result, "is_dir", json_new_bool(path_is_dir(path)));
    }

    /* Containment check */
    if (root && root[0]) {
        char *within = path_within_dir(path, root);
        json_object_set(result, "within_root", json_new_bool(within == NULL));
        if (within) {
            json_object_set(result, "security_warning", json_new_string(within));
            free(within);
        }
        json_object_set(result, "root", json_new_string(root));
    }

    json_free(args);
    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

void registry_init_path_security(void) {
    registry_register("path_security",
        "Check path security: detect traversal (..), normalize path, "
        "verify containment within a root directory, "
        "check if path exists and type (file/dir).",
        SCHEMA, path_security_handler);
}
