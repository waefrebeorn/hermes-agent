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
        TEST("Google finishReason STOP", r && strcmp(r->finish_reason, "stop") == 0);
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

    /* xAI length finish_reason */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"length\"}]}";
        provider_response_t *r = PROVIDER_OPS_XAI.parse_stream_chunk(&p, chunk);
        TEST("xAI finish_reason length", r && strcmp(r->finish_reason, "length") == 0);
        PROVIDER_OPS_XAI.free_response(r);
    }

    /* OpenRouter finish_reason stop */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}";
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_stream_chunk(&p, chunk);
        TEST("OpenRouter finish_reason stop", r && strcmp(r->finish_reason, "stop") == 0);
        PROVIDER_OPS_OPENROUTER.free_response(r);
    }

    /* OpenRouter finish_reason content_filter */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"content_filter\"}]}";
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_stream_chunk(&p, chunk);
        TEST("OpenRouter finish_reason content_filter", r && strcmp(r->finish_reason, "content_filter") == 0);
        PROVIDER_OPS_OPENROUTER.free_response(r);
    }

    /* Azure finish_reason stop (OpenAI-compatible) */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(&p, chunk);
        TEST("Azure finish_reason stop", r && strcmp(r->finish_reason, "stop") == 0);
        PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Azure finish_reason length */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"length\"}]}";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(&p, chunk);
        TEST("Azure finish_reason length", r && strcmp(r->finish_reason, "length") == 0);
        PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Bedrock stop reason via normalize_converse_response */
    {
        json_t *response = json_parse("{\"stopReason\":\"end_turn\",\"output\":{\"message\":{\"content\":[{\"text\":\"\"}]}}}", NULL);
        TEST("Bedrock parse end_turn response", response != NULL);
        json_t *norm = bedrock_normalize_converse_response(response);
        json_t *choices = norm ? json_obj_get(norm, "choices") : NULL;
        json_t *choice = (choices && json_len(choices) > 0) ? json_get(choices, 0) : NULL;
        const char *fr = choice ? json_get_str(choice, "finish_reason", "") : "";
        TEST("Bedrock end_turn -> stop", strcmp(fr, "stop") == 0);
        json_free(norm);
        json_free(response);
    }

    /* Bedrock tool_use stop reason */
    {
        json_t *response = json_parse("{\"stopReason\":\"tool_use\",\"output\":{\"message\":{\"content\":[{\"text\":\"\"}]}}}", NULL);
        json_t *norm = bedrock_normalize_converse_response(response);
        json_t *choices = norm ? json_obj_get(norm, "choices") : NULL;
        json_t *choice = (choices && json_len(choices) > 0) ? json_get(choices, 0) : NULL;
        const char *fr = choice ? json_get_str(choice, "finish_reason", "") : "";
        TEST("Bedrock tool_use -> tool_calls", strcmp(fr, "tool_calls") == 0);
        json_free(norm);
        json_free(response);
    }

    /* Bedrock max_tokens stop reason */
    {
        json_t *response = json_parse("{\"stopReason\":\"max_tokens\",\"output\":{\"message\":{\"content\":[{\"text\":\"\"}]}}}", NULL);
        json_t *norm = bedrock_normalize_converse_response(response);
        json_t *choices = norm ? json_obj_get(norm, "choices") : NULL;
        json_t *choice = (choices && json_len(choices) > 0) ? json_get(choices, 0) : NULL;
        const char *fr = choice ? json_get_str(choice, "finish_reason", "") : "";
        TEST("Bedrock max_tokens -> length", strcmp(fr, "length") == 0);
        json_free(norm);
        json_free(response);
    }

    /* Bedrock content_filtered stop reason */
    {
        json_t *response = json_parse("{\"stopReason\":\"content_filtered\",\"output\":{\"message\":{\"content\":[{\"text\":\"\"}]}}}", NULL);
        json_t *norm = bedrock_normalize_converse_response(response);
        json_t *choices = norm ? json_obj_get(norm, "choices") : NULL;
        json_t *choice = (choices && json_len(choices) > 0) ? json_get(choices, 0) : NULL;
        const char *fr = choice ? json_get_str(choice, "finish_reason", "") : "";
        TEST("Bedrock content_filtered -> content_filter", strcmp(fr, "content_filter") == 0);
        json_free(norm);
        json_free(response);
    }

    /* Bedrock NULL/empty response safety */
    {
        json_t *norm = bedrock_normalize_converse_response(NULL);
        TEST("Bedrock normalize NULL response returns NULL", norm == NULL);
    }

    /* NULL/empty chunk safety */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, NULL);
        TEST("OpenAI NULL chunk returns response", r != NULL);
        TEST("OpenAI NULL chunk empty finish_reason", r && r->finish_reason[0] == '\0');
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* Empty chunk */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, "");
        TEST("OpenAI empty chunk returns response", r != NULL);
        TEST("OpenAI empty chunk empty finish_reason", r && r->finish_reason[0] == '\0');
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* Malformed JSON chunk */
    {
        provider_t p = {0};
        const char *chunk = "data: {bad json}";
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, chunk);
        TEST("OpenAI malformed JSON returns response", r != NULL);
        TEST("OpenAI malformed JSON empty finish_reason", r && r->finish_reason[0] == '\0');
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* Non-stream (data: prefix missing) with finish_reason */
    {
        provider_t p = {0};
        const char *chunk = "{\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}";
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, chunk);
        TEST("OpenAI non-stream chunk extracts finish_reason", r && strcmp(r->finish_reason, "stop") == 0);
        PROVIDER_OPS_OPENAI.free_response(r);
    }

    /* DeepSeek finish_reason length */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"length\"}]}";
        provider_response_t *r = PROVIDER_OPS_DEEPSEEK.parse_stream_chunk(&p, chunk);
        TEST("DeepSeek finish_reason length", r && strcmp(r->finish_reason, "length") == 0);
        PROVIDER_OPS_DEEPSEEK.free_response(r);
    }

    /* Anthropic max_tokens stop_reason */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"max_tokens\"},\"usage\":{\"output_tokens\":4096}}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("Anthropic stop_reason max_tokens", r && strcmp(r->finish_reason, "max_tokens") == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Anthropic tool_use stop_reason */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":15}}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("Anthropic stop_reason tool_use", r && strcmp(r->finish_reason, "tool_use") == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Google MAX_TOKENS finishReason */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"candidates\":[{\"finishReason\":\"MAX_TOKENS\",\"content\":{\"parts\":[{\"text\":\"\"}]}}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, chunk);
        TEST("Google finishReason MAX_TOKENS", r && strcmp(r->finish_reason, "length") == 0);
        PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Google SAFETY finishReason */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"candidates\":[{\"finishReason\":\"SAFETY\",\"content\":{\"parts\":[{\"text\":\"\"}]}}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, chunk);
        TEST("Google finishReason SAFETY -> content_filter", r && strcmp(r->finish_reason, "content_filter") == 0);
        PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Google content with text update + finishReason in same chunk */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"parts\":[{\"text\":\"Final answer\"}]}}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, chunk);
        TEST("Google stop with content", r && strcmp(r->finish_reason, "stop") == 0);
        TEST("Google content preserved with stop", r && strcmp(r->content, "Final answer") == 0);
        PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
