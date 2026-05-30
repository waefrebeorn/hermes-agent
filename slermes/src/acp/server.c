/*
 * server.c — ACP (Agent Communication Protocol) server for Hermes C.
 *
 * JSON-RPC 2.0 over stdio with Content-Length framing.
 * Session management backed by Hermes agent_state_t.
 * Tool discovery via existing tool registry.
 *
 * B01 Session 2/4: full session CRUD + tool dispatch.
 */

#include "acp/server.h"
#include "acp/edit_approval.h"
#include "acp/events.h"
#include "acp/permissions.h"
#include "acp/resource.h"
#include "hermes_json.h"
#include "hermes_agent.h"   /* agent_init, agent_state_t, tool registry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

/* Max concurrent ACP sessions */
#define ACP_MAX_SESSIONS 16

/* ACP session wrapper */
typedef struct {
    char            id[64];
    char            name[128];
    agent_state_t   agent;            /* Hermes agent state */
    bool            active;
    time_t          created_at;
    int             message_count;
    char            model[128];
    char            mode[32];         /* "plan" | "act" */
    char            auto_approve_policy[32]; /* "ask" | "session" | "workspace_session" */
} acp_session_t;

/* ================================================================
 *  Server struct
 * ================================================================ */

struct acp_server_t {
    char            server_name[64];
    char            server_version[32];
    bool            initialized;        /* true after initialize handshake */
    int             next_id;            /* session ID counter */
    acp_session_t   sessions[ACP_MAX_SESSIONS];
    acp_approval_request_t pending_approval;  /* single pending edit approval */
    char            pending_permission_command[2048];  /* command awaiting permission response */
    char            pending_permission_desc[512];       /* description for pending cmd */
    bool            permission_pending;                  /* true while awaiting response */
    char            pending_permission_session_id[64];
};

/* ================================================================
 *  JSON-RPC framing
 * ================================================================ */

json_node_t *acp_read_message(void) {
    char header[256];
    int content_length = -1;

    while (1) {
        if (!fgets(header, sizeof(header), stdin)) {
            if (feof(stdin)) return NULL;
            return NULL;
        }

        size_t hlen = strlen(header);
        while (hlen > 0 && (header[hlen-1] == '\n' || header[hlen-1] == '\r'))
            header[--hlen] = '\0';

        if (hlen == 0) break;

        if (strncasecmp(header, "Content-Length:", 15) == 0) {
            const char *val = header + 15;
            while (*val && isspace((unsigned char)*val)) val++;
            content_length = atoi(val);
        }
    }

    if (content_length < 0) return NULL;
    if (content_length > ACP_MAX_MESSAGE_BYTES) {
        fprintf(stderr, "[acp] Message too large: %d bytes\n", content_length);
        return NULL;
    }

    char *body = (char *)malloc((size_t)content_length + 1);
    if (!body) return NULL;

    size_t total = 0;
    while (total < (size_t)content_length) {
        ssize_t n = fread(body + total, 1, (size_t)content_length - total, stdin);
        if (n <= 0) { free(body); return NULL; }
        total += (size_t)n;
    }
    body[total] = '\0';

    char *err = NULL;
    json_node_t *json = json_parse(body, &err);
    if (err) {
        fprintf(stderr, "[acp] JSON parse error: %s\n", err);
        free(err);
    }
    free(body);
    return json;
}

bool acp_write_message(json_node_t *msg) {
    char *serialized = json_serialize(msg);
    if (!serialized) return false;

    size_t len = strlen(serialized);
    int hlen = fprintf(stdout, "Content-Length: %zu\r\n\r\n", len);
    if (hlen < 0) { free(serialized); return false; }

    size_t written = fwrite(serialized, 1, len, stdout);
    fflush(stdout);
    free(serialized);
    return written == len;
}

/* ================================================================
 *  JSON-RPC helpers
 * ================================================================ */

json_node_t *acp_make_response(const char *id, json_node_t *result) {
    json_node_t *resp = json_new_object();
    json_object_set(resp, "jsonrpc", json_new_string("2.0"));
    json_object_set(resp, "id", id ? json_new_string(id) : json_new_null());
    json_object_set(resp, "result", result ? json_copy(result) : json_new_null());
    return resp;
}

json_node_t *acp_make_error(const char *id, int code, const char *message, json_node_t *data) {
    json_node_t *err = json_new_object();
    json_object_set(err, "code", json_new_number((double)code));
    json_object_set(err, "message", json_new_string(message));
    if (data) json_object_set(err, "data", json_copy(data));

    json_node_t *resp = json_new_object();
    json_object_set(resp, "jsonrpc", json_new_string("2.0"));
    json_object_set(resp, "id", id ? json_new_string(id) : json_new_null());
    json_object_set(resp, "error", err);
    return resp;
}

