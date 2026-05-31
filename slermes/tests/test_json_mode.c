/*
 * test_json_mode.c — B23: json_mode convenience flag
 *
 * Tests: provider request body includes response_format json_object when
 * json_mode=true, and omits it when json_mode=false or response_format set.
 * Tests 6 core providers (Azure/Bedrock/Google excluded — those providers
 * have complex build functions that need full provider.c linkage).
 */
#include "hermes.h"
#include "hermes_json.h"
#include "provider.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

/* Minimal test messages */
static const message_t msg = {.role = MSG_USER, .content = (char*)"hello"};
static const message_t *test_msgs[] = {&msg};

/* Check if JSON response body contains "response_format":{"type":"json_object"} */
static int has_json_mode(const char *body) {
    return body && strstr(body, "\"response_format\"") &&
           strstr(body, "\"json_object\"");
}

/* Check if JSON body has no response_format field */
static int no_response_format(const char *body) {
    return body && !strstr(body, "\"response_format\"");
}

/* Provider test macro — tests build_request_body with json_mode=true */
#define TEST_PROVIDER(name, ops) do { \
    provider_t p = {0}; \
    p.config.json_mode = true; \
    p.config.response_format[0] = '\0'; \
    char *body = ops.build_request_body(&p, test_msgs, 1, NULL, false); \
    TEST(name " json_mode", has_json_mode(body)); \
    free(body); \
} while(0)

int main(void) {
    printf("=== B23: json_mode tests ===\n\n");

    /* 1. Default: json_mode=false, no response_format set → no response_format in body */
    {
        provider_t p = {0};
        p.config.json_mode = false;
        p.config.response_format[0] = '\0';
        char *body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, false);
        TEST("default no json_mode", no_response_format(body));
        free(body);
    }

    /* 2. json_mode=true + no response_format → adds json_object */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        p.config.response_format[0] = '\0';
        char *body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, false);
        TEST("json_mode=true adds json_object", has_json_mode(body));
        free(body);
    }

    /* 3. json_mode=true + explicit response_format → explicit wins */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        snprintf(p.config.response_format, sizeof(p.config.response_format),
                 "{\"type\":\"json_schema\",\"json_schema\":{\"name\":\"test\"}}");
        char *body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, false);
        TEST("json_mode+explicit uses explicit", body && strstr(body, "json_schema"));
        TEST("json_mode+explicit no json_object", body && !strstr(body, "\"json_object\""));
        free(body);
    }

    /* 4-9. Test json_mode across all 6 core providers */
    TEST_PROVIDER("OpenAI",      PROVIDER_OPS_OPENAI);
    TEST_PROVIDER("OpenRouter",  PROVIDER_OPS_OPENROUTER);
    TEST_PROVIDER("DeepSeek",    PROVIDER_OPS_DEEPSEEK);
    TEST_PROVIDER("xAI",         PROVIDER_OPS_XAI);
    TEST_PROVIDER("Anthropic",   PROVIDER_OPS_ANTHROPIC);
    TEST_PROVIDER("Custom",      PROVIDER_OPS_CUSTOM);

    /* ── Additional providers ── */
    printf("\n--- Additional providers ---\n");
    TEST_PROVIDER("Azure",       PROVIDER_OPS_AZURE);
    TEST_PROVIDER("Google",      PROVIDER_OPS_GOOGLE);
    TEST_PROVIDER("Bedrock",     PROVIDER_OPS_BEDROCK);

    /* ── Edge cases ── */
    printf("\n--- Edge cases ---\n");

    /* 10. json_mode toggle: true→false→true */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        p.config.response_format[0] = '\0';
        char *body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, false);
        TEST("toggle: true first", has_json_mode(body));
        free(body);

        p.config.json_mode = false;
        body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, false);
        TEST("toggle: false second", no_response_format(body));
        free(body);

        p.config.json_mode = true;
        body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, false);
        TEST("toggle: true third", has_json_mode(body));
        free(body);
    }

    /* 11. json_mode=false + explicit response_format set → uses explicit, no json_object */
    {
        provider_t p = {0};
        p.config.json_mode = false;
        snprintf(p.config.response_format, sizeof(p.config.response_format),
                 "{\"type\":\"json_schema\",\"json_schema\":{\"name\":\"test\"}}");
        char *body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, false);
        TEST("json_mode=false+explicit uses explicit", body && strstr(body, "json_schema"));
        TEST("json_mode=false+explicit no json_object", body && !strstr(body, "\"json_object\""));
        free(body);
    }

    /* 12. json_mode=true with streaming */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        p.config.response_format[0] = '\0';
        char *body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, true);
        TEST("json_mode with streaming", has_json_mode(body));
        free(body);
    }

    /* 13. json_mode=true with empty messages (0 messages) */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        p.config.response_format[0] = '\0';
        char *body = PROVIDER_OPS_OPENAI.build_request_body(&p, NULL, 0, NULL, false);
        TEST("json_mode empty messages returns non-NULL", body != NULL);
        TEST("json_mode empty messages body is valid", body && body[0] == '{');
        free(body);
    }

    /* 14. json_mode with response_format_strict */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        p.config.response_format_strict = true;
        p.config.response_format[0] = '\0';
        char *body = PROVIDER_OPS_OPENAI.build_request_body(&p, test_msgs, 1, NULL, false);
        TEST("json_mode strict has json_object", has_json_mode(body));
        TEST("json_mode strict body valid", body != NULL);
        free(body);
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
