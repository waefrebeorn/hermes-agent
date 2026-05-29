/*
 * mcp_tool.c — P56-P60: MCP tool integration for Hermes C.
 *
 * Wires libmcp into the tool registry. Manages MCP server lifecycle,
 * discovers tools, registers them as agent-callable tools.
 *
 * Supported transports: stdio (P57), SSE (P62+).
 * Config discovery: parses mcp_servers from config.yaml (P60).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "mcp.h"
#include "mcp_oauth.h"
#include "osv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  State
 * ================================================================ */

#define MAX_MCP_SERVERS 16
#define MAX_DYNAMIC_TOOLS 256

static mcp_server_t *g_servers[MAX_MCP_SERVERS];
int g_server_count = 0; /* non-static — accessed by commands.c for /reload-mcp */

/* P63-P64: Per-server auth config (parallel to g_servers array) */
static mcp_auth_t g_server_auth[MAX_MCP_SERVERS];
static char g_credential_store_path[HERMES_PATH_MAX] = "";

/* P66: Per-server tool filter config (parallel to g_servers) */
#define MAX_FILTER_PATTERNS 16

/* Per-server sampling config */
typedef struct {
    bool  enabled;
    char  model[128];         /* override model (empty = use default) */
    int   max_tokens_cap;     /* max tokens per request (0 = default) */
    int   timeout_sec;        /* LLM call timeout */
    int   max_rpm;            /* max requests per minute (0 = unlimited) */
} mcp_sampling_cfg_t;

static mcp_sampling_cfg_t g_server_sampling[MAX_MCP_SERVERS];
typedef struct {
    char allow[MAX_FILTER_PATTERNS][128];  /* allowlist patterns */
    int  allow_count;
    char block[MAX_FILTER_PATTERNS][128];  /* blocklist patterns */
    int  block_count;
} mcp_filter_t;
static mcp_filter_t g_server_filters[MAX_MCP_SERVERS];

/* Dynamically registered tool handlers */
typedef struct {
    char server_name[64];
    char tool_name[128];
} mcp_dynamic_tool_t;

static mcp_dynamic_tool_t g_dynamic_tools[MAX_DYNAMIC_TOOLS];
static int g_dynamic_count = 0;

/* ================================================================
 *  Dynamic tool handler
 * ================================================================ */

/* Generic handler that dispatches to the right MCP server by looking up
 * the registered tool name in g_dynamic_tools table */
static char *mcp_dynamic_handler(const char *args_json, const char *task_id) {
    (void)args_json;
    (void)task_id;

    /* We don't get the tool name directly in the handler.
     * Workaround: iterate all dynamic tools and find the one whose
     * handler matches. Since we use the same handler for all, we
     * extract server/tool from a lookup-by-caller approach.
     *
     * Better approach: use mcp_tool_call as the primary interface.
     * This handler is a fallback that returns instructions. */
    return strdup("{\"error\":\"Call MCP tools via mcp_tool_call with {server, tool, arguments}\","
                  "\"hint\":\"Use mcp_status to discover servers and tools\"}");
}

/* ================================================================
 *  Connect an MCP server from config
 * ================================================================ */

/* Connect an MCP server via SSE (HTTP/SSE) transport */
/* Forward declarations */
static bool mcp_sampling_handler(const char *server_name,
                                  const mcp_sampling_params_t *params,
                                  mcp_sampling_content_t *result,
                                  void *userdata);
static bool connect_sse_server(const char *name, const char *url, int timeout,
                                const mcp_root_t *roots, int root_count) {
    if (g_server_count >= MAX_MCP_SERVERS) return false;

    mcp_server_t *srv = mcp_server_new(name);
    if (!srv) return false;

    mcp_server_set_sse(srv, url);
    mcp_server_set_timeout(srv, timeout > 0 ? timeout : 120);
    mcp_server_set_connect_timeout(srv, 60);

    /* Initialize credential store path */
    if (g_credential_store_path[0] == 0) {
        hermes_get_home(g_credential_store_path, sizeof(g_credential_store_path));
        strncat(g_credential_store_path, "/mcp_auth.json",
                sizeof(g_credential_store_path) - strlen(g_credential_store_path) - 1);
    }

    int aidx = g_server_count; /* index in g_server_auth */

    /* P63: Inject auth headers for SSE transport */
    if (g_server_auth[aidx].type[0] &&
        strcmp(g_server_auth[aidx].type, "header") == 0 &&
        g_server_auth[aidx].header_name[0] &&
        g_server_auth[aidx].token[0]) {
        char hdr[1024];
        snprintf(hdr, sizeof(hdr), "%s: %s", g_server_auth[aidx].header_name,
                 g_server_auth[aidx].token);
        mcp_server_set_headers(srv, hdr);
        fprintf(stderr, "MCP: Auth header set for '%s'\\n", name);
    }

    /* P70: Set workspace roots before connect */
    if (roots && root_count > 0) {
        mcp_server_set_roots(srv, roots, root_count);
        fprintf(stderr, "MCP: Roots set for '%s' (%d roots)\\n", name, root_count);
    }

    if (!mcp_server_connect(srv)) {
        fprintf(stderr, "MCP: Failed to connect SSE server '%s': %s\\n",
                name, mcp_server_last_error(srv));
        mcp_server_free(srv);
        return false;
    }

    /* Wire sampling callback if enabled */
    if (g_server_sampling[aidx].enabled) {
        mcp_server_set_sampling_callback(srv, mcp_sampling_handler,
                                          &g_server_sampling[aidx]);
        fprintf(stderr, "MCP: Sampling callback registered for '%s'\\n", name);
    }

    g_servers[g_server_count++] = srv;
    int sidx = g_server_count - 1;

    /* Discover and register tools */
    mcp_tool_t *tools = NULL;
    int count = mcp_server_list_tools(srv, &tools);
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            char reg_name[256];
            snprintf(reg_name, sizeof(reg_name), "mcp_%s_%s", name, tools[i].name);

            if (registry_find(reg_name)) {
                fprintf(stderr, "MCP:   WARNING — tool '%s' already registered, "
                        "skipping duplicate from '%s'\\n", reg_name, name);
                continue;
            }

            /* Apply filter */
            bool filtered = false;
            if (g_server_filters[sidx].allow_count > 0) {
                bool allowed = false;
                for (int fi = 0; fi < g_server_filters[sidx].allow_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].allow[fi])) {
                        allowed = true; break;
                    }
                }
                if (!allowed) filtered = true;
            }
            if (!filtered && g_server_filters[sidx].block_count > 0) {
                for (int fi = 0; fi < g_server_filters[sidx].block_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].block[fi])) {
                        filtered = true; break;
                    }
                }
            }
            if (filtered) continue;

            registry_register(reg_name, tools[i].description,
                tools[i].input_schema[0] ? tools[i].input_schema : NULL,
                mcp_dynamic_handler);

            if (g_dynamic_count < MAX_DYNAMIC_TOOLS) {
                snprintf(g_dynamic_tools[g_dynamic_count].server_name,
                         sizeof(g_dynamic_tools[0].server_name), "%s", name);
                snprintf(g_dynamic_tools[g_dynamic_count].tool_name,
                         sizeof(g_dynamic_tools[0].tool_name), "%s", tools[i].name);
                g_dynamic_count++;
            }
            fprintf(stderr, "MCP:   Registered tool '%s' from '%s'\\n", reg_name, name);
        }
        mcp_tool_list_free(tools, count);
    }

    fprintf(stderr, "MCP: SSE server '%s' connected (%d tools)\\\\n", name, count);
    return true;
}


/* ================================================================
 *  MCP Sampling handler
 * ================================================================ */

/* Sampling callback: called when an MCP server sends sampling/createMessage.
 * Calls the LLM and returns the response to the server. */
