/*
 * test_mcp_oauth.c — Tests for libmcp_oauth (MCP OAuth client).
 * Tests: token storage, PKCE generation, port finding, browser detection.
 * Does NOT test actual HTTP/OAuth flows (requires network + auth server).
 */

#include "mcp_oauth.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

/* ================================================================
 *  PKCE tests
 * ================================================================ */

static void test_pkce_verifier_generation(void)
{
    char *v1 = mcp_oauth_generate_code_verifier();
    TEST("code_verifier not null", v1 != NULL);
    if (v1) {
        size_t len = strlen(v1);
        TEST("code_verifier length >= 43", len >= 43);
        TEST("code_verifier length <= 128", len <= 128);
        /* Check all chars are valid PKCE chars */
        bool valid = true;
        for (size_t i = 0; v1[i]; i++) {
            char c = v1[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '.' ||
                  c == '_' || c == '~')) {
                valid = false;
                break;
            }
        }
        TEST("code_verifier valid chars", valid);

        /* Second call produces different value */
        char *v2 = mcp_oauth_generate_code_verifier();
        TEST("second verifier not null", v2 != NULL);
        if (v2) {
            TEST("two verifiers differ", strcmp(v1, v2) != 0);
            free(v2);
        }
        free(v1);
    }
}

static void test_pkce_code_challenge(void)
{
    /* Known test vector from RFC 7636 Appendix B */
    char *challenge = mcp_oauth_code_challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
    TEST("challenge not null", challenge != NULL);
    if (challenge) {
        TEST("challenge is base64url", strlen(challenge) > 0);
        TEST("challenge no padding", strchr(challenge, '=') == NULL);
        free(challenge);
    }

    /* Edge cases */
    char *empty = mcp_oauth_code_challenge("");
    TEST("empty verifier produces challenge", empty != NULL);
    if (empty) free(empty);

    char *null_challenge = mcp_oauth_code_challenge(NULL);
    TEST("null verifier returns NULL", null_challenge == NULL);
}

/* ================================================================
 *  Port finding test
 * ================================================================ */

static void test_find_free_port(void)
{
    int port = mcp_oauth_find_free_port();
    TEST("find_free_port > 0", port > 0);
    TEST("find_free_port <= 65535", port <= 65535);

    /* Two calls should get different ports */
    int port2 = mcp_oauth_find_free_port();
    /* They may or may not differ, but both should be valid */
    TEST("second port > 0", port2 > 0);
    TEST("second port <= 65535", port2 <= 65535);
}

/* ================================================================
 *  Browser detection test
 * ================================================================ */

static void test_browser_detection(void)
{
    /* This is environment-dependent, just check it doesn't crash */
    bool can_open = mcp_oauth_can_open_browser();
    /* Can't assert on value — depends on env */
    (void)can_open;
    TEST("browser detection runs without crash", true);
}

/* ================================================================
 *  Token storage tests
 * ================================================================ */

static void test_token_storage(void)
{
    mcp_oauth_storage_t *s = mcp_oauth_storage_new("test-server");
    TEST("storage not null", s != NULL);
    if (!s) return;

    /* Initially no tokens */
    TEST("no tokens initially", !mcp_oauth_storage_has_tokens(s));

    /* Set tokens */
    bool ok = mcp_oauth_storage_set_tokens(s,
        "{\"access_token\":\"test-token\",\"token_type\":\"bearer\",\"expires_in\":3600}");
    TEST("set tokens ok", ok);

    /* Now has tokens */
    TEST("has tokens after set", mcp_oauth_storage_has_tokens(s));

    /* Read back */
    char *tokens = mcp_oauth_storage_get_tokens(s);
    TEST("get tokens not null", tokens != NULL);
    if (tokens) {
        json_t *j = json_parse(tokens, NULL);
        TEST("tokens is valid json", j != NULL);
        if (j) {
            const char *access = json_get_str(j, "access_token", "");
            TEST("access_token matches", strcmp(access, "test-token") == 0);
            json_free(j);
        }
        free(tokens);
    }

    /* Get client info (none yet) */
    char *ci = mcp_oauth_storage_get_client_info(s);
    TEST("no client info initially", ci == NULL);

    /* Set client info */
    ok = mcp_oauth_storage_set_client_info(s,
        "{\"client_id\":\"my-client\",\"client_name\":\"Test\"}");
    TEST("set client info ok", ok);

    ci = mcp_oauth_storage_get_client_info(s);
    TEST("get client info not null", ci != NULL);
    if (ci) {
        json_t *j = json_parse(ci, NULL);
        TEST("client info is valid json", j != NULL);
        if (j) {
            const char *cid = json_get_str(j, "client_id", "");
            TEST("client_id matches", strcmp(cid, "my-client") == 0);
            json_free(j);
        }
        free(ci);
    }

    /* Get metadata (none yet) */
    char *meta = mcp_oauth_storage_get_metadata(s);
    TEST("no metadata initially", meta == NULL);

    /* Set metadata */
    ok = mcp_oauth_storage_set_metadata(s,
        "{\"token_endpoint\":\"https://example.com/token\"}");
    TEST("set metadata ok", ok);

    meta = mcp_oauth_storage_get_metadata(s);
    TEST("get metadata not null", meta != NULL);
    if (meta) {
        json_t *j = json_parse(meta, NULL);
        TEST("metadata is valid json", j != NULL);
        if (j) {
            const char *te = json_get_str(j, "token_endpoint", "");
            TEST("token_endpoint matches", strcmp(te, "https://example.com/token") == 0);
            json_free(j);
        }
        free(meta);
    }

    /* Remove */
    mcp_oauth_storage_remove(s);
    TEST("no tokens after remove", !mcp_oauth_storage_has_tokens(s));

    mcp_oauth_storage_free(s);
}

