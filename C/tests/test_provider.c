/* Provider registry tests — verify provider.c lifecycle functions.
 *
 * Uses mock provider ops to avoid linking to all provider implementations.
 * Tests: registration, creation, destruction, credential pool, FIM capability.
 */

#include "provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
    } else { \
        passed++; \
    } \
} while(0)

/* ================================================================
 *  Mock provider ops
 * ================================================================ */

static char *mock_build_url(const provider_t *p, const char *base_url) {
    (void)p;
    char *url = malloc(strlen(base_url) + 20);
    if (url) sprintf(url, "%s/chat/completions", base_url);
    return url;
}

static char *mock_build_headers(const provider_t *p, const char *api_key) {
    (void)p;
    char *h = malloc(128);
    if (h) sprintf(h, "Authorization: Bearer %s\r\n", api_key ? api_key : "");
    return h;
}

static char *mock_build_request_body(const provider_t *p,
                                      const message_t **messages, size_t msg_count,
                                      json_node_t *tools_json, bool streaming) {
    (void)p; (void)messages; (void)msg_count; (void)tools_json; (void)streaming;
    return strdup("{\"model\":\"mock\"}");
}

static provider_response_t *mock_parse_response(const provider_t *p,
                                                  const char *response_body) {
    (void)p; (void)response_body;
    provider_response_t *r = calloc(1, sizeof(provider_response_t));
    if (r) r->content = strdup("mock response");
    return r;
}

static provider_response_t *mock_parse_stream_chunk(const provider_t *p,
                                                     const char *chunk) {
    (void)p; (void)chunk;
    provider_response_t *r = calloc(1, sizeof(provider_response_t));
    if (r) r->content = strdup("mock chunk");
    return r;
}

static void mock_free_response(provider_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp->encrypted_content);
    free(resp);
}

static char *mock_build_fim_body(const provider_t *p,
                                  const char *prompt, const char *suffix,
                                  int max_tokens) {
    (void)p;
    char *body = malloc(256);
    if (body) snprintf(body, 256, "{\"prompt\":\"%s\",\"suffix\":\"%s\",\"max_tokens\":%d}",
                       prompt ? prompt : "", suffix ? suffix : "", max_tokens);
    return body;
}

static provider_response_t *mock_parse_fim_response(const provider_t *p,
                                                      const char *response_body) {
    (void)p; (void)response_body;
    provider_response_t *r = calloc(1, sizeof(provider_response_t));
    if (r) r->content = strdup("mock fim completion");
    return r;
}

static char *mock_build_fim_url(const provider_t *p, const char *base_url) {
    (void)p;
    char *url = malloc(strlen(base_url) + 20);
    if (url) sprintf(url, "%s/beta/completions", base_url);
    return url;
}

static const provider_ops_t MOCK_OPS_FIM = {
    .build_url = mock_build_url,
    .build_headers = mock_build_headers,
    .build_request_body = mock_build_request_body,
    .parse_response = mock_parse_response,
    .parse_stream_chunk = mock_parse_stream_chunk,
    .free_response = mock_free_response,
    .build_fim_body = mock_build_fim_body,
    .parse_fim_response = mock_parse_fim_response,
    .build_fim_url = mock_build_fim_url,
    .name = "mock_fim",
};

static const provider_ops_t MOCK_OPS_NOFIM = {
    .build_url = mock_build_url,
    .build_headers = mock_build_headers,
    .build_request_body = mock_build_request_body,
    .parse_response = mock_parse_response,
    .parse_stream_chunk = mock_parse_stream_chunk,
    .free_response = mock_free_response,
    .build_fim_body = NULL,      /* No FIM support */
    .parse_fim_response = NULL,  /* No FIM support */
    .build_fim_url = NULL,       /* No FIM support */
    .name = "mock_nofim",
};

/* ================================================================
 *  Tests
 * ================================================================ */

static void test_null_safety(void)
{
    /* provider_free(NULL) should not crash */
    provider_free(NULL);
    TEST("provider_free(NULL) safe", 1);

    /* provider_get_credential_pool(NULL) = NULL */
    TEST("get_cred_pool NULL", provider_get_credential_pool(NULL) == NULL);

    /* provider_has_fim(NULL) = false */
    TEST("has_fim NULL", !provider_has_fim(NULL));

    /* provider_ops(NULL) = NULL */
    TEST("provider_ops(NULL)", provider_ops(NULL) == NULL);

    /* provider_set_system_cached(NULL, true) no crash */
    provider_set_system_cached(NULL, true);
    TEST("set_system_cached(NULL)", 1);

    /* provider_get_system_cached(NULL) = false */
    TEST("get_system_cached(NULL)", !provider_get_system_cached(NULL));

    /* provider_set_credential_pool(NULL, pool) no crash */
    credential_pool_t dummy_pool;
    memset(&dummy_pool, 0, sizeof(dummy_pool));
    provider_set_credential_pool(NULL, &dummy_pool);
    TEST("set_cred_pool(NULL)", 1);
}

