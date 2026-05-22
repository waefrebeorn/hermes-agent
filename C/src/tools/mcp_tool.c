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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  State
 * ================================================================ */

#define MAX_MCP_SERVERS 16
#define MAX_DYNAMIC_TOOLS 256

static mcp_server_t *g_servers[MAX_MCP_SERVERS];
static int g_server_count = 0;

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

static bool connect_stdio_server(const char *name, const char *command,
                                  char **args, int timeout) {
    if (g_server_count >= MAX_MCP_SERVERS) return false;

    mcp_server_t *srv = mcp_server_new(name);
    if (!srv) return false;

    mcp_server_set_stdio(srv, command, args);
    mcp_server_set_timeout(srv, timeout > 0 ? timeout : 120);
    mcp_server_set_connect_timeout(srv, 60);

    if (!mcp_server_connect(srv)) {
        fprintf(stderr, "MCP: Failed to connect server '%s': %s\n",
                name, mcp_server_last_error(srv));
        mcp_server_free(srv);
        return false;
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
    /* yaml has limited iteration — try known server names by checking
     * mcp_servers.<name>.command or mcp_servers.<name>.url.
     * For now, iterate through potential indices. */
    size_t server_count = yaml_list_count(doc, "mcp_servers");

    if (server_count == 0) {
        /* Try checking if mcp_servers is an object (map) vs list.
         * The Python config uses: mcp_servers: { name: { command: ... } }
         * We iterate by checking common prefixes. */
        const char *known_servers[] = {"filesystem", "github", "searxng",
                                        "slack", "git", "puppeteer",
                                        "playwright", "memory", NULL};

        /* Check each known server name */
        for (int si = 0; known_servers[si]; si++) {
            char key[256];
            snprintf(key, sizeof(key), "mcp_servers.%s.command", known_servers[si]);
            const char *cmd = yaml_get_string(doc, key);
            if (!cmd) {
                /* Check for URL instead */
                snprintf(key, sizeof(key), "mcp_servers.%s.url", known_servers[si]);
                const char *url = yaml_get_string(doc, key);
                if (!url) continue;

                /* SSE transport — not yet implemented */
                fprintf(stderr, "MCP: Server '%s' uses SSE transport (not yet supported)\n",
                        known_servers[si]);
                continue;
            }

            /* Stdio transport — parse args */
            char **args = NULL;
            int arg_count = 0;

            /* Count args */
            char args_key[256];
            snprintf(args_key, sizeof(args_key), "mcp_servers.%s.args", known_servers[si]);
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
            snprintf(key, sizeof(key), "mcp_servers.%s.timeout", known_servers[si]);
            int timeout = yaml_get_int(doc, key, 120);

            connect_stdio_server(known_servers[si], cmd, args, timeout);
        }
    }

    yaml_free(doc);
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
}
