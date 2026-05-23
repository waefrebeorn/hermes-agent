/*
 * events.c — ACP event notification bridge for Hermes C.
 *
 * Bridges agent tool events (tool.started, tool.completed, tool.failed)
 * to ACP ToolCallStart / ToolCallComplete notifications over stdio.
 * Mirrors Python's acp_adapter/events.py with a simpler sync model.
 */
#include "acp/events.h"
#include "acp/server.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Tool Call ID Tracking
 * ================================================================ */

typedef struct {
    char  session_id[64];
    char  tool_name[64];
    char  call_id[128];  /* unique tool call ID */
    bool  active;
} acp_tool_call_entry_t;

static acp_tool_call_entry_t g_tool_call_ids[ACP_EVENTS_MAX_TOOL_CALLS];
static int g_tool_call_counter = 0;

const char *acp_tool_call_id_register(const char *session_id, const char *tool_name) {
    if (!session_id || !tool_name) return NULL;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < ACP_EVENTS_MAX_TOOL_CALLS; i++) {
        if (!g_tool_call_ids[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return NULL;

    /* Generate unique call ID */
    g_tool_call_counter++;
    snprintf(g_tool_call_ids[slot].call_id, sizeof(g_tool_call_ids[slot].call_id),
             "tc-%s-%d-%d", tool_name, g_tool_call_counter, (int)time(NULL));

    snprintf(g_tool_call_ids[slot].session_id, sizeof(g_tool_call_ids[slot].session_id),
             "%s", session_id);
    snprintf(g_tool_call_ids[slot].tool_name, sizeof(g_tool_call_ids[slot].tool_name),
             "%s", tool_name);
    g_tool_call_ids[slot].active = true;

    return g_tool_call_ids[slot].call_id;
}

char *acp_tool_call_id_pop(const char *session_id, const char *tool_name) {
    if (!session_id || !tool_name) return NULL;

    /* Find matching entry (FIFO: oldest first) */
    int slot = -1;
    for (int i = 0; i < ACP_EVENTS_MAX_TOOL_CALLS; i++) {
        if (g_tool_call_ids[i].active &&
            strcmp(g_tool_call_ids[i].session_id, session_id) == 0 &&
            strcmp(g_tool_call_ids[i].tool_name, tool_name) == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return NULL;

    char *call_id = strdup(g_tool_call_ids[slot].call_id);
    g_tool_call_ids[slot].active = false;
    return call_id;
}

/* ================================================================
 *  ACP Notification Sender (reuses server.c's write function)
 * ================================================================ */

/* Forward declaration — server.c's write function handles Content-Length */
extern bool acp_write_message(json_node_t *msg);

/* ================================================================
 *  Notification Builders
 * ================================================================ */

json_node_t *acp_build_tool_start_notification(
    const char *session_id,
    const char *tool_call_id,
    const char *tool_name,
    const char *args_json
) {
    json_node_t *notif = json_new_object();
    json_object_set(notif, "jsonrpc", json_new_string("2.0"));
    json_object_set(notif, "method", json_new_string("session_update"));

    json_node_t *params = json_new_object();
    json_object_set(params, "session_id", json_new_string(session_id));
    json_object_set(params, "type", json_new_string("tool_call_start"));

    json_node_t *tool_call = json_new_object();
    json_object_set(tool_call, "id", json_new_string(tool_call_id ? tool_call_id : ""));
    json_object_set(tool_call, "name", json_new_string(tool_name ? tool_name : ""));

    /* Try to parse args as JSON for structured display */
    if (args_json && *args_json) {
        char *parse_err = NULL;
        json_node_t *parsed = json_parse(args_json, &parse_err);
        if (parsed && !parse_err) {
            json_object_set(tool_call, "args", json_copy(parsed));
            json_free(parsed);
        } else {
            json_object_set(tool_call, "args", json_new_string(args_json));
        }
        free(parse_err);
    }

    json_object_set(params, "tool_call", tool_call);
    json_object_set(notif, "params", params);
    return notif;
}

json_node_t *acp_build_tool_complete_notification(
    const char *session_id,
    const char *tool_call_id,
    const char *tool_name,
    const char *result_json,
    bool failed
) {
    json_node_t *notif = json_new_object();
    json_object_set(notif, "jsonrpc", json_new_string("2.0"));
    json_object_set(notif, "method", json_new_string("session_update"));

    json_node_t *params = json_new_object();
    json_object_set(params, "session_id", json_new_string(session_id));
    json_object_set(params, "type", json_new_string(failed ? "tool_call_failed" : "tool_call_complete"));

    json_node_t *tool_call = json_new_object();
    json_object_set(tool_call, "id", json_new_string(tool_call_id ? tool_call_id : ""));
    json_object_set(tool_call, "name", json_new_string(tool_name ? tool_name : ""));

    /* Parse result as JSON if possible */
    if (result_json && *result_json) {
        char *parse_err = NULL;
        json_node_t *parsed = json_parse(result_json, &parse_err);
        if (parsed && !parse_err) {
            json_object_set(tool_call, "result", json_copy(parsed));
            json_free(parsed);
        } else {
            json_object_set(tool_call, "result", json_new_string(result_json));
        }
        free(parse_err);
    }

    json_object_set(params, "tool_call", tool_call);
    json_object_set(notif, "params", params);
    return notif;
}

json_node_t *acp_build_plan_update_notification(
    const char *session_id,
    const char *result_json
) {
    if (!session_id || !result_json || !*result_json)
        return NULL;

    /* Parse the result JSON */
    char *parse_err = NULL;
    json_node_t *parsed = json_parse(result_json, &parse_err);
    if (parse_err || !parsed) {
        free(parse_err);
        return NULL;
    }

    /* Check for todo list structure */
    json_node_t *todos = json_object_get(parsed, "todos");
    if (!todos || todos->type != JSON_ARRAY) {
        json_free(parsed);
        return NULL;
    }

    json_node_t *notif = json_new_object();
    json_object_set(notif, "jsonrpc", json_new_string("2.0"));
    json_object_set(notif, "method", json_new_string("session_update"));

    json_node_t *params = json_new_object();
    json_object_set(params, "session_id", json_new_string(session_id));
    json_object_set(params, "type", json_new_string("plan_update"));

    json_node_t *entries = json_new_array();

    int arr_len = json_len(todos);
    for (int i = 0; i < arr_len; i++) {
        json_node_t *item = json_array_get(todos, i);
        if (!item || item->type != JSON_OBJECT) continue;

        const char *content = json_object_get_string(item, "content", "");
        if (!*content) content = json_object_get_string(item, "id", "");
        if (!*content) continue;

        const char *status = json_object_get_string(item, "status", "pending");

        json_node_t *entry = json_new_object();
        json_object_set(entry, "content", json_new_string(content));
        json_object_set(entry, "priority", json_new_string("medium"));

        /* Map Hermes status to ACP status */
        if (strcmp(status, "in_progress") == 0)
            json_object_set(entry, "status", json_new_string("in_progress"));
        else if (strcmp(status, "completed") == 0 || strcmp(status, "cancelled") == 0)
            json_object_set(entry, "status", json_new_string("completed"));
        else
            json_object_set(entry, "status", json_new_string("pending"));

        json_array_append(entries, entry);
    }

    json_object_set(params, "entries", entries);
    json_object_set(notif, "params", params);

    json_free(parsed);
    return notif;
}

/* Forward declaration of ACP session — defined in server.c */
typedef struct {
    char id[64];
    /* other fields used internally by server.c */
} acp_session_t;

/* ================================================================
 *  Main callback implementation
 * ================================================================ */

int acp_tool_event_cb(const char *event_type, const char *tool_name,
                       const char *args_json, void *userdata)
{
    if (!event_type || !tool_name || !userdata)
        return 1;

    /* Extract session_id from userdata — it's the acp_session_t pointer */
    acp_session_t *sess = (acp_session_t *)userdata;
    const char *session_id = sess->id;
    if (!session_id || !*session_id)
        session_id = "acp-session";

    if (strcmp(event_type, "tool.started") == 0) {
        /* Register tool call ID */
        const char *tc_id = acp_tool_call_id_register(session_id, tool_name);
        if (!tc_id) return 1;

        json_node_t *notif = acp_build_tool_start_notification(
            session_id, tc_id, tool_name, args_json);
        if (notif) {
            acp_write_message(notif);
            json_free(notif);
        }
    } else if (strcmp(event_type, "tool.completed") == 0 ||
               strcmp(event_type, "tool.failed") == 0) {
        /* Don't send ACP complete for todo tool — plan update handles it */
        bool send_plan_update = (strcmp(tool_name, "todo") == 0);

        char *tc_id = acp_tool_call_id_pop(session_id, tool_name);
        if (!tc_id) return 1;

        json_node_t *notif = acp_build_tool_complete_notification(
            session_id, tc_id, tool_name, args_json,
            strcmp(event_type, "tool.failed") == 0);
        if (notif) {
            acp_write_message(notif);
            json_free(notif);
        }

        /* For todo tool, also send plan update */
        if (send_plan_update && args_json) {
            json_node_t *plan = acp_build_plan_update_notification(session_id, args_json);
            if (plan) {
                acp_write_message(plan);
                json_free(plan);
            }
        }

        free(tc_id);
    }

    return 1; /* Continue execution */
}
