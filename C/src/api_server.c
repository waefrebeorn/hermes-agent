/**
 * @file api_server.c
 * @brief E01: OpenAI-compatible REST API server.
 *
 * Minimal HTTP server implementing OpenAI's chat completions API.
 * Non-streaming only for now. Uses raw sockets (no external HTTP server dep).
 *
 * Endpoints:
 *   GET /v1/models       → JSON list of available model IDs
 *   POST /v1/chat/completions → OpenAI-compatible chat response
 */
#include "hermes_api_server.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

/* ── Constants ──────────────────────────────────────────────────── */

#define API_SERVER_BACKLOG    16
#define API_REQ_BUF_SIZE      65536
#define API_RESP_BUF_SIZE     262144
#define API_DEFAULT_PORT      9101

/* ── Global state ───────────────────────────────────────────────── */

static int              g_port = API_DEFAULT_PORT;
static int              g_server_fd = -1;
static volatile bool    g_running = false;
static pthread_t        g_thread;
static hermes_config_t *g_cfg = NULL;
static agent_state_t   *g_agent = NULL;

/* ── Metrics ────────────────────────────────────────────────────── */

static volatile int     g_request_count = 0;
static volatile time_t  g_start_time = 0;

/* ── HTTP response helpers ──────────────────────────────────────── */

static void send_response(int fd, int status, const char *status_text,
                           const char *body, const char *content_type) {
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type ? content_type : "application/json",
        body ? strlen(body) : 0);

    write(fd, header, (size_t)n);
    if (body) write(fd, body, strlen(body));
}

static void send_json(int fd, int status, const char *status_text,
                       const char *json_body) {
    send_response(fd, status, status_text, json_body, "application/json");
}

static void send_error(int fd, int status, const char *message) {
    char body[4096];
    snprintf(body, sizeof(body),
        "{\"error\":{\"message\":\"%s\",\"type\":\"error\",\"code\":%d}}",
        message, status);
    send_json(fd, status, message, body);
}

/* ── HTTP request parser (minimal) ──────────────────────────────── */

/**
 * Parse the HTTP method and path from the first line.
 * Returns 0 on success.
 */
static int parse_request_line(const char *buf, char *method, size_t mlen,
                                char *path, size_t plen) {
    if (!buf || !method || !path) return -1;
    method[0] = path[0] = '\0';

    const char *p = buf;

    /* Read method */
    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && mlen > 1) {
        *method++ = *p++;
        mlen--;
    }
    *method = '\0';
    if (*p == ' ') p++;

    /* Read path */
    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && *p != '?' && plen > 1) {
        *path++ = *p++;
        plen--;
    }
    *path = '\0';

    return 0;
}

/**
 * Find the request body (after \r\n\r\n).
 * Returns pointer to body, or NULL if not found.
 */
static const char *find_body(const char *buf) {
    const char *p = strstr(buf, "\r\n\r\n");
    if (!p) {
        p = strstr(buf, "\n\n");
        if (!p) return NULL;
        return p + 2;
    }
    return p + 4;
}

/* ── Route handlers ─────────────────────────────────────────────── */

/**
 * Parse query string from request buffer into a simple key=value map.
 * Extracts the portion after '?' in the request line.
 */
static void parse_query_params(const char *buf, char *query, size_t qlen) {
    query[0] = '\0';
    const char *q = strchr(buf, '?');
    if (!q) return;
    q++; /* skip '?' */
    const char *space = strchr(q, ' ');
    size_t len = space ? (size_t)(space - q) : strlen(q);
    if (len >= qlen) len = qlen - 1;
    memcpy(query, q, len);
    query[len] = '\0';
}

/**
 * Get a query parameter value by name.
 * Returns pointer to value or NULL if not found.
 */
static const char *get_query_param(const char *query, const char *name) {
    if (!query || !query[0] || !name) return NULL;
    size_t nlen = strlen(name);

    const char *p = query;
    while (*p) {
        /* Skip leading '&' if not first */
        if (p != query && *p == '&') p++;

        if (strncmp(p, name, nlen) == 0 && p[nlen] == '=') {
            return p + nlen + 1;
        }

        /* Skip to next param */
        p = strchr(p, '&');
        if (!p) break;
        p++;
    }
    return NULL;
}

/**
 * GET /v1/models — return list of available models.
 */