const char *acp_get_method(json_node_t *req) {
    return json_object_get_string(req, "method", NULL);
}

const char *acp_get_id(json_node_t *req) {
    json_node_t *id_node = json_object_get(req, "id");
    if (!id_node) return NULL;
    static char buf[64];
    if (id_node->type == JSON_STRING && id_node->str_val)
        return id_node->str_val;
    if (id_node->type == JSON_NUMBER) {
        snprintf(buf, sizeof(buf), "%.0f", id_node->num_val);
        return buf;
    }
    return NULL;
}

/* ================================================================
 *  Session helpers
 * ================================================================ */

static acp_session_t *find_session(acp_server_t *srv, const char *session_id) {
    for (int i = 0; i < ACP_MAX_SESSIONS; i++) {
        if (srv->sessions[i].active &&
            strcmp(srv->sessions[i].id, session_id) == 0)
            return &srv->sessions[i];
    }
    return NULL;
}

static acp_session_t *alloc_session(acp_server_t *srv) {
    for (int i = 0; i < ACP_MAX_SESSIONS; i++) {
        if (!srv->sessions[i].active) {
            memset(&srv->sessions[i], 0, sizeof(acp_session_t));
            srv->sessions[i].active = true;
            srv->sessions[i].created_at = time(NULL);
            snprintf(srv->sessions[i].mode, sizeof(srv->sessions[i].mode), "act");
            return &srv->sessions[i];
        }
    }
    return NULL;
}

static json_node_t *session_to_json(acp_session_t *sess) {
    json_node_t *info = json_new_object();
    json_object_set(info, "session_id", json_new_string(sess->id));
    json_object_set(info, "name", json_new_string(sess->name));
    json_object_set(info, "message_count", json_new_number((double)sess->message_count));
    json_object_set(info, "created_at", json_new_string(ctime(&sess->created_at)));
    if (sess->model[0])
        json_object_set(info, "model", json_new_string(sess->model));
    if (sess->mode[0])
        json_object_set(info, "mode", json_new_string(sess->mode));
    return info;
}

/* ================================================================
 *  Handlers
 * ================================================================ */

static json_node_t *handle_initialize(const char *id, json_node_t *params, acp_server_t *srv) {
    (void)params;
    srv->initialized = true;

    json_node_t *result = json_new_object();
    json_object_set(result, "protocolVersion", json_new_string(ACP_PROTOCOL_VERSION));

    json_node_t *server_info = json_new_object();
    json_object_set(server_info, "name", json_new_string(srv->server_name));
    json_object_set(server_info, "version", json_new_string(srv->server_version));
    json_object_set(result, "serverInfo", server_info);
    /* Capabilities */
    json_node_t *caps = json_new_object();
    json_object_set(caps, "prompts", json_new_object());
    json_node_t *session_caps = json_new_object();
    json_object_set(session_caps, "list", json_new_object());
    json_object_set(session_caps, "resume", json_new_object());
    json_object_set(session_caps, "fork", json_new_object());
    json_object_set(caps, "sessions", session_caps);

    /* Auth methods */
    json_node_t *auth_methods = json_new_array();
    json_node_t *agent_auth = json_new_object();
    json_object_set(agent_auth, "id", json_new_string("acp-bearer"));
    json_object_set(agent_auth, "name", json_new_string("ACP Bearer Token"));
    json_object_set(agent_auth, "description", json_new_string("Authenticate using a bearer token"));
    json_array_append(auth_methods, agent_auth);
    json_node_t *setup_auth = json_new_object();
    json_object_set(setup_auth, "id", json_new_string("hermes-setup"));
    json_object_set(setup_auth, "name", json_new_string("Configure Hermes provider"));
    json_object_set(setup_auth, "description", json_new_string("Open Hermes interactive provider setup"));
    json_object_set(setup_auth, "type", json_new_string("terminal"));
    json_array_append(auth_methods, setup_auth);
    json_object_set(caps, "authentication", auth_methods);
    json_object_set(result, "capabilities", caps);

    return acp_make_response(id, result);
}

/* --- Session: new --- */

