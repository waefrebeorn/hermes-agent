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

/* Forward declarations for R10 utility test functions */
static void test_model_grok_supports_reasoning_effort(void);
static void test_provider_is_openrouter_base_url(void);
static void test_provider_is_custom_endpoint(void);
static void test_provider_is_known_base_url(void);
static void test_provider_auth_headers(void);
static void test_provider_coerce_reasonable_int(void);
static void test_estimate_tokens_rough(void);
static void test_provider_resolve_requests_verify(void);
static void test_provider_extract_context_length(void);
static void test_provider_extract_max_completion_tokens(void);
static void test_provider_extract_pricing(void);
static void test_estimate_count_image_tokens(void);
static void test_estimate_message_chars(void);
static void test_estimate_messages_tokens_rough(void);
static void test_estimate_request_tokens_rough(void);
static void test_get_next_probe_tier(void);
static void test_provider_context_cache_path(void);
static void test_provider_context_cache_save_get_invalidate(void);
static void test_provider_extract_first_int(void);
static void test_provider_detect_local_server_type(void);
static void test_provider_query_ollama_api_show(void);
static void test_provider_query_local_context_length(void);
static void test_provider_add_model_aliases(void);
static void test_provider_get_context_length_from_provider_error(void);

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

    /* --- S06: vision_overrides tests --- */
    {
        provider_config_t pcfg;
        memset(&pcfg, 0, sizeof(pcfg));

        /* No override set: unknown model should NOT have vision */
        TEST("unknown model no vision without override",
             !model_supports_vision("unknown-model", &pcfg));

        /* Override set: model matching prefix should have vision */
        strncpy(pcfg.vision_overrides, "qwen-vl,llava,pixtral", sizeof(pcfg.vision_overrides) - 1);
        TEST("qwen-vl has vision via override",
             model_supports_vision("qwen-vl", &pcfg));
        TEST("qwen-vl-72b has vision via prefix override",
             model_supports_vision("qwen-vl-72b", &pcfg));
        TEST("llava has vision via override",
             model_supports_vision("llava", &pcfg));
        TEST("pixtral-12b has vision via override",
             model_supports_vision("pixtral-12b", &pcfg));

        /* Non-matching should NOT get extra vision from override alone */
        /* Note: gpt-4o-mini already has vision via metadata (MODEL_CAP_VISION) */
        TEST("model without override and no metadata has no vision",
             !model_supports_vision("unknown-nonvision-model", &pcfg));

        /* Known model (gpt-4o) should still have vision from metadata */
        TEST("gpt-4o has vision via metadata even with overrides set",
             model_supports_vision("gpt-4o", &pcfg));

        /* Empty overrides — no effect */
        pcfg.vision_overrides[0] = '\0';
        TEST("unknown model no vision with empty overrides",
             !model_supports_vision("unknown-model", &pcfg));

        /* Global supports_vision still works */
        pcfg.supports_vision = true;
        TEST("global supports_vision true overrides all",
             model_supports_vision("anything-here", &pcfg));
    }

    /* --- Test 7: provider_normalize_base_url --- */
    printf("\n[R10] provider_normalize_base_url:\n");

    char *url;
    url = provider_normalize_base_url("https://api.openai.com/v1/");
    TEST("normalize strips trailing slash", url && strcmp(url, "https://api.openai.com/v1") == 0);
    free(url);

    url = provider_normalize_base_url("https://api.openai.com");
    TEST("normalize no slash unchanged", url && strcmp(url, "https://api.openai.com") == 0);
    free(url);

    url = provider_normalize_base_url("  http://localhost:11434  ");
    TEST("normalize strips whitespace", url && strcmp(url, "http://localhost:11434") == 0);
    free(url);

    url = provider_normalize_base_url("");
    TEST("normalize empty returns NULL", url == NULL);

    url = provider_normalize_base_url(NULL);
    TEST("normalize NULL returns NULL", url == NULL);

    /* --- Test 8: provider_strip_prefix --- */
    printf("\n[R10] provider_strip_prefix:\n");

    url = provider_strip_prefix("openrouter/gpt-4");
    TEST("strip slash prefix", url && strcmp(url, "gpt-4") == 0);
    free(url);

    url = provider_strip_prefix("anthropic/claude-sonnet-4");
    TEST("strip anthropic slash", url && strcmp(url, "claude-sonnet-4") == 0);
    free(url);

    url = provider_strip_prefix("gpt-4o");
    TEST("no prefix unchanged", url && strcmp(url, "gpt-4o") == 0);
    free(url);

    url = provider_strip_prefix("qwen3.5:27b");
    TEST("model:tag preserved", url && strcmp(url, "qwen3.5:27b") == 0);
    free(url);

    url = provider_strip_prefix("deepseek:latest");
    TEST("model:latest preserved", url && strcmp(url, "deepseek:latest") == 0);
    free(url);

    url = provider_strip_prefix("openrouter:gpt-4");
    TEST("strip colon prefix", url && strcmp(url, "gpt-4") == 0);
    free(url);

    url = provider_strip_prefix("nous:hermes-3");
    TEST("strip nous colon", url && strcmp(url, "hermes-3") == 0);
    free(url);

    url = provider_strip_prefix(NULL);
    TEST("strip NULL returns NULL", url == NULL);

    url = provider_strip_prefix("");
    TEST("strip empty returns NULL", url == NULL);

    url = provider_strip_prefix("http://localhost:11434/v1/chat/completions");
    TEST("strip http URL unchanged", url && strncmp(url, "http", 4) == 0);
    free(url);

    /* --- Test 9: provider_is_local_endpoint --- */
    printf("\n[R10] provider_is_local_endpoint:\n");

    TEST("localhost is local", provider_is_local_endpoint("http://localhost:11434"));
    TEST("127.0.0.1 is local", provider_is_local_endpoint("http://127.0.0.1:8080"));
    TEST("::1 is local", provider_is_local_endpoint("http://[::1]:11434"));
    TEST("0.0.0.0 is local", provider_is_local_endpoint("http://0.0.0.0"));
    TEST("host.docker.internal is local",
         provider_is_local_endpoint("http://host.docker.internal:11434"));
    TEST("10.0.0.1 is local", provider_is_local_endpoint("http://10.0.0.1"));
    TEST("172.16.0.1 is local", provider_is_local_endpoint("http://172.16.0.1"));
    TEST("172.31.255.255 is local", provider_is_local_endpoint("http://172.31.255.255"));
    TEST("192.168.1.1 is local", provider_is_local_endpoint("http://192.168.1.1"));
    TEST("100.64.0.1 is local (Tailscale)",
         provider_is_local_endpoint("http://100.64.0.1"));
    TEST("100.127.255.255 is local (Tailscale)",
         provider_is_local_endpoint("http://100.127.255.255"));
    TEST("169.254.1.1 is local (link-local)",
         provider_is_local_endpoint("http://169.254.1.1"));
    TEST("172.15.0.1 NOT local", !provider_is_local_endpoint("http://172.15.0.1"));
    TEST("172.32.0.1 NOT local", !provider_is_local_endpoint("http://172.32.0.1"));
    TEST("192.167.1.1 NOT local", !provider_is_local_endpoint("http://192.167.1.1"));
    TEST("100.63.0.1 NOT local", !provider_is_local_endpoint("http://100.63.0.1"));
    TEST("100.128.0.1 NOT local", !provider_is_local_endpoint("http://100.128.0.1"));
    TEST("8.8.8.8 NOT local", !provider_is_local_endpoint("http://8.8.8.8"));
    TEST("NULL not local", !provider_is_local_endpoint(NULL));
    TEST("empty not local", !provider_is_local_endpoint(""));

    /* --- Test 10: provider_infer_from_url --- */
    printf("\n[R10] provider_infer_from_url:\n");

    char *prov;

    prov = provider_infer_from_url("https://api.openai.com/v1/chat");
    TEST("openai URL -> openai", prov && strcmp(prov, "openai") == 0);
    free(prov);

    prov = provider_infer_from_url("https://api.anthropic.com/v1");
    TEST("anthropic URL -> anthropic", prov && strcmp(prov, "anthropic") == 0);
    free(prov);

    prov = provider_infer_from_url("https://openrouter.ai/api/v1");
    TEST("openrouter URL -> openrouter", prov && strcmp(prov, "openrouter") == 0);
    free(prov);

    prov = provider_infer_from_url("https://api.deepseek.com/v1");
    TEST("deepseek URL -> deepseek", prov && strcmp(prov, "deepseek") == 0);
    free(prov);

    prov = provider_infer_from_url("https://api.x.ai/v1");
    TEST("xai URL -> xai", prov && strcmp(prov, "xai") == 0);
    free(prov);

    prov = provider_infer_from_url("https://api.groq.com/openai/v1");
    TEST("groq URL -> groq", prov && strcmp(prov, "groq") == 0);
    free(prov);

    prov = provider_infer_from_url("https://api.fireworks.ai/v1");
    TEST("fireworks URL -> fireworks", prov && strcmp(prov, "fireworks") == 0);
    free(prov);

    prov = provider_infer_from_url("http://localhost:11434");
    TEST("localhost -> local", prov && strcmp(prov, "local") == 0);
    free(prov);

    prov = provider_infer_from_url("https://unknown.example.com");
    TEST("unknown URL -> NULL", prov == NULL);

    prov = provider_infer_from_url(NULL);
    TEST("NULL -> NULL", prov == NULL);

    /* --- Test 11: provider_parse_context_limit_from_error --- */
    printf("\n[R10] provider_parse_context_limit_from_error:\n");

    int ctx;
    ctx = provider_parse_context_limit_from_error(
        "maximum context length is 32768 tokens");
    TEST("maximum context length is 32768", ctx == 32768);

    ctx = provider_parse_context_limit_from_error(
        "This model's maximum context length is 128000 tokens.");
    TEST("maximum context length is 128000", ctx == 128000);

    ctx = provider_parse_context_limit_from_error(
        "context_length_exceeded: 131072");
    TEST("context_length_exceeded: 131072", ctx == 131072);

    ctx = provider_parse_context_limit_from_error(
        "context length is 65536");
    TEST("context length is 65536", ctx == 65536);

    ctx = provider_parse_context_limit_from_error(
        "250000 tokens > 200000 maximum");
    TEST("250000 > 200000 maximum", ctx == 200000);

    ctx = provider_parse_context_limit_from_error(
        "max context size 32768 exceeded");
    TEST("max context size 32768 exceeded", ctx == 32768);

    ctx = provider_parse_context_limit_from_error("some random error");
    TEST("random error -> -1", ctx == -1);

    ctx = provider_parse_context_limit_from_error(NULL);
    TEST("NULL -> -1", ctx == -1);

    /* --- Test 12: provider_parse_available_output_tokens_from_error --- */
    printf("\n[R10] provider_parse_available_output_tokens_from_error:\n");

    int avail;
    avail = provider_parse_available_output_tokens_from_error(
        "max_tokens: 32768 > context_window: 200000 - input_tokens: 190000 = available_tokens: 10000");
    TEST("available_tokens: 10000", avail == 10000);

    avail = provider_parse_available_output_tokens_from_error(
        "max_tokens too large: max_tokens: 50000 > context_window: 128000 - input_tokens: 100000 = available_output_tokens: 28000");
    TEST("available_tokens: 28000", avail == 28000);

    avail = provider_parse_available_output_tokens_from_error(
        "prompt too long: 200000 > 128000");
    TEST("not a max_tokens error -> -1", avail == -1);

    avail = provider_parse_available_output_tokens_from_error(NULL);
    TEST("NULL -> -1", avail == -1);

    /* --- Test 13: provider_model_id_matches --- */
    printf("\n[R10] provider_model_id_matches:\n");

    TEST("exact match", provider_model_id_matches("claude-sonnet-4", "claude-sonnet-4") == true);
    TEST("slug match", provider_model_id_matches("anthropic/claude-sonnet-4", "claude-sonnet-4") == true);
    TEST("no match", provider_model_id_matches("gpt-4", "claude-sonnet-4") == false);
    TEST("null candidate", provider_model_id_matches(NULL, "claude-sonnet-4") == false);
    TEST("null lookup", provider_model_id_matches("claude-sonnet-4", NULL) == false);
    TEST("both null", provider_model_id_matches(NULL, NULL) == false);
    TEST("slug with no slash = exact", provider_model_id_matches("claude-sonnet-4", "claude-sonnet") == false);
    TEST("multi-segment slug", provider_model_id_matches("nvidia/nvidia-nemotron-super-49b-v1", "nvidia-nemotron-super-49b-v1") == true);

    /* --- Test 14: provider_model_suggests_kimi --- */
    printf("\n[R10] provider_model_suggests_kimi:\n");

    TEST("kimi-k2.6", provider_model_suggests_kimi("kimi-k2.6") == true);
    TEST("kimi-k2-thinking", provider_model_suggests_kimi("kimi-k2-thinking") == true);
    TEST("Kimi-K2.6 uppercase", provider_model_suggests_kimi("Kimi-K2.6") == true);
    TEST("moonshotai/Kimi-K2.6", provider_model_suggests_kimi("moonshotai/Kimi-K2.6") == true);
    TEST("claude not kimi", provider_model_suggests_kimi("claude-sonnet-4") == false);
    TEST("empty string", provider_model_suggests_kimi("") == false);
    TEST("null model", provider_model_suggests_kimi(NULL) == false);
    TEST("kimi alone", provider_model_suggests_kimi("kimi") == true);
    TEST("just moonshot", provider_model_suggests_kimi("moonshot") == true);

    /* --- Test 15: provider_normalize_model_version --- */
    printf("\n[R10] provider_normalize_model_version:\n");

    char *v;
    v = provider_normalize_model_version("claude-opus-4.6");
    TEST("dot to dash", v && strcmp(v, "claude-opus-4-6") == 0);
    free(v);

    v = provider_normalize_model_version("claude-sonnet-4.5");
    TEST("another dot version", v && strcmp(v, "claude-sonnet-4-5") == 0);
    free(v);

    v = provider_normalize_model_version("gpt-4");
    TEST("no dots unchanged", v && strcmp(v, "gpt-4") == 0);
    free(v);

    v = provider_normalize_model_version("");
    TEST("empty string", v && strcmp(v, "") == 0);
    free(v);

    v = provider_normalize_model_version(NULL);
    TEST("null returns null", v == NULL);
    free(v);

    v = provider_normalize_model_version("multiple.dots.version.1.2.3");
    TEST("multiple dots", v && strcmp(v, "multiple-dots-version-1-2-3") == 0);
    free(v);

    /* --- Summary --- */
    printf("\n=== R10 existing: %d passed, %d failed ===\n", passed, failed);

    /* ---- R10 utility functions ---- */
    test_model_grok_supports_reasoning_effort();
    test_provider_is_openrouter_base_url();
    test_provider_is_custom_endpoint();
    test_provider_is_known_base_url();
    test_provider_auth_headers();
    test_provider_coerce_reasonable_int();
    test_estimate_tokens_rough();
    test_provider_resolve_requests_verify();
    test_provider_extract_context_length();
    test_provider_extract_max_completion_tokens();
    test_provider_extract_pricing();
    test_estimate_count_image_tokens();
    test_estimate_message_chars();
    test_estimate_messages_tokens_rough();
    test_estimate_request_tokens_rough();
    test_get_next_probe_tier();
    test_provider_context_cache_path();
    test_provider_context_cache_save_get_invalidate();
    test_provider_extract_first_int();
    test_provider_detect_local_server_type();
    test_provider_query_ollama_api_show();
    test_provider_query_local_context_length();
    test_provider_add_model_aliases();
    test_provider_get_context_length_from_provider_error();

    printf("\n=== Overall: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}

/* ---- model_grok_supports_reasoning_effort ---- */
static void test_model_grok_supports_reasoning_effort(void) {
    printf("\n[R10] model_grok_supports_reasoning_effort:\n");
    TEST("null",            !model_grok_supports_reasoning_effort(NULL));
    TEST("grok-3-mini",      model_grok_supports_reasoning_effort("grok-3-mini"));
    TEST("grok-3-mini-beta", model_grok_supports_reasoning_effort("grok-3-mini-beta"));
    TEST("x-ai/grok-3-mini", model_grok_supports_reasoning_effort("x-ai/grok-3-mini"));
    TEST("openrouter/x-ai/grok-3-mini", model_grok_supports_reasoning_effort("openrouter/x-ai/grok-3-mini"));
    TEST("grok-4.3",         model_grok_supports_reasoning_effort("grok-4.3"));
    TEST("grok-4.20-multi-agent", model_grok_supports_reasoning_effort("grok-4.20-multi-agent"));
    TEST("grok-2",          !model_grok_supports_reasoning_effort("grok-2"));
    TEST("gpt-4",           !model_grok_supports_reasoning_effort("gpt-4"));
    TEST("empty",           !model_grok_supports_reasoning_effort(""));
}

/* ---- provider_is_openrouter_base_url ---- */
static void test_provider_is_openrouter_base_url(void) {
    printf("\n[R10] provider_is_openrouter_base_url:\n");
    TEST("null",            !provider_is_openrouter_base_url(NULL));
    TEST("openrouter.ai",    provider_is_openrouter_base_url("https://openrouter.ai/api/v1"));
    TEST("subdomain",        provider_is_openrouter_base_url("https://sub.openrouter.ai/v1"));
    TEST("not openrouter",  !provider_is_openrouter_base_url("https://api.openai.com/v1"));
    TEST("empty",           !provider_is_openrouter_base_url(""));
}

/* ---- provider_is_custom_endpoint ---- */
static void test_provider_is_custom_endpoint(void) {
    printf("\n[R10] provider_is_custom_endpoint:\n");
    TEST("null",            !provider_is_custom_endpoint(NULL));
    TEST("empty",           !provider_is_custom_endpoint(""));
    TEST("openrouter",      !provider_is_custom_endpoint("https://openrouter.ai/v1"));
    TEST("custom ollama",    provider_is_custom_endpoint("http://localhost:11434"));
    TEST("custom url",       provider_is_custom_endpoint("https://my-endpoint.example.com/api"));
}

/* ---- provider_is_known_base_url ---- */
static void test_provider_is_known_base_url(void) {
    printf("\n[R10] provider_is_known_base_url:\n");
    TEST("null",            !provider_is_known_base_url(NULL));
    TEST("openai",           provider_is_known_base_url("https://api.openai.com/v1"));
    TEST("anthropic",        provider_is_known_base_url("https://api.anthropic.com/v1"));
    TEST("deepseek",         provider_is_known_base_url("https://api.deepseek.com/v1"));
    TEST("unknown",         !provider_is_known_base_url("https://my-custom-server.example.com"));
    TEST("empty",           !provider_is_known_base_url(""));
}

/* ---- provider_auth_headers ---- */
static void test_provider_auth_headers(void) {
    printf("\n[R10] provider_auth_headers:\n");
    {
        json_t *h = provider_auth_headers(NULL);
        TEST("null key", h == NULL);
    }
    {
        json_t *h = provider_auth_headers("");
        TEST("empty key", h == NULL);
    }
    {
        json_t *h = provider_auth_headers("   ");
        TEST("whitespace key", h == NULL);
    }
    {
        json_t *h = provider_auth_headers("sk-test-123");
        TEST("non-null", h != NULL);
        const char *auth = json_get_str(h, "Authorization", NULL);
        TEST("Bearer prefix", auth && strncmp(auth, "Bearer ", 7) == 0);
        TEST("key present", auth && strstr(auth, "sk-test-123") != NULL);
        json_free(h);
    }
}

/* ---- provider_coerce_reasonable_int ---- */
static void test_provider_coerce_reasonable_int(void) {
    printf("\n[R10] provider_coerce_reasonable_int:\n");
    TEST("null",            provider_coerce_reasonable_int(NULL, 0, 10000) == -1);
    TEST("empty",           provider_coerce_reasonable_int("", 0, 10000) == -1);
    TEST("normal 4096",     provider_coerce_reasonable_int("4096", 1024, 10000000) == 4096);
    TEST("with commas",     provider_coerce_reasonable_int("8,192", 1024, 10000000) == 8192);
    TEST("127 below min",   provider_coerce_reasonable_int("127", 1024, 10000000) == -1);
    TEST("below min",       provider_coerce_reasonable_int("512", 1024, 10000000) == -1);
    TEST("above max",       provider_coerce_reasonable_int("99999999", 1024, 10000000) == -1);
    TEST("not a number",    provider_coerce_reasonable_int("abc", 0, 10000) == -1);
    TEST("bool false",      provider_coerce_reasonable_int("false", 0, 10000) == -1);
    TEST("leading spaces",  provider_coerce_reasonable_int("  16384", 1024, 10000000) == 16384);
}

/* ---- estimate_tokens_rough ---- */
static void test_estimate_tokens_rough(void) {
    printf("\n[R10] estimate_tokens_rough:\n");
    TEST("null",      estimate_tokens_rough(NULL) == 0);
    TEST("empty",     estimate_tokens_rough("") == 0);
    TEST("1 char",    estimate_tokens_rough("a") == 1);
    TEST("4 chars",   estimate_tokens_rough("abcd") == 1);
    TEST("5 chars",   estimate_tokens_rough("abcde") == 2);
    TEST("100 chars", estimate_tokens_rough("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") == 25);
    TEST("3 chars",   estimate_tokens_rough("abc") == 1);
    TEST("7 chars",   estimate_tokens_rough("abcdefg") == 2);
    TEST("8 chars",   estimate_tokens_rough("abcdefgh") == 2);
    TEST("1024 chars", estimate_tokens_rough("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") == 130);
}

/* ---- provider_resolve_requests_verify ---- */
static void test_provider_resolve_requests_verify(void) {
    printf("\n[R10] provider_resolve_requests_verify:\n");
    const char *saved = getenv("HERMES_VERIFY_SSL");

    unsetenv("HERMES_VERIFY_SSL");
    TEST("default verify", provider_resolve_requests_verify() == 1);

    setenv("HERMES_VERIFY_SSL", "0", 1);
    TEST("0 disables", provider_resolve_requests_verify() == 0);
    setenv("HERMES_VERIFY_SSL", "false", 1);
    TEST("false disables", provider_resolve_requests_verify() == 0);
    setenv("HERMES_VERIFY_SSL", "no", 1);
    TEST("no disables", provider_resolve_requests_verify() == 0);
    setenv("HERMES_VERIFY_SSL", "off", 1);
    TEST("off disables", provider_resolve_requests_verify() == 0);

    setenv("HERMES_VERIFY_SSL", "1", 1);
    TEST("1 enables", provider_resolve_requests_verify() == 1);
    setenv("HERMES_VERIFY_SSL", "true", 1);
    TEST("true enables", provider_resolve_requests_verify() == 1);
    setenv("HERMES_VERIFY_SSL", "yes", 1);
    TEST("yes enables", provider_resolve_requests_verify() == 1);
    setenv("HERMES_VERIFY_SSL", "on", 1);
    TEST("on enables", provider_resolve_requests_verify() == 1);

    setenv("HERMES_VERIFY_SSL", "/etc/ssl/certs/ca-certificates.crt", 1);
    TEST("custom path", provider_resolve_requests_verify() == -1);
    const char *p = provider_requests_verify_path();
    TEST("path returned", p != NULL);
    TEST("path matches", p && strcmp(p, "/etc/ssl/certs/ca-certificates.crt") == 0);

    /* Restore */
    if (saved) setenv("HERMES_VERIFY_SSL", saved, 1);
    else unsetenv("HERMES_VERIFY_SSL");
}

/* ---- provider_extract_context_length ---- */
static void test_provider_extract_context_length(void) {
    printf("\n[R10] provider_extract_context_length:\n");
    TEST("null payload", provider_extract_context_length(NULL) == -1);

    /* Simple payload with context_length */
    {
        json_t *p = json_object();
        json_set(p, "context_length", json_number(128000));
        int n = provider_extract_context_length(p);
        TEST("context_length 128000", n == 128000);
        json_free(p);
    }

    /* context_window */
    {
        json_t *p = json_object();
        json_set(p, "context_window", json_number(65536));
        TEST("context_window 65536", provider_extract_context_length(p) == 65536);
        json_free(p);
    }

    /* max_position_embeddings */
    {
        json_t *p = json_object();
        json_set(p, "max_position_embeddings", json_number(4096));
        TEST("max_position_embeddings", provider_extract_context_length(p) == 4096);
        json_free(p);
    }

    /* Nested payload */
    {
        json_t *inner = json_object();
        json_set(inner, "n_ctx", json_number(8192));
        json_t *p = json_object();
        json_set(p, "config", inner);
        TEST("nested n_ctx", provider_extract_context_length(p) == 8192);
        json_free(p);
    }

    /* Too small (below 1024 minimum) */
    {
        json_t *p = json_object();
        json_set(p, "context_length", json_number(127));
        TEST("too small returns -1", provider_extract_context_length(p) == -1);
        json_free(p);
    }

    /* No matching key */
    {
        json_t *p = json_object();
        json_set(p, "name", json_string("gpt-4o"));
        TEST("no matching key", provider_extract_context_length(p) == -1);
        json_free(p);
    }

    /* Empty object */
    {
        json_t *p = json_object();
        TEST("empty object", provider_extract_context_length(p) == -1);
        json_free(p);
    }
}

/* ---- provider_extract_max_completion_tokens ---- */
static void test_provider_extract_max_completion_tokens(void) {
    printf("\n[R10] provider_extract_max_completion_tokens:\n");
    TEST("null payload", provider_extract_max_completion_tokens(NULL) == -1);

    {
        json_t *p = json_object();
        json_set(p, "max_completion_tokens", json_number(16384));
        TEST("max_completion_tokens 16384",
             provider_extract_max_completion_tokens(p) == 16384);
        json_free(p);
    }

    {
        json_t *p = json_object();
        json_set(p, "max_output_tokens", json_number(8192));
        TEST("max_output_tokens 8192",
             provider_extract_max_completion_tokens(p) == 8192);
        json_free(p);
    }

    {
        json_t *p = json_object();
        json_set(p, "max_tokens", json_number(4096));
        TEST("max_tokens 4096",
             provider_extract_max_completion_tokens(p) == 4096);
        json_free(p);
    }

    {
        json_t *p = json_object();
        json_set(p, "unknown_key", json_number(5000));
        TEST("no matching key", provider_extract_max_completion_tokens(p) == -1);
        json_free(p);
    }

    {
        json_t *p = json_object();
        TEST("empty object", provider_extract_max_completion_tokens(p) == -1);
        json_free(p);
    }
}

/* ---- provider_extract_pricing ---- */
static void test_provider_extract_pricing(void) {
    printf("\n[R10] provider_extract_pricing:\n");
    json_t *pricing;

    /* NULL payload */
    pricing = provider_extract_pricing(NULL);
    TEST("null payload", pricing == NULL);

    /* Empty object */
    {
        json_t *p = json_object();
        pricing = provider_extract_pricing(p);
        TEST("empty object", pricing == NULL);
        json_free(p);
    }

    /* Novita pricing */
    {
        json_t *p = json_object();
        json_set(p, "input_token_price_per_m", json_number(150));
        json_set(p, "output_token_price_per_m", json_number(600));
        pricing = provider_extract_pricing(p);
        TEST("novita non-null", pricing != NULL);
        if (pricing) {
            const char *prompt = json_get_str(pricing, "prompt", "");
            const char *completion = json_get_str(pricing, "completion", "");
            TEST("novita prompt", prompt && strlen(prompt) > 0);
            TEST("novita completion", completion && strlen(completion) > 0);
        }
        json_free(pricing);
        json_free(p);
    }

    /* Standard prompt pricing via alias map */
    {
        json_t *pricing_obj = json_object();
        json_set(pricing_obj, "prompt", json_number(0.000003));
        json_t *p = json_object();
        json_set(p, "pricing", pricing_obj);

        pricing = provider_extract_pricing(p);
        TEST("standard prompt", pricing != NULL);
        if (pricing) {
            const char *prompt = json_get_str(pricing, "prompt", "");
            TEST("has prompt", prompt && strlen(prompt) > 0);
        }
        json_free(pricing);
        json_free(p);
    }

    /* Input alias */
    {
        json_t *p = json_object();
        json_set(p, "input", json_number(2.5e-6));
        pricing = provider_extract_pricing(p);
        TEST("input alias", pricing != NULL);
        if (pricing) {
            const char *prompt = json_get_str(pricing, "prompt", "");
            TEST("input -> prompt", prompt && strlen(prompt) > 0);
        }
        json_free(pricing);
        json_free(p);
    }

    /* Completion via output alias */
    {
        json_t *p = json_object();
        json_set(p, "output", json_number(1.0e-5));
        pricing = provider_extract_pricing(p);
        TEST("output alias", pricing != NULL);
        json_free(pricing);
        json_free(p);
    }

    /* Both prompt and completion */
    {
        json_t *p = json_object();
        json_set(p, "prompt", json_number(3e-6));
        json_set(p, "completion", json_number(1.5e-5));
        pricing = provider_extract_pricing(p);
        TEST("both pricing", pricing != NULL);
        if (pricing) {
            const char *prompt = json_get_str(pricing, "prompt", "");
            const char *completion = json_get_str(pricing, "completion", "");
            TEST("has both prompt", prompt && strlen(prompt) > 0);
            TEST("has both completion", completion && strlen(completion) > 0);
        }
        json_free(pricing);
        json_free(p);
    }

    /* Nested pricing (in sub-object) */
    {
        json_t *inner = json_object();
        json_set(inner, "prompt", json_number(5e-7));
        json_set(inner, "completion", json_number(2e-6));
        json_t *p = json_object();
        json_set(p, "costs", inner);
        pricing = provider_extract_pricing(p);
        TEST("nested pricing", pricing != NULL);
        json_free(pricing);
        json_free(p);
    }

    /* NULL values should be skipped */
    {
        json_t *p = json_object();
        json_set(p, "prompt", json_null());
        json_set(p, "completion", json_number(1.0e-5));
        pricing = provider_extract_pricing(p);
        TEST("skip null prompt", pricing != NULL);
        if (pricing) {
            const char *completion = json_get_str(pricing, "completion", "");
            TEST("has completion only", completion && strlen(completion) > 0);
            const char *prompt = json_get_str(pricing, "prompt", "__missing__");
            TEST("prompt absent", prompt && strcmp(prompt, "__missing__") == 0);
        }
        json_free(pricing);
        json_free(p);
    }

    /* Cache-related pricing */
    {
        json_t *p = json_object();
        json_set(p, "cache_read", json_number(1.0e-6));
        pricing = provider_extract_pricing(p);
        TEST("cache_read", pricing != NULL);
        if (pricing) {
            const char *cr = json_get_str(pricing, "cache_read", "");
            TEST("has cache_read", cr && strlen(cr) > 0);
        }
        json_free(pricing);
        json_free(p);
    }
}

/* ---- estimate_count_image_tokens ---- */
static void test_estimate_count_image_tokens(void) {
    printf("\n[R10] estimate_count_image_tokens:\n");

    /* NULL/empty */
    TEST("null msg",      estimate_count_image_tokens(NULL, 1500) == 0);

    /* No content field */
    {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        TEST("no content", estimate_count_image_tokens(msg, 1500) == 0);
        json_free(msg);
    }

    /* String content (not array) */
    {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("hello"));
        TEST("string content", estimate_count_image_tokens(msg, 1500) == 0);
        json_free(msg);
    }

    /* Empty content array */
    {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_array());
        TEST("empty array", estimate_count_image_tokens(msg, 1500) == 0);
        json_free(msg);
    }

    /* Single image */
    {
        json_t *msg = json_object();
        json_t *content = json_array();
        json_t *part = json_object();
        json_set(part, "type", json_string("image"));
        json_set(part, "source", json_string("data:..."));
        json_append(content, part);
        json_set(msg, "content", content);
        TEST("1 image", estimate_count_image_tokens(msg, 1500) == 1500);
        json_free(msg);
    }

    /* Multiple images */
    {
        json_t *msg = json_object();
        json_t *content = json_array();
        json_t *p1 = json_object();
        json_set(p1, "type", json_string("image_url"));
        json_set(p1, "url", json_string("http://example.com/img1.png"));
        json_append(content, p1);
        json_t *p2 = json_object();
        json_set(p2, "type", json_string("image_url"));
        json_set(p2, "url", json_string("http://example.com/img2.jpg"));
        json_append(content, p2);
        json_t *p3 = json_object();
        json_set(p3, "type", json_string("text"));
        json_set(p3, "text", json_string("not an image"));
        json_append(content, p3);
        json_set(msg, "content", content);
        TEST("2 images + text", estimate_count_image_tokens(msg, 1500) == 3000);
        json_free(msg);
    }

    /* input_image type */
    {
        json_t *msg = json_object();
        json_t *content = json_array();
        json_t *part = json_object();
        json_set(part, "type", json_string("input_image"));
        json_set(part, "data", json_string("base64..."));
        json_append(content, part);
        json_set(msg, "content", content);
        TEST("input_image type", estimate_count_image_tokens(msg, 1500) == 1500);
        json_free(msg);
    }

    /* _anthropic_content_blocks */
    {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("assistant"));
        json_t *blocks = json_array();
        json_t *img = json_object();
        json_set(img, "type", json_string("image"));
        json_set(img, "source", json_string("base64..."));
        json_append(blocks, img);
        json_set(msg, "_anthropic_content_blocks", blocks);
        TEST("anthropic blocks", estimate_count_image_tokens(msg, 1500) == 1500);
        json_free(msg);
    }
}

