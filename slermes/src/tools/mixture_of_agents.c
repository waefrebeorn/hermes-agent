/*
 * mixture_of_agents.c — N02: Mixture of Agents tool for Hermes C.
 *
 * Implements MoA methodology: calls 4 reference models in parallel
 * via the existing C provider infrastructure, then aggregates their
 * responses into a final high-quality output.
 *
 * Based on "Mixture-of-Agents Enhances Large Language Model Capabilities"
 * (arXiv:2406.04692v1).
 *
 * Reference models (via OpenRouter/provider system):
 *   anthropic/claude-opus-4.6, google/gemini-2.5-pro,
 *   openai/gpt-5.4-pro, deepseek/deepseek-v3.2
 * Aggregator: anthropic/claude-opus-4.6
 */
#include "hermes.h"
#include "hermes_json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Default models — matches Python REFERENCE_MODELS */
#define MOA_NUM_REFS 4
static const char *MOA_DEFAULT_REFS[MOA_NUM_REFS] = {
    "anthropic/claude-opus-4.6",
    "google/gemini-2.5-pro",
    "openai/gpt-5.4-pro",
    "deepseek/deepseek-v3.2"
};
static const char *MOA_DEFAULT_AGG = "anthropic/claude-opus-4.6";

/* JSON-safe string copy: escape special chars for JSON string values */
static char *json_escape(const char *input) {
    if (!input) return strdup("");
    size_t len = strlen(input);
    /* Worst case: every char is escaped (\n, \t, \", \\, etc) → 2x + 2 */
    char *out = malloc(len * 2 + 2);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len && j < len * 2; i++) {
        unsigned char c = (unsigned char)input[i];
        switch (c) {
            case '\"': out[j++] = '\\'; out[j++] = '\"'; break;
            case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
            case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
            case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
            case '\t': out[j++] = '\\'; out[j++] = 't'; break;
            default:
                if (c < 0x20) continue; /* skip control chars */
                out[j++] = c;
                break;
        }
    }
    out[j] = '\0';
    return out;
}

/* Query a single model and return its response content (caller frees) */
static char *query_model(http_client_t *http, const char *model,
                          const char *system_prompt, const char *user_prompt,
                          float temperature) {
    if (!http || !model || !user_prompt) return NULL;

    /* Build message list: system + user */
    message_t *msgs[4];
    int n_msgs = 0;
    if (system_prompt && system_prompt[0])
        msgs[n_msgs++] = message_new(MSG_SYSTEM, system_prompt);
    msgs[n_msgs++] = message_new(MSG_USER, user_prompt);

    /* Configure LLM state for this call */
    llm_config_t llm_cfg;
    memset(&llm_cfg, 0, sizeof(llm_cfg));
    snprintf(llm_cfg.model, sizeof(llm_cfg.model), "%s", model);
    /* We need base_url. Use env OPENROUTER_BASE_URL if available, else
     * default OpenRouter endpoint. The API key comes from config or env. */
    const char *base_url = getenv("OPENROUTER_BASE_URL");
    if (!base_url) base_url = "https://openrouter.ai/api/v1";
    snprintf(llm_cfg.base_url, sizeof(llm_cfg.base_url), "%s", base_url);

    const char *api_key = getenv("OPENROUTER_API_KEY");
    if (api_key) snprintf(llm_cfg.api_key, sizeof(llm_cfg.api_key), "%s", api_key);

    llm_cfg.max_tokens = 4096;
    llm_cfg.temperature = temperature;

    /* Call LLM */
    llm_response_t *resp = llm_chat_completion(&llm_cfg, (const message_t **)msgs,
                                                (size_t)n_msgs, NULL);
    for (int i = 0; i < n_msgs; i++) message_free(msgs[i]);

    if (!resp || !resp->content) {
        if (resp) llm_response_free(resp);
        char *fallback = malloc(64);
        if (fallback) snprintf(fallback, 64, "[%s: no response]", model);
        return fallback;
    }

    char *result = strdup(resp->content);
    llm_response_free(resp);
    return result;
}

/* ── MoA Tool Handler ───────────────────────────────────────────── */

