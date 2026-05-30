/*
 * test_managed_gateway.c — Tests for lib/libmangateway/managed_gateway.c
 */

#include "managed_gateway.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define ASSERT_STR(actual, expected, msg) do { \
    if (strcmp((actual), (expected)) != 0) { \
        char _b[256]; snprintf(_b, sizeof(_b), "%s: expected \"%s\", got \"%s\"", msg, (expected), (actual)); \
        FAIL(_b); return; \
    } \
} while(0)

/* ─── Auth JSON path ────────────────────────────────────── */

static void test_auth_json_path_default(void)
{
    TEST("auth json path with hermes home");
    char buf[512];
    managed_gw_auth_json_path("/home/user/.slermes", buf, sizeof(buf));
    ASSERT_STR(buf, "/home/user/.slermes/auth.json", "path");
    PASS();
}

/* ─── Scheme ────────────────────────────────────────────── */

static void test_scheme_default(void)
{
    TEST("scheme default https");
    unsetenv("TOOL_GATEWAY_SCHEME");
    const char *s = managed_gw_get_scheme();
    ASSERT_STR(s, "https", "default scheme");
    PASS();
}

static void test_scheme_http(void)
{
    TEST("scheme http from env");
    setenv("TOOL_GATEWAY_SCHEME", "http", 1);
    const char *s = managed_gw_get_scheme();
    ASSERT_STR(s, "http", "http scheme");
    PASS();
}

static void test_scheme_invalid(void)
{
    TEST("scheme invalid falls back to https");
    setenv("TOOL_GATEWAY_SCHEME", "ftp", 1);
    const char *s = managed_gw_get_scheme();
    ASSERT_STR(s, "https", "fallback https");
    PASS();
}

/* ─── Auth JSON path ────────────────────────────────── */

static void test_auth_json_path_null_home(void)
{
    TEST("auth json path with null home");
    char buf[512];
    managed_gw_auth_json_path(NULL, buf, sizeof(buf));
    /* Falls back to $HOME/.slermes/auth.json */
    ASSERT(strlen(buf) > 10, "non-empty path");
    ASSERT(strstr(buf, ".slermes") != NULL, "contains .slermes");
    PASS();
}

static void test_auth_json_path_empty_home(void)
{
    TEST("auth json path with empty home");
    char buf[512];
    managed_gw_auth_json_path("", buf, sizeof(buf));
    ASSERT(strlen(buf) > 10, "non-empty path");
    ASSERT(strstr(buf, ".slermes") != NULL, "contains .slermes");
    PASS();
}

/* ─── URL builder edge cases ────────────────────────── */

static void test_build_url_null_vendor(void)
{
    TEST("build url null vendor returns empty");
    char buf[512];
    buf[0] = 'X';
    managed_gw_build_url(NULL, buf, sizeof(buf));
    ASSERT(buf[0] == '\0', "empty string");
    PASS();
}

static void test_build_url_browser_vendor(void)
{
    TEST("build url browser vendor");
    unsetenv("TOOL_GATEWAY_SCHEME");
    unsetenv("BROWSER_GATEWAY_URL");
    unsetenv("TOOL_GATEWAY_DOMAIN");
    char buf[512];
    managed_gw_build_url("browser", buf, sizeof(buf));
    ASSERT(strstr(buf, "browser-gateway") != NULL, "browser gateway");
    ASSERT(strstr(buf, "nousresearch.com") != NULL, "default domain");
    PASS();
}

/* ─── URL builder ───────────────────────────────────────── */

static void test_build_url_default(void)
{
    TEST("build url default domain");
    unsetenv("FAL_GATEWAY_URL");
    unsetenv("TOOL_GATEWAY_DOMAIN");
    unsetenv("TOOL_GATEWAY_SCHEME");
    char buf[512];
    managed_gw_build_url("fal", buf, sizeof(buf));
    ASSERT_STR(buf, "https://fal-gateway.nousresearch.com", "url");
    PASS();
}

static void test_build_url_with_env(void)
{
    TEST("build url from env var");
    setenv("FAL_GATEWAY_URL", "https://custom.fal.ai/gateway", 1);
    char buf[512];
    managed_gw_build_url("fal", buf, sizeof(buf));
    ASSERT_STR(buf, "https://custom.fal.ai/gateway", "url from env");
    PASS();
}