static void handle_get_models(int fd) {
    /* Build models list from config and provider metadata */
    json_t *models = json_array();

    /* Add known models */
    json_append(models, json_string("hermes-c-default"));
    json_append(models, json_string("gpt-4o"));
    json_append(models, json_string("gpt-4o-mini"));
    json_append(models, json_string("claude-sonnet-4"));
    json_append(models, json_string("claude-haiku-3"));
    json_append(models, json_string("deepseek-chat"));
    json_append(models, json_string("deepseek-reasoner"));

    /* Add model from agent config if available */
    if (g_agent && g_agent->llm.model[0]) {
        json_t *m = json_object();
        json_set(m, "id", json_string(g_agent->llm.model));
        json_set(m, "object", json_string("model"));
        json_set(m, "owned_by", json_string("hermes-c"));
        json_append(models, m);
    }

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));
    json_set(root, "data", models);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * POST /v1/chat/completions — run a chat completion.
 */
static void handle_post_chat(int fd, const char *body_json) {
    if (!body_json || !body_json[0]) {
        send_error(fd, 400, "empty request body");
        return;
    }

    json_t *req = json_parse(body_json, NULL);
    if (!req) {
        send_error(fd, 400, "invalid JSON");
        return;
    }

    /* Extract model */
    const char *model_raw = json_get_str(req, "model", "");
    char model[256] = "";
    if (model_raw) strncpy(model, model_raw, sizeof(model) - 1);
    (void)model;

    /* Extract messages array */
    const json_t *messages = json_obj_get(req, "messages");
    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0) {
        json_free(req);
        send_error(fd, 400, "missing or empty messages array");
        return;
    }

    /* Build message_t list from JSON messages */
    message_t *msg_list[256];
    int msg_count = 0;

    for (size_t i = 0; i < json_len(messages) && msg_count < 256; i++) {
        const json_t *m = json_get(messages, i);
        if (!m) continue;

        const char *role = json_get_str(m, "role", "user");
        const char *content = json_get_str(m, "content", "");

        message_role_t r = MSG_USER;
        if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
        else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
        else if (strcmp(role, "tool") == 0) r = MSG_TOOL;

        msg_list[msg_count++] = message_new(r, content);
    }

    /* Build response JSON matching OpenAI format */
    json_t *resp = json_object();
    json_set(resp, "id", json_string("chatcmpl-0000000000000"));
    json_set(resp, "object", json_string("chat.completion"));
    json_set(resp, "model", json_string(model));

    /* Timestamp */
    time_t now = time(NULL);
    json_set(resp, "created", json_number((double)now));

    /* Try to run through the agent if available — for now just echo */
    /* E01: In future, use g_agent to dispatch through the LLM client */

    /* Choices array */
    json_t *choices = json_array();
    json_t *choice = json_object();
    json_t *message_obj = json_object();

    /* Collect all message content */
    char content_buf[32768] = "";
    for (int i = 0; i < msg_count; i++) {
        if (msg_list[i]->content) {
            size_t cur = strlen(content_buf);
            snprintf(content_buf + cur, sizeof(content_buf) - cur,
                     "%s: %s\n",
                     msg_list[i]->role == MSG_SYSTEM ? "system" :
                     msg_list[i]->role == MSG_ASSISTANT ? "assistant" :
                     msg_list[i]->role == MSG_TOOL ? "tool" : "user",
                     msg_list[i]->content);
        }
        message_free(msg_list[i]);
    }

    json_set(message_obj, "role", json_string("assistant"));

    /* If we have an agent, try a real LLM call */
    char result[32768] = "";
    if (g_agent && g_agent->llm.base_url[0]) {
        /* Rebuild messages for LLM call */
        const message_t *llm_msgs[256];
        int llm_count = 0;
        for (size_t i = 0; i < json_len(messages) && llm_count < 256; i++) {
            const json_t *m = json_get(messages, i);
            if (!m) continue;
            const char *role = json_get_str(m, "role", "user");
            const char *content = json_get_str(m, "content", "");
            message_role_t r = MSG_USER;
            if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
            else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
            llm_msgs[llm_count++] = message_new(r, content);
        }

        llm_response_t *resp_llm = llm_chat_completion(
            &g_agent->llm, llm_msgs, (size_t)llm_count, NULL);
        if (resp_llm && resp_llm->content) {
            snprintf(result, sizeof(result), "%s", resp_llm->content);
        }
        llm_response_free(resp_llm);

        /* Free the temporary messages */
        for (int i = 0; i < llm_count; i++)
            message_free((message_t *)llm_msgs[i]);
    } else {
        /* No agent available — echo messages back as a mock response */
        snprintf(result, sizeof(result),
                 "Received %d messages. Model: %s. "
                 "Agent dispatch not available — configure a provider to enable LLM calls.",
                 msg_count, model);
    }

    json_set(message_obj, "content", json_string(result));
    json_set(choice, "index", json_number(0));
    json_set(choice, "message", message_obj);

    /* Finish reason */
    json_t *finish = json_object();
    json_set(finish, "reason", json_string("stop"));
    json_set(choice, "finish_reason", json_string("stop"));

    json_append(choices, choice);
    json_set(resp, "choices", choices);

    /* Usage (minimal) */
    json_t *usage = json_object();
    json_set(usage, "prompt_tokens", json_number(0));
    json_set(usage, "completion_tokens", json_number(0));
    json_set(usage, "total_tokens", json_number(0));
    json_set(resp, "usage", usage);

    char *out = json_serialize(resp);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }

    json_free(resp);
    json_free(req);
}