static bool mcp_sampling_handler(const char *server_name,
                                  const mcp_sampling_params_t *params,
                                  mcp_sampling_content_t *result,
                                  void *userdata) {
    (void)server_name;
    if (!params) return false;
    mcp_sampling_cfg_t *cfg = (mcp_sampling_cfg_t *)userdata;
    if (!cfg || !cfg->enabled) return false;

    /* Build LLM config from sampling params */
    llm_config_t llm_cfg;
    memset(&llm_cfg, 0, sizeof(llm_cfg));

    /* Use override model if configured, otherwise use server's hint */
    if (cfg->model[0]) {
        snprintf(llm_cfg.model, sizeof(llm_cfg.model), "%s", cfg->model);
    } else if (params->model_preference[0]) {
        snprintf(llm_cfg.model, sizeof(llm_cfg.model), "%s", params->model_preference);
    } else {
        snprintf(llm_cfg.model, sizeof(llm_cfg.model), "%s", "default");
    }

    if (cfg->max_tokens_cap > 0)
        llm_cfg.max_tokens = cfg->max_tokens_cap;
    else if (params->max_tokens > 0)
        llm_cfg.max_tokens = params->max_tokens;
    else
        llm_cfg.max_tokens = 4096;

    if (params->temperature > 0)
        llm_cfg.temperature = (float)params->temperature;
    else
        llm_cfg.temperature = 1.0f;

    /* Parse messages from JSON array */
    message_t **msgs = NULL;
    int msg_count = 0;

    if (params->messages[0]) {
        char *jerr = NULL;
        json_t *json_msgs = json_parse(params->messages, &jerr);
        if (json_msgs) {
            size_t arr_len = json_len(json_msgs);
            if (arr_len > 0) {
                msgs = calloc(arr_len, sizeof(message_t *));
                for (size_t i = 0; i < arr_len; i++) {
                    json_t *m = json_get(json_msgs, i);
                    if (!m) continue;
                    const char *role_str = json_get_str(m, "role", "user");
                    const char *content_str = json_get_str(m, "content", "");

                    message_t *msg = calloc(1, sizeof(message_t));
                    if (strcmp(role_str, "system") == 0)
                        msg->role = MSG_SYSTEM;
                    else if (strcmp(role_str, "assistant") == 0)
                        msg->role = MSG_ASSISTANT;
                    else
                        msg->role = MSG_USER;
                    msg->content = strdup(content_str ? content_str : "");
                    msgs[msg_count++] = msg;
                }
            }
            json_free(json_msgs);
        }
        if (jerr) free(jerr);
    }

    /* Add system prompt if present */
    if (params->system_prompt[0]) {
        message_t **new_msgs = calloc(msg_count + 1, sizeof(message_t *));
        if (new_msgs) {
            new_msgs[0] = calloc(1, sizeof(message_t));
            if (new_msgs[0]) {
                new_msgs[0]->role = MSG_SYSTEM;
                new_msgs[0]->content = strdup(params->system_prompt);
            }
            for (int i = 0; i < msg_count; i++)
                new_msgs[i + 1] = msgs[i];
            free(msgs);
            msgs = new_msgs;
            msg_count++;
        }
    }

    if (msg_count == 0) {
        /* No valid messages -- send error */
        snprintf(result->role, sizeof(result->role), "assistant");
        snprintf(result->type, sizeof(result->type), "text");
        snprintf(result->text, sizeof(result->text),
                 "MCP sampling error: no valid messages");
        free(msgs);
        return false;
    }

    /* Call LLM */
    llm_response_t *resp = llm_chat_completion(&llm_cfg,
                                               (const message_t **)msgs,
                                               msg_count, NULL);

    /* Extract response */
    snprintf(result->role, sizeof(result->role), "assistant");
    snprintf(result->type, sizeof(result->type), "text");
    if (resp && resp->content[0]) {
        snprintf(result->text, sizeof(result->text), "%s", resp->content);
    } else {
        snprintf(result->text, sizeof(result->text),
                 "MCP sampling: LLM returned empty response");
    }

    if (resp) llm_response_free(resp);

    /* Free messages */
    for (int i = 0; i < msg_count; i++) {
        if (msgs[i]) {
            free(msgs[i]->content);
            free(msgs[i]);
        }
    }
    free(msgs);

    return true;
}

/* Connect an MCP server using Streamable HTTP transport */
static bool connect_http_server(const char *name, const char *url, int timeout,
                                 const mcp_root_t *roots, int root_count) {
    if (g_server_count >= MAX_MCP_SERVERS) return false;

    mcp_server_t *srv = mcp_server_new(name);
    if (!srv) return false;

    mcp_server_set_http(srv, url);
    mcp_server_set_timeout(srv, timeout > 0 ? timeout : 120);
    mcp_server_set_connect_timeout(srv, 60);

    /* Initialize credential store path */
    if (g_credential_store_path[0] == 0) {
        hermes_get_home(g_credential_store_path, sizeof(g_credential_store_path));
        strncat(g_credential_store_path, "/mcp_auth.json",
                sizeof(g_credential_store_path) - strlen(g_credential_store_path) - 1);
    }

    int aidx = g_server_count;
    /* P63: Inject auth headers for HTTP transport */
    if (g_server_auth[aidx].type[0] &&
        strcmp(g_server_auth[aidx].type, "header") == 0 &&
        g_server_auth[aidx].header_name[0] &&
        g_server_auth[aidx].token[0]) {
        char hdr[1024];
        snprintf(hdr, sizeof(hdr), "%s: %s", g_server_auth[aidx].header_name,
                 g_server_auth[aidx].token);
        mcp_server_set_headers(srv, hdr);
        fprintf(stderr, "MCP: Auth header set for '%s'\\n", name);
    }

    /* Set workspace roots before connect */
    if (roots && root_count > 0) {
        mcp_server_set_roots(srv, roots, root_count);
        fprintf(stderr, "MCP: Roots set for '%s' (%d roots)\\n", name, root_count);
    }

    if (!mcp_server_connect(srv)) {
        fprintf(stderr, "MCP: Failed to connect HTTP server '%s': %s\\n",
                name, mcp_server_last_error(srv));
        mcp_server_free(srv);
        return false;
    }

    /* Wire sampling callback if enabled */
    if (g_server_sampling[aidx].enabled) {
        mcp_server_set_sampling_callback(srv, mcp_sampling_handler,
                                          &g_server_sampling[aidx]);
        fprintf(stderr, "MCP: Sampling callback registered for '%s'\\n", name);
    }

    g_servers[g_server_count++] = srv;
    int sidx = g_server_count - 1;

    /* Discover and register tools */
    mcp_tool_t *tools = NULL;
    int count = mcp_server_list_tools(srv, &tools);
    if (count > 0 && tools) {
        int registered = 0;
                for (int i = 0; i < count; i++) {
            char reg_name[256];
            snprintf(reg_name, sizeof(reg_name), "mcp_%s_%s", name, tools[i].name);

            if (registry_find(reg_name)) {
                fprintf(stderr, "MCP:   WARNING -- tool '%s' already registered, "
                        "skipping duplicate from '%s'\n", reg_name, name);
                continue;
            }

            /* Apply filter */
            bool filtered = false;
            if (g_server_filters[sidx].allow_count > 0) {
                bool allowed = false;
                for (int fi = 0; fi < g_server_filters[sidx].allow_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].allow[fi])) {
                        allowed = true; break;
                    }
                }
                if (!allowed) filtered = true;
            }
            if (!filtered && g_server_filters[sidx].block_count > 0) {
                for (int fi = 0; fi < g_server_filters[sidx].block_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].block[fi])) {
                        filtered = true; break;
                    }
                }
            }
            if (filtered) continue;

            registry_register(reg_name, tools[i].description,
                tools[i].input_schema[0] ? tools[i].input_schema : NULL,
                mcp_dynamic_handler);

            if (g_dynamic_count < MAX_DYNAMIC_TOOLS) {
                snprintf(g_dynamic_tools[g_dynamic_count].server_name,
                         sizeof(g_dynamic_tools[0].server_name), "%s", name);
                snprintf(g_dynamic_tools[g_dynamic_count].tool_name,
                         sizeof(g_dynamic_tools[0].tool_name), "%s", tools[i].name);
                g_dynamic_count++;
            }
        }
        fprintf(stderr, "MCP: HTTP server '%s' connected (%d tools)\\n", name, registered);
        mcp_tool_list_free(tools, count);
    }

    return true;
}

