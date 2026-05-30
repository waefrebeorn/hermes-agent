/* test_env_passthrough.c -- Tests for libenvpassthrough module.
 * Tests: is_blocked, register, is_allowed, get_all, clear.
 * Compile with env_passthrough.c -lpthread.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env_passthrough.h"

static int pass = 0, fail = 0;
#define T(n, e) do { if(e) { pass++; printf("  OK %s\n", n); } else { fail++; printf("  FAIL %s\n", n); } } while(0)

static void test_blocked_vars(void) {
    T("OPENAI_API_KEY is blocked", env_passthrough_is_blocked("OPENAI_API_KEY"));
    T("ANTHROPIC_API_KEY is blocked", env_passthrough_is_blocked("ANTHROPIC_API_KEY"));
    T("ANTHROPIC_BASE_URL is blocked", env_passthrough_is_blocked("ANTHROPIC_BASE_URL"));
    T("OPENAI_BASE_URL is blocked", env_passthrough_is_blocked("OPENAI_BASE_URL"));
    T("MISTRAL_API_KEY is blocked", env_passthrough_is_blocked("MISTRAL_API_KEY"));
    T("GROQ_API_KEY is blocked", env_passthrough_is_blocked("GROQ_API_KEY"));
    T("FIRECRAWL_API_KEY is blocked", env_passthrough_is_blocked("FIRECRAWL_API_KEY"));
    T("TELEGRAM_BOT_TOKEN is blocked", env_passthrough_is_blocked("TELEGRAM_BOT_TOKEN"));
    T("DISCORD_BOT_TOKEN is blocked", env_passthrough_is_blocked("DISCORD_BOT_TOKEN"));
    T("GH_TOKEN is blocked", env_passthrough_is_blocked("GH_TOKEN"));
    T("DOCKER_HOST is blocked", env_passthrough_is_blocked("DOCKER_HOST"));
    T("SSH_AUTH_SOCK is blocked", env_passthrough_is_blocked("SSH_AUTH_SOCK"));
    T("MODAL_TOKEN_ID is blocked", env_passthrough_is_blocked("MODAL_TOKEN_ID"));
    T("HOME is not blocked", !env_passthrough_is_blocked("HOME"));
    T("PATH is not blocked", !env_passthrough_is_blocked("PATH"));
    T("TENOR_API_KEY is not blocked", !env_passthrough_is_blocked("TENOR_API_KEY"));
    T("NULL is not blocked", !env_passthrough_is_blocked(NULL));
    T("empty is not blocked", !env_passthrough_is_blocked(""));
}

static void test_ghsa_hardening(void) {
    printf("\n--- GHSA Hardening (67 blocked vars) ---\n");

    /* Provider API keys — core providers */
    T("ANTHROPIC_TOKEN blocked", env_passthrough_is_blocked("ANTHROPIC_TOKEN"));
    T("OPENAI_TOKEN blocked", env_passthrough_is_blocked("OPENAI_TOKEN"));
    T("OPENAI_API_BASE blocked", env_passthrough_is_blocked("OPENAI_API_BASE"));
    T("OPENAI_ORG_ID blocked", env_passthrough_is_blocked("OPENAI_ORG_ID"));
    T("OPENAI_ORGANIZATION blocked", env_passthrough_is_blocked("OPENAI_ORGANIZATION"));
    T("OPENROUTER_API_KEY blocked", env_passthrough_is_blocked("OPENROUTER_API_KEY"));
    T("OPENROUTER_TOKEN blocked", env_passthrough_is_blocked("OPENROUTER_TOKEN"));

    /* Provider API keys — hermes-prefixed variants */
    T("HERMES_ANTHROPIC_API_KEY blocked", env_passthrough_is_blocked("HERMES_ANTHROPIC_API_KEY"));
    T("HERMES_OPENAI_API_KEY blocked", env_passthrough_is_blocked("HERMES_OPENAI_API_KEY"));
    T("HERMES_OPENROUTER_API_KEY blocked", env_passthrough_is_blocked("HERMES_OPENROUTER_API_KEY"));
    T("HERMES_DEEPSEEK_API_KEY blocked", env_passthrough_is_blocked("HERMES_DEEPSEEK_API_KEY"));
    T("HERMES_XAI_API_KEY blocked", env_passthrough_is_blocked("HERMES_XAI_API_KEY"));
    T("HERMES_GOOGLE_API_KEY blocked", env_passthrough_is_blocked("HERMES_GOOGLE_API_KEY"));
    T("HERMES_AZURE_API_KEY blocked", env_passthrough_is_blocked("HERMES_AZURE_API_KEY"));
    T("HERMES_BEDROCK_API_KEY blocked", env_passthrough_is_blocked("HERMES_BEDROCK_API_KEY"));

    /* Provider API keys — extended providers */
    T("XAI_API_KEY blocked", env_passthrough_is_blocked("XAI_API_KEY"));
    T("DEEPSEEK_API_KEY blocked", env_passthrough_is_blocked("DEEPSEEK_API_KEY"));
    T("TOGETHER_API_KEY blocked", env_passthrough_is_blocked("TOGETHER_API_KEY"));
    T("PERPLEXITY_API_KEY blocked", env_passthrough_is_blocked("PERPLEXITY_API_KEY"));
    T("COHERE_API_KEY blocked", env_passthrough_is_blocked("COHERE_API_KEY"));
    T("FIREWORKS_API_KEY blocked", env_passthrough_is_blocked("FIREWORKS_API_KEY"));
    T("HELICONE_API_KEY blocked", env_passthrough_is_blocked("HELICONE_API_KEY"));
    T("PARALLEL_API_KEY blocked", env_passthrough_is_blocked("PARALLEL_API_KEY"));
    T("GOOGLE_API_KEY blocked", env_passthrough_is_blocked("GOOGLE_API_KEY"));
    T("AZURE_API_KEY blocked", env_passthrough_is_blocked("AZURE_API_KEY"));

    /* AWS credentials */
    T("AWS_ACCESS_KEY_ID blocked", env_passthrough_is_blocked("AWS_ACCESS_KEY_ID"));
    T("AWS_SECRET_ACCESS_KEY blocked", env_passthrough_is_blocked("AWS_SECRET_ACCESS_KEY"));
    T("AWS_SESSION_TOKEN blocked", env_passthrough_is_blocked("AWS_SESSION_TOKEN"));

    /* LLM model config */
    T("LLM_MODEL blocked", env_passthrough_is_blocked("LLM_MODEL"));

    /* Gateway credentials */
    T("TELEGRAM_HOME_CHANNEL blocked", env_passthrough_is_blocked("TELEGRAM_HOME_CHANNEL"));
    T("TELEGRAM_HOME_CHANNEL_NAME blocked", env_passthrough_is_blocked("TELEGRAM_HOME_CHANNEL_NAME"));
    T("DISCORD_HOME_CHANNEL blocked", env_passthrough_is_blocked("DISCORD_HOME_CHANNEL"));
    T("DISCORD_HOME_CHANNEL_NAME blocked", env_passthrough_is_blocked("DISCORD_HOME_CHANNEL_NAME"));
    T("DISCORD_REQUIRE_MENTION blocked", env_passthrough_is_blocked("DISCORD_REQUIRE_MENTION"));
    T("DISCORD_FREE_RESPONSE_CHANNELS blocked", env_passthrough_is_blocked("DISCORD_FREE_RESPONSE_CHANNELS"));
    T("DISCORD_AUTO_THREAD blocked", env_passthrough_is_blocked("DISCORD_AUTO_THREAD"));
    T("SLACK_BOT_TOKEN blocked", env_passthrough_is_blocked("SLACK_BOT_TOKEN"));
    T("SLACK_HOME_CHANNEL blocked", env_passthrough_is_blocked("SLACK_HOME_CHANNEL"));
    T("SLACK_ALLOWED_USERS blocked", env_passthrough_is_blocked("SLACK_ALLOWED_USERS"));
    T("SIGNAL_HTTP_URL blocked", env_passthrough_is_blocked("SIGNAL_HTTP_URL"));
    T("SIGNAL_ACCOUNT blocked", env_passthrough_is_blocked("SIGNAL_ACCOUNT"));
    T("SIGNAL_ALLOWED_USERS blocked", env_passthrough_is_blocked("SIGNAL_ALLOWED_USERS"));
    T("SIGNAL_HOME_CHANNEL blocked", env_passthrough_is_blocked("SIGNAL_HOME_CHANNEL"));
    T("HASS_TOKEN blocked", env_passthrough_is_blocked("HASS_TOKEN"));
    T("HASS_URL blocked", env_passthrough_is_blocked("HASS_URL"));

    /* GitHub credentials */
    T("GITHUB_APP_ID blocked", env_passthrough_is_blocked("GITHUB_APP_ID"));
    T("GITHUB_APP_PRIVATE_KEY_PATH blocked", env_passthrough_is_blocked("GITHUB_APP_PRIVATE_KEY_PATH"));
    T("GITHUB_APP_INSTALLATION_ID blocked", env_passthrough_is_blocked("GITHUB_APP_INSTALLATION_ID"));

    /* Infrastructure */
    T("FIRECRAWL_API_URL blocked", env_passthrough_is_blocked("FIRECRAWL_API_URL"));
    T("DOCKER_CERT_PATH blocked", env_passthrough_is_blocked("DOCKER_CERT_PATH"));
    T("DOCKER_TLS_VERIFY blocked", env_passthrough_is_blocked("DOCKER_TLS_VERIFY"));
    T("MODAL_TOKEN_SECRET blocked", env_passthrough_is_blocked("MODAL_TOKEN_SECRET"));
    T("HERMES_HOME blocked", env_passthrough_is_blocked("HERMES_HOME"));
    T("SLERMES_HOME blocked", env_passthrough_is_blocked("SLERMES_HOME"));
}