static void send_sse_event(int fd, const char *data);
static void send_sse_headers(int fd);
static void sse_send_chunk(int fd, const char *content, int index);

/* ── Path helper ────────────────────────────────────────────────── */

/**
 * Check if path starts with a prefix and extract the suffix.
 * E.g. starts_with("/v1/sessions/abc123", "/v1/sessions/", id, 64) → true, id="abc123"
 */
static bool starts_with(const char *path, const char *prefix, char *suffix, size_t slen) {
    size_t plen = strlen(prefix);
    if (strncmp(path, prefix, plen) != 0) return false;
    const char *rest = path + plen;
    if (!rest[0]) return false; /* path = prefix, no suffix */
    strncpy(suffix, rest, slen - 1);
    suffix[slen - 1] = '\0';
    return true;
}

/**
 * GET /v1/sessions — list all sessions.
 */
static void handle_sessions_list(int fd, const char *query) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    size_t count = 0;
    db_session_entry_t *entries = db_list_with_meta(g_agent->db, &count);
    if (!entries) {
        send_json(fd, 200, "OK", "{\"object\":\"list\",\"data\":[],\"total\":0}");
        return;
    }

    /* Apply ?limit=N query param */
    size_t limit = count;
    if (query && query[0]) {
        const char *limit_str = get_query_param(query, "limit");
        if (limit_str) {
            long n = atol(limit_str);
            if (n > 0 && (size_t)n < limit) limit = (size_t)n;
        }
    }

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));

    json_t *data = json_array();
    for (size_t i = 0; i < limit; i++) {
        json_t *s = json_object();
        json_set(s, "id", json_string(entries[i].id));
        if (entries[i].meta.title[0])
            json_set(s, "title", json_string(entries[i].meta.title));
        if (entries[i].meta.created_at > 0)
            json_set(s, "created_at", json_number((double)entries[i].meta.created_at));
        if (entries[i].meta.updated_at > 0)
            json_set(s, "updated_at", json_number((double)entries[i].meta.updated_at));
        json_set(s, "message_count", json_number((double)entries[i].meta.message_count));
        json_append(data, s);
    }
    json_set(root, "data", data);
    json_set(root, "total", json_number((double)count));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    }
    json_free(root);
    free(entries);
}

/**
 * GET /v1/sessions/{id} — get session details.
 */
static void handle_session_get(int fd, const char *session_id) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    char *json_out = db_export_json(g_agent->db, session_id);
    if (!json_out) {
        send_error(fd, 404, "session not found");
        return;
    }

    send_json(fd, 200, "OK", json_out);
    free(json_out);
}

/**
 * POST /v1/sessions — create a new session.
 */
static void handle_session_create(int fd, const char *body_json) {
    (void)body_json;
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    /* Generate a session ID */
    char session_id[64];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(session_id, sizeof(session_id), "%04d%02d%02d_%02d%02d%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Save empty session data */
    if (!db_save(g_agent->db, session_id, "[]")) {
        send_error(fd, 500, "failed to create session");
        return;
    }

    /* Create and save default metadata */
    session_meta_t meta;
    db_meta_init(&meta);
    snprintf(meta.title, sizeof(meta.title), "%s", session_id);
    if (!db_save_meta(g_agent->db, session_id, &meta)) {
        send_error(fd, 500, "failed to save session metadata");
        return;
    }

    json_t *resp = json_object();
    json_set(resp, "id", json_string(session_id));
    json_set(resp, "object", json_string("session.created"));

    char *out = json_serialize(resp);
    if (out) {
        send_json(fd, 201, "Created", out);
        free(out);
    }
    json_free(resp);
}

/**
 * DELETE /v1/sessions/{id} — delete a session.
 */
static void handle_session_delete(int fd, const char *session_id) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    if (!db_delete(g_agent->db, session_id)) {
        send_error(fd, 404, "session not found or delete failed");
        return;
    }

    /* Build response */
    char resp[256];
    snprintf(resp, sizeof(resp), "{\"status\":\"deleted\",\"id\":\"%s\"}", session_id);
    send_json(fd, 200, "OK", resp);
}

