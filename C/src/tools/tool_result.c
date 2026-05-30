/*
 * tool_result.c — Tool result classification + truncation helpers.
 * Port of Python agent/tool_result_classification.py (26 lines),
 * agent/context_compressor._truncate_tool_call_args_json(),
 * and agent/chat_completion_helpers.estimate_request_context_tokens().
 */

#include "hermes_tool_result.h"
#include "hermes_json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 *  File mutation detection
 * ================================================================ */

bool tool_result_file_mutation_landed(const char *tool_name, const char *result) {
    if (!tool_name || !result || !result[0]) return false;

    /* Only applicable to write_file and patch */
    if (strcmp(tool_name, "write_file") != 0 && strcmp(tool_name, "patch") != 0)
        return false;

    /* Parse the result as JSON */
    char *err = NULL;
    json_t *data = json_parse(result, &err);
    if (!data || err) {
        json_free(data);
        free(err);
        return false;
    }
    if (data->type != JSON_OBJECT) {
        json_free(data);
        return false;
    }

    /* Check for error field */
    json_t *err_node = json_obj_get(data, "error");
    if (err_node && err_node->type != JSON_NULL) {
        json_free(data);
        return false;
    }

    bool landed = false;
    if (strcmp(tool_name, "write_file") == 0) {
        /* write_file should have "bytes_written" field > 0 */
        json_t *bw = json_obj_get(data, "bytes_written");
        if (bw && bw->type == JSON_NUMBER) {
            landed = (bw->num_val > 0);
        }
    } else if (strcmp(tool_name, "patch") == 0) {
        /* patch should have "success": true */
        json_t *succ = json_obj_get(data, "success");
        if (succ && succ->type == JSON_BOOL) {
            landed = succ->bool_val;
        }
    }

    json_free(data);
    return landed;
}

/* ================================================================
 *  Tool-call args JSON truncation
 *  Port of Python agent/context_compressor._truncate_tool_call_args_json()
 * ================================================================ */

/* Recursively copy a json_t tree, truncating string values longer than
 * head_chars to "prefix...[truncated]". */
static json_t *json_copy_truncate_strings(const json_t *node, size_t head_chars) {
    if (!node) return NULL;

    switch (node->type) {
        case JSON_NULL:
            return json_null();

        case JSON_BOOL:
            return json_bool(node->bool_val);

        case JSON_NUMBER:
            return json_number(node->num_val);

        case JSON_STRING: {
            const char *s = node->str_val ? node->str_val : "";
            size_t len = strlen(s);
            if (len <= head_chars)
                return json_string(s);

            /* Truncate: prefix + "...[truncated]" */
            size_t suffix_len = 14; /* strlen("...[truncated]") */
            size_t buf_len = head_chars + suffix_len + 1;
            char *buf = malloc(buf_len);
            if (!buf) return json_string(s);
            memcpy(buf, s, head_chars);
            memcpy(buf + head_chars, "...[truncated]", suffix_len);
            buf[head_chars + suffix_len] = '\0';
            json_t *result = json_string(buf);
            free(buf);
            return result;
        }

        case JSON_ARRAY: {
            json_t *arr = json_array();
            for (size_t i = 0; i < node->c.count; i++) {
                json_append(arr, json_copy_truncate_strings(node->c.items[i], head_chars));
            }
            return arr;
        }

        case JSON_OBJECT: {
            json_t *obj = json_object();
            for (size_t i = 0; i < node->c.count; i++) {
                json_t *val = json_copy_truncate_strings(node->c.items[i], head_chars);
                json_set(obj, node->c.keys[i], val);
            }
            return obj;
        }

        default:
            return NULL;
    }
}

char *tool_call_args_truncate(const char *args, size_t head_chars) {
    if (!args || !args[0])
        return NULL;

    /* Parse the arguments JSON */
    char *err = NULL;
    json_t *parsed = json_parse(args, &err);
    if (!parsed || err) {
        json_free(parsed);
        free(err);
        /* Not valid JSON — return NULL so caller falls back to original */
        return NULL;
    }

    /* Clone with truncation */
    json_t *truncated = json_copy_truncate_strings(parsed, head_chars);
    json_free(parsed);

    if (!truncated) return NULL;

    /* Serialize back */
    char *result = json_serialize(truncated);
    json_free(truncated);
    return result;
}

/* ================================================================
 *  Payload context token estimation
 *  Port of Python agent/chat_completion_helpers.estimate_request_context_tokens()
 * ================================================================ */

/* Count characters in a JSON value recursively — string lengths + serialized
 * overhead for non-string types. */
static size_t json_count_chars(const json_t *node) {
    if (!node) return 0;

    switch (node->type) {
        case JSON_NULL:
            return 4; /* strlen("null") */

        case JSON_BOOL:
            return node->bool_val ? 4 : 5; /* "true"/"false" */

        case JSON_NUMBER: {
            char buf[64];
            int n = snprintf(buf, sizeof(buf), "%.17g", node->num_val);
            return (n > 0) ? (size_t)n : 8;
        }

        case JSON_STRING:
            return node->str_val ? strlen(node->str_val) : 0;

        case JSON_ARRAY: {
            size_t total = 0;
            for (size_t i = 0; i < node->c.count; i++)
                total += json_count_chars(node->c.items[i]);
            return total;
        }

        case JSON_OBJECT: {
            size_t total = 0;
            for (size_t i = 0; i < node->c.count; i++) {
                /* Key name */
                total += node->c.keys[i] ? strlen(node->c.keys[i]) : 0;
                /* Value */
                total += json_count_chars(node->c.items[i]);
            }
            return total;
        }

        default:
            return 0;
    }
}

size_t estimate_payload_context_tokens(const char *payload_json) {
    if (!payload_json || !payload_json[0])
        return 0;

    char *err = NULL;
    json_t *parsed = json_parse(payload_json, &err);
    if (!parsed || err) {
        json_free(parsed);
        free(err);
        /* Not valid JSON — count raw chars / 4 as fallback */
        return strlen(payload_json) / 4;
    }

    size_t total_chars = 0;

    if (parsed->type == JSON_ARRAY) {
        /* Bare list -> treat as Chat Completions messages */
        total_chars = json_count_chars(parsed);
        json_free(parsed);
        return total_chars / 4;
    }

    if (parsed->type == JSON_OBJECT) {
        /* Check for Chat Completions shape (has "messages") */
        json_t *messages = json_obj_get(parsed, "messages");
        if (messages) {
            total_chars = json_count_chars(messages);
            json_t *tools = json_obj_get(parsed, "tools");
            if (tools)
                total_chars += json_count_chars(tools);
            json_free(parsed);
            return total_chars / 4;
        }

        /* Check for Responses API shape (has "input") */
        json_t *input = json_obj_get(parsed, "input");
        if (input) {
            total_chars = json_count_chars(input);
            json_t *instructions = json_obj_get(parsed, "instructions");
            if (instructions)
                total_chars += json_count_chars(instructions);
            json_t *tools = json_obj_get(parsed, "tools");
            if (tools)
                total_chars += json_count_chars(tools);
            json_free(parsed);
            return total_chars / 4;
        }

        /* Any other dict — sum all values */
        total_chars = json_count_chars(parsed);
        json_free(parsed);
        return total_chars / 4;
    }

    /* Scalar value */
    total_chars = json_count_chars(parsed);
    json_free(parsed);
    return total_chars / 4;
}