static void test_build_url_custom_domain(void)
{
    TEST("build url custom domain");
    unsetenv("FAL_GATEWAY_URL");
    setenv("TOOL_GATEWAY_DOMAIN", "example.com", 1);
    unsetenv("TOOL_GATEWAY_SCHEME");
    char buf[512];
    managed_gw_build_url("fal", buf, sizeof(buf));
    ASSERT_STR(buf, "https://fal-gateway.example.com", "url custom domain");
    PASS();
}

/* ─── Resolve ───────────────────────────────────────────── */

static void test_resolve_disabled(void)
{
    TEST("resolve returns false when nous disabled");
    unsetenv("NOUS_MANAGED_TOOLS");
    managed_gateway_config_t cfg;
    bool ok = managed_gw_resolve("/tmp", "fal", &cfg);
    ASSERT(!ok, "should be false");
    PASS();
}

static void test_resolve_no_vendor(void)
{
    TEST("resolve returns false for empty vendor");
    setenv("NOUS_MANAGED_TOOLS", "true", 1);
    managed_gateway_config_t cfg;
    bool ok = managed_gw_resolve("/tmp", "", &cfg);
    ASSERT(!ok, "should be false");
    PASS();
}

static void test_resolve_no_token(void)
{
    TEST("resolve returns false without token");
    setenv("NOUS_MANAGED_TOOLS", "true", 1);
    unsetenv("TOOL_GATEWAY_USER_TOKEN");
    /* Ensure no auth.json exists */
    managed_gateway_config_t cfg;
    bool ok = managed_gw_resolve("/nonexistent", "fal", &cfg);
    ASSERT(!ok, "should be false without token");
    PASS();
}

static void test_resolve_with_env_token(void)
{
    TEST("resolve succeeds with env token");
    setenv("NOUS_MANAGED_TOOLS", "true", 1);
    setenv("TOOL_GATEWAY_USER_TOKEN", "test-nous-token-123", 1);
    unsetenv("FAL_GATEWAY_URL");
    unsetenv("TOOL_GATEWAY_DOMAIN");
    unsetenv("TOOL_GATEWAY_SCHEME");
    managed_gateway_config_t cfg;
    bool ok = managed_gw_resolve("/tmp", "fal", &cfg);
    ASSERT(ok, "should be true");
    ASSERT_STR(cfg.vendor, "fal", "vendor");
    ASSERT_STR(cfg.gateway_origin, "https://fal-gateway.nousresearch.com", "origin");
    ASSERT_STR(cfg.nous_user_token, "test-nous-token-123", "token");
    ASSERT(cfg.managed_mode, "managed_mode");
    PASS();
}

/* ─── Readiness ─────────────────────────────────────────── */

static void test_is_ready_true(void)
{
    TEST("is_ready true with env token");
    setenv("NOUS_MANAGED_TOOLS", "true", 1);
    setenv("TOOL_GATEWAY_USER_TOKEN", "test-token", 1);
    bool ok = managed_gw_is_ready("/tmp", "browser");
    ASSERT(ok, "should be ready");
    PASS();
}

static void test_is_ready_false(void)
{
    TEST("is_ready false without token");
    unsetenv("NOUS_MANAGED_TOOLS");
    unsetenv("TOOL_GATEWAY_USER_TOKEN");
    bool ok = managed_gw_is_ready("/tmp", "browser");
    ASSERT(!ok, "should not be ready");
    PASS();
}

static void test_resolve_null_config(void)
{
    TEST("resolve null config returns false");
    setenv("NOUS_MANAGED_TOOLS", "true", 1);
    setenv("TOOL_GATEWAY_USER_TOKEN", "tok", 1);
    bool ok = managed_gw_resolve("/tmp", "fal", NULL);
    ASSERT(!ok, "should be false");
    PASS();
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    printf("=== managed_gateway tests ===\n\n");

    test_auth_json_path_default();
    test_auth_json_path_null_home();
    test_auth_json_path_empty_home();
    test_scheme_default();
    test_scheme_http();
    test_scheme_invalid();
    test_build_url_default();
    test_build_url_with_env();
    test_build_url_custom_domain();
    test_build_url_null_vendor();
    test_build_url_browser_vendor();
    test_resolve_disabled();
    test_resolve_no_vendor();
    test_resolve_no_token();
    test_resolve_null_config();
    test_resolve_with_env_token();
    test_is_ready_true();
    test_is_ready_false();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