/* Connect an MCP server from config */

static bool connect_stdio_server(const char *name, const char *command,
                                  char **args, int arg_count, int timeout,
                                  const mcp_root_t *roots, int root_count) {
    (void)arg_count;
    if (g_server_count >= MAX_MCP_SERVERS) return false;

    mcp_server_t *srv = mcp_server_new(name);
    if (!srv) return false;

    mcp_server_set_stdio(srv, command, args);
    mcp_server_set_timeout(srv, timeout > 0 ? timeout : 120);
    mcp_server_set_connect_timeout(srv, 60);

    /* P63: Inject auth for stdio — set env vars from auth config */
    if (g_credential_store_path[0] == 0) {
        hermes_get_home(g_credential_store_path, sizeof(g_credential_store_path));
        strncat(g_credential_store_path, "/mcp_auth.json",
                sizeof(g_credential_store_path) - strlen(g_credential_store_path) - 1);
    }

    int aidx = g_server_count; /* index in g_server_auth */
    if (g_server_auth[aidx].type[0]) {
        if (strcmp(g_server_auth[aidx].type, "env") == 0 &&
            g_server_auth[aidx].env_var[0] &&
            g_server_auth[aidx].token[0]) {
            /* Build env array: pass existing env + add API key */
            extern char **environ;
            int env_count = 0;
            while (environ[env_count]) env_count++;
            char **new_env = (char **)calloc(env_count + 2, sizeof(char *));
            for (int ei = 0; ei < env_count; ei++)
                new_env[ei] = environ[ei];

            char env_buf[640];
            snprintf(env_buf, sizeof(env_buf), "%s=%s",
                     g_server_auth[aidx].env_var, g_server_auth[aidx].token);
            new_env[env_count] = strdup(env_buf);
            new_env[env_count + 1] = NULL;
            mcp_server_set_env(srv, new_env);
        }
        /* "header" type for stdio: inject as HTTP_AUTHORIZATION env var */
        if (strcmp(g_server_auth[aidx].type, "header") == 0 &&
            g_server_auth[aidx].token[0]) {
            extern char **environ;
            int env_count = 0;
            while (environ[env_count]) env_count++;
            char **new_env = (char **)calloc(env_count + 2, sizeof(char *));
            for (int ei = 0; ei < env_count; ei++)
                new_env[ei] = environ[ei];

            /* MCP stdio servers commonly read from env. Example: OPENAI_API_KEY */
            if (g_server_auth[aidx].env_var[0]) {
                char env_buf[640];
                snprintf(env_buf, sizeof(env_buf), "%s=%s",
                         g_server_auth[aidx].env_var, g_server_auth[aidx].token);
                new_env[env_count] = strdup(env_buf);
                new_env[env_count + 1] = NULL;
                mcp_server_set_env(srv, new_env);
            }
        }
    }

    /* P70: Set workspace roots before connect */
    if (roots && root_count > 0) {
        mcp_server_set_roots(srv, roots, root_count);
        fprintf(stderr, "MCP:   Roots set for '%s' (%d roots)\\n", name, root_count);
    }

    /* C17: Wire header-based auth for SSE transport */
    if (g_server_auth[aidx].type[0] &&
        strcmp(g_server_auth[aidx].type, "header") == 0 &&
        g_server_auth[aidx].header_name[0] &&
        g_server_auth[aidx].token[0]) {
        char hdr[1024];
        snprintf(hdr, sizeof(hdr), "%s: %s", g_server_auth[aidx].header_name,
                 g_server_auth[aidx].token);
        mcp_server_set_headers(srv, hdr);
        fprintf(stderr, "MCP:   Auth header set for '%s'\\n", name);
    }

    if (!mcp_server_connect(srv)) {
        fprintf(stderr, "MCP: Failed to connect server '%s': %s\n",
                name, mcp_server_last_error(srv));
        mcp_server_free(srv);
        return false;
    }

    /* Wire sampling callback if enabled */
    if (g_server_sampling[aidx].enabled) {
        mcp_server_set_sampling_callback(srv, mcp_sampling_handler,
                                          &g_server_sampling[aidx]);
        fprintf(stderr, "MCP:   Sampling callback registered for '%s'\n", name);
    }

    g_servers[g_server_count++] = srv;
    fprintf(stderr, "MCP: Connected server '%s'\n", name);

    /* Discover and register tools */
    mcp_tool_t *tools = NULL;
    int count = mcp_server_list_tools(srv, &tools);
    if (count > 0) {
        /* Register each tool with a prefixed name: mcp_<server>_<tool>
         * P65: Check for name collisions — skip with warning if name exists */
        for (int i = 0; i < count; i++) {
            char reg_name[256];
            snprintf(reg_name, sizeof(reg_name), "mcp_%s_%s", name, tools[i].name);

            /* Collision detection: check if already registered */
            if (registry_find(reg_name)) {
                fprintf(stderr, "MCP:   WARNING — tool '%s' already registered, "
                        "skipping duplicate from '%s'\n", reg_name, name);
                continue;
            }

            /* P66: Apply per-server tool filter */
            int sidx = g_server_count - 1; /* index of current server */
            bool filtered = false;

            /* Check allowlist: if allow patterns exist, tool must match one */
            if (g_server_filters[sidx].allow_count > 0) {
                bool allowed = false;
                for (int fi = 0; fi < g_server_filters[sidx].allow_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].allow[fi])) {
                        allowed = true;
                        break;
                    }
                }
                if (!allowed) {
                    fprintf(stderr, "MCP:   Skipped tool '%s' (not in allowlist)\n",
                            tools[i].name);
                    filtered = true;
                }
            }

            /* Check blocklist: if block patterns exist, tool must not match any */
            if (!filtered && g_server_filters[sidx].block_count > 0) {
                for (int fi = 0; fi < g_server_filters[sidx].block_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].block[fi])) {
                        fprintf(stderr, "MCP:   Skipped tool '%s' (blocked by '%s')\n",
                                tools[i].name, g_server_filters[sidx].block[fi]);
                        filtered = true;
                        break;
                    }
                }
            }

            if (filtered) continue;

            registry_register(
                reg_name,
                tools[i].description,
                tools[i].input_schema[0] ? tools[i].input_schema : NULL,
                mcp_dynamic_handler
            );

            /* Store mapping for mcp_tool_call dispatch */
            if (g_dynamic_count < MAX_DYNAMIC_TOOLS) {
                snprintf(g_dynamic_tools[g_dynamic_count].server_name,
                         sizeof(g_dynamic_tools[0].server_name), "%s", name);
                snprintf(g_dynamic_tools[g_dynamic_count].tool_name,
                         sizeof(g_dynamic_tools[0].tool_name), "%s", tools[i].name);
                g_dynamic_count++;
            }

            fprintf(stderr, "MCP:   Registered tool '%s' from '%s'\n",
                    reg_name, name);
        }
        mcp_tool_list_free(tools, count);
    }

    return true;
}

/* ================================================================
 *  MCP call tool — call any MCP tool by server + tool name
 * ================================================================ */