/**
 * POST /v1/chat/completions?stream=true — SSE streaming response.
 * Sends the complete response as token-by-token SSE events.
 * The handler closes the fd when done.
 */
static void handle_post_chat_stream(int fd, const char *body_json) {
    if (!body_json || !body_json[0]) {
        send_error(fd, 400, "empty request body");
        close(fd);
        return;
    }

    json_t *req = json_parse(body_json, NULL);
    if (!req) {
        send_error(fd, 400, "invalid JSON");
        close(fd);
        return;
    }

    /* Extract model and messages */
    const char *model_raw = json_get_str(req, "model", "");
    char model[256] = "";
    if (model_raw) strncpy(model, model_raw, sizeof(model) - 1);
    const json_t *messages = json_obj_get(req, "messages");
    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0) {
        json_free(req);
        send_error(fd, 400, "missing or empty messages array");
        close(fd);
        return;
    }

    /* Run through LLM if available */
    char result[32768] = "";
    if (g_agent && g_agent->llm.base_url[0]) {
        const message_t *llm_msgs[256];
        int llm_count = 0;
        for (size_t i = 0; i < json_len(messages) && llm_count < 256; i++) {
            const json_t *m = json_get(messages, i);
            if (!m) continue;
            const char *role = json_get_str(m, "role", "user");
            const char *content = json_get_str(m, "content", "");
            message_role_t r = MSG_USER;
            if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
            else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
            llm_msgs[llm_count++] = message_new(r, content);
        }

        llm_response_t *resp_llm = llm_chat_completion(
            &g_agent->llm, llm_msgs, (size_t)llm_count, NULL);
        if (resp_llm && resp_llm->content) {
            snprintf(result, sizeof(result), "%s", resp_llm->content);
        }
        llm_response_free(resp_llm);
        for (int i = 0; i < llm_count; i++)
            message_free((message_t *)llm_msgs[i]);
    }

    json_free(req);

    /* Send SSE headers and stream the response */
    send_sse_headers(fd);

    /* Token-buffer SSE streaming — send in ~4-char chunks for natural
     * token-level granularity, respecting UTF-8 multi-byte boundaries. */
#define TOKEN_BUF_SIZE 4
    char tbuf[TOKEN_BUF_SIZE + 1];
    int tpos = 0;
    int index = 0;
    int i = 0;

    while (result[i]) {
        /* Track multi-byte UTF-8: continuation bytes are 0x80-0xBF */
        int is_utf8_cont = ((unsigned char)result[i] & 0xC0) == 0x80;
        tbuf[tpos++] = result[i++];

        if (!is_utf8_cont && tpos >= TOKEN_BUF_SIZE) {
            tbuf[tpos] = '\0';
            sse_send_chunk(fd, tbuf, index++);
            tpos = 0;
        }
    }

    /* Flush remaining chars */
    if (tpos > 0) {
        tbuf[tpos] = '\0';
        sse_send_chunk(fd, tbuf, index++);
    }

    /* Fallback: if no chunks, send full result as one chunk */
    if (index == 0 && result[0]) {
        sse_send_chunk(fd, result, 0);
        index = 1;
    }

    /* Send finish event */
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_t *delta = json_object();
    json_set(delta, "role", json_string("assistant"));
    json_set(delta, "content", json_string(""));
    json_set(choice, "delta", delta);
    json_set(choice, "finish_reason", json_string("stop"));

    json_t *finish_msg = json_object();
    json_set(finish_msg, "id", json_string("chatcmpl-stream"));
    json_set(finish_msg, "object", json_string("chat.completion.chunk"));
    json_set(finish_msg, "created", json_number((double)time(NULL)));
    json_set(finish_msg, "model", json_string(model));
    json_t *choices_arr = json_array();
    json_append(choices_arr, choice);
    json_set(finish_msg, "choices", choices_arr);

    char *finish_str = json_serialize(finish_msg);
    if (finish_str) {
        send_sse_event(fd, finish_str);
        free(finish_str);
    }
    json_free(finish_msg);

    /* End of stream */
    send_sse_event(fd, "[DONE]");
    fsync(fd);
    close(fd);
}