static json_node_t *handle_new_session(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_name = json_object_get_string(params, "name", "default");

    acp_session_t *sess = alloc_session(srv);
    if (!sess)
        return acp_make_error(id, -32000, "Max sessions reached", NULL);

    srv->next_id++;
    snprintf(sess->id, sizeof(sess->id), "acp-%d", srv->next_id);
    snprintf(sess->name, sizeof(sess->name), "%s", session_name);

    /* Initialize agent state with tools */
    agent_init(&sess->agent);
    sess->agent.tool_event_cb = acp_tool_event_cb;
    sess->agent.tool_event_data = (void *)sess;
    if (!sess->agent.tools.tools) {
        /* First session: init tools */
        extern void tools_init_all(void);
        tools_init_all();
        sess->agent.tools = *registry_get();
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "session_id", json_new_string(sess->id));
    json_object_set(result, "name", json_new_string(session_name));
    json_object_set(result, "session", session_to_json(sess));
    return acp_make_response(id, result);
}

/* --- Session: list --- */

static json_node_t *handle_list_sessions(const char *id, json_node_t *params, acp_server_t *srv) {
    (void)params;
    json_node_t *result = json_new_object();
    json_node_t *sessions = json_new_array();
    for (int i = 0; i < ACP_MAX_SESSIONS; i++) {
        if (srv->sessions[i].active)
            json_array_append(sessions, session_to_json(&srv->sessions[i]));
    }
    json_object_set(result, "sessions", sessions);
    json_object_set(result, "next_cursor", json_new_null());
    return acp_make_response(id, result);
}

/* --- Session: load --- */

static json_node_t *handle_load_session(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    if (!session_id)
        return acp_make_error(id, -32602, "Missing session_id", NULL);

    acp_session_t *sess = find_session(srv, session_id);
    if (!sess)
        return acp_make_error(id, -32000, "Session not found", NULL);

    json_node_t *result = json_new_object();
    json_object_set(result, "session", session_to_json(sess));
    return acp_make_response(id, result);
}

/* --- Session: resume --- */

static json_node_t *handle_resume_session(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    if (!session_id)
        return acp_make_error(id, -32602, "Missing session_id", NULL);

    acp_session_t *sess = find_session(srv, session_id);
    if (!sess)
        return acp_make_error(id, -32000, "Session not found", NULL);

    json_node_t *result = json_new_object();
    json_object_set(result, "session_id", json_new_string(sess->id));
    json_object_set(result, "session", session_to_json(sess));
    return acp_make_response(id, result);
}

/* --- Session: delete --- */

static json_node_t *handle_delete_session(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    if (!session_id)
        return acp_make_error(id, -32602, "Missing session_id", NULL);

    acp_session_t *sess = find_session(srv, session_id);
    if (!sess)
        return acp_make_error(id, -32000, "Session not found", NULL);

    agent_free(&sess->agent);
    memset(sess, 0, sizeof(acp_session_t));

    json_node_t *result = json_new_object();
    json_object_set(result, "ok", json_new_bool(true));
    return acp_make_response(id, result);
}

/* --- Tools: list (first-class ACP protocol method) --- */

static json_node_t *handle_tools_list(const char *id, json_node_t *params, acp_server_t *srv) {
    (void)params;
    (void)srv;

    json_node_t *result = json_new_object();
    json_node_t *tools_arr = json_new_array();

    tool_registry_t *reg = registry_get();
    if (reg) {
        for (size_t i = 0; i < reg->count; i++) {
            json_node_t *t = json_new_object();
            json_object_set(t, "name", json_new_string(reg->tools[i].name));
            json_object_set(t, "description", json_new_string(reg->tools[i].description));
            if (reg->tools[i].schema_json[0])
                json_object_set(t, "schema", json_new_string(reg->tools[i].schema_json));
            json_object_set(t, "available", json_new_bool(reg->tools[i].available));
            if (reg->tools[i].toolset[0])
                json_object_set(t, "toolset", json_new_string(reg->tools[i].toolset));
            json_array_append(tools_arr, t);
        }
    }
    json_object_set(result, "tools", tools_arr);
    return acp_make_response(id, result);
}

/* --- Tools: call (invoke tool handler directly) --- */

/* --- Tools: call (invoke tool handler directly) with edit approval --- */

/* Lookup auto-approve policy for a session */
static const char *get_session_auto_approve_policy(acp_server_t *srv, const char *session_id) {
    if (!session_id || !*session_id)
        return "ask";
    acp_session_t *sess = find_session(srv, session_id);
    if (!sess || !sess->active)
        return "ask";
    return sess->auto_approve_policy;
}