static char *mcp_call_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON arguments\"}");

    const char *server_name = json_get_str(args, "server", "");
    const char *tool_name   = json_get_str(args, "tool", "");
    const char *tool_args   = json_get_str(args, "arguments", NULL);

    if (!server_name[0] || !tool_name[0]) {
        json_free(args);
        return strdup("{\"error\":\"server and tool fields required\"}");
    }

    /* Find server */
    mcp_server_t *srv = NULL;
    for (int i = 0; i < g_server_count; i++) {
        if (strcmp(mcp_server_name(g_servers[i]), server_name) == 0) {
            srv = g_servers[i];
            break;
        }
    }

    if (!srv) {
        json_free(args);
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\":\"Unknown MCP server: %s\"}", server_name);
        return strdup(buf);
    }

    char *result = mcp_server_call_tool(srv, tool_name, tool_args);
    json_free(args);

    if (!result) {
        const char *err = mcp_server_last_error(srv);
        char buf[1024];
        snprintf(buf, sizeof(buf), "{\"error\":\"MCP call failed: %s\"}",
                 err ? err : "unknown error");
        return strdup(buf);
    }

    return result;
}

/* ================================================================
 *  MCP status tool
 * ================================================================ */

static char *mcp_status_handler(const char *args_json, const char *task_id) {
    (void)args_json;
    (void)task_id;

    json_t *result = json_object();
    json_t *servers_arr = json_array();

    for (int i = 0; i < g_server_count; i++) {
        json_t *srv = json_object();
        json_set(srv, "name", json_string(mcp_server_name(g_servers[i])));
        json_set(srv, "connected", json_bool(mcp_server_is_connected(g_servers[i])));

        /* C15-C16: Report health + reconnection info */
        mcp_server_status_t status = mcp_server_status(g_servers[i]);
        const char *status_str = "unknown";
        if (status == MCP_STATUS_CONNECTED) status_str = "connected";
        else if (status == MCP_STATUS_CONNECTING) status_str = "connecting";
        else if (status == MCP_STATUS_DISCONNECTED) status_str = "disconnected";
        else if (status == MCP_STATUS_FAILED) status_str = "failed";
        json_set(srv, "status", json_string(status_str));
        json_set(srv, "reconnect_count", json_number((double)mcp_server_reconnect_count(g_servers[i])));
        json_set(srv, "last_error", json_string(mcp_server_last_error(g_servers[i])));

        /* C15: Run a health check ping on each server */
        if (mcp_server_is_connected(g_servers[i])) {
            bool healthy = mcp_server_health_check(g_servers[i]);
            json_set(srv, "healthy", json_bool(healthy));
        } else {
            json_set(srv, "healthy", json_bool(false));
            /* C16: Attempt reconnect if disconnected but max_retries not exhausted */
            mcp_server_reconnect(g_servers[i]);
            json_set(srv, "reconnect_attempted", json_bool(true));
            json_set(srv, "reconnected", json_bool(mcp_server_is_connected(g_servers[i])));
        }

        mcp_capabilities_t caps = mcp_server_capabilities(g_servers[i]);
        json_t *caps_obj = json_object();
        json_set(caps_obj, "tools", json_bool(caps.supports_tools));
        json_set(caps_obj, "resources", json_bool(caps.supports_resources));
        json_set(caps_obj, "prompts", json_bool(caps.supports_prompts));
        json_set(srv, "capabilities", caps_obj);

        json_append(servers_arr, srv);
    }

    json_set(result, "servers", servers_arr);
    json_set(result, "count", json_number((double)g_server_count));
    json_set(result, "dynamic_tools", json_number((double)g_dynamic_count));

    char *s = json_serialize(result);
    json_free(result);
    return s;
}

/* ================================================================
 *  Initialization — connect configured MCP servers from config
 * ================================================================ */