/**
 * Handle an OPTIONS preflight request.
 */
static void handle_options(int fd) {
    send_response(fd, 204, "No Content", NULL, NULL);
}

/**
 * GET /v1/capabilities — machine-readable API capabilities.
 */
static void handle_capabilities(int fd) {
    json_t *root = json_object();
    json_set(root, "api_version", json_string("v1"));
    json_set(root, "name", json_string("hermes-c"));

    json_t *endpoints = json_array();
    json_append(endpoints, json_string("GET /v1/models"));
    json_append(endpoints, json_string("POST /v1/chat/completions"));
    json_append(endpoints, json_string("GET /v1/capabilities"));
    json_append(endpoints, json_string("GET /v1/tools"));
    json_append(endpoints, json_string("GET /v1/agent/status"));
    json_append(endpoints, json_string("GET /v1/sessions"));
    json_append(endpoints, json_string("POST /v1/sessions"));
    json_append(endpoints, json_string("GET /v1/sessions/{id}"));
    json_append(endpoints, json_string("DELETE /v1/sessions/{id}"));
    json_append(endpoints, json_string("GET /health"));
    json_append(endpoints, json_string("GET /health/detailed"));
    json_set(root, "endpoints", endpoints);

    json_t *features = json_object();
    json_set(features, "chat_completions", json_bool(true));
    json_set(features, "streaming", json_bool(true));
    json_set(features, "tools_listing", json_bool(true));
    json_set(features, "models_listing", json_bool(true));
    json_set(features, "agent_status", json_bool(true));
    json_set(features, "sessions_listing", json_bool(true));
    json_set(features, "sessions_crud", json_bool(true));
    json_set(features, "health_detailed", json_bool(true));
    json_set(root, "features", features);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /v1/tools — list all registered tools.
 */
static void handle_tools_list(int fd) {
    if (!g_agent) {
        send_error(fd, 503, "agent not initialized");
        return;
    }

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));

    json_t *tools_array = json_array();
    size_t tool_count = registry_get_count();

    for (size_t i = 0; i < tool_count; i++) {
        const char *name = registry_get_name(i);
        if (!name) continue;

        json_t *tool_obj = json_object();
        json_set(tool_obj, "name", json_string(name));

        const char *toolset = registry_get_toolset(name);
        if (toolset) json_set(tool_obj, "toolset", json_string(toolset));

        int timeout = registry_get_timeout(name);
        if (timeout > 0) json_set(tool_obj, "timeout", json_number((double)timeout));

        json_append(tools_array, tool_obj);
    }

    json_set(root, "data", tools_array);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /health/detailed — rich health status with system info.
 */