/* ---- estimate_message_chars ---- */
static void test_estimate_message_chars(void) {
    printf("\n[R10] estimate_message_chars:\n");

    TEST("null msg", estimate_message_chars(NULL) == 0);

    /* Simple message */
    {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("Hello, world!"));
        int chars = estimate_message_chars(msg);
        TEST("simple msg > 0", chars > 0);
        TEST("at least field names", chars >= 30);
        json_free(msg);
    }

    /* Empty object */
    {
        json_t *msg = json_object();
        int chars = estimate_message_chars(msg);
        TEST("empty obj > 0", chars == 2);  /* {} */
        json_free(msg);
    }

    /* Message with image content (char count includes image placeholders) */
    {
        json_t *msg = json_object();
        json_t *content = json_array();
        json_t *text_part = json_object();
        json_set(text_part, "type", json_string("text"));
        json_set(text_part, "text", json_string("What's in this image?"));
        json_append(content, text_part);
        json_t *img_part = json_object();
        json_set(img_part, "type", json_string("image_url"));
        json_set(img_part, "url", json_string("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAA="));
        json_append(content, img_part);
        json_set(msg, "content", content);
        json_set(msg, "role", json_string("user"));
        int chars = estimate_message_chars(msg);
        TEST("msg with image > 0", chars > 0);
        json_free(msg);
    }

    /* String content (not array) */
    {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("assistant"));
        json_set(msg, "content", json_string("Short reply"));
        int chars = estimate_message_chars(msg);
        TEST("string content > 0", chars > 0);
        json_free(msg);
    }
}

