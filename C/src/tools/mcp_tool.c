/*
 * mcp_tool.c — P56-P58: MCP tool integration for Hermes C.
 *
 * Wires libmcp into the tool registry. Manages MCP server lifecycle
 * and exposes discovered MCP tools as agent-callable tools.
 *
 * Currently supports stdio transport. SSE transport (P62) pending.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  State
 * ================================================================ */

#define MAX_MCP_SERVERS 16

static mcp_server_t *g_servers[MAX_MCP_SERVERS];
static int g_server_count = 0;

/* ================================================================
 *  MCP management tool
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

    char *s = json_serialize(result);
    json_free(result);
    return s;
}

/* ================================================================
 *  Initialization — connect configured MCP servers
 * ================================================================ */

void mcp_init_all(void) {
    /* In a full implementation, this reads mcp_servers from config.yaml
     * and connects each one. For now, register the management tool. */

    /* Future: parse config.yaml mcp_servers section, create + connect servers.
     * For each server with "command" key, call
     *   mcp_server_set_stdio(srv, cmd, args);
     * For each with "url" key, call
     *   mcp_server_set_sse(srv, url);
     * Then mcp_server_connect(srv) and mcp_server_list_tools() to register.
     */
}

/* ================================================================
 *  Registry registration
 * ================================================================ */

void registry_init_mcp(void) {
    registry_register(
        "mcp_status",
        "Show connected MCP (Model Context Protocol) servers and their capabilities. "
        "Lists all configured MCP servers and their available tools.",
        "{\"type\":\"object\",\"properties\":{}}",
        mcp_status_handler
    );
}
