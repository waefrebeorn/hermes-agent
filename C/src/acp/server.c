/*
 * server.c — ACP (Agent Communication Protocol) server for Hermes C.
 *
 * JSON-RPC 2.0 over stdio with Content-Length framing.
 * Implements session management + tool discovery for ACP clients.
 *
 * B01: Copilot ACP provider foundation.
 */

#include "acp/server.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

/* ================================================================
 *  Server struct
 * ================================================================ */

struct acp_server_t {
    char  server_name[64];
    char  server_version[32];
    bool  initialized;        /* true after successful initialize handshake */
    int   session_count;      /* running session counter */
};

/* ================================================================
 *  JSON-RPC framing: Content-Length header + JSON body
 *
 *  Format:
 *    Content-Length: <N>\r\n
 *    \r\n
 *    <JSON body of N bytes>
 * ================================================================ */

json_node_t *acp_read_message(void) {
    char header[256];
    int content_length = -1;

    /* Read headers until blank line */
    while (1) {
        if (!fgets(header, sizeof(header), stdin)) {
            if (feof(stdin)) return NULL;  /* Normal EOF */
            return NULL;
        }

        /* Strip \r\n */
        size_t hlen = strlen(header);
        while (hlen > 0 && (header[hlen-1] == '\n' || header[hlen-1] == '\r'))
            header[--hlen] = '\0';

        if (hlen == 0) break;  /* Blank line → end of headers */

        if (strncasecmp(header, "Content-Length:", 15) == 0) {
            const char *val = header + 15;
            while (*val && isspace((unsigned char)*val)) val++;
            content_length = atoi(val);
        }
    }

    if (content_length < 0) {
        /* No Content-Length header — invalid framing */
        return NULL;
    }

    if (content_length > ACP_MAX_MESSAGE_BYTES) {
        fprintf(stderr, "[acp] Message too large: %d bytes\n", content_length);
        return NULL;
    }

    /* Read JSON body */
    char *body = (char *)malloc((size_t)content_length + 1);
    if (!body) return NULL;

    size_t total = 0;
    while (total < (size_t)content_length) {
        ssize_t n = fread(body + total, 1, (size_t)content_length - total, stdin);
        if (n <= 0) {
            free(body);
            return NULL;
        }
        total += (size_t)n;
    }
    body[total] = '\0';

    /* Parse JSON */
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

    /* Write Content-Length header */
    int hlen = fprintf(stdout, "Content-Length: %zu\r\n\r\n", len);
    if (hlen < 0) {
        free(serialized);
        return false;
    }

    /* Write JSON body */
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
    if (id)
        json_object_set(resp, "id", json_new_string(id));
    else
        json_object_set(resp, "id", json_new_null());
    if (result)
        json_object_set(resp, "result", json_copy(result));
    else
        json_object_set(resp, "result", json_new_null());
    return resp;
}

json_node_t *acp_make_error(const char *id, int code, const char *message, json_node_t *data) {
    json_node_t *err = json_new_object();
    json_object_set(err, "code", json_new_number((double)code));
    json_object_set(err, "message", json_new_string(message));
    if (data)
        json_object_set(err, "data", json_copy(data));

    json_node_t *resp = json_new_object();
    json_object_set(resp, "jsonrpc", json_new_string("2.0"));
    if (id)
        json_object_set(resp, "id", json_new_string(id));
    else
        json_object_set(resp, "id", json_new_null());
    json_object_set(resp, "error", err);
    return resp;
}

const char *acp_get_method(json_node_t *req) {
    return json_object_get_string(req, "method", NULL);
}

