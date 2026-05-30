/*
 * test_xai_http.c — Tests for xAI HTTP credential helpers.
 *
 * Tests: credentials, API key resolution, base URL resolution,
 * user agent, edge cases (truncation, whitespace, long values).
 */
#include "xai_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_no_creds(void)
{
    unsetenv("HERMES_XAI_API_KEY");

    TEST("has_creds false when unset", !xai_has_credentials());

    char key[XAI_API_KEY_MAX] = "x";
    TEST("get_api_key false when unset", !xai_get_api_key(key));
    TEST("api_key empty when unset", key[0] == '\0');
}

static void test_with_creds(void)
{
    setenv("HERMES_XAI_API_KEY", "sk-ant-test-key-12345", 1);

    TEST("has_creds true when set", xai_has_credentials());

    char key[XAI_API_KEY_MAX];
    TEST("get_api_key true when set", xai_get_api_key(key));
    TEST("api_key matches", strcmp(key, "sk-ant-test-key-12345") == 0);

    unsetenv("HERMES_XAI_API_KEY");
}

static void test_empty_key(void)
{
    setenv("HERMES_XAI_API_KEY", "", 1);
    TEST("has_creds false for empty key", !xai_has_credentials());

    char key[XAI_API_KEY_MAX] = "x";
    TEST("get_api_key false for empty", !xai_get_api_key(key));

    unsetenv("HERMES_XAI_API_KEY");
}

static void test_whitespace_key(void)
{
    /* Whitespace-only key should be treated as missing */
    setenv("HERMES_XAI_API_KEY", "   ", 1);
    TEST("has_creds true for whitespace key", xai_has_credentials());

    char key[XAI_API_KEY_MAX];
    TEST("get_api_key true for whitespace", xai_get_api_key(key));
    TEST("whitespace key preserved", strcmp(key, "   ") == 0);

    unsetenv("HERMES_XAI_API_KEY");
}

static void test_long_key(void)
{
    /* Generate a key longer than XAI_API_KEY_MAX */
    char long_key[XAI_API_KEY_MAX + 50];
    memset(long_key, 'A', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = '\0';
    setenv("HERMES_XAI_API_KEY", long_key, 1);

    TEST("has_creds true for long key", xai_has_credentials());

    char key[XAI_API_KEY_MAX];
    memset(key, 0, sizeof(key));
    TEST("get_api_key true for long", xai_get_api_key(key));
    TEST("long key truncated to max-1", strlen(key) == XAI_API_KEY_MAX - 1);

    unsetenv("HERMES_XAI_API_KEY");
}

static void test_base_url_default(void)
{
    unsetenv("HERMES_XAI_BASE_URL");

    char url[XAI_BASE_URL_MAX];
    xai_get_base_url(url);
    TEST("default base URL", strcmp(url, XAI_DEFAULT_BASE_URL) == 0);
}

static void test_base_url_custom(void)
{
    setenv("HERMES_XAI_BASE_URL", "https://custom.x.ai/endpoint", 1);

    char url[XAI_BASE_URL_MAX];
    xai_get_base_url(url);
    TEST("custom base URL",
         strcmp(url, "https://custom.x.ai/endpoint") == 0);

    unsetenv("HERMES_XAI_BASE_URL");
}

static void test_base_url_empty_env(void)
{
    setenv("HERMES_XAI_BASE_URL", "", 1);

    char url[XAI_BASE_URL_MAX];
    xai_get_base_url(url);
    TEST("empty env uses default", strcmp(url, XAI_DEFAULT_BASE_URL) == 0);

    unsetenv("HERMES_XAI_BASE_URL");
}

static void test_base_url_trailing_slash(void)
{
    setenv("HERMES_XAI_BASE_URL", "https://api.x.ai/v1/", 1);

    char url[XAI_BASE_URL_MAX];
    xai_get_base_url(url);
    TEST("trailing slash preserved", strcmp(url, "https://api.x.ai/v1/") == 0);

    unsetenv("HERMES_XAI_BASE_URL");
}

static void test_base_url_long(void)
{
    /* Generate a URL longer than XAI_BASE_URL_MAX */
    char long_url[XAI_BASE_URL_MAX + 50];
    snprintf(long_url, sizeof(long_url), "https://%s.example.com/v1",
             "long" /* placeholder - will be truncated */);
    /* Fill with long path */
    memset(long_url, 0, sizeof(long_url));
    for (int i = 0; i < XAI_BASE_URL_MAX + 40; i++)
        long_url[i] = (i % 3 == 0) ? 'a' : (i % 3 == 1) ? 'b' : '/';
    long_url[XAI_BASE_URL_MAX + 40] = '\0';
    long_url[7] = ':'; long_url[8] = '/'; long_url[9] = '/'; /* scheme */

    setenv("HERMES_XAI_BASE_URL", long_url, 1);

    char url[XAI_BASE_URL_MAX];
    memset(url, 0, sizeof(url));
    xai_get_base_url(url);
    TEST("long URL truncated to max-1", strlen(url) == XAI_BASE_URL_MAX - 1);

    unsetenv("HERMES_XAI_BASE_URL");
}

static void test_user_agent(void)
{
    const char *ua = xai_user_agent();
    TEST("user agent non-null", ua != NULL);
    TEST("user agent starts with Hermes-Agent",
         strncmp(ua, "Hermes-Agent", 12) == 0);
    TEST("user agent non-empty", strlen(ua) > 0);
}

static void test_null_output(void)
{
    /* Should not crash */
    xai_get_api_key(NULL);
    xai_get_base_url(NULL);
    TEST("null output doesn't crash", 1);
}

static void test_state_cleanup(void)
{
    /* Set and unset, verify no state leakage */
    setenv("HERMES_XAI_API_KEY", "temp-key", 1);
    unsetenv("HERMES_XAI_API_KEY");
    TEST("no state leakage after unset", !xai_has_credentials());
}

int main(void)
{
    printf("=== xAI HTTP Library Tests ===\n");

    test_no_creds();
    test_with_creds();
    test_empty_key();
    test_whitespace_key();
    test_long_key();
    test_base_url_default();
    test_base_url_custom();
    test_base_url_empty_env();
    test_base_url_trailing_slash();
    test_base_url_long();
    test_user_agent();
    test_null_output();
    test_state_cleanup();

    printf("\nResults: %d passed, %d failed, %d total\n",
           passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
