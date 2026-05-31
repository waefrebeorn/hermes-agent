/*
 * test_vision_supports_media.c — Standalone test for vision_supports_media_in_tool_results.
 * Compile: gcc -O2 -Wall -Wextra -I../include -I../lib/libjson -I../lib/libbase64 -o /tmp/t_vm test_vision_supports_media.c ../src/tools/vision.c ../lib/libjson/json.c -lm -Wl,--unresolved-symbols=ignore-all
 */
#include "../include/hermes.h"
#include "../include/hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern bool vision_supports_media_in_tool_results(const char *provider, const char *model);

static int passed = 0, failed = 0;
#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s\n", name); } \
} while(0)

int main(void) {
    printf("=== Media-in-Tool-Results Support Tests ===\n");

    /* Aggregators */
    TEST("openrouter", vision_supports_media_in_tool_results("openrouter", NULL));
    TEST("nous", vision_supports_media_in_tool_results("nous", NULL));
    TEST("vertex", vision_supports_media_in_tool_results("vertex", NULL));
    TEST("bedrock", vision_supports_media_in_tool_results("bedrock", NULL));
    TEST("anthropic-vertex", vision_supports_media_in_tool_results("anthropic-vertex", NULL));
    TEST("google-vertex", vision_supports_media_in_tool_results("google-vertex", NULL));

    /* Native Anthropic */
    TEST("anthropic", vision_supports_media_in_tool_results("anthropic", NULL));
    TEST("claude", vision_supports_media_in_tool_results("claude", NULL));
    TEST("anthropic-direct", vision_supports_media_in_tool_results("anthropic-direct", NULL));

    /* OpenAI */
    TEST("openai", vision_supports_media_in_tool_results("openai", NULL));
    TEST("openai-chat", vision_supports_media_in_tool_results("openai-chat", NULL));
    TEST("openai-codex", vision_supports_media_in_tool_results("openai-codex", NULL));
    TEST("azure-openai", vision_supports_media_in_tool_results("azure-openai", NULL));

    /* Gemini 3+ */
    TEST("google + gemini-3-flash", vision_supports_media_in_tool_results("google", "gemini-3-flash"));
    TEST("gemini + gemini-pro-3", vision_supports_media_in_tool_results("gemini", "gemini-pro-3"));
    TEST("google-gemini + gemini-flash-3", vision_supports_media_in_tool_results("google-gemini", "gemini-flash-3"));

    /* Gemini <3 — must return false */
    TEST("google + gemini-2.5 rejects", !vision_supports_media_in_tool_results("google", "gemini-2.5-flash"));
    TEST("google + gemini-pro-2 rejects", !vision_supports_media_in_tool_results("google", "gemini-pro-2"));
    TEST("gemini + NULL model rejects", !vision_supports_media_in_tool_results("gemini", NULL));
    TEST("google + NULL model rejects", !vision_supports_media_in_tool_results("google", NULL));

    /* Unknown / edge */
    TEST("NULL provider rejects", !vision_supports_media_in_tool_results(NULL, NULL));
    TEST("empty provider rejects", !vision_supports_media_in_tool_results("", NULL));
    TEST("unknown provider rejects", !vision_supports_media_in_tool_results("unknown-provider", "some-model"));

    /* Case sensitivity — strcmp is case-sensitive */
    TEST("OpenRouter (capitalized) rejects", !vision_supports_media_in_tool_results("OpenRouter", NULL));
    TEST("GEMINI-3-FLASH model with google rejects", !vision_supports_media_in_tool_results("google", "GEMINI-3-FLASH"));

    /* Model version boundary — strstr false-positives */
    TEST("gemini-30 model rejects (30 != 3.x)", !vision_supports_media_in_tool_results("google", "gemini-30-flash"));
    TEST("gemini-pro-3.5 supports (dot version)", vision_supports_media_in_tool_results("google", "gemini-pro-3.5-flash"));
    TEST("gemini-3 exact model supports", vision_supports_media_in_tool_results("google", "gemini-3"));

    /* Anthropic/OpenAI always support regardless of model */
    TEST("anthropic + NULL model supports", vision_supports_media_in_tool_results("anthropic", NULL));
    TEST("openai + claude-3 model supports", vision_supports_media_in_tool_results("openai", "claude-3"));
    TEST("openai + empty model supports", vision_supports_media_in_tool_results("openai", ""));

    /* Provider whitespace — strcmp is exact */
    TEST("leading space provider rejects", !vision_supports_media_in_tool_results(" openai", NULL));
    TEST("trailing space provider rejects", !vision_supports_media_in_tool_results("openai ", NULL));

    /* Anthropic aliases */
    TEST("anthropic + claude-4 supports (future model)", vision_supports_media_in_tool_results("anthropic", "claude-4-sonnet"));
    TEST("claude + any model supports", vision_supports_media_in_tool_results("claude", "claude-opus-4"));

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