/* ---- estimate_messages_tokens_rough ---- */
static void test_estimate_messages_tokens_rough(void) {
    printf("\n[R10] estimate_messages_tokens_rough:\n");

    TEST("null messages",  estimate_messages_tokens_rough(NULL) == 0);

    /* Empty array */
    {
        json_t *msgs = json_array();
        TEST("empty array", estimate_messages_tokens_rough(msgs) == 0);
        json_free(msgs);
    }

    /* Single text message */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("Hi"));
        json_append(msgs, msg);
        int tokens = estimate_messages_tokens_rough(msgs);
        TEST("1 msg > 0", tokens > 0);
        json_free(msgs);
    }

    /* Multiple messages */
    {
        json_t *msgs = json_array();
        json_t *m1 = json_object();
        json_set(m1, "role", json_string("system"));
        json_set(m1, "content", json_string("You are a helpful assistant."));
        json_append(msgs, m1);
        json_t *m2 = json_object();
        json_set(m2, "role", json_string("user"));
        json_set(m2, "content", json_string("Hello! How are you?"));
        json_append(msgs, m2);
        json_t *m3 = json_object();
        json_set(m3, "role", json_string("assistant"));
        json_set(m3, "content", json_string("I'm doing well, thank you!"));
        json_append(msgs, m3);
        int tokens = estimate_messages_tokens_rough(msgs);
        TEST("3 msgs > 0", tokens > 0);
        json_free(msgs);
    }

    /* Message with image (should add image tokens) */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_t *content = json_array();
        json_t *img = json_object();
        json_set(img, "type", json_string("image_url"));
        json_set(img, "url", json_string("data:image/png;base64,aaaa"));
        json_append(content, img);
        json_set(msg, "content", content);
        json_append(msgs, msg);
        int tokens = estimate_messages_tokens_rough(msgs);
        TEST("1 image msg > 1500", tokens > 1500);
        json_free(msgs);
    }

    /* Two images */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_t *content = json_array();
        json_t *i1 = json_object();
        json_set(i1, "type", json_string("image"));
        json_set(i1, "source", json_string("img1"));
        json_append(content, i1);
        json_t *i2 = json_object();
        json_set(i2, "type", json_string("image"));
        json_set(i2, "source", json_string("img2"));
        json_append(content, i2);
        json_set(msg, "content", content);
        json_append(msgs, msg);
        int tokens = estimate_messages_tokens_rough(msgs);
        TEST("2 images msg > 3000", tokens > 3000);
        json_free(msgs);
    }
}