static json_node_t *handle_tools_call(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *tool_name = json_object_get_string(params, "name", NULL);
    json_node_t *args_node = json_object_get(params, "args");
    const char *session_id = json_object_get_string(params, "session_id", NULL);

    if (!tool_name)
        return acp_make_error(id, -32602, "Missing tool name", NULL);

    /* === Edit approval check === */
    if (args_node) {
        acp_edit_proposal_t proposal;
        char *build_err = NULL;

        if (acp_build_edit_proposal(tool_name, args_node, &proposal, &build_err)) {
            /* Check auto-approve policy */
            const char *policy = get_session_auto_approve_policy(srv, session_id);

            /* Check for cached approval (previously approved by client) */
            bool has_cached_approval = false;
            if (srv->pending_approval.status == ACP_APPROVAL_GRANTED &&
                strcmp(srv->pending_approval.proposal.path, proposal.path) == 0 &&
                strcmp(srv->pending_approval.proposal.tool_name, proposal.tool_name) == 0)
            {
                has_cached_approval = true;
            }

            if (!has_cached_approval && !acp_should_auto_approve_edit(&proposal, policy, NULL)) {
                /* Need approval — notify client and return pending */
                /* Set pending approval state */
                acp_approval_request_begin(&srv->pending_approval, &proposal, 120.0);

                /* Send notification with proposal details */
                json_node_t *notif = json_new_object();
                json_object_set(notif, "jsonrpc", json_new_string("2.0"));
                json_object_set(notif, "method", json_new_string("edit_proposal"));
                json_node_t *notif_params = json_new_object();
                json_object_set(notif_params, "proposal_id", json_new_string(srv->pending_approval.proposal_id));
                json_object_set(notif_params, "tool_name", json_new_string(proposal.tool_name));
                json_object_set(notif_params, "path", json_new_string(proposal.path));
                if (proposal.old_text)
                    json_object_set(notif_params, "old_text", json_new_string(proposal.old_text));
                json_object_set(notif_params, "new_text", json_new_string(proposal.new_text));
                json_object_set(notif_params, "arguments", json_new_string(proposal.arguments_json));
                json_object_set(notif, "params", notif_params);
                acp_write_message(notif);
                json_free(notif);

                acp_edit_proposal_free(&proposal);
                return acp_make_error(id, -32998,
                    "Edit approval required for file mutation", NULL);
            }

            acp_edit_proposal_free(&proposal);

            /* If we had a cached approval, clear it now that it's consumed */
            if (has_cached_approval) {
                srv->pending_approval.status = ACP_APPROVAL_PENDING;
            }
        }
        free(build_err);
    }

    tool_t *tool = registry_find(tool_name);
    if (!tool)
        return acp_make_error(id, -32602, "Tool not found", NULL);
    if (!tool->handler)
        return acp_make_error(id, -32000, "Tool has no handler", NULL);

    const char *task_id = json_object_get_string(params, "task_id", NULL);

    /* Serialize args to JSON string for tool handler */
    char *args_json = NULL;
    if (args_node) {
        args_json = json_serialize(args_node);
    } else {
        args_json = strdup("{}");
    }
    if (!args_json)
        return acp_make_error(id, -32000, "Failed to serialize args", NULL);

    /* Invoke tool handler */
    char *handler_result = tool->handler(args_json, task_id);
    free(args_json);

    json_node_t *result = json_new_object();
    if (handler_result) {
        /* Try to parse result as JSON for structured response */
        char *parse_err = NULL;
        json_node_t *parsed = json_parse(handler_result, &parse_err);
        if (parsed && !parse_err) {
            json_object_set(result, "result", json_copy(parsed));
            json_free(parsed);
        } else {
            json_object_set(result, "result", json_new_string(handler_result));
        }
        free(parse_err);
        free(handler_result);
    } else {
        json_object_set(result, "result", json_new_null());
    }
    return acp_make_response(id, result);
}

/* --- ACP Edit approval response --- */

static json_node_t *handle_edit_approval_response(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *proposal_id = json_object_get_string(params, "proposal_id", NULL);
    if (!proposal_id)
        return acp_make_error(id, -32602, "Missing proposal_id", NULL);

    /* Verify proposal_id matches pending approval */
    if (srv->pending_approval.status != ACP_APPROVAL_PENDING ||
        strcmp(srv->pending_approval.proposal_id, proposal_id) != 0) {
        return acp_make_error(id, -32000, "No pending approval for this proposal_id", NULL);
    }

    bool approved = json_object_get_bool(params, "approved", false);
    acp_approval_request_resolve(&srv->pending_approval, approved);
    (void)id; /* Notification — no response needed */

    /* If approved, store the decision so the next tools/call re-execution auto-approves */
    json_node_t *result = json_new_object();
    json_object_set(result, "status", json_new_string(approved ? "approved" : "denied"));
    json_object_set(result, "proposal_id", json_new_string(proposal_id));

    /* Send result notification confirming the resolution */
    json_node_t *notif = json_new_object();
    json_object_set(notif, "jsonrpc", json_new_string("2.0"));
    json_object_set(notif, "method", json_new_string("edit_proposal_result"));
    json_node_t *nr = json_new_object();
    json_object_set(nr, "proposal_id", json_new_string(proposal_id));
    json_object_set(nr, "status", json_new_string(approved ? "approved" : "denied"));
    json_object_set(notif, "params", nr);
    acp_write_message(notif);
    json_free(notif);

    return acp_make_response(id, result);
}

