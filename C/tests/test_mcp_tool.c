/*
 * test_mcp_tool.c — Registration and basic response tests for MCP tool.
 * Provides stubs for libmcp functions with safe defaults,
 * then includes the source under test.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ================================================================
 *  Stub types matching mcp.h
 * ================================================================ */
typedef struct mcp_server mcp_server_t;
typedef enum { MCP_STATUS_CONNECTED, MCP_STATUS_CONNECTING,
               MCP_STATUS_DISCONNECTED, MCP_STATUS_FAILED } mcp_server_status_t;
typedef struct { void *data; size_t len; const char *mime_type; } mcp_resource_content_t;

/* ================================================================
 *  Stub globals referenced by mcp_tool.c
 * ================================================================ */
mcp_server_t *g_servers[64];
int g_server_count = 0;

/* ================================================================
 *  Stub implementations (return safe defaults)
 * ================================================================ */
const char *mcp_server_name(mcp_server_t *s) { (void)s; return ""; }
bool mcp_server_is_connected(mcp_server_t *s) { (void)s; return false; }
mcp_server_status_t mcp_server_status(mcp_server_t *s) { (void)s; return MCP_STATUS_DISCONNECTED; }
int mcp_server_reconnect_count(mcp_server_t *s) { (void)s; return 0; }
const char *mcp_server_last_error(mcp_server_t *s) { (void)s; return ""; }
bool mcp_server_health_check(mcp_server_t *s) { (void)s; return false; }
int mcp_server_try_reconnect(mcp_server_t *s) { (void)s; return -1; }
mcp_server_t *mcp_server_by_name(const char *name) { (void)name; return NULL; }
int mcp_call_tool(mcp_server_t *srv, const char *tool_name,
                  const char *args_json, char **result_out) {
    (void)srv; (void)tool_name; (void)args_json; (void)result_out; return -1; }
char *mcp_list_tools(mcp_server_t *srv) { (void)srv; return strdup("[]"); }
char *mcp_list_resources(mcp_server_t *srv) { (void)srv; return strdup("[]"); }
char *mcp_read_resource(mcp_server_t *srv, const char *uri) {
    (void)srv; (void)uri; return strdup("{\"error\":\"no server\"}"); }
mcp_resource_content_t *mcp_read_resource_blob(mcp_server_t *srv, const char *uri) {
    (void)srv; (void)uri; return NULL; }
void mcp_resource_content_free(mcp_resource_content_t *c) { (void)c; }
char *mcp_list_prompts(mcp_server_t *srv) { (void)srv; return strdup("[]"); }
char *mcp_get_prompt(mcp_server_t *srv, const char *name, const char *args_json) {
    (void)srv; (void)name; (void)args_json; return strdup("{}"); }

/* Stubs for mcp_oauth */
typedef struct mcp_oauth_storage mcp_oauth_storage_t;
mcp_oauth_storage_t *mcp_oauth_storage_new(const char *name) { (void)name; return NULL; }
void mcp_oauth_storage_free(mcp_oauth_storage_t *s) { (void)s; }
bool mcp_oauth_storage_has_tokens(mcp_oauth_storage_t *s) { (void)s; return false; }
char *mcp_oauth_storage_get_tokens(mcp_oauth_storage_t *s) { (void)s; return NULL; }
void mcp_oauth_storage_set_tokens(mcp_oauth_storage_t *s, const char *json) {
    (void)s; (void)json; }

/* Stubs for credential */
int credential_store_save(const char *key, const char *token, long long expires_at) {
    (void)key; (void)token; (void)expires_at; return 0; }
bool credential_store_load(const char *key, char *out, size_t out_sz, long long *expires_at) {
    (void)key; (void)out; (void)out_sz; (void)expires_at; return false; }

/* Stubs for mcp server */
#include "hermes_json.h"
typedef void (*mcp_sampling_callback_t)(const char *, const char *, void *);
void mcp_server_set_sampling_callback(mcp_server_t *srv, mcp_sampling_callback_t cb, void *ud) {
    (void)srv; (void)cb; (void)ud; }

/* Now include the source under test */
#include "../src/tools/mcp_tool.c"

/* Test counters */
static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [L%d]: %s\n", __LINE__, msg); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

#define ASSERT_CONTAINS(str, substr, msg) do { \
    if (!strstr(str, substr)) { \
        fprintf(stderr, "  FAIL [L%d]: %s — expected '%s' in result\n", __LINE__, msg, substr); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

int main(void) {
    printf("=== MCP Tool Tests ===\n");

    /* Test 1: mcp_status_handler with no servers returns empty array */
    {
        char *res = mcp_status_handler("{}", "");
        ASSERT(res != NULL, "status handler returns non-NULL");
        ASSERT_CONTAINS(res, "\"servers\"", "status response has servers field");
        ASSERT_CONTAINS(res, "\"count\":0", "status response shows 0 servers");
        json_free(res); /* Using json_free from included source */
    }

    /* Test 2: mcp_status_handler with null args still works */
    {
        char *res = mcp_status_handler(NULL, "");
        ASSERT(res != NULL, "status handler null args returns non-NULL");
        ASSERT_CONTAINS(res, "\"servers\"", "status response has servers field");
        json_free(res);
    }

    /* Test 3: mcp_resource_list_handler with no servers */
    {
        char *res = mcp_resource_list_handler("{}", "");
        ASSERT(res != NULL, "resource list returns non-NULL");
        ASSERT_CONTAINS(res, "\"success\"", "resource list has success field");
        json_free(res);
    }

    /* Test 4: mcp_prompt_list_handler with no servers */
    {
        char *res = mcp_prompt_list_handler("{}", "");
        ASSERT(res != NULL, "prompt list returns non-NULL");
        ASSERT_CONTAINS(res, "\"success\"", "prompt list has success field");
        json_free(res);
    }

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