/* ---- estimate_request_tokens_rough ---- */
static void test_estimate_request_tokens_rough(void) {
    printf("\n[R10] estimate_request_tokens_rough:\n");

    /* All NULL */
    TEST("all null", estimate_request_tokens_rough(NULL, NULL, NULL) == 0);

    /* Only system prompt */
    {
        int tokens = estimate_request_tokens_rough(NULL, "Hello", NULL);
        TEST("system only", tokens > 0);
    }

    /* Only messages */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("Hi"));
        json_append(msgs, msg);
        int tokens = estimate_request_tokens_rough(msgs, NULL, NULL);
        TEST("msgs only", tokens > 0);
        json_free(msgs);
    }

    /* System + messages */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("Test message here"));
        json_append(msgs, msg);
        int tokens = estimate_request_tokens_rough(msgs, "You are an AI.", NULL);
        TEST("system + msgs", tokens > 0);
        json_free(msgs);
    }

    /* System + messages + tools */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("Use a tool"));
        json_append(msgs, msg);

        json_t *tools = json_array();
        json_t *tool = json_object();
        json_set(tool, "type", json_string("function"));
        json_t *fn = json_object();
        json_set(fn, "name", json_string("test_tool"));
        json_set(fn, "description", json_string("A test tool"));
        json_set(tool, "function", fn);
        json_append(tools, tool);

        int tokens = estimate_request_tokens_rough(msgs, "System", tools);
        TEST("all three", tokens > 0);
        json_free(msgs);
        json_free(tools);
    }

    /* Empty system prompt */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("test"));
        json_append(msgs, msg);
        int t1 = estimate_request_tokens_rough(msgs, "", NULL);
        int t2 = estimate_request_tokens_rough(msgs, "system prompt here", NULL);
        TEST("empty sys < with sys", t1 < t2);
        json_free(msgs);
    }
}

