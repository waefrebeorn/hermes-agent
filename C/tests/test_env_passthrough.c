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
}

static void test_register_and_allowed(void) {
    T("register MY_CUSTOM_VAR", env_passthrough_register("MY_CUSTOM_VAR"));
    T("MY_CUSTOM_VAR is allowed", env_passthrough_is_allowed("MY_CUSTOM_VAR"));
    T("unregistered VAR not allowed", !env_passthrough_is_allowed("UNREGISTERED_VAR"));
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
}

static void test_clear(void) {
    env_passthrough_clear();
    T("after clear, MY_CUSTOM_VAR not allowed", !env_passthrough_is_allowed("MY_CUSTOM_VAR"));
    T("after clear, VAR_A not allowed", !env_passthrough_is_allowed("VAR_A"));
}

int main(void) {
    printf("=== Env Passthrough Tests ===\n\n");
    printf("--- Blocked Vars ---\n"); test_blocked_vars();
    printf("\n--- Register & Allowed ---\n"); test_register_and_allowed();
    printf("\n--- Batch Register ---\n"); test_register_batch();
    printf("\n--- Get All ---\n"); test_get_all();
    printf("\n--- Clear ---\n"); test_clear();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0;
}
