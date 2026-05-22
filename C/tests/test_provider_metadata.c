/*
 * test_provider_metadata.c — Quick verification test for provider metadata (P85).
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include test_provider_metadata.c \
 *       src/agent/provider_metadata.c -o /tmp/test_provmeta -lm
 *
 * Run:
 *   /tmp/test_provmeta
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "provider_metadata.h"

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        printf("  \xe2\x9c\x93 %s\n", name); \
        passed++; \
    } else { \
        printf("  \xe2\x9c\x97 %s (FAILED)\n", name); \
        failed++; \
    } \
} while(0)

int main(void) {
    printf("=== Provider Metadata Tests (P85) ===\n\n");

    /* --- Test 1: Provider metadata lookup --- */
    printf("[P85] Provider metadata:\n");

    const provider_metadata_t *pm;

    pm = provider_metadata_find("openai");
    TEST("find openai", pm != NULL);
    TEST("openai supports streaming", pm && pm->supports_streaming);
    TEST("openai supports tool calling", pm && pm->supports_tool_calling);

    pm = provider_metadata_find("anthropic");
    TEST("find anthropic", pm != NULL);
    TEST("anthropic supports tool calling", pm && pm->supports_tool_calling);
    TEST("anthropic supports thinking", pm && pm->supports_thinking);

    pm = provider_metadata_find("deepseek");
    TEST("find deepseek", pm != NULL);
    TEST("deepseek supports streaming", pm && pm->supports_streaming);

    pm = provider_metadata_find("xai");
    TEST("find xai", pm != NULL);

    pm = provider_metadata_find("azure");
    TEST("find azure", pm != NULL);

    pm = provider_metadata_find("bedrock");
    TEST("find bedrock", pm != NULL);

    pm = provider_metadata_find("nonexistent_provider");
    TEST("unknown provider returns NULL", pm == NULL);

    /* Case insensitivity */
    pm = provider_metadata_find("OpenAI");
    TEST("case-insensitive 'OpenAI'", pm != NULL);

    pm = provider_metadata_find("DEEPSEEK");
    TEST("case-insensitive 'DEEPSEEK'", pm != NULL);

    /* --- Test 2: Model metadata lookup --- */
    printf("\n[P85] Model metadata lookup:\n");

    const model_metadata_t *mm;

    mm = model_metadata_find("gpt-4o");
    TEST("find gpt-4o", mm != NULL);
    TEST("gpt-4o has function calling", mm && model_has_capability(mm, MODEL_CAP_FUNCTION_CALLING));
    TEST("gpt-4o has vision", mm && model_has_capability(mm, MODEL_CAP_VISION));
    TEST("gpt-4o has streaming", mm && model_has_capability(mm, MODEL_CAP_STREAMING));

    mm = model_metadata_find("claude-3.5-sonnet");
    TEST("find claude-3.5-sonnet", mm != NULL);
    TEST("claude-3.5 has thinking", mm && model_has_capability(mm, MODEL_CAP_THINKING));

    mm = model_metadata_find("gemini-2.0-flash");
    TEST("find gemini-2.0-flash", mm != NULL);
    TEST("gemini has context caching", mm && model_has_capability(mm, MODEL_CAP_CONTEXT_CACHING));

    mm = model_metadata_find("deepseek-chat");
    TEST("find deepseek-chat", mm != NULL);
    TEST("deepseek-chat context window 65536", mm && mm->context_window == 65536);

    mm = model_metadata_find("deepseek-reasoner");
    TEST("find deepseek-reasoner", mm != NULL);
    TEST("deepseek-reasoner has thinking", mm && model_has_capability(mm, MODEL_CAP_THINKING));

    /* Prefix matching */
    mm = model_metadata_find("gpt-4o-mini-2025-01-01");
    TEST("prefix match gpt-4o-mini-*", mm != NULL);
    TEST("prefix match is gpt-4o-mini", mm != NULL && strcmp(mm->model_prefix, "gpt-4o-mini") == 0);

    mm = model_metadata_find("gpt-4.1-nano");
    TEST("prefix match gpt-4.1-*", mm != NULL);
    TEST("prefix match is gpt-4.1", mm != NULL && strcmp(mm->model_prefix, "gpt-4.1") == 0);

    /* Model name without matching prefix */
    mm = model_metadata_find("completely-unknown-model-v99");
    TEST("unknown model returns NULL", mm == NULL);

    /* --- Test 3: Capability queries --- */
    printf("\n[P85] Capability query helpers:\n");

    TEST("has_capability gpt-4o vision",
         model_name_has_capability("gpt-4o", MODEL_CAP_VISION));
    TEST("has_capability gpt-3.5 NOT vision",
         !model_name_has_capability("gpt-3.5", MODEL_CAP_VISION));
    TEST("has_capability deepseek-chat context caching",
         model_name_has_capability("deepseek-chat", MODEL_CAP_CONTEXT_CACHING));
    TEST("has_capability unknown model NOT streaming",
         !model_name_has_capability("unknown-model", MODEL_CAP_STREAMING));

    /* --- Test 4: Context window / max output --- */
    printf("\n[P85] Context/max output:\n");

    TEST("context gpt-4o = 131072", model_context_window("gpt-4o") == 131072);
    TEST("context gemini-2.0-pro = 1048576", model_context_window("gemini-2.0-pro") == 1048576);
    TEST("context unknown = -1", model_context_window("no-such-model") == -1);

    TEST("max_output gpt-4o = 16384", model_max_output("gpt-4o") == 16384);
    TEST("max_output unknown = -1", model_max_output("no-such-model") == -1);

    /* --- Test 5: Cost estimation --- */
    printf("\n[P85] Cost estimation:\n");

    double cost;

    cost = model_estimate_cost("gpt-4o", 1000, 500);
    /* gpt-4o: $2.50/M input, $10/M output */
    /* 1000 input = 0.001M * 2.50 = $0.0025, 500 output = 0.0005M * 10 = $0.005 */
    /* total = $0.0075 */
    TEST("gpt-4o 1000+500 tokens cost ~$0.0075",
         cost > 0.007 && cost < 0.008);

    cost = model_estimate_cost("deepseek-chat", 100000, 1000);
    /* deepseek-chat: $0.27/M input, $1.10/M output */
    /* 100000 input = 0.1M * 0.27 = $0.027, 1000 output = 0.001M * 1.10 = $0.0011 */
    /* total ~$0.0281 */
    TEST("deepseek-chat 100K+1K tokens cost ~$0.028",
         cost > 0.025 && cost < 0.031);

    /* Unknown model uses fallback ($1/M in, $4/M out) */
    cost = model_estimate_cost("unknown-model-42", 1000, 500);
    TEST("unknown model uses fallback pricing",
         cost > 0.002 && cost < 0.004);

    /* --- Test 6: Metadata listing */ 
    printf("\n[P85] JSON listing:\n");

    char *json;

    json = model_metadata_list_json();
    TEST("model list JSON non-null", json != NULL);
    TEST("model list includes gpt-4o", json && strstr(json, "gpt-4o") != NULL);
    TEST("model list includes deepseek-chat", json && strstr(json, "deepseek-chat") != NULL);
    TEST("model list includes gemini-2.0-flash", json && strstr(json, "gemini-2.0-flash") != NULL);
    free(json);

    json = provider_metadata_list_json();
    TEST("provider list JSON non-null", json != NULL);
    TEST("provider list includes openai", json && strstr(json, "openai") != NULL);
    TEST("provider list includes deepseek", json && strstr(json, "deepseek") != NULL);
    TEST("provider list includes azure", json && strstr(json, "azure") != NULL);
    TEST("provider list includes bedrock", json && strstr(json, "bedrock") != NULL);
    free(json);

    /* --- Summary --- */
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