/* ---- get_next_probe_tier ---- */
static void test_get_next_probe_tier(void) {
    printf("\n[R10] get_next_probe_tier:\n");

    /* Test CONTEXT_PROBE_TIERS array */
    TEST("tier[0] == 256000", CONTEXT_PROBE_TIERS[0] == 256000);
    TEST("tier[1] == 128000", CONTEXT_PROBE_TIERS[1] == 128000);
    TEST("tier[2] == 64000",  CONTEXT_PROBE_TIERS[2] == 64000);
    TEST("tier[3] == 32000",  CONTEXT_PROBE_TIERS[3] == 32000);
    TEST("tier[4] == 16000",  CONTEXT_PROBE_TIERS[4] == 16000);
    TEST("tier[5] == 8000",   CONTEXT_PROBE_TIERS[5] == 8000);
    TEST("tier count == 6",   CONTEXT_PROBE_TIER_COUNT == 6);

    /* Test DEFAULT_FALLBACK_CONTEXT */
    TEST("default fallback == 256000", DEFAULT_FALLBACK_CONTEXT == 256000);

    /* Test MINIMUM_CONTEXT_LENGTH */
    TEST("minimum == 64000", MINIMUM_CONTEXT_LENGTH == 64000);

    /* Test get_next_probe_tier */
    TEST("above 256K -> 256000",  get_next_probe_tier(300000) == 256000);
    TEST("above 128K -> 128000",  get_next_probe_tier(200000) == 128000);
    TEST("above 64K -> 64000",    get_next_probe_tier(100000) == 64000);
    TEST("above 32K -> 32000",    get_next_probe_tier(50000)  == 32000);
    TEST("above 16K -> 16000",    get_next_probe_tier(30000)  == 16000);
    TEST("above 8K -> 8000",      get_next_probe_tier(12000)  == 8000);
    TEST("at 8K -> -1 (min)",     get_next_probe_tier(8000)   == -1);
    TEST("below 8K -> -1",        get_next_probe_tier(4000)   == -1);
    TEST("at 256K -> 128000",     get_next_probe_tier(256000) == 128000);
    TEST("negative -> -1",        get_next_probe_tier(-1)     == -1);
    TEST("zero -> -1",            get_next_probe_tier(0)      == -1);
    TEST("256001 -> 256000",      get_next_probe_tier(256001) == 256000);
}

