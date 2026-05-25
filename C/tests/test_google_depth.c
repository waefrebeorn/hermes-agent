/*
 * test_google_depth.c — B30/B31: Google generation_config depth tests
 *
 * Tests: top_k, candidate_count in gen_config, system_instruction.
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

static const message_t msg = {.role = MSG_USER, .content = (char*)"hello"};
static const message_t *msgs[] = {&msg};

int main(void) {
    printf("=== B30/B31: Google provider depth ===\n\n");

    /* B30: top_k + candidate_count in gen_config */
    {
        provider_t p = {0};
        p.config.top_k = 40;
        p.config.candidate_count = 2;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("topK present", body && strstr(body, "\"topK\":40"));
        TEST("candidateCount present", body && strstr(body, "\"candidateCount\":2"));
        free(body);
    }

    /* B30: defaults (0) omit both fields */
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("topK omitted when 0", body && !strstr(body, "\"topK\""));
        TEST("candidateCount omitted when 0", body && !strstr(body, "\"candidateCount\""));
        free(body);
    }

    /* B31: system_instruction from MSG_SYSTEM messages */
    {
        provider_t p = {0};
        message_t sys = {.role = MSG_SYSTEM, .content = (char*)"Be helpful and concise"};
        const message_t *msgs_sys[] = {&sys, &msg};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs_sys, 2, NULL, false);
        TEST("systemInstruction present", body && strstr(body, "\"systemInstruction\""));
        TEST("systemInstruction has text", body && strstr(body, "Be helpful and concise"));
        free(body);
    }

    /* B31: no system_instruction when no MSG_SYSTEM */
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("no systemInstruction when absent", body && !strstr(body, "\"systemInstruction\""));
        free(body);
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