static void test_register_and_allowed(void) {
    T("register MY_CUSTOM_VAR", env_passthrough_register("MY_CUSTOM_VAR"));
    T("MY_CUSTOM_VAR is allowed", env_passthrough_is_allowed("MY_CUSTOM_VAR"));
    T("unregistered VAR not allowed", !env_passthrough_is_allowed("UNREGISTERED_VAR"));
    T("NULL name not allowed", !env_passthrough_is_allowed(NULL));
    T("empty name not allowed", !env_passthrough_is_allowed(""));
    T("register duplicate returns true", env_passthrough_register("MY_CUSTOM_VAR"));
    T("register blocked var rejected", !env_passthrough_register("OPENAI_API_KEY"));
    T("register NULL name rejected", !env_passthrough_register(NULL));
    T("register empty name rejected", !env_passthrough_register(""));
}

static void test_register_batch(void) {
    const char *vars[] = {"VAR_A", "VAR_B", "VAR_C"};
    int r = env_passthrough_register_batch(vars, 3);
    T("register_batch returns 3", r == 3);
    T("VAR_A allowed after batch", env_passthrough_is_allowed("VAR_A"));
    T("VAR_B allowed after batch", env_passthrough_is_allowed("VAR_B"));
    T("VAR_C allowed after batch", env_passthrough_is_allowed("VAR_C"));
}