/* --- ACP Set auto-approve policy --- */

static json_node_t *handle_set_auto_approve(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    const char *policy = json_object_get_string(params, "policy", "ask");

    if (!session_id || !*session_id)
        return acp_make_error(id, -32602, "Missing session_id", NULL);

    /* Validate policy */
    if (strcmp(policy, "ask") != 0 && strcmp(policy, "session") != 0 &&
        strcmp(policy, "workspace_session") != 0) {
        return acp_make_error(id, -32602, "Invalid policy: must be ask, session, or workspace_session", NULL);
    }

    acp_session_t *sess = find_session(srv, session_id);
    if (!sess || !sess->active)
        return acp_make_error(id, -32000, "Session not found", NULL);

    snprintf(sess->auto_approve_policy, sizeof(sess->auto_approve_policy), "%s", policy);

    json_node_t *result = json_new_object();
    json_object_set(result, "status", json_new_string("ok"));
    json_object_set(result, "policy", json_new_string(policy));
    json_object_set(result, "session_id", json_new_string(session_id));
    return acp_make_response(id, result);
}

/* --- ACP Permission request --- */

static json_node_t *handle_request_permission(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    const char *command = json_object_get_string(params, "command", NULL);
    const char *description = json_object_get_string(params, "description", "");
    bool allow_permanent = json_object_get_bool(params, "allow_permanent", true);

    if (!session_id || !command)
        return acp_make_error(id, -32602, "Missing session_id or command", NULL);

    /* Check if another permission request is pending */
    if (srv->permission_pending)
        return acp_make_error(id, -32000, "Permission request already pending", NULL);

    /* Store pending state */
    snprintf(srv->pending_permission_command, sizeof(srv->pending_permission_command), "%s", command);
    snprintf(srv->pending_permission_desc, sizeof(srv->pending_permission_desc), "%s", description);
    snprintf(srv->pending_permission_session_id, sizeof(srv->pending_permission_session_id), "%s", session_id);
    srv->permission_pending = true;

    /* Build permission tool call */
    json_node_t *tool_call = acp_build_permission_tool_call(command, description);

    /* Build permission options */
    json_node_t *options = acp_build_permission_options(allow_permanent);

    /* Send permission_request notification */
    json_node_t *notif = json_new_object();
    json_object_set(notif, "jsonrpc", json_new_string("2.0"));
    json_object_set(notif, "method", json_new_string("permission_request"));
    json_node_t *np = json_new_object();
    json_object_set(np, "session_id", json_new_string(session_id));
    json_object_set(np, "tool_call", tool_call);
    json_object_set(np, "options", options);
    json_object_set(notif, "params", np);
    acp_write_message(notif);
    json_free(notif);

    return acp_make_response(id, NULL);
}

/* --- ACP Permission response --- */

static json_node_t *handle_permission_response(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *option_id = json_object_get_string(params, "option_id", NULL);
    const char *session_id = json_object_get_string(params, "session_id", NULL);

    if (!srv->permission_pending) {
        return acp_make_error(id, -32000, "No pending permission request", NULL);
    }

    if (!option_id) {
        srv->permission_pending = false;
        return acp_make_error(id, -32602, "Missing option_id", NULL);
    }

    /* Build allowed option IDs list for validation */
    char allowed_ids[512] = "allow_once,allow_session,allow_always,deny,deny_always";

    /* Map outcome to Hermes approval string */
    const char *hermes_result = acp_map_outcome_to_hermes(option_id, allowed_ids);

    json_node_t *result = json_new_object();
    json_object_set(result, "status", json_new_string("ok"));
    json_object_set(result, "command", json_new_string(srv->pending_permission_command));
    json_object_set(result, "hermes_result", json_new_string(hermes_result));
    json_object_set(result, "option_id", json_new_string(option_id));
    if (session_id)
        json_object_set(result, "session_id", json_new_string(session_id));

    /* Clear pending state */
    srv->permission_pending = false;

    return acp_make_response(id, result);
}

/* --- Session: fork --- */

