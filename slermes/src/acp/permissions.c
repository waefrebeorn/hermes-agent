/*
 * permissions.c — ACP permission bridging for Hermes C.
 *
 * Bridges ACP permission requests (terminal command approvals) to
 * Hermes' existing approval system. Mirrors Python's
 * acp_adapter/permissions.py.
 */
#include "acp/permissions.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Permission Option Building
 * ================================================================ */

json_node_t *acp_build_permission_options(bool allow_permanent) {
    json_node_t *arr = json_new_array();

    /* Allow once */
    json_node_t *opt_allow_once = json_new_object();
    json_object_set(opt_allow_once, "option_id", json_new_string("allow_once"));
    json_object_set(opt_allow_once, "kind", json_new_string("allow_once"));
    json_object_set(opt_allow_once, "name", json_new_string("Allow once"));
    json_array_append(arr, opt_allow_once);

    /* Allow for session (uses allow_always kind in ACP schema) */
    json_node_t *opt_allow_session = json_new_object();
    json_object_set(opt_allow_session, "option_id", json_new_string("allow_session"));
    json_object_set(opt_allow_session, "kind", json_new_string("allow_always"));
    json_object_set(opt_allow_session, "name", json_new_string("Allow for session"));
    json_array_append(arr, opt_allow_session);

    /* Allow always (only if allow_permanent) */
    if (allow_permanent) {
        json_node_t *opt_allow_always = json_new_object();
        json_object_set(opt_allow_always, "option_id", json_new_string("allow_always"));
        json_object_set(opt_allow_always, "kind", json_new_string("allow_always"));
        json_object_set(opt_allow_always, "name", json_new_string("Allow always"));
        json_array_append(arr, opt_allow_always);
    }

    /* Deny */
    json_node_t *opt_deny = json_new_object();
    json_object_set(opt_deny, "option_id", json_new_string("deny"));
    json_object_set(opt_deny, "kind", json_new_string("reject_once"));
    json_object_set(opt_deny, "name", json_new_string("Deny"));
    json_array_append(arr, opt_deny);

    /* Deny always (always included in C — no SDK version check needed) */
    json_node_t *opt_deny_always = json_new_object();
    json_object_set(opt_deny_always, "option_id", json_new_string("deny_always"));
    json_object_set(opt_deny_always, "kind", json_new_string("reject_always"));
    json_object_set(opt_deny_always, "name", json_new_string("Deny always"));
    json_array_append(arr, opt_deny_always);

    return arr;
}

/* ================================================================
 *  Permission Tool Call Building
 * ================================================================ */

static int g_permission_request_counter = 0;

json_node_t *acp_build_permission_tool_call(const char *command, const char *description) {
    if (!command) return NULL;

    /* Generate unique permission check ID */
    g_permission_request_counter++;
    char tc_id[128];
    snprintf(tc_id, sizeof(tc_id), "perm-check-%d", g_permission_request_counter);

    /* Build content string */
    char content_text[4096];
    if (description && *description) {
        snprintf(content_text, sizeof(content_text), "%s\n$ %s", description, command);
    } else {
        snprintf(content_text, sizeof(content_text), "$ %s", command);
    }

    /* Build title */
    char title[2048];
    if (description && *description) {
        snprintf(title, sizeof(title), "%s: %s", description, command);
    } else {
        snprintf(title, sizeof(title), "%s", command);
    }

    /* Build tool call update JSON */
    json_node_t *tc = json_new_object();
    json_object_set(tc, "tool_call_id", json_new_string(tc_id));
    json_object_set(tc, "title", json_new_string(title));
    json_object_set(tc, "kind", json_new_string("execute"));
    json_object_set(tc, "status", json_new_string("pending"));

    /* Build content array with text block */
    json_node_t *content_arr = json_new_array();
    json_node_t *text_block = json_new_object();
    json_object_set(text_block, "type", json_new_string("text"));
    json_object_set(text_block, "text", json_new_string(content_text));
    json_array_append(content_arr, text_block);
    json_object_set(tc, "content", content_arr);

    /* Build raw_input */
    json_node_t *raw_input = json_new_object();
    json_object_set(raw_input, "command", json_new_string(command));
    if (description && *description)
        json_object_set(raw_input, "description", json_new_string(description));
    json_object_set(tc, "raw_input", raw_input);

    return tc;
}

/* ================================================================
 *  Outcome Mapping
 * ================================================================ */

/* Maps ACP permission option ids to Hermes approval result strings */
static const char *option_id_to_hermes(const char *option_id) {
    if (!option_id) return ACP_HERMES_DENY;
    if (strcmp(option_id, "allow_once") == 0)   return ACP_HERMES_APPROVE_ONCE;
    if (strcmp(option_id, "allow_session") == 0) return ACP_HERMES_APPROVE_SESSION;
    if (strcmp(option_id, "allow_always") == 0)  return ACP_HERMES_APPROVE_ALWAYS;
    if (strcmp(option_id, "deny") == 0)          return ACP_HERMES_DENY;
    if (strcmp(option_id, "deny_always") == 0)   return ACP_HERMES_DENY;
    return ACP_HERMES_DENY;  /* Unknown option — deny by default */
}

const char *acp_map_outcome_to_hermes(const char *option_id, const char *allowed_option_ids_str) {
    if (!option_id || !allowed_option_ids_str)
        return ACP_HERMES_DENY;

    /* Check if option_id is in allowed list */
    const char *found = strstr(allowed_option_ids_str, option_id);
    if (!found)
        return ACP_HERMES_DENY;  /* Unknown option — deny */

    /* Verify it's a complete word match (not substring) */
    if (found != allowed_option_ids_str && *(found - 1) != ',')
        return ACP_HERMES_DENY;
    size_t id_len = strlen(option_id);
    if (found[id_len] != '\0' && found[id_len] != ',')
        return ACP_HERMES_DENY;

    return option_id_to_hermes(option_id);
}