void mcp_init_all(void) {
    /* Read config to find mcp_servers section */
    char config_path[HERMES_PATH_MAX];
    hermes_get_home(config_path, sizeof(config_path));
    strncat(config_path, "/config.yaml", sizeof(config_path) - strlen(config_path) - 1);

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(config_path, &err);
    if (!doc) {
        if (err) {
            fprintf(stderr, "MCP: No config (%s)\n", err);
            free(err);
        }
        return;
    }

    /* Get list of server names under mcp_servers */
    /* Support both list and object config formats:
     *   mcp_servers: [ { name: "...", command: "..." } ]   OR
     *   mcp_servers: { name: { command: "..." } }          */
    size_t server_count = yaml_list_count(doc, "mcp_servers");

    /* Collect server names either from list items or map keys */
    char *dynamic_names[MAX_MCP_SERVERS];
    int dynamic_count = 0;

    if (server_count > 0) {
        /* List format: each item has a "name" key */
        for (size_t i = 0; i < server_count && dynamic_count < MAX_MCP_SERVERS; i++) {
            char name_key[256];
            snprintf(name_key, sizeof(name_key), "mcp_servers.%zu.name", i);
            const char *name = yaml_get_string(doc, name_key);
            if (name) {
                dynamic_names[dynamic_count] = strdup(name);
                if (dynamic_names[dynamic_count]) dynamic_count++;
            }
        }
    }

    /* If no list items found, try object (map) format */
    if (dynamic_count == 0) {
        size_t map_count = 0;
        char **keys = yaml_map_keys(doc, "mcp_servers", &map_count);
        if (keys && map_count > 0) {
            for (size_t i = 0; i < map_count && dynamic_count < MAX_MCP_SERVERS; i++) {
                dynamic_names[dynamic_count] = strdup(keys[i]);
                if (dynamic_names[dynamic_count]) dynamic_count++;
                free(keys[i]);
            }
            free(keys);
        }
    }

    /* Connect each discovered server */
    for (int si = 0; si < dynamic_count; si++) {
        const char *server_name = dynamic_names[si];
        (void)server_name; /* suppress null warning */
        char key[256];
        snprintf(key, sizeof(key), "mcp_servers.%s.command", dynamic_names[si]);
        const char *cmd = yaml_get_string(doc, key);
            if (!cmd) {
                /* Check for URL instead */
                snprintf(key, sizeof(key), "mcp_servers.%s.url", dynamic_names[si]);
                const char *url = yaml_get_string(doc, key);
                if (!url) continue;

                /* Check if explicit transport type is specified */
                snprintf(key, sizeof(key), "mcp_servers.%s.transport", dynamic_names[si]);
                const char *transport_type = yaml_get_string(doc, key);
                bool use_sse = (transport_type && strcmp(transport_type, "sse") == 0);

                if (use_sse) {
                    /* SSE transport — wire via HTTP/SSE */
                    connect_sse_server(dynamic_names[si], url, 120, NULL, 0);
                } else {
                    /* Default: Streamable HTTP transport (POST JSON-RPC to URL) */
                    connect_http_server(dynamic_names[si], url, 120, NULL, 0);
                }
                continue;
            }

            /* Stdio transport — parse args */
            char **args = NULL;
            int arg_count = 0;

            /* Count args */
            char args_key[256];
            snprintf(args_key, sizeof(args_key), "mcp_servers.%s.args", dynamic_names[si]);
            size_t acount = yaml_list_count(doc, args_key);
            if (acount > 0) {
                args = (char **)calloc(acount + 1, sizeof(char *));
                for (size_t ai = 0; ai < acount; ai++) {
                    const char *a = yaml_list_get(doc, args_key, ai);
                    if (a) {
                        args[arg_count++] = strdup(a);
                    }
                }
                args[arg_count] = NULL;
            }

            /* Get timeout */
            snprintf(key, sizeof(key), "mcp_servers.%s.timeout", dynamic_names[si]);
            int timeout = yaml_get_int(doc, key, 120);

            /* P63-P64: Parse auth config for this server */
            char auth_key[256];
            snprintf(auth_key, sizeof(auth_key), "mcp_servers.%s.auth", dynamic_names[si]);
            int aidx = g_server_count; /* parallel to g_servers */
            memset(&g_server_auth[aidx], 0, sizeof(mcp_auth_t));

            /* Check for auth.type in YAML */
            char auth_type_key[256];
            snprintf(auth_type_key, sizeof(auth_type_key), "%s.type", auth_key);
            const char *auth_type = yaml_get_string(doc, auth_type_key);
            if (auth_type && auth_type[0]) {
                snprintf(g_server_auth[aidx].type, sizeof(g_server_auth[aidx].type),
                         "%s", auth_type);

                /* Read auth.header_name and auth.token */
                char ah_key[256];
                snprintf(ah_key, sizeof(ah_key), "%s.header_name", auth_key);
                const char *hname = yaml_get_string(doc, ah_key);
                if (hname) snprintf(g_server_auth[aidx].header_name,
                                     sizeof(g_server_auth[aidx].header_name), "%s", hname);

                snprintf(ah_key, sizeof(ah_key), "%s.token", auth_key);
                const char *tok = yaml_get_string(doc, ah_key);
                if (tok) snprintf(g_server_auth[aidx].token,
                                   sizeof(g_server_auth[aidx].token), "%s", tok);

                snprintf(ah_key, sizeof(ah_key), "%s.env_var", auth_key);
                const char *ev = yaml_get_string(doc, ah_key);
                if (ev) snprintf(g_server_auth[aidx].env_var,
                                  sizeof(g_server_auth[aidx].env_var), "%s", ev);

                /* OAuth fields */
                snprintf(ah_key, sizeof(ah_key), "%s.client_id", auth_key);
                const char *cid = yaml_get_string(doc, ah_key);
                if (cid) snprintf(g_server_auth[aidx].client_id,
                                   sizeof(g_server_auth[aidx].client_id), "%s", cid);

                snprintf(ah_key, sizeof(ah_key), "%s.client_secret", auth_key);
                const char *csec = yaml_get_string(doc, ah_key);
                if (csec) snprintf(g_server_auth[aidx].client_secret,
                                    sizeof(g_server_auth[aidx].client_secret), "%s", csec);

                snprintf(ah_key, sizeof(ah_key), "%s.token_url", auth_key);
                const char *turl = yaml_get_string(doc, ah_key);
                if (turl) snprintf(g_server_auth[aidx].token_url,
                                    sizeof(g_server_auth[aidx].token_url), "%s", turl);

                snprintf(ah_key, sizeof(ah_key), "%s.scopes", auth_key);
                const char *sc = yaml_get_string(doc, ah_key);
                if (sc) snprintf(g_server_auth[aidx].scopes,
                                  sizeof(g_server_auth[aidx].scopes), "%s", sc);

                /* OAuth PKCE fields: authorization_url + redirect_uri */
                snprintf(ah_key, sizeof(ah_key), "%s.authorization_url", auth_key);
                const char *authz_url = yaml_get_string(doc, ah_key);
                if (authz_url) snprintf(g_server_auth[aidx].authorization_url,
                                         sizeof(g_server_auth[aidx].authorization_url), "%s", authz_url);

                snprintf(ah_key, sizeof(ah_key), "%s.redirect_uri", auth_key);
                const char *redir = yaml_get_string(doc, ah_key);
                if (redir) snprintf(g_server_auth[aidx].redirect_uri,
                                     sizeof(g_server_auth[aidx].redirect_uri), "%s", redir);

                /* OAuth refresh_before_sec */
                {
                    char rbk[256];
                    snprintf(rbk, sizeof(rbk), "%s.refresh_before_sec", auth_key);
                    g_server_auth[aidx].refresh_before_sec = yaml_get_int(doc, rbk, 60);
                }

                fprintf(stderr, "MCP: Auth enabled for '%s' (type=%s)\n",
                        dynamic_names[si], auth_type);
            }

            /* P66: Parse tool filter config for this server */
            memset(&g_server_filters[aidx], 0, sizeof(mcp_filter_t));

            /* Parse allow_tools list */
            char filter_key[256];
            snprintf(filter_key, sizeof(filter_key), "mcp_servers.%s.allow_tools",
                     dynamic_names[si]);
            size_t ac = yaml_list_count(doc, filter_key);
            if (ac > 0) {
                for (size_t fi = 0; fi < ac && g_server_filters[aidx].allow_count < MAX_FILTER_PATTERNS; fi++) {
                    const char *pat = yaml_list_get(doc, filter_key, fi);
                    if (pat) {
                        snprintf(g_server_filters[aidx].allow[
                            g_server_filters[aidx].allow_count++],
                            sizeof(g_server_filters[aidx].allow[0]), "%s", pat);
                    }
                }
                fprintf(stderr, "MCP: Filter for '%s' — allow %d patterns\n",
                        dynamic_names[si], g_server_filters[aidx].allow_count);
            }

            /* Parse block_tools list */
            snprintf(filter_key, sizeof(filter_key), "mcp_servers.%s.block_tools",
                     dynamic_names[si]);
            size_t bc = yaml_list_count(doc, filter_key);
            if (bc > 0) {
                for (size_t fi = 0; fi < bc && g_server_filters[aidx].block_count < MAX_FILTER_PATTERNS; fi++) {
                    const char *pat = yaml_list_get(doc, filter_key, fi);
                    if (pat) {
                        snprintf(g_server_filters[aidx].block[
                            g_server_filters[aidx].block_count++],
                            sizeof(g_server_filters[aidx].block[0]), "%s", pat);
                    }
                }
                fprintf(stderr, "MCP: Filter for '%s' — block %d patterns\n",
                        dynamic_names[si], g_server_filters[aidx].block_count);
            }

            /* MCP sampling config */
            memset(&g_server_sampling[aidx], 0, sizeof(mcp_sampling_cfg_t));
            g_server_sampling[aidx].timeout_sec = 30;
            {
                char smp_key[256];
                snprintf(smp_key, sizeof(smp_key), "mcp_servers.%s.sampling", dynamic_names[si]);
                g_server_sampling[aidx].enabled = yaml_get_bool(doc, smp_key, false);

                if (g_server_sampling[aidx].enabled) {
                    char sk[256];
                    snprintf(sk, sizeof(sk), "%s.model", smp_key);
                    const char *smp_model = yaml_get_string(doc, sk);
                    if (smp_model) snprintf(g_server_sampling[aidx].model,
                                            sizeof(g_server_sampling[aidx].model), "%s", smp_model);

                    snprintf(sk, sizeof(sk), "%s.max_tokens_cap", smp_key);
                    g_server_sampling[aidx].max_tokens_cap = yaml_get_int(doc, sk, 0);

                    snprintf(sk, sizeof(sk), "%s.timeout", smp_key);
                    g_server_sampling[aidx].timeout_sec = yaml_get_int(doc, sk, 30);

                    snprintf(sk, sizeof(sk), "%s.max_rpm", smp_key);
                    g_server_sampling[aidx].max_rpm = yaml_get_int(doc, sk, 0);

                    fprintf(stderr, "MCP: Sampling enabled for '%s' (model=%s)\n",
                            dynamic_names[si],
                            g_server_sampling[aidx].model[0] ? g_server_sampling[aidx].model : "default");
                }
            }

            /* P70: Parse root directories config */
            char root_key[256];
            mcp_root_t roots[MAX_MCP_ROOTS];
            int root_count = 0;

            snprintf(root_key, sizeof(root_key), "mcp_servers.%s.roots",
                     dynamic_names[si]);
            size_t roots_in_yaml = yaml_list_count(doc, root_key);
            if (roots_in_yaml > 0) {
                for (size_t ri = 0; ri < roots_in_yaml && root_count < MAX_MCP_ROOTS; ri++) {
                    const char *root_path = yaml_list_get(doc, root_key, ri);
                    if (root_path && root_path[0]) {
                        if (strncmp(root_path, "file://", 7) == 0) {
                            snprintf(roots[root_count].uri, sizeof(roots[0].uri),
                                     "%s", root_path);
                        } else {
                            snprintf(roots[root_count].uri, sizeof(roots[0].uri),
                                     "file://%s", root_path);
                        }
                        snprintf(roots[root_count].name, sizeof(roots[0].name),
                                 "%s-root", dynamic_names[si]);
                        root_count++;
                    }
                }
                fprintf(stderr, "MCP: Roots for '%s' — %d directories\n",
                        dynamic_names[si], root_count);
            }

            /* P71: OSV malware check — block known malicious MCP packages */
            if (cmd) {
                char *osv_err = osv_check_package_for_malware(cmd, (const char **)args, arg_count);
                if (osv_err) {
                    fprintf(stderr, "MCP: BLOCKED '%s': %s\n", dynamic_names[si], osv_err);
                    free(osv_err);
                    goto skip_server;
                }
            }

            connect_stdio_server(dynamic_names[si], cmd, args, arg_count, timeout,
                                 root_count > 0 ? roots : NULL, root_count);

        /* Free dynamic server names */
        free(dynamic_names[si]);
        continue;

    skip_server:
        /* Cleanup when server is skipped (OSV block, auth error, etc.) */
        for (int ai = 0; ai < arg_count; ai++) free(args[ai]);
        free(args);
        free(dynamic_names[si]);
    }

    yaml_free(doc);
}