static json_node_t *handle_fork_session(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *src_id = json_object_get_string(params, "session_id", NULL);
    if (!src_id)
        return acp_make_error(id, -32602, "Missing session_id", NULL);

    acp_session_t *src = find_session(srv, src_id);
    if (!src)
        return acp_make_error(id, -32000, "Source session not found", NULL);

    acp_session_t *sess = alloc_session(srv);
    if (!sess)
        return acp_make_error(id, -32000, "Max sessions reached", NULL);

    srv->next_id++;
    snprintf(sess->id, sizeof(sess->id), "acp-%d", srv->next_id);
    snprintf(sess->name, sizeof(sess->name), "%s", src->name);
    /* Append " (fork)" if room */
    size_t nlen = strlen(sess->name);
    if (nlen + 8 < sizeof(sess->name))
        memcpy(sess->name + nlen, " (fork)", 8);

    /* Copy agent state from source */
    agent_init(&sess->agent);
    sess->agent.tool_event_cb = acp_tool_event_cb;
    sess->agent.tool_event_data = (void *)sess;
    if (src->agent.tools.tools)
        sess->agent.tools = src->agent.tools;

    json_node_t *result = json_new_object();
    json_object_set(result, "session_id", json_new_string(sess->id));
    json_object_set(result, "session", session_to_json(sess));
    return acp_make_response(id, result);
}

/* --- Session: set_model --- */

static json_node_t *handle_set_session_model(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    const char *model = json_object_get_string(params, "model", NULL);
    if (!session_id || !model)
        return acp_make_error(id, -32602, "Missing session_id or model", NULL);

    acp_session_t *sess = find_session(srv, session_id);
    if (!sess)
        return acp_make_error(id, -32000, "Session not found", NULL);

    snprintf(sess->model, sizeof(sess->model), "%s", model);
    if (sess->agent.llm.model[0])
        snprintf(sess->agent.llm.model, sizeof(sess->agent.llm.model), "%s", model);

    json_node_t *result = json_new_object();
    json_object_set(result, "session_id", json_new_string(sess->id));
    json_object_set(result, "model", json_new_string(model));
    return acp_make_response(id, result);
}

/* --- Session: set_mode --- */

static json_node_t *handle_set_session_mode(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    const char *mode = json_object_get_string(params, "mode", "act");
    if (!session_id)
        return acp_make_error(id, -32602, "Missing session_id", NULL);

    acp_session_t *sess = find_session(srv, session_id);
    if (!sess)
        return acp_make_error(id, -32000, "Session not found", NULL);

    snprintf(sess->mode, sizeof(sess->mode), "%s", mode);
    json_node_t *result = json_new_object();
    json_object_set(result, "session_id", json_new_string(sess->id));
    json_object_set(result, "mode", json_new_string(mode));
    return acp_make_response(id, result);
}

/* --- Session: set_config_option --- */

static json_node_t *handle_set_session_config_option(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    (void)srv;
    if (!session_id)
        return acp_make_error(id, -32602, "Missing session_id", NULL);
    json_node_t *result = json_new_object();
    json_object_set(result, "ok", json_new_bool(true));
    return acp_make_response(id, result);
}

/* --- available_commands --- */

static json_node_t *handle_available_commands(const char *id, json_node_t *params, acp_server_t *srv) {
    (void)params;
    (void)srv;

    json_node_t *result = json_new_object();
    json_node_t *commands = json_new_array();

    const char *cmd_names[] = {"help", "model", "tools", "context", "reset", "compact", "version"};
    const char *cmd_descs[] = {
        "List available commands",
        "Show current model and provider, or switch models",
        "List available tools with descriptions",
        "Show conversation message counts by role",
        "Clear conversation history",
        "Compress conversation context",
        "Show Hermes version"
    };

    for (int i = 0; i < 7; i++) {
        json_node_t *cmd = json_new_object();
        json_object_set(cmd, "name", json_new_string(cmd_names[i]));
        json_object_set(cmd, "description", json_new_string(cmd_descs[i]));
        json_array_append(commands, cmd);
    }
    json_object_set(result, "commands", commands);
    return acp_make_response(id, result);
}

/* --- execute_command (slash command dispatch) --- */

