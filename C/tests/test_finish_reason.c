/*
 * test_finish_reason.c — B22: finish_reason tracking in stream chunks
 *
 * Tests: all providers extract finish_reason from stream chunks,
 * and it's stored in provider_response_t.
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

int main(void) {
    printf("=== B22: finish_reason tracking ===\n\n");

    /* OpenAI-style "stop" chunk */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}";
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, chunk);
        TEST("OpenAI finish_reason stop", r && strcmp(r->finish_reason, "stop") == 0);
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* OpenAI-style "length" chunk */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"length\"}]}";
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, chunk);
        TEST("OpenAI finish_reason length", r && strcmp(r->finish_reason, "length") == 0);
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* OpenAI-style "tool_calls" */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"tool_calls\"}]}";
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, chunk);
        TEST("OpenAI finish_reason tool_calls", r && strcmp(r->finish_reason, "tool_calls") == 0);
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* OpenAI-style no finish_reason (content-only chunk) */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"}}]}";
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, chunk);
        TEST("OpenAI no finish_reason", r && r->finish_reason[0] == '\0');
        TEST("OpenAI content preserved", r && strcmp(r->content, "Hello") == 0);
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* [DONE] has no finish_reason */
    {
        provider_t p = {0};
        const char *chunk = "data: [DONE]";
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, chunk);
        TEST("OpenAI DONE has no finish_reason", r && r->finish_reason[0] == '\0');
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* Anthropic message_delta with stop_reason */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":42}}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("Anthropic stop_reason end_turn", r && strcmp(r->finish_reason, "end_turn") == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Anthropic content_block_delta (no stop_reason) */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("Anthropic text delta no finish_reason", r && r->finish_reason[0] == '\0');
        PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Google finishReason in candidate */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"parts\":[{\"text\":\"\"}]}}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, chunk);
        TEST("Google finishReason STOP", r && strcmp(r->finish_reason, "STOP") == 0);
        PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* No finishReason in candidate */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, chunk);
        TEST("Google no finishReason", r && r->finish_reason[0] == '\0');
        PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* DeepSeek finish_reason */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}";
        provider_response_t *r = PROVIDER_OPS_DEEPSEEK.parse_stream_chunk(&p, chunk);
        TEST("DeepSeek finish_reason stop", r && strcmp(r->finish_reason, "stop") == 0);
        PROVIDER_OPS_DEEPSEEK.free_response(r);
    }

    /* xAI finish_reason */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}";
        provider_response_t *r = PROVIDER_OPS_XAI.parse_stream_chunk(&p, chunk);
        TEST("xAI finish_reason stop", r && strcmp(r->finish_reason, "stop") == 0);
        PROVIDER_OPS_XAI.free_response(r);
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