static void test_get_all(void) {
    char **list = NULL;
    int count = 0;
    env_passthrough_get_all(&list, &count);
    T("get_all returns count > 0", count > 0);
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i], "MY_CUSTOM_VAR") == 0) found++;
    }
    T("MY_CUSTOM_VAR found in list", found == 1);
    env_passthrough_free_list(list, count);
    /* free_list with NULL should not crash */
    env_passthrough_free_list(NULL, 0);
    T("free_list NULL no crash", 1);
}

static void test_clear(void) {
    env_passthrough_clear();
    T("after clear, MY_CUSTOM_VAR not allowed", !env_passthrough_is_allowed("MY_CUSTOM_VAR"));
    T("after clear, VAR_A not allowed", !env_passthrough_is_allowed("VAR_A"));
    /* Re-register for subsequent tests */
    env_passthrough_register("MY_CUSTOM_VAR");
    env_passthrough_register("VAR_A");
    T("re-register works after clear", env_passthrough_is_allowed("MY_CUSTOM_VAR"));
}

int main(void) {
    printf("=== Env Passthrough Tests ===\n\n");
    printf("--- Blocked Vars ---\n"); test_blocked_vars();
    printf("\n--- GHSA Hardening ---\n"); test_ghsa_hardening();
    printf("\n--- Register & Allowed ---\n"); test_register_and_allowed();
    printf("\n--- Batch Register ---\n"); test_register_batch();
    printf("\n--- Get All ---\n"); test_get_all();
    printf("\n--- Clear ---\n"); test_clear();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0;
}
