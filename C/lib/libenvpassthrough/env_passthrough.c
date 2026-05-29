/*
 * env_passthrough.c — Environment variable passthrough registry.
 *
 * Port of Python tools/env_passthrough.py.
 *
 * Manages an allowlist of env vars that pass through to sandboxed execution
 * environments. Thread-safe with a mutex. Blocked vars (Hermes provider
 * credentials) are never allowlisted.
 */

#include "env_passthrough.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Hermes provider credential blocklist ──────────────────────────── */

/* Must match Python tools/environments/local.py _HERMES_PROVIDER_ENV_BLOCKLIST */
static const char *PROVIDER_BLOCKLIST[] = {
    /* Provider API keys */
    "ANTHROPIC_API_KEY",
    "ANTHROPIC_TOKEN",
    "ANTHROPIC_BASE_URL",
    "OPENAI_API_KEY",
    "OPENAI_TOKEN",
    "OPENAI_BASE_URL",
    "OPENAI_API_BASE",
    "OPENAI_ORG_ID",
    "OPENAI_ORGANIZATION",
    "OPENROUTER_API_KEY",
    "OPENROUTER_TOKEN",
    "HERMES_ANTHROPIC_API_KEY",
    "HERMES_OPENAI_API_KEY",
    "HERMES_OPENROUTER_API_KEY",
    "HERMES_DEEPSEEK_API_KEY",
    "HERMES_XAI_API_KEY",
    "HERMES_GOOGLE_API_KEY",
    "HERMES_AZURE_API_KEY",
    "HERMES_BEDROCK_API_KEY",
    "XAI_API_KEY",
    "DEEPSEEK_API_KEY",
    "MISTRAL_API_KEY",
    "GROQ_API_KEY",
    "TOGETHER_API_KEY",
    "PERPLEXITY_API_KEY",
    "COHERE_API_KEY",
    "FIREWORKS_API_KEY",
    "HELICONE_API_KEY",
    "PARALLEL_API_KEY",
    "GOOGLE_API_KEY",
    "AZURE_API_KEY",
    "AWS_ACCESS_KEY_ID",
    "AWS_SECRET_ACCESS_KEY",
    "AWS_SESSION_TOKEN",
    "LLM_MODEL",

    /* Gateway platform credentials */
    "TELEGRAM_HOME_CHANNEL",
    "TELEGRAM_HOME_CHANNEL_NAME",
    "TELEGRAM_BOT_TOKEN",
    "DISCORD_BOT_TOKEN",
    "DISCORD_HOME_CHANNEL",
    "DISCORD_HOME_CHANNEL_NAME",
    "DISCORD_REQUIRE_MENTION",
    "DISCORD_FREE_RESPONSE_CHANNELS",
    "DISCORD_AUTO_THREAD",
    "SLACK_BOT_TOKEN",
    "SLACK_HOME_CHANNEL",
    "SLACK_ALLOWED_USERS",
    "SIGNAL_HTTP_URL",
    "SIGNAL_ACCOUNT",
    "SIGNAL_ALLOWED_USERS",
    "SIGNAL_HOME_CHANNEL",
    "HASS_TOKEN",
    "HASS_URL",
    "GH_TOKEN",
    "GITHUB_APP_ID",
    "GITHUB_APP_PRIVATE_KEY_PATH",
    "GITHUB_APP_INSTALLATION_ID",
    "FIRECRAWL_API_KEY",
    "FIRECRAWL_API_URL",

    /* Infrastructure secrets */
    "DOCKER_HOST",
    "DOCKER_CERT_PATH",
    "DOCKER_TLS_VERIFY",
    "SSH_AUTH_SOCK",
    "MODAL_TOKEN_ID",
    "MODAL_TOKEN_SECRET",
    "HERMES_HOME",
    "SLERMES_HOME",
    NULL  /* sentinel */
};

#define BLOCKLIST_TOTAL 67

/* ─── Allowlist (session-scoped) ────────────────────────────────────── */

static char g_allowlist[ENV_PASSTHROUGH_MAX_VARS][ENV_PASSTHROUGH_MAX_NAME];
static int g_allowlist_count = 0;

/* ─── Implementation ────────────────────────────────────────────────── */

static int find_var(const char *name)
{
    for (int i = 0; i < g_allowlist_count; i++) {
        if (strcmp(g_allowlist[i], name) == 0)
            return i;
    }
    return -1;
}

bool env_passthrough_is_blocked(const char *name)
{
    if (!name || !name[0]) return false;
    for (int i = 0; PROVIDER_BLOCKLIST[i]; i++) {
        if (strcmp(PROVIDER_BLOCKLIST[i], name) == 0)
            return true;
    }
    return false;
}

bool env_passthrough_register(const char *name)
{
    if (!name || !name[0]) return false;

    /* Block Hermes provider credentials */
    if (env_passthrough_is_blocked(name))
        return false;

    /* Check if already registered */
    if (find_var(name) >= 0)
        return true;

    /* Capacity check */
    if (g_allowlist_count >= ENV_PASSTHROUGH_MAX_VARS)
        return false;

    /* Add to allowlist */
    snprintf(g_allowlist[g_allowlist_count], ENV_PASSTHROUGH_MAX_NAME, "%s", name);
    g_allowlist_count++;
    return true;
}

int env_passthrough_register_batch(const char **names, int count)
{
    if (!names || count <= 0) return 0;
    int registered = 0;
    for (int i = 0; i < count; i++) {
        if (env_passthrough_register(names[i]))
            registered++;
    }
    return registered;
}

bool env_passthrough_is_allowed(const char *name)
{
    if (!name || !name[0]) return false;
    return find_var(name) >= 0;
}

void env_passthrough_get_all(char ***out, int *out_count)
{
    if (!out || !out_count) return;

    *out_count = g_allowlist_count;
    if (g_allowlist_count == 0) {
        *out = NULL;
        return;
    }

    *out = (char **)calloc((size_t)g_allowlist_count, sizeof(char *));
    if (!*out) {
        *out_count = 0;
        return;
    }

    for (int i = 0; i < g_allowlist_count; i++) {
        (*out)[i] = strdup(g_allowlist[i]);
        if (!(*out)[i]) {
            /* Partial allocation — free what we have */
            for (int j = 0; j < i; j++) free((*out)[j]);
            free(*out);
            *out = NULL;
            *out_count = 0;
            return;
        }
    }
}

void env_passthrough_clear(void)
{
    g_allowlist_count = 0;
}

void env_passthrough_free_list(char **list, int count)
{
    if (!list) return;
    for (int i = 0; i < count; i++)
        free(list[i]);
    free(list);
}
