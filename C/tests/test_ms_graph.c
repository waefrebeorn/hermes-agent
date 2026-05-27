/*
 * test_ms_graph.c — Tests for Microsoft Graph client module.
 * Tests: credential loading, token URL, caching, URL resolution.
 * Network-dependent tests are skipped (require live Graph API).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "ms_graph.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

int main(void)
{
    printf("=== Microsoft Graph Client Tests ===\n\n");

    /* ── Credentials from env ── */
    printf("-- Credentials from env --\n");
    {
        /* Set env vars */
        setenv("MSGRAPH_TENANT_ID", "test-tenant", 1);
        setenv("MSGRAPH_CLIENT_ID", "test-client", 1);
        setenv("MSGRAPH_CLIENT_SECRET", "test-secret", 1);
        setenv("MSGRAPH_SCOPE", "https://graph.microsoft.com/.default", 1);
        unsetenv("MSGRAPH_AUTHORITY_URL");

        msgraph_credentials_t creds;
        bool ok = msgraph_credentials_from_env(&creds, true);
        TEST("credentials loaded", ok);
        TEST("tenant_id correct", strcmp(creds.tenant_id, "test-tenant") == 0);
        TEST("client_id correct", strcmp(creds.client_id, "test-client") == 0);
        TEST("client_secret correct", strcmp(creds.client_secret, "test-secret") == 0);
        TEST("scope set", strcmp(creds.scope, "https://graph.microsoft.com/.default") == 0);
        TEST("authority default", strcmp(creds.authority_url, "https://login.microsoftonline.com") == 0);

        /* Token URL */
        TEST("token URL correct",
             strcmp(creds.token_url,
                    "https://login.microsoftonline.com/test-tenant/oauth2/v2.0/token") == 0);
    }

    /* ── Missing credentials ── */
    printf("\n-- Missing credentials --\n");
    {
        unsetenv("MSGRAPH_TENANT_ID");
        unsetenv("MSGRAPH_CLIENT_ID");
        unsetenv("MSGRAPH_CLIENT_SECRET");

        msgraph_credentials_t creds;
        bool ok = msgraph_credentials_from_env(&creds, true);
        TEST("missing creds returns false", !ok);

        ok = msgraph_credentials_from_env(&creds, false);
        TEST("missing creds returns false even when not required", !ok);
    }

    /* ── Partial credentials ── */
    printf("\n-- Partial credentials --\n");
    {
        setenv("MSGRAPH_TENANT_ID", "t", 1);
        unsetenv("MSGRAPH_CLIENT_ID");
        unsetenv("MSGRAPH_CLIENT_SECRET");

        msgraph_credentials_t creds;
        bool ok = msgraph_credentials_from_env(&creds, true);
        TEST("missing client_id returns false", !ok);
    }

    /* ── Token URL with custom authority ── */
    printf("\n-- Custom authority --\n");
    {
        setenv("MSGRAPH_TENANT_ID", "my-tenant", 1);
        setenv("MSGRAPH_CLIENT_ID", "my-client", 1);
        setenv("MSGRAPH_CLIENT_SECRET", "my-secret", 1);
        setenv("MSGRAPH_AUTHORITY_URL", "https://login.chinacloudapi.cn/", 1);

        msgraph_credentials_t creds;
        msgraph_credentials_from_env(&creds, true);
        TEST("custom authority loaded",
             strcmp(creds.authority_url, "https://login.chinacloudapi.cn") == 0);
        TEST("token URL with custom authority",
             strcmp(creds.token_url,
                    "https://login.chinacloudapi.cn/my-tenant/oauth2/v2.0/token") == 0);
    }

    /* ── Token caching ── */
    printf("\n-- Token caching --\n");
    {
        msgraph_credentials_t creds;
        setenv("MSGRAPH_TENANT_ID", "t", 1);
        setenv("MSGRAPH_CLIENT_ID", "c", 1);
        setenv("MSGRAPH_CLIENT_SECRET", "s", 1);
        msgraph_credentials_from_env(&creds, true);

        msgraph_token_provider_t tp;
        msgraph_token_provider_init(&tp, &creds);

        TEST("no cached token initially", !tp.has_cached);

        /* Cache a token */
        strncpy(tp.cached.access_token, "test-token-value",
                sizeof(tp.cached.access_token) - 1);
        tp.cached.access_token[sizeof(tp.cached.access_token) - 1] = '\0';
        tp.cached.expires_at = (double)time(NULL) + 3600; /* 1 hour from now */
        tp.has_cached = true;

        TEST("token not expired (1h left)",
             !msgraph_token_is_expired(&tp.cached, 120));

        /* Expire it */
        tp.cached.expires_at = (double)time(NULL) - 10; /* 10s ago */
        TEST("token expired (10s ago)",
             msgraph_token_is_expired(&tp.cached, 0));

        /* Clear cache */
        msgraph_clear_cache(&tp);
        TEST("cache cleared", !tp.has_cached);
    }

    /* ── URL resolution ── */
    printf("\n-- URL resolution --\n");
    {
        msgraph_token_provider_t tp; /* dummy, unused in URL resolution */
        msgraph_client_t client;
        msgraph_client_init(&client, &tp);
        TEST("default base URL",
             strcmp(client.base_url, "https://graph.microsoft.com/v1.0") == 0);
        TEST("default user agent",
             strcmp(client.user_agent, "Hermes-Agent/msgraph-c") == 0);
        TEST("default timeout", client.timeout_seconds == 60);
        TEST("default retries", client.max_retries == 3);
    }

    /* ── Error cleanup (no-op) ── */
    printf("\n-- Error cleanup --\n");
    {
        msgraph_error_t err;
        memset(&err, 0, sizeof(err));
        msgraph_error_cleanup(&err);
        TEST("error cleanup (no-op) is safe", 1);
    }

    /* ── Summary ── */
    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