static void test_registration_and_lookup(void)
{
    /* Register mock providers */
    provider_register(PROVIDER_CUSTOM, &MOCK_OPS_FIM);
    provider_register(PROVIDER_OPENAI, &MOCK_OPS_NOFIM);

    /* Verify registration succeeded (can create instances now) */

    /* Create FIM-capable provider with known name */
    provider_t *p = provider_create("mock_fim", "test-model", "test-key", "https://api.test.com");
    TEST("create mock_fim", p != NULL);
    if (p) {
        TEST("name set", strcmp(p->name, "mock_fim") == 0);
        TEST("model set", strcmp(p->model, "test-model") == 0);
        TEST("api_key set", strcmp(p->api_key, "test-key") == 0);
        TEST("base_url set", strcmp(p->base_url, "https://api.test.com") == 0);
        TEST("type default", p->type == PROVIDER_OPENAI);
        TEST("ops assigned", p->ops == &MOCK_OPS_FIM);
        provider_free(p);
    }

    /* Create no-FIM provider */
    provider_t *p2 = provider_create("mock_nofim", "model2", "key2", "https://test2.com");
    TEST("create mock_nofim", p2 != NULL);
    if (p2) {
        TEST("no-fim name", strcmp(p2->name, "mock_nofim") == 0);
        TEST("no-fim ops", p2->ops == &MOCK_OPS_NOFIM);
        provider_free(p2);
    }

    /* Create with NULL provider_name (defaults to openai) */
    provider_t *p3 = provider_create(NULL, "default-model", NULL, NULL);
    TEST("create with NULL name (defaults)", p3 != NULL);
    if (p3) {
        TEST("default name", strcmp(p3->name, "openai") == 0);
        provider_free(p3);
    }

    /* Create with empty name (code treats "" as valid non-null) */
    provider_t *p4 = provider_create("", "empty-name", NULL, NULL);
    TEST("create with empty name", p4 != NULL);
    if (p4) {
        TEST("empty name stays empty", strlen(p4->name) == 0);
        provider_free(p4);
    }

    /* Create with NULL model, api_key, base_url (all optional) */
    provider_t *p5 = provider_create("mock_fim", NULL, NULL, NULL);
    TEST("create with NULL optional params", p5 != NULL);
    if (p5) {
        TEST("model empty when NULL", p5->model[0] == '\0');
        TEST("api_key empty when NULL", p5->api_key[0] == '\0');
        TEST("base_url empty when NULL", p5->base_url[0] == '\0');
        provider_free(p5);
    }
}

static void test_fim_capability(void)
{
    provider_t *p_fim = provider_create("mock_fim", "m", "k", "u");
    provider_t *p_nofim = provider_create("mock_nofim", "m", "k", "u");
    provider_t *p_corrupt = calloc(1, sizeof(provider_t));
    p_corrupt->ops = NULL; /* Ops-less provider */

    TEST("FIM-capable provider", provider_has_fim(p_fim));
    TEST("non-FIM provider", !provider_has_fim(p_nofim));
    TEST("corrupt provider (no ops)", provider_has_fim(p_corrupt) == 0);

    /* provider_fim() null safety */
    TEST("fim NULL provider", provider_fim(NULL, "prompt", NULL, 100) == NULL);
    TEST("fim NULL prompt", provider_fim(p_fim, NULL, NULL, 100) == NULL);
    TEST("fim on nofim", provider_fim(p_nofim, "prompt", NULL, 100) == NULL);

    /* These should not crash even though they don't make HTTP calls
     * (the test will attempt the mock but fail at HTTP client creation) */
    provider_free(p_fim);
    provider_free(p_nofim);
    free(p_corrupt);
}

static void test_credential_pool(void)
{
    provider_t *p = provider_create("mock_fim", "m", "k", "u");
    credential_pool_t dummy;
    memset(&dummy, 0, sizeof(dummy));

    /* NULL pool initially */
    TEST("initial pool is NULL", provider_get_credential_pool(p) == NULL);

    /* Set and get pool */
    provider_set_credential_pool(p, &dummy);
    TEST("pool after set", provider_get_credential_pool(p) == &dummy);

    /* Detach (set NULL) */
    provider_set_credential_pool(p, NULL);
    TEST("pool after detach", provider_get_credential_pool(p) == NULL);

    provider_free(p);
}

static void test_system_cached(void)
{
    provider_t *p = provider_create("mock_fim", "m", "k", "u");
    TEST("initially not cached", !provider_get_system_cached(p));

    provider_set_system_cached(p, true);
    TEST("cached after set", provider_get_system_cached(p));

    provider_set_system_cached(p, false);
    TEST("uncached after reset", !provider_get_system_cached(p));

    provider_free(p);
}

static void test_ops_inline(void)
{
    provider_t *p = provider_create("mock_fim", "m", "k", "u");
    TEST("provider_ops() returns ops", provider_ops(p) == &MOCK_OPS_FIM);
    provider_free(p);
}

static void test_provider_create_truncation(void)
{
    /* Test that long strings are truncated gracefully */
    char long_name[512];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    provider_t *p = provider_create("mock_fim", long_name, long_name, long_name);
    TEST("create with long strings", p != NULL);
    if (p) {
        /* model/name/api_key/base_url are bounded, should not overflow */
        TEST("model bounded", strlen(p->model) < sizeof(p->model));
        TEST("name bounded", strlen(p->name) < sizeof(p->name));
        TEST("api_key bounded", strlen(p->api_key) < sizeof(p->api_key));
        TEST("base_url bounded", strlen(p->base_url) < sizeof(p->base_url));
        provider_free(p);
    }
}

int main(void)
{
    test_null_safety();
    test_registration_and_lookup();
    test_fim_capability();
    test_credential_pool();
    test_system_cached();
    test_ops_inline();
    test_provider_create_truncation();

    printf("provider: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