/* ================================================================
 *  P64: Credential store — load/save MCP auth tokens
 * ================================================================ */

/* Load stored credentials from mcp_auth.json.
 * Format: { "server_name": { "access_token": "...", "expires_at": 1234567890 } }
 * Returns true if file loaded successfully (may be empty). */
static bool credential_store_load(const char *server_name, char *token_out,
                                   size_t token_sz, long long *expires_at) {
    if (!g_credential_store_path[0]) return false;

    FILE *f = fopen(g_credential_store_path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);
    if (fsize <= 0) { fclose(f); return false; }

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return false; }

    size_t nread = fread(buf, 1, (size_t)fsize, f);
    buf[nread] = '\0';
    fclose(f);

    char *jerr = NULL;
    json_t *root = json_parse(buf, &jerr);
    free(buf);
    if (jerr) { free(jerr); return false; }

    json_t *srv_entry = json_obj_get(root, server_name);
    if (!srv_entry) { json_free(root); return false; }

    const char *tok = json_get_str(srv_entry, "access_token", "");
    if (tok[0]) {
        snprintf(token_out, token_sz, "%s", tok);
    }
    if (expires_at) {
        *expires_at = (long long)json_get_num(srv_entry, "expires_at", 0.0);
    }

    json_free(root);
    return tok[0] != '\0';
}

/* Save credentials for a server to mcp_auth.json */
static bool credential_store_save(const char *server_name,
                                   const char *access_token,
                                   long long expires_at) {
    if (!g_credential_store_path[0]) return false;

    /* Load existing store, update entry, save */
    json_t *root = NULL;

    FILE *f = fopen(g_credential_store_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        rewind(f);
        if (fsize > 0) {
            char *buf = (char *)malloc((size_t)fsize + 1);
            if (buf) {
                size_t nread = fread(buf, 1, (size_t)fsize, f);
                buf[nread] = '\0';
                char *jerr = NULL;
                root = json_parse(buf, &jerr);
                free(buf);
                if (jerr) free(jerr);
            }
        }
        fclose(f);
    }

    if (!root) root = json_object();

    /* Update or create entry for this server */
    json_t *entry = json_obj_get(root, server_name);
    if (!entry) {
        entry = json_object();
        json_set(root, server_name, entry);
    }

    json_set(entry, "access_token", json_string(access_token));
    json_set(entry, "expires_at", json_number((double)expires_at));

    char *serialized = json_serialize(root);
    json_free(root);

    if (!serialized) return false;

    /* Write atomically */
    char tmp_path[HERMES_PATH_MAX + 8];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_credential_store_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) { free(serialized); return false; }
    fwrite(serialized, 1, strlen(serialized), out);
    fclose(out);
    free(serialized);

    rename(tmp_path, g_credential_store_path);
    return true;
}

/* ================================================================
 *  P64: OAuth token refresh
 * ================================================================ */

/* Perform OAuth client_credentials grant to get a new token.
 * Returns malloc'd access_token on success, NULL on error. */