/* ---- provider_context_cache_path ---- */
static void test_provider_context_cache_path(void) {
    printf("\n[R10] provider_context_cache_path:\n");
    char path[HERMES_PATH_MAX];
    provider_context_cache_path(path, sizeof(path));
    TEST("path non-empty", path[0] != '\0');
    TEST("ends with context_length_cache.json",
         strstr(path, "context_length_cache.json") != NULL);
    TEST("contains hermes home or slermes",
         strstr(path, "slermes") != NULL || strstr(path, "hermes") != NULL);
}

/* ---- provider_context_cache + get + invalidate ---- */
static void test_provider_context_cache_save_get_invalidate(void) {
    printf("\n[R10] provider_context_cache_save/get/invalidate:\n");

    /* Test save and get */
    {
        int ok = provider_context_cache_save("test-model", "http://test.url", 128000);
        if (!ok) {
            /* May fail if hermes home doesn't exist — skip gracefully */
            printf("  ~ cache save skipped (no write access)\n");
            return;
        }
        TEST("save returned 1", ok == 1);

        int got = provider_context_cache_get("test-model", "http://test.url");
        TEST("get returned 128000", got == 128000);
    }

    /* Test get non-existent */
    {
        int got = provider_context_cache_get("nonexistent-model", "http://nonexistent.url");
        TEST("get non-existent returns -1", got == -1);
    }

    /* Test get with NULL params */
    {
        TEST("get NULL model", provider_context_cache_get(NULL, "url") == -1);
        TEST("get NULL url",   provider_context_cache_get("model", NULL) == -1);
    }

    /* Test invalidate */
    {
        int ok = provider_context_cache_invalidate("test-model", "http://test.url");
        TEST("invalidate returned 1", ok == 1);

        int got = provider_context_cache_get("test-model", "http://test.url");
        TEST("get after invalidate returns -1", got == -1);
    }

    /* Test invalidate non-existent is success */
    {
        int ok = provider_context_cache_invalidate("no-such-model", "no-such-url");
        TEST("invalidate non-existent returns 1", ok == 1);
    }

    /* Test save with NULL params */
    {
        TEST("save NULL model",  provider_context_cache_save(NULL, "url", 100) == 0);
        TEST("save NULL url",    provider_context_cache_save("m", NULL, 100) == 0);
    }

    /* Test invalidate with NULL params */
    {
        TEST("invalidate NULL model", provider_context_cache_invalidate(NULL, "url") == 0);
        TEST("invalidate NULL url",   provider_context_cache_invalidate("m", NULL) == 0);
    }
}