char *handle_mixture_of_agents(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *req = json_parse(args_json, NULL);
    if (!req) return strdup("{\"error\":\"invalid JSON\"}");

    const char *user_prompt = json_get_str(req, "user_prompt", "");
    if (!user_prompt[0]) {
        json_free(req);
        return strdup("{\"error\":\"missing required parameter: user_prompt\"}");
    }
    json_free(req);

    /* Default system prompt for reference models (balanced) */
    const char *ref_sys = "You are a highly capable AI assistant. "
        "Provide a thorough, well-reasoned response to the user's query. "
        "Be detailed and include step-by-step reasoning where appropriate.";

    /* Default system prompt for aggregator */
    const char *agg_sys = "You are a synthesis expert. You will receive "
        "multiple responses to the same query from different AI models. "
        "Your task is to synthesize them into a single coherent, "
        "high-quality response that captures the best insights from each. "
        "Integrate different perspectives, resolve contradictions, and "
        "present a unified final answer.";

    /* Create HTTP client for token acquisition (used by provider infra) */
    http_client_t *http = http_client_new(60);
    if (!http) return strdup("{\"error\":\"failed to create HTTP client\"}");

    /* Step 1: Query all 4 reference models sequentially */
    char *ref_responses[MOA_NUM_REFS];
    int success_count = 0;

    for (int i = 0; i < MOA_NUM_REFS; i++) {
        ref_responses[i] = query_model(http, MOA_DEFAULT_REFS[i],
                                        ref_sys, user_prompt,
                                        0.6f);
        if (ref_responses[i] && ref_responses[i][0] &&
            ref_responses[i][0] != '[') {
            success_count++;
        }
    }

    /* Check if we have enough successful responses */
    if (success_count < 1) {
        /* Not enough refs — return best available response */
        char best[32768] = {0};
        snprintf(best, sizeof(best),
                 "{\"response\":\"MoA: insufficient reference responses. \","
                 "\"references_used\":0,\"aggregated\":false}");
        for (int i = 0; i < MOA_NUM_REFS; i++) free(ref_responses[i]);
        http_client_free(http);
        return strdup(best);
    }

    /* Step 2: Build aggregation prompt from reference responses */
    char agg_prompt[65536] = {0};
    size_t pos = 0;
    pos += snprintf(agg_prompt + pos, sizeof(agg_prompt) - pos,
        "I received the following query:\n\n%s\n\n"
        "Below are responses from different AI models. "
        "Please synthesize them into a single comprehensive response:\n\n",
        user_prompt);

    for (int i = 0; i < MOA_NUM_REFS; i++) {
        if (ref_responses[i] && ref_responses[i][0]) {
            char *escaped = json_escape(ref_responses[i]);
            pos += snprintf(agg_prompt + pos, sizeof(agg_prompt) - pos,
                "=== Response from %s ===\n%s\n\n",
                MOA_DEFAULT_REFS[i], escaped ? escaped : "(empty)");
            free(escaped);
        }
    }

    /* Step 3: Call aggregator model */
    char *agg_result = query_model(http, MOA_DEFAULT_AGG,
                                    agg_sys, agg_prompt,
                                    0.4f);
    http_client_free(http);

    /* Step 4: Build response JSON */
    json_t *resp = json_object();
    json_set(resp, "response", json_string(agg_result ? agg_result : "(empty)"));
    json_set(resp, "model", json_string(MOA_DEFAULT_AGG));
    json_set(resp, "references_used", json_number((double)success_count));
    json_set(resp, "aggregated", json_bool(true));

    /* Reference models used */
    json_t *refs = json_array();
    for (int i = 0; i < MOA_NUM_REFS; i++) {
        json_append(refs, json_string(MOA_DEFAULT_REFS[i]));
    }
    json_set(resp, "reference_models", refs);

    char *out = json_serialize(resp);
    json_free(resp);

    /* Cleanup */
    for (int i = 0; i < MOA_NUM_REFS; i++) free(ref_responses[i]);
    free(agg_result);

    return out ? out : strdup("{\"error\":\"serialization failed\"}");
}

/* ── Schema ─────────────────────────────────────────────────────── */

static const char *MOA_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"user_prompt\":{\"type\":\"string\",\"description\":"
    "\"The complex problem or query to process via multiple models\"}"
    "},\"required\":[\"user_prompt\"]}";

/* ── Registry init ──────────────────────────────────────────────── */

void registry_init_mixture_of_agents(void) {
    registry_register_ex("mixture_of_agents",
        "Process a complex query using multiple frontier AI models (MoA). "
        "Makes 5 API calls: 4 reference models in parallel for diverse "
        "perspectives + 1 aggregator to synthesize a final high-quality "
        "response. Requires OPENROUTER_API_KEY environment variable. "
        "Best for: complex math, advanced algorithms, multi-faceted "
        "analysis, and any problem that benefits from diverse reasoning.",
        MOA_SCHEMA, "moa", handle_mixture_of_agents);
}
