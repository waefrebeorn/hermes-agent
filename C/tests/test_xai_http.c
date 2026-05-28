/*
 * test_xai_http.c — Tests for xAI HTTP credential helpers.
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
    /* Ensure env var is not set */
    unsetenv("HERMES_XAI_API_KEY");

    TEST("has_creds false when unset", !xai_has_credentials());

    char key[XAI_API_KEY_MAX] = "x";
    TEST("get_api_key false when unset", !xai_get_api_key(key));
    TEST("api_key empty when unset", key[0] == '\0');
}

static void test_with_creds(void)
{
    setenv("HERMES_XAI_API_KEY", "sk-test-key-12345", 1);

    TEST("has_creds true when set", xai_has_credentials());

    char key[XAI_API_KEY_MAX];
    TEST("get_api_key true when set", xai_get_api_key(key));
    TEST("api_key matches", strcmp(key, "sk-test-key-12345") == 0);

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

static void test_user_agent(void)
{
    const char *ua = xai_user_agent();
    TEST("user agent non-null", ua != NULL);
    TEST("user agent starts with Hermes-Agent",
         strncmp(ua, "Hermes-Agent", 12) == 0);
}

static void test_null_output(void)
{
    /* Should not crash */
    xai_get_api_key(NULL);
    xai_get_base_url(NULL);
    TEST("null output doesn't crash", 1);
}

int main(void)
{
    printf("=== xAI HTTP Library Tests ===\n");

    test_no_creds();
    test_with_creds();
    test_empty_key();
    test_base_url_default();
    test_base_url_custom();
    test_base_url_empty_env();
    test_user_agent();
    test_null_output();

    printf("\nResults: %d passed, %d failed, %d total\n",
           passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