static json_node_t *handle_execute_command(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    const char *command = json_object_get_string(params, "command", NULL);
    if (!session_id || !command)
        return acp_make_error(id, -32602, "Missing session_id or command", NULL);

    acp_session_t *sess = find_session(srv, session_id);
    if (!sess)
        return acp_make_error(id, -32000, "Session not found", NULL);

    json_node_t *result = json_new_object();
    json_node_t *messages = json_new_array();

    if (strcmp(command, "tools") == 0) {
        /* List tools from registry */
        tool_registry_t *reg = registry_get();
        if (reg) {
            for (size_t i = 0; i < reg->count; i++) {
                json_node_t *msg = json_new_object();
                json_object_set(msg, "role", json_new_string("tool_info"));
                char buf[1024];
                snprintf(buf, sizeof(buf), "%s: %s", reg->tools[i].name, reg->tools[i].description);
                json_object_set(msg, "content", json_new_string(buf));
                json_array_append(messages, msg);
            }
        }
    } else if (strcmp(command, "help") == 0) {
        json_node_t *msg = json_new_object();
        json_object_set(msg, "role", json_new_string("help"));
        json_object_set(msg, "content", json_new_string(
            "Available commands: help, model, tools, context, reset, compact, version"));
        json_array_append(messages, msg);
    } else if (strcmp(command, "version") == 0) {
        json_node_t *msg = json_new_object();
        json_object_set(msg, "role", json_new_string("info"));
        json_object_set(msg, "content", json_new_string(ACP_SERVER_VERSION));
        json_array_append(messages, msg);
    } else {
        json_node_t *msg = json_new_object();
        json_object_set(msg, "role", json_new_string("error"));
        json_object_set(msg, "content", json_new_string("Unknown command"));
        json_array_append(messages, msg);
    }

    json_object_set(result, "messages", messages);
    return acp_make_response(id, result);
}

/* --- User message (core agent execution) --- */

/* --- Authenticate --- */

static json_node_t *handle_authenticate(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *method_id = json_object_get_string(params, "method_id", NULL);
    (void)srv;

    if (!method_id)
        return acp_make_error(id, -32602, "Missing method_id", NULL);

    json_node_t *result = json_new_object();
    if (strcmp(method_id, "acp-bearer") == 0) {
        json_object_set(result, "authenticated", json_new_bool(true));
    } else if (strcmp(method_id, "hermes-setup") == 0) {
        json_object_set(result, "authenticated", json_new_bool(true));
        json_object_set(result, "note", json_new_string("Setup via hermes model command"));
    } else {
        return acp_make_error(id, -32000, "Unknown auth method", NULL);
    }
    return acp_make_response(id, result);
}

/* ACP notification sender — writes a JSON-RPC notification (no id) to stdout */
static bool send_notification(const char *method, json_node_t *params) {
    json_node_t *notif = json_new_object();
    json_object_set(notif, "jsonrpc", json_new_string("2.0"));
    json_object_set(notif, "method", json_new_string(method));
    if (params)
        json_object_set(notif, "params", json_copy(params));
    else
        json_object_set(notif, "params", json_new_object());
    bool ok = acp_write_message(notif);
    json_free(notif);
    return ok;
}

/* Stream callback: called by agent loop for each token/event */
static int acp_stream_cb(const char *token, void *userdata) {
    const char *session_id = (const char *)userdata;
    json_node_t *params = json_new_object();
    json_object_set(params, "session_id", json_new_string(session_id));
    json_object_set(params, "content", json_new_string(token));
    json_object_set(params, "type", json_new_string("text"));
    send_notification("agent_message_chunk", params);
    json_free(params);
    return 0; /* Continue streaming */
}

static json_node_t *handle_user_message(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_id = json_object_get_string(params, "session_id", NULL);
    json_node_t *content_node = json_object_get(params, "content");

    if (!session_id)
        return acp_make_error(id, -32602, "Missing session_id", NULL);
    if (!content_node)
        return acp_make_error(id, -32602, "Missing content", NULL);

    acp_session_t *sess = find_session(srv, session_id);
    if (!sess)
        return acp_make_error(id, -32000, "Session not found", NULL);

    /* Extract text content (support string, array-of-parts, and resource links) */
    char *resolved_text = acp_content_to_text(content_node);
    if (!resolved_text || !*resolved_text) {
        free(resolved_text);
        return acp_make_error(id, -32602, "Empty content", NULL);
    }

    /* Set up streaming callback */
    sess->agent.stream_cb = acp_stream_cb;
    sess->agent.stream_data = (void *)sess->id;

    /* Run the agent */
    sess->message_count++;
    fprintf(stderr, "[acp] Running agent for session %s: %s\n", session_id, resolved_text);
    char *response = agent_run_conversation(&sess->agent, resolved_text, NULL);
    free(resolved_text);
    sess->agent.stream_cb = NULL;
    sess->agent.stream_data = NULL;

    if (!response)
        response = strdup("");

    /* Send final response notification */
    {
        json_node_t *upd = json_new_object();
        json_object_set(upd, "session_id", json_new_string(session_id));
        json_object_set(upd, "content", json_new_string(response));
        json_object_set(upd, "type", json_new_string("final_text"));
        send_notification("update_agent_message_text", upd);
        json_free(upd);
    }

    /* Return final result */
    json_node_t *result = json_new_object();
    json_object_set(result, "session_id", json_new_string(session_id));
    json_object_set(result, "response", json_new_string(response));
    free(response);

    return acp_make_response(id, result);
}