static char *oauth_refresh_token(mcp_auth_t *auth) {
    if (!auth || !auth->token_url[0] || !auth->client_id[0]) {
        fprintf(stderr, "MCP OAuth: missing token_url or client_id\n");
        return NULL;
    }

    /* Build POST body: client_id=...&client_secret=...&grant_type=client_credentials */
    char body[2048];
    int n = snprintf(body, sizeof(body),
                     "grant_type=client_credentials&client_id=%s",
                     auth->client_id);
    if (auth->client_secret[0]) {
        n += snprintf(body + n, sizeof(body) - (size_t)n,
                      "&client_secret=%s", auth->client_secret);
    }
    if (auth->scopes[0]) {
        n += snprintf(body + n, sizeof(body) - (size_t)n,
                      "&scope=%s", auth->scopes);
    }

    http_t *h = http_new(30);
    if (!h) return NULL;

    http_resp_t *resp = http_post_json(h, auth->token_url, body);
    if (!resp) {
        fprintf(stderr, "MCP OAuth: HTTP request failed\n");
        http_free(h);
        return NULL;
    }

    if (resp->status < 200 || resp->status >= 300) {
        fprintf(stderr, "MCP OAuth: HTTP %d from token endpoint\n", resp->status);
        http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    char *jerr = NULL;
    json_t *json = json_parse(resp->body ? resp->body : "", &jerr);
    http_resp_free(resp);
    http_free(h);
    if (jerr) { free(jerr); return NULL; }

    const char *access_token = json_get_str(json, "access_token", "");
    char *result = NULL;
    if (access_token[0]) {
        result = strdup(access_token);
        /* Save to credential store */
        long long expires_in = (long long)json_get_num(json, "expires_in", 3600.0);
        long long expires_at = (long long)time(NULL) + expires_in;
        credential_store_save(auth->client_id, access_token, expires_at);
    }

    json_free(json);
    return result;
}

/* Refresh token for a specific server if it's near expiry */
static bool mcp_auth_refresh_if_needed(int server_idx) {
    if (server_idx < 0 || server_idx >= g_server_count) return false;
    if (strcmp(g_server_auth[server_idx].type, "oauth") != 0) return false;

    const char *srv_name = mcp_server_name(g_servers[server_idx]);
    if (!srv_name) return false;

    /* Use libmcp_oauth storage for token persistence */
    mcp_oauth_storage_t *oauth_st = mcp_oauth_storage_new(srv_name);
    if (!oauth_st) {
        /* Fall back to credential_store if storage alloc fails */
        char stored_token[1024] = "";
        long long expires_at = 0;
        if (credential_store_load(srv_name, stored_token, sizeof(stored_token), &expires_at)) {
            long long now = (long long)time(NULL);
            int refresh_before = g_server_auth[server_idx].refresh_before_sec;
            if (refresh_before <= 0) refresh_before = 60;
            if (expires_at > 0 && (now + refresh_before) < expires_at) {
                snprintf(g_server_auth[server_idx].token, sizeof(g_server_auth[server_idx].token),
                         "%s", stored_token);
                return true;
            }
        }
        char *new_token = oauth_refresh_token(&g_server_auth[server_idx]);
        if (new_token) {
            snprintf(g_server_auth[server_idx].token, sizeof(g_server_auth[server_idx].token),
                     "%s", new_token);
            free(new_token);
            return true;
        }
        return false;
    }

    /* Check stored tokens via libmcp_oauth */
    if (mcp_oauth_storage_has_tokens(oauth_st)) {
        char *tokens_json = mcp_oauth_storage_get_tokens(oauth_st);
        if (tokens_json) {
            char *jerr = NULL;
            json_t *tokens = json_parse(tokens_json, &jerr);
            free(tokens_json);
            if (jerr) {
                free(jerr);
            }
            if (tokens) {
                long long expires_at = (long long)json_get_num(tokens, "expires_at", 0);
                const char *access_token = json_get_str(tokens, "access_token", "");
                long long now = (long long)time(NULL);
                int refresh_before = g_server_auth[server_idx].refresh_before_sec;
                if (refresh_before <= 0) refresh_before = 60;

                if (expires_at > 0 && (now + refresh_before) < expires_at && access_token[0]) {
                    /* Token still valid */
                    snprintf(g_server_auth[server_idx].token, sizeof(g_server_auth[server_idx].token),
                             "%s", access_token);
                    json_free(tokens);
                    mcp_oauth_storage_free(oauth_st);
                    return true;
                }
                json_free(tokens);
            }
        }
    }

    /* Need to refresh */
    char *new_token = oauth_refresh_token(&g_server_auth[server_idx]);
    if (new_token) {
        snprintf(g_server_auth[server_idx].token, sizeof(g_server_auth[server_idx].token),
                 "%s", new_token);

        /* Save to libmcp_oauth storage — wrap in JSON with expires_at */
        json_t *tj = json_object();
        json_set(tj, "access_token", json_string(new_token));
        long long expires_in = 3600;
        /* Parse expires_in from HTTP response if available */
        /* For client_credentials, the refresh already handles this */
        long long expires_at = (long long)time(NULL) + expires_in;
        json_set(tj, "expires_at", json_number((double)expires_at));
        char *token_json = json_serialize(tj);
        if (token_json) {
            mcp_oauth_storage_set_tokens(oauth_st, token_json);
            free(token_json);
        }
        json_free(tj);

        free(new_token);
        mcp_oauth_storage_free(oauth_st);
        return true;
    }

    mcp_oauth_storage_free(oauth_st);
    return false;
}

/* ================================================================
 *  P64: mcp_auth_reconfigure tool handler
 * ================================================================ */

static char *mcp_auth_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    json_t *args = json_parse(args_json, NULL);
    if (!args) {
        return strdup("{\"error\":\"Invalid JSON arguments\"}");
    }

    const char *action = json_get_str(args, "action", "status");
    const char *server = json_get_str(args, "server", "");
    const char *token  = json_get_str(args, "token", "");

    json_t *result = json_object();

    if (strcmp(action, "status") == 0) {
        json_t *servers_arr = json_array();
        for (int i = 0; i < g_server_count; i++) {
            json_t *srv = json_object();
            json_set(srv, "name", json_string(mcp_server_name(g_servers[i])));
            json_set(srv, "auth_type", json_string(g_server_auth[i].type[0]
                        ? g_server_auth[i].type : "none"));
            json_set(srv, "connected",
                     json_bool(mcp_server_is_connected(g_servers[i])));
            json_append(servers_arr, srv);
        }
        json_set(result, "servers", servers_arr);
        json_set(result, "credential_store",
                 json_string(g_credential_store_path[0]
                             ? g_credential_store_path : "not initialized"));

    } else if (strcmp(action, "refresh") == 0) {
        if (!server[0]) {
            /* Refresh all oauth servers */
            int refreshed = 0;
            for (int i = 0; i < g_server_count; i++) {
                if (strcmp(g_server_auth[i].type, "oauth") == 0) {
                    if (mcp_auth_refresh_if_needed(i)) refreshed++;
                }
            }
            json_set(result, "refreshed", json_number((double)refreshed));
        } else {
            /* Find server by name */
            int idx = -1;
            for (int i = 0; i < g_server_count; i++) {
                if (strcmp(mcp_server_name(g_servers[i]), server) == 0) {
                    idx = i; break;
                }
            }
            if (idx < 0) {
                json_set(result, "error", json_string("Server not found"));
            } else if (strcmp(g_server_auth[idx].type, "oauth") != 0) {
                json_set(result, "error", json_string("Server does not use OAuth"));
            } else {
                bool ok = mcp_auth_refresh_if_needed(idx);
                json_set(result, "refreshed", json_bool(ok));
            }
        }
    } else if (strcmp(action, "set_token") == 0) {
        if (!server[0] || !token[0]) {
            json_set(result, "error",
                     json_string("server and token fields required"));
        } else {
            /* Store token and optionally inject into a running server */
            int idx = -1;
            for (int i = 0; i < g_server_count; i++) {
                if (strcmp(mcp_server_name(g_servers[i]), server) == 0) {
                    idx = i; break;
                }
            }
            if (idx >= 0) {
                snprintf(g_server_auth[idx].token, sizeof(g_server_auth[0].token),
                         "%s", token);
                /* Also save to credential store */
                credential_store_save(server, token, 0);
            } else {
                /* Save for future server connections */
                credential_store_save(server, token, 0);
                /* Also set a placeholder auth entry */
                for (int i = 0; i < MAX_MCP_SERVERS; i++) {
                    if (!g_server_auth[i].type[0]) {
                        snprintf(g_server_auth[i].type, sizeof(g_server_auth[i].type),
                                 "header");
                        snprintf(g_server_auth[i].token, sizeof(g_server_auth[0].token),
                                 "%s", token);
                        if (!g_server_auth[i].header_name[0])
                            snprintf(g_server_auth[i].header_name,
                                     sizeof(g_server_auth[i].header_name),
                                     "Authorization");
                        break;
                    }
                }
            }
            json_set(result, "stored", json_bool(true));
        }
    } else {
        json_set(result, "error", json_string("Unknown action (use: status, refresh, set_token)"));
    }

    char *s = json_serialize(result);
    json_free(result);
    json_free(args);
    return s;
}

/* ================================================================
 *  P67: MCP resource handler
 * ================================================================ */

static char *mcp_resource_list_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    (void)args; /* no params yet, could add server filter */

    json_t *result = json_object();
    json_t *resources_arr = json_array();

    for (int si = 0; si < g_server_count; si++) {
        mcp_server_t *srv = g_servers[si];
        if (!mcp_server_is_connected(srv)) continue;
        if (!mcp_server_capabilities(srv).supports_resources) continue;

        mcp_resource_t *resources = NULL;
        int count = mcp_server_list_resources(srv, &resources);
        if (count <= 0) continue;

        for (int ri = 0; ri < count; ri++) {
            json_t *r = json_object();
            json_set(r, "server", json_string(mcp_server_name(srv)));
            json_set(r, "uri", json_string(resources[ri].uri));
            json_set(r, "name", json_string(resources[ri].name));
            if (resources[ri].description[0])
                json_set(r, "description", json_string(resources[ri].description));
            if (resources[ri].mime_type[0])
                json_set(r, "mimeType", json_string(resources[ri].mime_type));
            json_append(resources_arr, r);
        }

        mcp_resource_list_free(resources, count);
    }

    json_set(result, "resources", resources_arr);
    json_set(result, "count", json_number((double)json_len(resources_arr)));

    char *s = json_serialize(result);
    json_free(result);
    if (args) json_free(args);
    return s;
}