/* ---- provider_extract_first_int ---- */
static void test_provider_extract_first_int(void) {
    printf("\n[R10] provider_extract_first_int:\n");

    const char *keys[] = {"token_count", "num_tokens", NULL};

    /* NULL payload */
    TEST("null payload", provider_extract_first_int(NULL, keys) == -1);

    /* NULL keys */
    {
        json_t *p = json_object();
        TEST("null keys", provider_extract_first_int(p, NULL) == -1);
        json_free(p);
    }

    /* Empty object */
    {
        json_t *p = json_object();
        TEST("empty obj", provider_extract_first_int(p, keys) == -1);
        json_free(p);
    }

    /* Direct key match */
    {
        json_t *p = json_object();
        json_set(p, "token_count", json_number(4096));
        TEST("direct match", provider_extract_first_int(p, keys) == 4096);
        json_free(p);
    }

    /* String value numeric */
    {
        json_t *p = json_object();
        json_set(p, "num_tokens", json_string("8192"));
        TEST("string numeric", provider_extract_first_int(p, keys) == 8192);
        json_free(p);
    }

    /* Nested match (one level deep) */
    {
        json_t *inner = json_object();
        json_set(inner, "token_count", json_number(16384));
        json_t *p = json_object();
        json_set(p, "model", inner);
        TEST("nested match", provider_extract_first_int(p, keys) == 16384);
        json_free(p);
    }

    /* No matching key */
    {
        json_t *p = json_object();
        json_set(p, "unrelated", json_number(42));
        TEST("no match", provider_extract_first_int(p, keys) == -1);
        json_free(p);
    }

    /* Multiple keys — returns one of them */
    {
        json_t *p = json_object();
        json_set(p, "num_tokens", json_number(4096));
        json_set(p, "token_count", json_number(8192));
        int val = provider_extract_first_int(p, keys);
        TEST("returns one valid value", val == 4096 || val == 8192);
        json_free(p);
    }

    /* Value below minimum range */
    {
        json_t *p = json_object();
        json_set(p, "token_count", json_number(1));
        TEST("below min", provider_extract_first_int(p, keys) == -1);
        json_free(p);
    }

    /* Value above maximum range */
    {
        json_t *p = json_object();
        json_set(p, "token_count", json_number(99999999));
        TEST("above max", provider_extract_first_int(p, keys) == -1);
        json_free(p);
    }

    /* String value that isn't a number */
    {
        json_t *p = json_object();
        json_set(p, "token_count", json_string("not-a-number"));
        TEST("non-numeric string", provider_extract_first_int(p, keys) == -1);
        json_free(p);
    }

    /* Case insensitive matching */
    {
        json_t *p = json_object();
        json_set(p, "TOKEN_COUNT", json_number(32000));
        TEST("case insensitive", provider_extract_first_int(p, keys) == 32000);
        json_free(p);
    }

    /* Empty keys array (just {NULL}) */
    {
        const char *empty_keys[] = {NULL};
        json_t *p = json_object();
        json_set(p, "anything", json_number(100));
        TEST("empty keys", provider_extract_first_int(p, empty_keys) == -1);
        json_free(p);
    }
}

/* ---- provider_detect_local_server_type ---- */
static void test_provider_detect_local_server_type(void) {
    printf("\n[R10] provider_detect_local_server_type:\n");

    char *result;

    /* NULL base_url — no crash */
    result = provider_detect_local_server_type(NULL, NULL);
    TEST("NULL base_url returns NULL", result == NULL);
    free(result);

    /* Empty base_url — no crash */
    result = provider_detect_local_server_type("", NULL);
    TEST("empty base_url returns NULL", result == NULL);
    free(result);

    /* Invalid URL — no crash, returns NULL (HTTP probe fails) */
    result = provider_detect_local_server_type("http://127.0.0.1:1", NULL);
    TEST("unreachable endpoint returns NULL", result == NULL);
    free(result);
}