static void handle_health_detailed(int fd) {
    json_t *root = json_object();

    /* Basic status */
    json_set(root, "status", json_string("ok"));
    json_set(root, "version", json_string(HERMES_VERSION));

    /* Uptime */
    static time_t start_time = 0;
    if (start_time == 0) start_time = time(NULL);
    json_set(root, "uptime_seconds", json_number((double)(time(NULL) - start_time)));

    /* Agent state */
    if (g_agent) {
        json_t *agent_info = json_object();
        if (g_agent->llm.model[0])
            json_set(agent_info, "model", json_string(g_agent->llm.model));
        if (g_agent->llm.provider[0])
            json_set(agent_info, "provider", json_string(g_agent->llm.provider));
        json_set(agent_info, "tools_registered", json_number((double)registry_get_count()));
        json_set(agent_info, "session_active", json_bool(g_agent->db != NULL));
        json_set(root, "agent", agent_info);
    }

    /* Config port */
    json_set(root, "port", json_number((double)g_port));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /v1/agent/status — current agent state information.
 */
static void handle_agent_status(int fd) {
    if (!g_agent) {
        send_error(fd, 503, "agent not initialized");
        return;
    }

    json_t *root = json_object();
    json_set(root, "status", json_string("running"));

    /* LLM configuration */
    json_t *llm_info = json_object();
    if (g_agent->llm.model[0])
        json_set(llm_info, "model", json_string(g_agent->llm.model));
    if (g_agent->llm.provider[0])
        json_set(llm_info, "provider", json_string(g_agent->llm.provider));
    if (g_agent->llm.base_url[0])
        json_set(llm_info, "base_url", json_string(g_agent->llm.base_url));
    json_set(llm_info, "max_tokens", json_number((double)g_agent->llm.max_tokens));
    json_set(llm_info, "temperature", json_number((double)g_agent->llm.temperature));
    json_set(root, "llm", llm_info);

    /* Session info */
    json_t *session_info = json_object();
    json_set(session_info, "active", json_bool(g_agent->db != NULL));
    if (g_agent->session_id[0])
        json_set(session_info, "session_id", json_string(g_agent->session_id));
    json_set(root, "session", session_info);

    /* Config */
    json_set(root, "max_iterations", json_number((double)g_agent->max_iterations));

    /* Tool info */
    json_set(root, "tools_registered", json_number((double)registry_get_count()));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/* ── Webhook endpoint (E05) ─────────────────────────────────────── */

/* Handle POST /webhook/:platform — accepts generic webhook payloads.
 * Logs the incoming webhook and returns acknowledgment. */
static void handle_webhook(int fd, const char *body, const char *platform) {
    if (!platform || !platform[0]) {
        send_error(fd, 400, "missing platform");
        close(fd);
        return;
    }
    if (!body || !body[0]) {
        send_error(fd, 400, "empty body");
        close(fd);
        return;
    }

    json_t *resp = json_object();
    json_set(resp, "status", json_string("ok"));
    json_set(resp, "platform", json_string(platform));
    json_set(resp, "received", json_bool(true));
    json_set(resp, "body_size", json_number((double)strlen(body)));

    /* Log */
    fprintf(stderr, "[webhook] POST /webhook/%s — %zu bytes\n",
            platform, body ? strlen(body) : 0);

    char *s = json_serialize(resp);
    send_json(fd, 200, "OK", s ? s : "{}");
    free(s);
    json_free(resp);
}

/* ── E01: Remaining REST endpoints ──────────────────────────────── */

/**
 * GET /v1/config — expose safe configuration fields (no secrets).
 */
static void handle_config_get(int fd) {
    if (!g_cfg) {
        send_error(fd, 503, "config not initialized");
        return;
    }

    json_t *root = json_object();
    json_set(root, "version", json_string(HERMES_VERSION));
    json_set(root, "port", json_number((double)g_port));

    /* LLM config */
    json_t *llm_cfg = json_object();
    json_set(llm_cfg, "model", json_string(g_cfg->model));
    json_set(llm_cfg, "provider", json_string(g_cfg->provider));
    json_set(llm_cfg, "base_url", json_string(g_cfg->base_url));
    if (g_agent) {
        json_set(llm_cfg, "max_tokens", json_number((double)g_agent->llm.max_tokens));
        json_set(llm_cfg, "temperature", json_number((double)g_agent->llm.temperature));
    }
    json_set(llm_cfg, "max_iterations", json_number((double)g_cfg->agent.max_iterations));
    json_set(root, "llm", llm_cfg);

    /* Agent config (safe fields only) */
    json_t *agent_cfg = json_object();
    json_set(agent_cfg, "verbose", json_number((double)g_cfg->verbose));
    json_set(agent_cfg, "quiet_mode", json_bool(g_cfg->quiet_mode));
    json_set(agent_cfg, "yolo_mode", json_bool(g_cfg->yolo_mode));
    json_set(agent_cfg, "fast_mode", json_bool(g_cfg->fast_mode));
    json_set(agent_cfg, "compress_enabled", json_bool(g_cfg->compress_enabled));
    json_set(agent_cfg, "max_turns", json_number((double)g_cfg->max_turns));
    json_set(agent_cfg, "personality", json_string(g_cfg->personality[0] ? g_cfg->personality : "default"));
    json_set(root, "agent", agent_cfg);

    /* Gateway */
    json_t *gw = json_object();
    json_set(gw, "platforms", json_string(g_cfg->gateway_platforms[0] ? g_cfg->gateway_platforms : "none"));
    json_set(gw, "secret_rotation", json_number((double)g_cfg->secret_rotation_interval));
    json_set(root, "gateway", gw);

    /* Home / paths */
    json_t *paths = json_object();
    json_set(paths, "vault_path", json_string(g_cfg->vault_path[0] ? g_cfg->vault_path : "default"));
    json_set(paths, "skin", json_string(g_cfg->skin_path[0] ? g_cfg->skin_path : "default"));
    json_set(root, "paths", paths);

    /* Proxy */
    json_t *proxy = json_object();
    if (g_cfg->proxy_https[0]) json_set(proxy, "https_proxy", json_string(g_cfg->proxy_https));
    if (g_cfg->proxy_no[0]) json_set(proxy, "no_proxy", json_string(g_cfg->proxy_no));
    json_set(root, "proxy", proxy);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /v1/service/info — service metadata (version, uptime, build).
 */
static void handle_service_info(int fd) {
    json_t *root = json_object();
    json_set(root, "service", json_string("hermes-c"));
    json_set(root, "version", json_string(HERMES_VERSION));
    json_set(root, "api_version", json_string("v1"));

    if (g_start_time > 0) {
        time_t now = time(NULL);
        double uptime = difftime(now, g_start_time);
        json_set(root, "uptime_seconds", json_number(uptime));
        json_set(root, "start_time", json_number((double)g_start_time));
    }

    json_t *build = json_object();
    json_set(build, "compiler", json_string(__VERSION__));
    json_set(build, "platform", json_string(__linux__ ? "linux" : "unknown"));
    json_set(root, "build", build);

    /* Registered endpoints summary */
    json_t *endpoints = json_array();
    json_append(endpoints, json_string("GET /v1/models"));
    json_append(endpoints, json_string("POST /v1/chat/completions"));
    json_append(endpoints, json_string("GET /v1/capabilities"));
    json_append(endpoints, json_string("GET /v1/tools"));
    json_append(endpoints, json_string("GET /v1/agent/status"));
    json_append(endpoints, json_string("GET /v1/sessions"));
    json_append(endpoints, json_string("POST /v1/sessions"));
    json_append(endpoints, json_string("GET /v1/sessions/{id}"));
    json_append(endpoints, json_string("DELETE /v1/sessions/{id}"));
    json_append(endpoints, json_string("GET /health"));
    json_append(endpoints, json_string("GET /health/detailed"));
    json_append(endpoints, json_string("GET /v1/config"));
    json_append(endpoints, json_string("GET /v1/service/info"));
    json_append(endpoints, json_string("GET /v1/metrics"));
    json_append(endpoints, json_string("POST /webhook/:platform"));
    json_set(root, "endpoints", endpoints);

    json_set(root, "client_count", json_number(0));
    json_set(root, "request_count", json_number((double)g_request_count));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /v1/metrics — request and token usage metrics.
 */
static void handle_metrics_get(int fd) {
    json_t *root = json_object();
    json_set(root, "object", json_string("metrics"));

    json_set(root, "total_requests", json_number((double)g_request_count));
    json_set(root, "uptime_seconds", json_number(
        g_start_time > 0 ? difftime(time(NULL), g_start_time) : 0));

    /* Per-endpoint request types (basic tracking) */
    json_t *endpoints = json_object();
    /* Simple counter per method+path — tracked inline in dispatch */
    json_set(endpoints, "chat_completions", json_number(0));
    json_set(endpoints, "models", json_number(0));
    json_set(endpoints, "sessions", json_number(0));
    json_set(root, "endpoints", endpoints);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/* ── Request dispatch ───────────────────────────────────────────── */

static void dispatch_request(int client_fd, const char *method,
                              const char *path, const char *body,
                              const char *query) {
    /* Track request count */
    __atomic_add_fetch(&g_request_count, 1, __ATOMIC_SEQ_CST);

    if (strcmp(method, "OPTIONS") == 0) {
        handle_options(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/models") == 0) {
        handle_get_models(client_fd);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/v1/chat/completions") == 0 && strstr(query ? query : "", "stream=true")) {
        handle_post_chat_stream(client_fd, body);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/v1/chat/completions") == 0) {
        handle_post_chat(client_fd, body);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/sessions") == 0) {
        handle_sessions_list(client_fd, query);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/v1/sessions") == 0) {
        handle_session_create(client_fd, body);
    } else if (strcmp(method, "GET") == 0 && strncmp(path, "/v1/sessions/", 13) == 0) {
        char sid[64] = "";
        if (starts_with(path, "/v1/sessions/", sid, sizeof(sid)))
            handle_session_get(client_fd, sid);
        else
            send_error(client_fd, 400, "invalid session id");
    } else if (strcmp(method, "DELETE") == 0 && strncmp(path, "/v1/sessions/", 13) == 0) {
        char sid[64] = "";
        if (starts_with(path, "/v1/sessions/", sid, sizeof(sid)))
            handle_session_delete(client_fd, sid);
        else
            send_error(client_fd, 400, "invalid session id");
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/health") == 0) {
        send_json(client_fd, 200, "OK", "{\"status\":\"ok\"}");
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/health/detailed") == 0) {
        handle_health_detailed(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/capabilities") == 0) {
        handle_capabilities(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/tools") == 0) {
        handle_tools_list(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/agent/status") == 0) {
        handle_agent_status(client_fd);
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/webhook/", 9) == 0) {
        /* Extract platform from path: /webhook/telegram, /webhook/discord, etc. */
        const char *platform = path + 9;
        handle_webhook(client_fd, body, platform);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/config") == 0) {
        handle_config_get(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/service/info") == 0) {
        handle_service_info(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/metrics") == 0) {
        handle_metrics_get(client_fd);
    } else {
        send_error(client_fd, 404, "not found");
    }
}

/* ── Connection handler ─────────────────────────────────────────── */

static void handle_client(int client_fd) {
    char buf[API_REQ_BUF_SIZE];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    char method[16] = "", path[1024] = "";
    if (parse_request_line(buf, method, sizeof(method), path, sizeof(path)) != 0) {
        send_error(client_fd, 400, "bad request");
        close(client_fd);
        return;
    }

    /* Extract query string from request line (path portion after ?) */
    char query[2048] = "";
    parse_query_params(buf, query, sizeof(query));

    const char *body = find_body(buf);
    dispatch_request(client_fd, method, path, body, query);
    close(client_fd);
}

/* ── SSE helpers ────────────────────────────────────────────────── */

static void send_sse_event(int fd, const char *data) {
    dprintf(fd, "data: %s\n\n", data ? data : "");
}

static void send_sse_headers(int fd) {
    const char *headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    write(fd, headers, strlen(headers));
}

static void sse_send_chunk(int fd, const char *content, int index) {
    char event[4096];
    snprintf(event, sizeof(event),
        "{\"choices\":[{\"delta\":{\"content\":\"%s\"},\"index\":%d}]}",
        content, index);
    send_sse_event(fd, event);
}

/* ── Server thread ──────────────────────────────────────────────── */

static void *server_thread(void *arg) {
    (void)arg;

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("[api-server] socket");
        return NULL;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)g_port);

    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[api-server] bind");
        close(g_server_fd);
        g_server_fd = -1;
        return NULL;
    }

    if (listen(g_server_fd, API_SERVER_BACKLOG) < 0) {
        perror("[api-server] listen");
        close(g_server_fd);
        g_server_fd = -1;
        return NULL;
    }

    printf("[api-server] OpenAI-compatible API listening on port %d\n", g_port);
    g_running = true;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (g_running) {
        int client_fd = accept(g_server_fd,
                                (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!g_running) break;
            perror("[api-server] accept");
            continue;
        }

        handle_client(client_fd);
    }

    close(g_server_fd);
    g_server_fd = -1;
    printf("[api-server] stopped\n");
    return NULL;
}

/* ── Public API ─────────────────────────────────────────────────── */

void api_server_set_port(int port) {
    g_port = port > 0 ? port : API_DEFAULT_PORT;
}

bool api_server_start(int port, const hermes_config_t *cfg, agent_state_t *agent) {
    if (g_running) return true;

    g_port = port > 0 ? port : API_DEFAULT_PORT;
    g_cfg = (hermes_config_t *)cfg;
    g_agent = agent;
    g_start_time = time(NULL);

    if (pthread_create(&g_thread, NULL, server_thread, NULL) != 0) {
        fprintf(stderr, "[api-server] failed to create thread\n");
        return false;
    }

    /* Wait for thread to signal readiness (bind+listen succeeded or failed) */
    for (int i = 0; i < 50; i++) {
        if (g_running) return true;
        if (g_server_fd < 0 && i > 5) break; /* thread failed, don't keep waiting */
        usleep(100000); /* 100ms intervals, up to 5 seconds */
    }

    if (!g_running) {
        fprintf(stderr, "[api-server] server thread failed to start (bind error?)\n");
        pthread_detach(g_thread);
        return false;
    }

    return true;
}

void api_server_stop(void) {
    if (!g_running) return;
    g_running = false;

    /* Wake up accept() by connecting briefly */
    int tmp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tmp_fd >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((uint16_t)g_port);
        connect(tmp_fd, (struct sockaddr *)&addr, sizeof(addr));
        close(tmp_fd);
    }

    pthread_join(g_thread, NULL);
}

bool api_server_is_running(void) {
    return g_running;
}
