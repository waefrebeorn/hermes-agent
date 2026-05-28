/*
 * tool_result.c — Tool result classification helpers.
 * Port of Python agent/tool_result_classification.py.
 */

#include "hermes_tool_result.h"
#include "hermes_json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
