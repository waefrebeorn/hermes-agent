/*
 * tui_json_rpc.c — JSON-RPC 2.0 gateway server for TUI backend.
 *
 * Replaces ad-hoc strstr/sscanf inline parsing in tui_fullscreen.c
 * with proper method dispatch, type-safe parameter extraction, and
 * standard error responses.
 *
 * MIT License — WuBu Hermes Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "tui_json_rpc.h"
#include "../../include/hermes_json.h"

/* ── Method registry ── */
#define MAX_METHODS 64

static tui_rpc_method_t  s_methods[MAX_METHODS];
static int               s_method_count = 0;
static bool              s_initialized = false;

/* ── Built-in utility methods ── */

/* "ping" → returns "pong" */
static const char *rpc_ping(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "\"pong\"");
    return scratch;
}

/* "echo" → returns the params object back */
static const char *rpc_echo(const void *params, char *scratch, size_t sz) {
    if (!params) {
        snprintf(scratch, sz, "{}");
        return scratch;
    }
    /* Serialize params back */
    char *s = json_serialize((json_t *)params);
    if (!s) { snprintf(scratch, sz, "{}"); return scratch; }
    snprintf(scratch, sz, "%s", s);
    free(s);
    return scratch;
}

/* ── Internal: extract request id from parsed JSON ── */
static int extract_id(const json_t *root) {
    json_t *id_node = json_obj_get(root, "id");
    if (!id_node) return -1;
    if (id_node->type == JSON_NUMBER) return (int)id_node->num_val;
    if (id_node->type == JSON_STRING) return atoi(id_node->str_val);
    return -1;
}

/* ── Internal: build error response ── */
static void build_error_json(char *buf, size_t sz, int id,
                              int code, const char *msg) {
    if (id < 0) {
        /* Notification error — log only */
        snprintf(buf, sz, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":\"%s\"}}",
                 code, msg);
    } else {
        snprintf(buf, sz,
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
                 id, code, msg);
    }
}

/* ── API: Build success response ── */
void tui_rpc_build_result(char *buf, size_t sz, int id, const char *result_json) {
    if (id < 0) {
        /* Notification — no response */
        buf[0] = '\0';
        return;
    }
    snprintf(buf, sz,
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}",
             id, result_json ? result_json : "null");
}

/* ── API: Build error response ── */
void tui_rpc_build_error(char *buf, size_t sz, int id, int code,
                          const char *message, const void *data) {
    (void)data; /* Data field for structured error objects — unused for now */
    build_error_json(buf, sz, id, code, message);
}

/* ── API: Initialize ── */
void tui_rpc_init(void) {
    if (s_initialized) return;
    s_initialized = true;
    s_method_count = 0;

    /* Register built-in methods */
    tui_rpc_register("ping", rpc_ping, "Health check — returns \"pong\"");
    tui_rpc_register("echo", rpc_echo, "Echoes back params (debug)");
}

/* ── API: Register method ── */
void tui_rpc_register(const char *method, tui_rpc_handler_t handler,
                       const char *desc) {
    if (s_method_count >= MAX_METHODS) return;
    s_methods[s_method_count].method  = method;
    s_methods[s_method_count].handler = handler;
    s_methods[s_method_count].desc    = desc ? desc : "";
    s_method_count++;
}

/* ── API: Dispatch a JSON-RPC message ──
 *
 * Parses request_json, looks up the method in the dispatch table,
 * calls the handler, and builds a response string into out_response.
 *
 * Returns NULL for notifications (no response needed).
 * Returns allocated response string for requests (caller must free).
 */
const char *tui_rpc_dispatch(const char *request_json,
                              char *out_response, size_t out_sz) {
    if (!request_json || !*request_json) {
        return NULL;
    }

    /* Parse JSON */
    char *error_msg = NULL;
    json_t *root = json_parse(request_json, &error_msg);
    if (!root) {
        build_error_json(out_response, out_sz, -1,
                         JSON_RPC_PARSE_ERROR,
                         error_msg ? error_msg : "Parse error");
        free(error_msg);
        return NULL;
    }

    /* Validate jsonrpc version */
    const char *version = json_get_str(root, "jsonrpc", "");
    if (strcmp(version, "2.0") != 0) {
        build_error_json(out_response, out_sz, -1,
                         JSON_RPC_INVALID_REQUEST,
                         "Must use jsonrpc 2.0");
        json_free(root);
        return NULL;
    }

    /* Extract method name */
    const char *method_name = json_get_str(root, "method", "");
    if (!*method_name) {
        build_error_json(out_response, out_sz, -1,
                         JSON_RPC_INVALID_REQUEST,
                         "Missing method field");
        json_free(root);
        return NULL;
    }

    /* Extract request ID (if present) */
    int id = extract_id(root);
    bool is_notification = (id < 0);

    /* Look up method in dispatch table */
    tui_rpc_handler_t handler = NULL;
    for (int i = 0; i < s_method_count; i++) {
        if (strcmp(s_methods[i].method, method_name) == 0) {
            handler = s_methods[i].handler;
            break;
        }
    }

    if (!handler) {
        build_error_json(out_response, out_sz, id,
                         JSON_RPC_METHOD_NOT_FOUND,
                         "Method not found");
        json_free(root);
        return NULL;
    }

    /* Extract params (may be NULL for no params) */
    json_t *params = json_obj_get(root, "params");

    /* Call handler */
    char scratch[4096];
    const char *result = handler((const void *)params, scratch, sizeof(scratch));

    /* Build response */
    if (is_notification) {
        out_response[0] = '\0';
    } else if (result) {
        snprintf(out_response, out_sz,
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}",
                 id, result);
    } else {
        /* Handler returned NULL but request needs response — error */
        build_error_json(out_response, out_sz, id,
                         JSON_RPC_INTERNAL_ERROR,
                         "Handler produced no result");
    }

    json_free(root);
    return NULL;
}

/* ── API: Parameter extraction ── */

const char *tui_rpc_param_string(const void *params, const char *key,
                                  const char *default_val) {
    if (!params || !key) return default_val;
    return json_get_str((const json_t *)params, key, default_val);
}

int tui_rpc_param_int(const void *params, const char *key, int default_val) {
    if (!params || !key) return default_val;
    return (int)json_get_num((const json_t *)params, key, (double)default_val);
}

bool tui_rpc_param_bool(const void *params, const char *key, bool default_val) {
    if (!params || !key) return default_val;
    return json_get_bool((const json_t *)params, key, default_val);
}

double tui_rpc_param_double(const void *params, const char *key, double default_val) {
    if (!params || !key) return default_val;
    return json_get_num((const json_t *)params, key, default_val);
}

/* ── API: Get all registered methods ── */
const tui_rpc_method_t **tui_rpc_get_all(void) {
    /* Allocate +1 for NULL sentinel */
    const tui_rpc_method_t **arr =
        (const tui_rpc_method_t **)malloc(sizeof(tui_rpc_method_t *) *
                                           (s_method_count + 1));
    if (!arr) return NULL;
    for (int i = 0; i < s_method_count; i++) {
        arr[i] = &s_methods[i];
    }
    arr[s_method_count] = NULL;
    return arr;
}