static char *mcp_resource_read_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON arguments\"}");

    const char *server_name = json_get_str(args, "server", "");
    const char *uri = json_get_str(args, "uri", "");

    if (!server_name[0] || !uri[0]) {
        json_free(args);
        return strdup("{\"error\":\"server and uri fields required\"}");
    }

    /* Find server */
    mcp_server_t *srv = NULL;
    for (int i = 0; i < g_server_count; i++) {
        if (strcmp(mcp_server_name(g_servers[i]), server_name) == 0) {
            srv = g_servers[i];
            break;
        }
    }

    if (!srv) {
        json_free(args);
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\":\"Unknown MCP server: %s\"}", server_name);
        return strdup(buf);
    }

    mcp_resource_content_t *content = mcp_server_read_resource(srv, uri);
    if (!content) {
        const char *err = mcp_server_last_error(srv);
        json_free(args);
        char buf[1024];
        snprintf(buf, sizeof(buf), "{\"error\":\"Read failed: %s\"}",
                 err ? err : "unknown error");
        return strdup(buf);
    }

    json_t *result = json_object();
    json_set(result, "uri", json_string(content->uri));
    if (content->mime_type[0])
        json_set(result, "mimeType", json_string(content->mime_type));
    if (content->is_text && content->text)
        json_set(result, "text", json_string(content->text));
    else if (content->blob) {
        /* Include warning that this is base64-encoded binary */
        json_set(result, "blob", json_string((const char *)content->blob));
        json_set(result, "isBinary", json_bool(true));
    }

    mcp_resource_content_free(content);
    char *s = json_serialize(result);
    json_free(result);
    json_free(args);
    return s;
}

/* ================================================================
 *  P69: MCP prompt handlers
 * ================================================================ */

static char *mcp_prompt_list_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    (void)args_json;

    json_t *result = json_object();
    json_t *prompts_arr = json_array();

    for (int si = 0; si < g_server_count; si++) {
        mcp_server_t *srv = g_servers[si];
        if (!mcp_server_is_connected(srv)) continue;
        if (!mcp_server_capabilities(srv).supports_prompts) continue;

        mcp_prompt_t *prompts = NULL;
        int count = mcp_server_list_prompts(srv, &prompts);
        if (count <= 0) continue;

        for (int pi = 0; pi < count; pi++) {
            json_t *p = json_object();
            json_set(p, "server", json_string(mcp_server_name(srv)));
            json_set(p, "name", json_string(prompts[pi].name));
            if (prompts[pi].description[0])
                json_set(p, "description", json_string(prompts[pi].description));
            if (prompts[pi].arguments_schema[0])
                json_set(p, "arguments", json_string(prompts[pi].arguments_schema));
            json_append(prompts_arr, p);
        }

        mcp_prompt_list_free(prompts, count);
    }

    json_set(result, "prompts", prompts_arr);
    json_set(result, "count", json_number((double)json_len(prompts_arr)));

    char *s = json_serialize(result);
    json_free(result);
    return s;
}

static char *mcp_prompt_get_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"Invalid JSON arguments\"}");

    const char *server_name = json_get_str(args, "server", "");
    const char *prompt_name = json_get_str(args, "name", "");

    if (!server_name[0] || !prompt_name[0]) {
        json_free(args);
        return strdup("{\"error\":\"server and name fields required\"}");
    }

    /* Find server */
    mcp_server_t *srv = NULL;
    for (int i = 0; i < g_server_count; i++) {
        if (strcmp(mcp_server_name(g_servers[i]), server_name) == 0) {
            srv = g_servers[i];
            break;
        }
    }

    if (!srv) {
        json_free(args);
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\":\"Unknown MCP server: %s\"}", server_name);
        return strdup(buf);
    }

    /* Get prompt arguments as JSON string (if any) */
    json_t *prompt_args = json_obj_get(args, "arguments");
    char *args_str = NULL;
    if (prompt_args) {
        args_str = json_serialize(prompt_args);
    }

    char *result = mcp_server_get_prompt(srv, prompt_name, args_str);
    free(args_str);
    json_free(args);

    if (!result) {
        const char *err = mcp_server_last_error(srv);
        char buf[1024];
        snprintf(buf, sizeof(buf), "{\"error\":\"Get prompt failed: %s\"}",
                 err ? err : "unknown error");
        return strdup(buf);
    }

    return result;
}

/* ================================================================
 *  Registry registration
 * ================================================================ */

void registry_init_mcp(void) {
    /* MCP status: list connected servers and capabilities */
    registry_register(
        "mcp_status",
        "Show connected MCP (Model Context Protocol) servers and "
        "their capabilities. Lists all configured MCP servers.",
        "{\"type\":\"object\",\"properties\":{}}",
        mcp_status_handler
    );

    /* MCP call: call any tool on any connected MCP server */
    registry_register(
        "mcp_tool_call",
        "Call a tool on a connected MCP server. "
        "Use mcp_status first to discover available servers and tools. "
        "Pass server name, tool name, and tool arguments.",
        "{"
          "\"type\":\"object\",\"properties\":{"
            "\"server\":{\"type\":\"string\","
              "\"description\":\"MCP server name (from mcp_status)\"},"
            "\"tool\":{\"type\":\"string\","
              "\"description\":\"Tool name on the MCP server\"},"
            "\"arguments\":{\"type\":\"object\","
              "\"description\":\"Tool-specific arguments\"}"
          "},"
          "\"required\":[\"server\",\"tool\"]"
        "}",
        mcp_call_handler
    );

    registry_set_timeout("mcp_tool_call", 180);

    /* P64: MCP auth management tool */
    registry_register(
        "mcp_auth",
        "Manage MCP server authentication. "
        "Actions: status (show auth state), "
        "refresh [server] (renew OAuth tokens), "
        "set_token server=<name> token=<value> (store API key or Bearer token).",
        "{"
          "\"type\":\"object\",\"properties\":{"
            "\"action\":{\"type\":\"string\","
              "\"enum\":[\"status\",\"refresh\",\"set_token\"],"
              "\"description\":\"Action to perform\"},"
            "\"server\":{\"type\":\"string\","
              "\"description\":\"MCP server name\"},"
            "\"token\":{\"type\":\"string\","
              "\"description\":\"Token value (for set_token action)\"}"
          "},"
          "\"required\":[\"action\"]"
        "}",
        mcp_auth_handler
    );

    /* P67: MCP resource tools */
    registry_register(
        "mcp_resource_list",
        "List all available resources from connected MCP servers. "
        "Returns resource URIs, names, descriptions, and MIME types.",
        "{\"type\":\"object\",\"properties\":{}}",
        mcp_resource_list_handler
    );

    registry_register(
        "mcp_resource_read",
        "Read the content of a specific resource from an MCP server. "
        "Use mcp_resource_list first to discover available URIs.",
        "{"
          "\"type\":\"object\",\"properties\":{"
            "\"server\":{\"type\":\"string\","
              "\"description\":\"MCP server name\"},"
            "\"uri\":{\"type\":\"string\","
              "\"description\":\"Resource URI to read\"}"
          "},"
          "\"required\":[\"server\",\"uri\"]"
        "}",
        mcp_resource_read_handler
    );

    registry_set_timeout("mcp_resource_read", 60);

    /* P69: MCP prompt tools */
    registry_register(
        "mcp_prompt_list",
        "List all available prompt templates from connected MCP servers. "
        "Returns prompt names, descriptions, and argument schemas.",
        "{\"type\":\"object\",\"properties\":{}}",
        mcp_prompt_list_handler
    );

    registry_register(
        "mcp_prompt_get",
        "Get a specific prompt template from an MCP server. "
        "Use mcp_prompt_list first to discover available prompts.",
        "{"
          "\"type\":\"object\",\"properties\":{"
            "\"server\":{\"type\":\"string\","
              "\"description\":\"MCP server name\"},"
            "\"name\":{\"type\":\"string\","
              "\"description\":\"Prompt template name\"},"
            "\"arguments\":{\"type\":\"object\","
              "\"description\":\"Optional arguments for the prompt template\"}"
          "},"
          "\"required\":[\"server\",\"name\"]"
        "}",
        mcp_prompt_get_handler
    );
}
