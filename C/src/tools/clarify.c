/*
 * clarify.c — Clarify tool for Hermes C.
 * Asks user a question with optional multiple-choice options.
 * Returns user's response.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"question\":{\"type\":\"string\",\"description\":\"Question to ask the user\"},"
      "\"choices\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Optional multiple choice options\",\"maxItems\":4}"
    "},"
    "\"required\":[\"question\"]"
"}";

char *clarify_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *question = json_object_get_string(args, "question", NULL);
    if (!question) { json_free(args); return strdup("{\"error\":\"Missing question\"}"); }

    /* Print question to stdout for the CLI user */
    printf("\n=== CLARIFY ===\n");
    printf("%s\n", question);

    json_node_t *choices = json_object_get(args, "choices");
    if (choices && json_array_count(choices) > 0) {
        printf("Choices:\n");
        for (size_t i = 0; i < json_array_count(choices); i++) {
            json_node_t *c = json_array_get(choices, i);
            if (c && c->type == JSON_STRING)
                printf("  %zu) %s\n", i + 1, c->str_val);
        }
    }
    printf("(Enter response, or 'skip' to cancel)\n> ");
    fflush(stdout);

    /* Read user response */
    char response[4096];
    if (!fgets(response, sizeof(response), stdin)) {
        json_free(args);
        return strdup("{\"response\":\"(no input)\"}");
    }

    /* Strip newline */
    size_t len = strlen(response);
    while (len > 0 && (response[len-1] == '\n' || response[len-1] == '\r'))
        response[--len] = '\0';

    json_node_t *result = json_new_object();
    json_object_set(result, "response", json_new_string(response));

    /* If choice-based, capture choice index */
    if (choices && json_array_count(choices) > 0) {
        int choice_idx = atoi(response) - 1;
        if (choice_idx >= 0 && choice_idx < (int)json_array_count(choices)) {
            json_node_t *selected = json_array_get(choices, (size_t)choice_idx);
            if (selected && selected->type == JSON_STRING)
                json_object_set(result, "selected", json_new_string(selected->str_val));
        }
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_clarify(void) {
    registry_register("clarify",
        "Ask the user a question. Supports optional multiple-choice options. "
        "Use when you need user input or a decision.",
        SCHEMA, clarify_handler);
}
