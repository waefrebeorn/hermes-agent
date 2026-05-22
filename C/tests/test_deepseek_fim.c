/*
 * test_deepseek_fim.c — B32: DeepSeek FIM (Fill-in-the-Middle) tests.
 * Tests: FIM URL building, request body, response parsing, error handling.
 */
#include "hermes.h"
#include "hermes_json.h"
#include "provider.h"
#include "json.h"
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

int main(void) {
    printf("=== DeepSeek FIM Tests (B32) ===\n\n");

    /* ──────────── FIM URL building ──────────── */

    printf("--- FIM URL ---\n");

    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_DEEPSEEK.build_fim_url(&p,
            "https://api.deepseek.com/v1");
        TEST("FIM URL replaces /chat/completions with /beta/completions",
             url && strstr(url, "/beta/completions") != NULL);
        TEST("FIM URL does not contain chat/completions",
             url && strstr(url, "/chat/completions") == NULL);
        free(url);
    }

    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_DEEPSEEK.build_fim_url(&p,
            "https://api.deepseek.com/v1/chat/completions");
        TEST("FIM URL strips chat/completions suffix",
             url && strstr(url, "/beta/completions") != NULL);
        TEST("FIM URL no double slash",
             url && strstr(url, "//beta") == NULL);
        free(url);
    }

    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_DEEPSEEK.build_fim_url(&p, NULL);
        TEST("FIM URL defaults to deepseek.com when base_url is NULL",
             url && strstr(url, "api.deepseek.com") != NULL);
        free(url);
    }

    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_DEEPSEEK.build_fim_url(&p, "");
        TEST("FIM URL defaults when base_url is empty",
             url && strstr(url, "api.deepseek.com") != NULL);
        free(url);
    }

    /* ──────────── FIM request body ──────────── */

    printf("\n--- FIM Request Body ---\n");

    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "deepseek-coder");
        char *body = PROVIDER_OPS_DEEPSEEK.build_fim_body(&p,
            "def hello():", "print('world')", 100);
        TEST("FIM body not NULL", body != NULL);

        json_t *j = json_parse(body, NULL);
        TEST("FIM body is valid JSON", j != NULL);
        TEST("FIM body has model field",
             j && strcmp(json_get_str(j, "model", ""), "deepseek-coder") == 0);
        TEST("FIM body has prompt field",
             j && strcmp(json_get_str(j, "prompt", ""), "def hello():") == 0);
        TEST("FIM body has suffix field",
             j && strcmp(json_get_str(j, "suffix", ""), "print('world')") == 0);
        TEST("FIM body has max_tokens=100",
             j && (int)json_get_num(j, "max_tokens", 0) == 100);
        {
            json_t *s = json_object_get(j, "stream");
            TEST("FIM body has stream=false",
                 s && s->type == JSON_BOOL && s->bool_val == false);
        }
        json_free(j);
        free(body);
    }

    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "deepseek-chat");
        /* Test without suffix (prefix-only FIM) */
        char *body = PROVIDER_OPS_DEEPSEEK.build_fim_body(&p, "function foo()", NULL, 0);
        TEST("FIM body without suffix is valid JSON", body != NULL);

        json_t *j = json_parse(body, NULL);
        TEST("FIM without suffix: default max_tokens=256",
             j && (int)json_get_num(j, "max_tokens", 0) == 256);
        TEST("FIM without suffix: no suffix field when NULL",
             j && json_object_get(j, "suffix") == NULL);
        json_free(j);
        free(body);
    }

    {
        /* Test model default (when model field not set) */
        provider_t p = {0};
        char *body = PROVIDER_OPS_DEEPSEEK.build_fim_body(&p, "test", NULL, 50);
        json_t *j = json_parse(body, NULL);
        TEST("FIM body defaults model to deepseek-chat",
             j && strcmp(json_get_str(j, "model", ""), "deepseek-chat") == 0);
        json_free(j);
        free(body);
    }

    /* ──────────── FIM response parsing ──────────── */

    printf("\n--- FIM Response Parsing ---\n");

    {
        /* Normal FIM response */
        const char *fim_resp =
            "{\"id\":\"fim-123\",\"object\":\"text_completion\","
            "\"choices\":[{\"index\":0,\"text\":\"return 42\\n\",\"finish_reason\":\"stop\","
            "\"logprobs\":null}],"
            "\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":5,\"total_tokens\":15}}";

        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_DEEPSEEK.parse_fim_response(&p, fim_resp);
        TEST("FIM parse: response not NULL", r != NULL);
        TEST("FIM parse: content matches", r && strcmp(r->content, "return 42\n") == 0);
        TEST("FIM parse: finish_reason=stop",
             r && strcmp(r->finish_reason, "stop") == 0);
        TEST("FIM parse: input_tokens=10", r && r->input_tokens == 10);
        TEST("FIM parse: output_tokens=5", r && r->output_tokens == 5);
        PROVIDER_OPS_DEEPSEEK.free_response(r);
    }

    {
        /* Empty choices */
        const char *fim_resp = "{\"id\":\"fim-0\",\"object\":\"text_completion\","
                               "\"choices\":[],\"usage\":{}}";
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_DEEPSEEK.parse_fim_response(&p, fim_resp);
        TEST("FIM empty choices: content empty string", r && strcmp(r->content, "") == 0);
        PROVIDER_OPS_DEEPSEEK.free_response(r);
    }

    {
        /* API error response */
        const char *err_resp = "{\"error\":{\"message\":\"Insufficient balance\","
                               "\"type\":\"insufficient_quota\"}}";
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_DEEPSEEK.parse_fim_response(&p, err_resp);
        TEST("FIM error: content mentions DeepSeek API error",
             r && strstr(r->content, "DeepSeek FIM API error") != NULL);
        TEST("FIM error: content mentions insufficient balance",
             r && strstr(r->content, "Insufficient balance") != NULL);
        PROVIDER_OPS_DEEPSEEK.free_response(r);
    }

    {
        /* Malformed JSON */
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_DEEPSEEK.parse_fim_response(&p, "{bad json");
        TEST("FIM malformed JSON: content mentions error",
             r && strstr(r->content, "JSON parse error") != NULL);
        PROVIDER_OPS_DEEPSEEK.free_response(r);
    }

    {
        /* NULL response body */
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_DEEPSEEK.parse_fim_response(&p, NULL);
        TEST("FIM NULL body: content is empty string", r && strcmp(r->content, "") == 0);
        PROVIDER_OPS_DEEPSEEK.free_response(r);
    }

    /* ──────────── FIM dispatch ──────────── */

    printf("\n--- FIM Dispatch ---\n");

    {
        /* provider_has_fim with DeepSeek ops */
        provider_t p = {0};
        p.ops = &PROVIDER_OPS_DEEPSEEK;
        TEST("DeepSeek has FIM support", provider_has_fim(&p));
    }

    {
        /* provider_has_fim with OpenAI ops (no FIM) */
        provider_t p = {0};
        p.ops = &PROVIDER_OPS_OPENAI;
        TEST("OpenAI does not have FIM support", !provider_has_fim(&p));
    }

    {
        /* provider_has_fim with NULL provider */
        TEST("NULL provider no FIM", !provider_has_fim(NULL));
    }

    {
        /* provider_fim with NULL args */
        TEST("provider_fim NULL provider", provider_fim(NULL, "test", NULL, 50) == NULL);
    }

    {
        /* provider_fim with non-FIM provider returns NULL */
        provider_t p = {0};
        p.ops = &PROVIDER_OPS_OPENAI;
        TEST("provider_fim non-FIM provider returns NULL",
             provider_fim(&p, "test", NULL, 50) == NULL);
    }

    {
        /* provider_fim with FIM provider but NULL prompt */
        provider_t p = {0};
        p.ops = &PROVIDER_OPS_DEEPSEEK;
        TEST("provider_fim NULL prompt returns NULL",
             provider_fim(&p, NULL, NULL, 50) == NULL);
    }

    /* ──────────── Summary ──────────── */

    printf("\n========================================\n");
    printf("  Results: %s\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    printf("  Failures: %d\n", failures);
    printf("========================================\n");

    return failures > 0 ? 1 : 0;
}