const char *acp_get_id(json_node_t *req) {
    json_node_t *id_node = json_object_get(req, "id");
    if (!id_node) return NULL;
    /* id can be string or number — convert to static string */
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
 *  Handler: initialize
 * ================================================================ */

static json_node_t *handle_initialize(const char *id, json_node_t *params, acp_server_t *srv) {
    (void)params;

    srv->initialized = true;

    json_node_t *result = json_new_object();
    json_object_set(result, "protocolVersion", json_new_string(ACP_PROTOCOL_VERSION));

    /* Server info */
    json_node_t *server_info = json_new_object();
    json_object_set(server_info, "name", json_new_string(srv->server_name));
    json_object_set(server_info, "version", json_new_string(srv->server_version));
    json_object_set(result, "serverInfo", server_info);

    /* Capabilities */
    json_node_t *caps = json_new_object();
    json_object_set(caps, "prompts", json_new_object());  /* empty prompts caps */

    /* Session capabilities */
    json_node_t *session_caps = json_new_object();
    json_object_set(session_caps, "list", json_new_object());      /* SessionListCapabilities */
    json_object_set(session_caps, "resume", json_new_object());    /* SessionResumeCapabilities */
    json_object_set(session_caps, "fork", json_new_object());      /* SessionForkCapabilities */
    json_object_set(caps, "sessions", session_caps);

    json_object_set(result, "capabilities", caps);

    return acp_make_response(id, result);
}

/* ================================================================
 *  Handler: new_session
 * ================================================================ */

static json_node_t *handle_new_session(const char *id, json_node_t *params, acp_server_t *srv) {
    const char *session_name = json_object_get_string(params, "name", "default");

    srv->session_count++;

    char session_id[64];
    snprintf(session_id, sizeof(session_id), "acp-session-%d", srv->session_count);

    json_node_t *result = json_new_object();
    json_object_set(result, "session_id", json_new_string(session_id));
    json_object_set(result, "name", json_new_string(session_name));
    json_object_set(result, "created_at", json_new_string("now"));

    /* Session info */
    json_node_t *session_info = json_new_object();
    json_object_set(session_info, "session_id", json_new_string(session_id));
    json_object_set(session_info, "name", json_new_string(session_name));
    json_object_set(session_info, "created_at", json_new_string("now"));
    json_object_set(session_info, "message_count", json_new_number(0));
    json_object_set(result, "session", session_info);

    return acp_make_response(id, result);
}

/* ================================================================
 *  Handler: list_sessions
 * ================================================================ */

static json_node_t *handle_list_sessions(const char *id, json_node_t *params, acp_server_t *srv) {
    (void)params;
    (void)srv;

    json_node_t *result = json_new_object();
    json_node_t *sessions = json_new_array();

    /* Return the current session if one exists */
    json_node_t *session_info = json_new_object();
    json_object_set(session_info, "session_id", json_new_string("acp-session-1"));
    json_object_set(session_info, "name", json_new_string("default"));
    json_object_set(session_info, "created_at", json_new_string("now"));
    json_object_set(session_info, "message_count", json_new_number(0));
    json_array_append(sessions, session_info);

    json_object_set(result, "sessions", sessions);
    json_object_set(result, "next_cursor", json_new_null());

    return acp_make_response(id, result);
}

/* ================================================================
 *  Handler: available_commands
 * ================================================================ */

static json_node_t *handle_available_commands(const char *id, json_node_t *params, acp_server_t *srv) {
    (void)params;
    (void)srv;

    json_node_t *result = json_new_object();
    json_node_t *commands = json_new_array();

    /* Advertised commands matching Python Hermes ACP */
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
    if (strcmp(method, "available_commands") == 0)
        return handle_available_commands(id, params, srv);

    /* Method not found */
    return acp_make_error(id, -32601, "Method not found", NULL);
}

/* ================================================================
 *  ACP server main loop
 * ================================================================ */

acp_server_t *acp_server_new(void) {
    acp_server_t *srv = (acp_server_t *)calloc(1, sizeof(acp_server_t));
    if (!srv) return NULL;

    snprintf(srv->server_name, sizeof(srv->server_name), "%s", ACP_SERVER_NAME);
    snprintf(srv->server_version, sizeof(srv->server_version), "%s", ACP_SERVER_VERSION);
    srv->initialized = false;
    srv->session_count = 0;

    return srv;
}

void acp_server_free(acp_server_t *srv) {
    if (srv) free(srv);
}

void acp_server_run(acp_server_t *srv) {
    if (!srv) return;

    fprintf(stderr, "[acp] ACP server starting (%s v%s)\n",
            srv->server_name, srv->server_version);
    fprintf(stderr, "[acp] Protocol: JSON-RPC 2.0 over stdio\n");
    fprintf(stderr, "[acp] Listening on stdin/stdout\n");

    /* Main loop: read → process → respond */
    while (1) {
        json_node_t *req = acp_read_message();
        if (!req) {
            /* EOF or error — shut down */
            break;
        }

        const char *method = acp_get_method(req);
        const char *id = acp_get_id(req);

        if (!method) {
            /* Invalid request — missing method */
            json_node_t *err = acp_make_error(id, -32600, "Invalid Request: missing method", NULL);
            if (id) acp_write_message(err);
            json_free(err);
            json_free(req);
            continue;
        }

        /* Check if initialized for methods that require it */
        if (!srv->initialized && strcmp(method, "initialize") != 0) {
            json_node_t *err = acp_make_error(id, -32000, "Server not initialized", NULL);
            if (id) acp_write_message(err);
            json_free(err);
            json_free(req);
            continue;
        }

        /* Get params */
        json_node_t *params = json_object_get(req, "params");

        /* Dispatch */
        json_node_t *response = dispatch_request(method, id, params, srv);
        if (response) {
            acp_write_message(response);
            json_free(response);
        }

        json_free(req);
    }

    fprintf(stderr, "[acp] ACP server shutting down\n");
}