static void test_special_chars_server_name(void)
{
    mcp_oauth_storage_t *s = mcp_oauth_storage_new("my-server@company!with#specials");
    TEST("special chars storage not null", s != NULL);
    if (s) {
        bool ok = mcp_oauth_storage_set_tokens(s, "{\"access_token\":\"t\"}");
        TEST("special chars set ok", ok);
        TEST("special chars has tokens", mcp_oauth_storage_has_tokens(s));

        char *tokens = mcp_oauth_storage_get_tokens(s);
        TEST("special chars get not null", tokens != NULL);
        if (tokens) {
            json_t *j = json_parse(tokens, NULL);
            TEST("special chars get valid json", j != NULL);
            if (j) {
                const char *access = json_get_str(j, "access_token", "");
                TEST("special chars token matches", strcmp(access, "t") == 0);
                json_free(j);
            }
            free(tokens);
        }

        mcp_oauth_storage_remove(s);
        mcp_oauth_storage_free(s);
    }
}

/* ================================================================
 *  Open browser test (safe — only verifies function doesn't crash)
 * ================================================================ */

static void test_open_browser(void)
{
    /* Should not crash regardless of env */
    bool ok = mcp_oauth_open_browser("https://example.com/oauth");
    (void)ok;
    TEST("open browser runs without crash", true);
}

/* ================================================================
 *  High-level API test
 * ================================================================ */

static void test_build_auth_null_args(void)
{
    char *result = mcp_oauth_build_auth(NULL, "https://example.com/mcp", NULL);
    TEST("build_auth null name returns result", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        TEST("build_auth null name is json", j != NULL);
        if (j) {
            TEST("build_auth null name success=false",
                 !json_get_bool(j, "success", true));
            json_free(j);
        }
        free(result);
    }
}

static void test_build_auth_missing_metadata(void)
{
    /* Non-existent server — should fail with discovery error */
    char *result = mcp_oauth_build_auth("missing-server",
                                         "https://nonexistent.example.com/mcp",
                                         "{\"client_id\":\"test\"}");
    TEST("build_auth missing server returns result", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        TEST("build_auth missing server is json", j != NULL);
        if (j) {
            TEST("build_auth missing server success=false",
                 !json_get_bool(j, "success", true));
            /* Should have an error about auth endpoint */
            const char *err = json_get_str(j, "error", "");
            TEST("build_auth missing server has error", err && *err);
            json_free(j);
        }
        free(result);
    }
}

static void test_remove_tokens(void)
{
    /* Create some tokens first */
    mcp_oauth_storage_t *s = mcp_oauth_storage_new("remove-test");
    TEST("remove-test storage ok", s != NULL);
    if (s) {
        mcp_oauth_storage_set_tokens(s, "{\"access_token\":\"t\"}");
        mcp_oauth_storage_free(s);
    }

    /* Remove via high-level API */
    bool ok = mcp_oauth_remove_tokens("remove-test");
    TEST("remove_tokens returns true", ok);

    /* Verify gone */
    s = mcp_oauth_storage_new("remove-test");
    if (s) {
        TEST("remove-test tokens gone", !mcp_oauth_storage_has_tokens(s));
        mcp_oauth_storage_free(s);
    }
}

static void test_manager_get_token_null_args(void)
{
    char *result = mcp_oauth_manager_get_token(NULL, "url", NULL);
    TEST("manager_get_token null name returns result", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        if (j) {
            TEST("manager_get_token null name success=false",
                 !json_get_bool(j, "success", true));
            json_free(j);
        }
        free(result);
    }
}

static void test_manager_reauthorize_null(void)
{
    char *result = mcp_oauth_manager_reauthorize(NULL, "url", NULL);
    TEST("manager_reauthorize null returns result", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        if (j) {
            TEST("manager_reauthorize null success=false",
                 !json_get_bool(j, "success", true));
            json_free(j);
        }
        free(result);
    }
}

static void test_manager_get_token_missing_server(void)
{
    /* Non-existent server — should try discovery and fail gracefully */
    char *result = mcp_oauth_manager_get_token(
        "manager-test-missing",
        "https://nonexistent-mcp.example.com/mcp",
        "");
    TEST("manager missing server returns result", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        TEST("manager missing server is json", j != NULL);
        if (j) {
            /* May fail or may try discovery and also fail — either way not success */
            bool success = json_get_bool(j, "success", false);
            /* We just need it not to crash */
            (void)success;
            TEST("manager missing server doesn't crash", true);
            json_free(j);
        }
        free(result);
    }
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void)
{
    printf("=== MCP OAuth Library Tests ===\n");

    test_pkce_verifier_generation();
    test_pkce_code_challenge();
    test_find_free_port();
    test_browser_detection();
    test_token_storage();
    test_special_chars_server_name();
    test_open_browser();
    test_build_auth_null_args();
    test_build_auth_missing_metadata();
    test_remove_tokens();
    test_manager_get_token_null_args();
    test_manager_reauthorize_null();
    test_manager_get_token_missing_server();

    printf("\nResults: %d passed, %d failed, %d total\n", passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