/* ---- provider_query_ollama_api_show ---- */
static void test_provider_query_ollama_api_show(void) {
    printf("\n[R10] provider_query_ollama_api_show:\n");

    /* NULL model — no crash */
    int val = provider_query_ollama_api_show(NULL, "http://localhost:11434", NULL);
    TEST("NULL model returns -1", val == -1);

    /* Empty model — no crash */
    val = provider_query_ollama_api_show("", "http://localhost:11434", NULL);
    TEST("empty model returns -1", val == -1);

    /* NULL base_url — no crash */
    val = provider_query_ollama_api_show("llama3", NULL, NULL);
    TEST("NULL base_url returns -1", val == -1);

    /* Unreachable endpoint — no crash, returns -1 */
    val = provider_query_ollama_api_show("llama3", "http://127.0.0.1:1", NULL);
    TEST("unreachable returns -1", val == -1);
}

/* ---- provider_query_local_context_length ---- */
static void test_provider_query_local_context_length(void) {
    printf("\n[R10] provider_query_local_context_length:\n");

    /* NULL model — no crash */
    int val = provider_query_local_context_length(NULL, "http://127.0.0.1:1", NULL);
    TEST("NULL model returns -1", val == -1);

    /* Empty model — no crash */
    val = provider_query_local_context_length("", "http://127.0.0.1:1", NULL);
    TEST("empty model returns -1", val == -1);

    /* NULL base_url — no crash */
    val = provider_query_local_context_length("llama3", NULL, NULL);
    TEST("NULL base_url returns -1", val == -1);

    /* Unreachable endpoint — no crash */
    val = provider_query_local_context_length("llama3", "http://127.0.0.1:1", NULL);
    TEST("unreachable returns -1", val == -1);
}

/* ---- provider_add_model_aliases ---- */
static void test_provider_add_model_aliases(void) {
    printf("\n[R10] provider_add_model_aliases:\n");

    /* NULL cache — no crash */
    {
        json_t *entry = json_object();
        json_set(entry, "context_length", json_number(128000));
        provider_add_model_aliases(NULL, "openai/gpt-4o", entry);
        json_free(entry);
        TEST("NULL cache no crash", 1);
    }

    /* NULL model_id — no crash */
    {
        json_t *cache = json_object();
        json_t *entry = json_object();
        json_set(entry, "context_length", json_number(128000));
        provider_add_model_aliases(cache, NULL, entry);
        json_free(entry);
        json_free(cache);
        TEST("NULL model_id no crash", 1);
    }

    /* Empty model_id — no crash */
    {
        json_t *cache = json_object();
        json_t *entry = json_object();
        json_set(entry, "context_length", json_number(128000));
        provider_add_model_aliases(cache, "", entry);
        json_free(entry);
        json_free(cache);
        TEST("empty model_id no crash", 1);
    }

    /* NULL entry — no crash */
    {
        json_t *cache = json_object();
        provider_add_model_aliases(cache, "openai/gpt-4o", NULL);
        json_free(cache);
        TEST("NULL entry no crash", 1);
    }

    /* No slash — just primary key */
    {
        json_t *cache = json_object();
        json_t *entry = json_object();
        json_set(entry, "context_length", json_number(128000));
        provider_add_model_aliases(cache, "gpt-4o", entry);

        json_t *found = json_obj_get(cache, "gpt-4o");
        TEST("primary key stored", found != NULL);
        if (found) {
            json_t *cl = json_obj_get(found, "context_length");
            TEST("primary value intact", cl && cl->type == JSON_NUMBER && cl->num_val == 128000);
        }

        json_free(cache);
    }

    /* Slash present — alias created */
    {
        json_t *cache = json_object();
        json_t *entry = json_object();
        json_set(entry, "context_length", json_number(256000));
        provider_add_model_aliases(cache, "openai/gpt-4o", entry);

        json_t *primary = json_obj_get(cache, "openai/gpt-4o");
        TEST("primary key with prefix", primary != NULL);

        json_t *alias = json_obj_get(cache, "gpt-4o");
        TEST("alias created for bare name", alias != NULL);
        if (alias) {
            json_t *cl = json_obj_get(alias, "context_length");
            TEST("alias value intact", cl && cl->type == JSON_NUMBER && cl->num_val == 256000);
        }

        json_free(cache);
    }

    /* Alias already exists — setdefault semantics */
    {
        json_t *cache = json_object();
        json_t *original_entry = json_object();
        json_set(original_entry, "context_length", json_number(1));
        json_set(cache, "gpt-4o", json_copy(original_entry));

        json_t *new_entry = json_object();
        json_set(new_entry, "context_length", json_number(999999));
        provider_add_model_aliases(cache, "openai/gpt-4o", new_entry);

        json_t *alias = json_obj_get(cache, "gpt-4o");
        TEST("alias preserved (setdefault)", alias != NULL);
        if (alias) {
            json_t *cl = json_obj_get(alias, "context_length");
            TEST("original value kept", cl && cl->type == JSON_NUMBER && cl->num_val == 1);
        }

        json_free(original_entry);
        json_free(new_entry);
        json_free(cache);
    }

    /* Trailing slash only (e.g. "openai/") — no alias for empty string */
    {
        json_t *cache = json_object();
        json_t *entry = json_object();
        json_set(entry, "context_length", json_number(128000));
        provider_add_model_aliases(cache, "openai/", entry);

        json_t *primary = json_obj_get(cache, "openai/");
        TEST("trailing slash primary stored", primary != NULL);

        json_t *alias = json_obj_get(cache, "");
        TEST("no alias for empty string", alias == NULL);

        json_free(cache);
    }
}

/* ---- provider_get_context_length_from_provider_error ---- */
static void test_provider_get_context_length_from_provider_error(void) {
    printf("\n[R10] provider_get_context_length_from_provider_error:\n");

    /* NULL error message */
    TEST("NULL error", provider_get_context_length_from_provider_error(NULL, 128000) == -1);

    /* Empty error message */
    TEST("empty error", provider_get_context_length_from_provider_error("", 128000) == -1);

    /* Error with context limit lower than current */
    int result = provider_get_context_length_from_provider_error(
        "This model's maximum context length is 32000 tokens", 128000);
    TEST("lower limit returned", result == 32000);

    /* Error with context limit higher than current — no downgrade */
    result = provider_get_context_length_from_provider_error(
        "This model's maximum context length is 256000 tokens", 128000);
    TEST("higher limit ignored", result == -1);

    /* Error with context limit equal to current — no downgrade */
    result = provider_get_context_length_from_provider_error(
        "maximum context length is 128000 tokens", 128000);
    TEST("equal limit ignored", result == -1);

    /* Error with no context limit */
    result = provider_get_context_length_from_provider_error(
        "Internal server error", 128000);
    TEST("no limit in error", result == -1);
}