/* ================================================================
 *  Request dispatcher
 * ================================================================ */

static json_node_t *dispatch_request(const char *method, const char *id,
                                      json_node_t *params, acp_server_t *srv) {
    if (strcmp(method, "initialize") == 0)
        return handle_initialize(id, params, srv);
    if (strcmp(method, "new_session") == 0)
        return handle_new_session(id, params, srv);
    if (strcmp(method, "list_sessions") == 0)
        return handle_list_sessions(id, params, srv);
    if (strcmp(method, "load_session") == 0)
        return handle_load_session(id, params, srv);
    if (strcmp(method, "resume_session") == 0)
        return handle_resume_session(id, params, srv);
    if (strcmp(method, "fork_session") == 0)
        return handle_fork_session(id, params, srv);
    if (strcmp(method, "set_session_model") == 0)
        return handle_set_session_model(id, params, srv);
    if (strcmp(method, "set_session_mode") == 0)
        return handle_set_session_mode(id, params, srv);
    if (strcmp(method, "set_session_config_option") == 0)
        return handle_set_session_config_option(id, params, srv);
    if (strcmp(method, "available_commands") == 0)
        return handle_available_commands(id, params, srv);
    if (strcmp(method, "execute_command") == 0)
        return handle_execute_command(id, params, srv);
    if (strcmp(method, "user_message") == 0)
        return handle_user_message(id, params, srv);
    if (strcmp(method, "authenticate") == 0)
        return handle_authenticate(id, params, srv);
    if (strcmp(method, "tools/list") == 0)
        return handle_tools_list(id, params, srv);
    if (strcmp(method, "tools/call") == 0)
        return handle_tools_call(id, params, srv);
    if (strcmp(method, "session/delete") == 0)
        return handle_delete_session(id, params, srv);
    if (strcmp(method, "edit_approval_response") == 0)
        return handle_edit_approval_response(id, params, srv);
    if (strcmp(method, "set_auto_approve") == 0)
        return handle_set_auto_approve(id, params, srv);
    if (strcmp(method, "request_permission") == 0)
        return handle_request_permission(id, params, srv);
    if (strcmp(method, "permission_response") == 0)
        return handle_permission_response(id, params, srv);

    return acp_make_error(id, -32601, "Method not found", NULL);
}

/* ================================================================
 *  Main loop
 * ================================================================ */

acp_server_t *acp_server_new(void) {
    acp_server_t *srv = (acp_server_t *)calloc(1, sizeof(acp_server_t));
    if (!srv) return NULL;

    snprintf(srv->server_name, sizeof(srv->server_name), "%s", ACP_SERVER_NAME);
    snprintf(srv->server_version, sizeof(srv->server_version), "%s", ACP_SERVER_VERSION);
    srv->initialized = false;
    srv->next_id = 0;
    return srv;
}

void acp_server_free(acp_server_t *srv) {
    if (!srv) return;
    for (int i = 0; i < ACP_MAX_SESSIONS; i++) {
        if (srv->sessions[i].active)
            agent_free(&srv->sessions[i].agent);
    }
    free(srv);
}

void acp_server_run(acp_server_t *srv) {
    if (!srv) return;

    fprintf(stderr, "[acp] ACP server starting (%s v%s)\n",
            srv->server_name, srv->server_version);
    fprintf(stderr, "[acp] Protocol: JSON-RPC 2.0 over stdio\n");

    /* Initialize tool registry once */
    extern void tools_init_all(void);
    tools_init_all();

    while (1) {
        json_node_t *req = acp_read_message();
        if (!req) break;

        const char *method = acp_get_method(req);
        const char *id = acp_get_id(req);

        if (!method) {
            json_node_t *err = acp_make_error(id, -32600, "Invalid Request: missing method", NULL);
            if (id) acp_write_message(err);
            json_free(err);
            json_free(req);
            continue;
        }

        if (!srv->initialized && strcmp(method, "initialize") != 0) {
            json_node_t *err = acp_make_error(id, -32000, "Server not initialized", NULL);
            if (id) acp_write_message(err);
            json_free(err);
            json_free(req);
            continue;
        }

        json_node_t *params = json_object_get(req, "params");
        json_node_t *response = dispatch_request(method, id, params, srv);
        if (response) {
            acp_write_message(response);
            json_free(response);
        }

        json_free(req);
    }

    fprintf(stderr, "[acp] ACP server shutting down\n");
}
