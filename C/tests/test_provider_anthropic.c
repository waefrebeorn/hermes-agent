/*
 * test_provider_anthropic.c — Tests for Anthropic provider depth.
 *
 * Tests: normalize_model_key, model matching predicates, build_url, headers.
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

/* Duplicate of the internal normalize_model_key for testing */
static void norm_key(const char *model, char *out, size_t out_size) {
    if (!model || !*model) { out[0] = '\0'; return; }
    const char *slash = strrchr(model, '/');
    if (slash) model = slash + 1;
    size_t i, j = 0;
    for (i = 0; model[i] && j < out_size - 1; i++) {
        if (model[i] == '.')
            out[j++] = '-';
        else
            out[j++] = model[i];
    }
    out[j] = '\0';
}

static bool contains_any(const char *model, const char **patterns) {
    if (!model || !*model) return false;
    char normalized[128];
    norm_key(model, normalized, sizeof(normalized));
    for (int i = 0; patterns[i]; i++) {
        if (strstr(normalized, patterns[i]))
            return true;
    }
    return false;
}

int main(void) {
    printf("=== Anthropic Provider Depth Tests ===\n\n");

    /* ── normalize_model_key ────────────────────────────────────── */
    {
        char out[128];
        norm_key("anthropic/claude-sonnet-4-20250514", out, sizeof(out));
        TEST("strip provider prefix", strcmp(out, "claude-sonnet-4-20250514") == 0);
    }
    {
        char out[128];
        norm_key("claude-opus-4.6", out, sizeof(out));
        TEST("dots to hyphens", strcmp(out, "claude-opus-4-6") == 0);
    }
    {
        char out[128];
        norm_key("anthropic/claude-opus-4.7-20250514", out, sizeof(out));
        TEST("prefix and dots", strcmp(out, "claude-opus-4-7-20250514") == 0);
    }
    {
        char out[128];
        norm_key(NULL, out, sizeof(out));
        TEST("NULL model", out[0] == '\0');
    }
    {
        char out[128];
        norm_key("", out, sizeof(out));
        TEST("empty model", out[0] == '\0');
    }
    {
        char out[128];
        norm_key("claude-sonnet-4", out, sizeof(out));
        TEST("no prefix or dots", strcmp(out, "claude-sonnet-4") == 0);
    }

    /* ── model_contains_any ──────────────────────────────────────── */
    {
        const char *pats[] = {"4-7", "4.7", NULL};
        TEST("detect 4.7 via dot", contains_any("claude-opus-4.7", pats));
    }
    {
        const char *pats[] = {"4-7", "4.7", NULL};
        TEST("detect 4.7 with prefix", contains_any("anthropic/claude-opus-4.7-20250514", pats));
    }
    {
        const char *pats[] = {"4-6", "4.6", NULL};
        TEST("detect 4.6 via hyphen", contains_any("claude-sonnet-4-6-20250514", pats));
    }
    {
        const char *pats[] = {"4-6", "4.7", "4-8", NULL};
        TEST("reject 3.5 sonnet", !contains_any("claude-3-5-sonnet", pats));
    }
    {
        const char *pats[] = {"4-6", "4.7", NULL};
        TEST("reject NULL model", !contains_any(NULL, pats));
    }
    {
        const char *pats[] = {"4-6", "4.7", NULL};
        TEST("reject empty model", !contains_any("", pats));
    }
    {
        const char *op46[] = {"opus-4-6", "opus-4.6", NULL};
        TEST("fast mode opus-4-6", contains_any("claude-opus-4-6", op46));
    }
    {
        const char *op46[] = {"opus-4-6", "opus-4.6", NULL};
        TEST("fast mode opus-4.6", contains_any("claude-opus-4.6", op46));
    }
    {
        const char *op46[] = {"opus-4-6", "opus-4.6", NULL};
        TEST("fast mode NOT sonnet-4-6", !contains_any("claude-sonnet-4-6", op46));
    }
    {
        const char *xhigh[] = {"4-7", "4.7", "4-8", "4.8", NULL};
        TEST("xhigh matches 4.7", contains_any("claude-opus-4.7", xhigh));
    }
    {
        const char *xhigh[] = {"4-7", "4.7", "4-8", "4.8", NULL};
        TEST("xhigh matches 4.8", contains_any("claude-opus-4-8", xhigh));
    }
    {
        const char *xhigh[] = {"4-7", "4.7", "4-8", "4.8", NULL};
        TEST("xhigh NOT 4.6", !contains_any("claude-opus-4-6", xhigh));
    }

    /* ── build_url ──────────────────────────────────────────────── */
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://api.anthropic.com/v1");
        TEST("build_url appends /messages", url && strcmp(url, "https://api.anthropic.com/v1/messages") == 0);
        free(url);
    }
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://custom.com/messages");
        TEST("build_url preserves existing /messages", url && strcmp(url, "https://custom.com/messages") == 0);
        free(url);
    }
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://api.anthropic.com/v1/");
        TEST("build_url trailing slash", url && strcmp(url, "https://api.anthropic.com/v1/messages") == 0);
        free(url);
    }
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, NULL);
        TEST("build_url NULL base", url && strstr(url, "/messages") != NULL);
        free(url);
    }
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "");
        TEST("build_url empty base", url && strstr(url, "/messages") != NULL);
        free(url);
    }

    /* ── headers ────────────────────────────────────────────────── */
    {
        provider_t p;
        memset(&p, 0, sizeof(p));
        p.system_cached = false;
        char *h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "sk-ant-test");
        TEST("headers contain anthropic-beta", h && strstr(h, "anthropic-beta:") != NULL);
        TEST("headers contain interleaved-thinking", h && strstr(h, "interleaved-thinking") != NULL);
        TEST("headers contain fine-grained-tool-streaming", h && strstr(h, "fine-grained-tool-streaming") != NULL);
        TEST("headers contain x-api-key", h && strstr(h, "x-api-key: sk-ant-test") != NULL);
        free(h);
    }
    {
        provider_t p;
        memset(&p, 0, sizeof(p));
        p.system_cached = true;
        char *h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "sk-ant-test");
        TEST("headers contain ephemeral-cache when cached", h && strstr(h, "ephemeral-cache-2025-05-20") != NULL);
        free(h);
    }
    {
        provider_t p;
        memset(&p, 0, sizeof(p));
        p.system_cached = false;
        char *h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "");
        TEST("headers no API key still valid", h && strstr(h, "anthropic-version: 2023-06-01") != NULL);
        free(h);
    }

    /* ── report ───────────────────────────────────────────────────── */
    printf("\n==============================================\n");
    if (failures == 0)
        printf("  All 28 tests PASSED\n");
    else
        printf("  %d test(s) FAILED\n", failures);
    printf("==============================================\n\n");
    return failures > 0 ? 1 : 0;
}
