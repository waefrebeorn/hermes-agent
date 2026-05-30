/*
 * ansi_strip.c — ANSI escape code stripping tool for Hermes C.
 * Removes ANSI escape sequences from text.
 * Wraps the ansi_strip library function as a registered tool.
 */
#include "hermes.h"
#include "hermes_json.h"
#include "ansi_strip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"text\":{\"type\":\"string\",\"description\":\"Text containing ANSI escape codes to strip\"}"
    "},"
    "\"required\":[\"text\"]"
"}";

static char *ansi_strip_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *text = json_object_get_string(args, "text", NULL);
    if (!text) {
        json_free(args);
        return strdup("{\"error\":\"Missing text\"}");
    }

    char *stripped = ansi_strip(text);
    if (!stripped) {
        json_free(args);
        return strdup("{\"error\":\"Strip failed\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "original_length", json_new_number((double)strlen(text)));
    json_object_set(result, "stripped", json_new_string(stripped));
    json_object_set(result, "stripped_length", json_new_number((double)strlen(stripped)));
    free(stripped);

    json_free(args);
    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

void registry_init_ansi_strip(void) {
    registry_register("strip_ansi",
        "Remove ANSI escape codes from text. Returns cleaned text with original and stripped lengths.",
        SCHEMA, ansi_strip_handler);
}
