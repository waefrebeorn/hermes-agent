/*
 * tool_backend.c — Shared helpers for tool backend selection.
 * Port of Python tools/tool_backend_helpers.py.
 *
 * Provides backend-selection helpers for browser, Modal, gateway,
 * and image-gen tools.
 */

#include "tool_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

/* ─── Internal helpers ──────────────────────────────────── */

static const char *s_openai_key = NULL;
static char s_key_buf[4096] = {0};

/* Testing support — reset cached state */
void tool_backend_reset(void) {
    s_openai_key = NULL;
    s_key_buf[0] = '\0';
}

/* ─── Nous managed-tool gateway ─────────────────────────── */

/*
 * The Python version calls into hermes_cli.auth and hermes_cli.models
 * to check Nous subscription status. The C build currently has no
 * auth/plan-check infrastructure, so we rely on env-var opt-in:
 * NOUS_MANAGED_TOOLS=true enables gateway features.
 *
 * This is conservative — it mirrors the Python default of "off unless
 * authenticated and non-free-tier" without requiring the full OAuth
 * flow to be ported first.
 */
bool managed_nous_tools_enabled(void)
{
    const char *val = getenv("NOUS_MANAGED_TOOLS");
    if (!val) return false;
    return (strcmp(val, "true") == 0 ||
            strcmp(val, "1") == 0 ||
            strcasecmp(val, "yes") == 0);
}

/* ─── Browser provider helpers ──────────────────────────── */

const char *normalize_browser_cloud_provider(const char *value)
{
    if (!value || !value[0])
        return BROWSER_PROVIDER_LOCAL;

    /* Simple comparison — could expand to a provider registry */
    if (strcmp(value, "local") == 0 ||
        strcmp(value, "camofox") == 0 ||
        strcmp(value, "playwright") == 0)
        return value;

    return BROWSER_PROVIDER_LOCAL;
}

/* ─── Modal backend helpers ──────────────────────────────── */

const char *coerce_modal_mode(const char *value)
{
    if (!value || !value[0])
        return MODAL_MODE_AUTO;

    if (strcmp(value, MODAL_MODE_DIRECT) == 0)
        return MODAL_MODE_DIRECT;
    if (strcmp(value, MODAL_MODE_MANAGED) == 0)
        return MODAL_MODE_MANAGED;

    return MODAL_MODE_AUTO;
}

const char *normalize_modal_mode(const char *value)
{
    return coerce_modal_mode(value);
}

bool has_direct_modal_credentials(void)
{
    /* Check env vars */
    const char *id = getenv("MODAL_TOKEN_ID");
    const char *secret = getenv("MODAL_TOKEN_SECRET");
    if (id && id[0] && secret && secret[0])
        return true;

    /* Check ~/.modal.toml */
    const char *home = getenv("HOME");
    if (home) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/.modal.toml", home);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            return true;
    }

    return false;
}

void modal_backend_state_free(modal_backend_state_t *state)
{
    (void)state; /* Currently no heap-allocated fields */
}

modal_backend_state_t resolve_modal_backend_state(
    const char *modal_mode,
    bool has_direct,
    bool managed_ready)
{
    modal_backend_state_t result;

    const char *requested = coerce_modal_mode(modal_mode);
    const char *normalized = requested;
    bool nous_enabled = managed_nous_tools_enabled();

    result.requested_mode = requested;
    result.mode = normalized;
    result.has_direct = has_direct;
    result.managed_ready = managed_ready;
    result.managed_mode_blocked = (strcmp(requested, MODAL_MODE_MANAGED) == 0 &&
                                   (!nous_enabled || !managed_ready));

    if (strcmp(normalized, MODAL_MODE_MANAGED) == 0) {
        result.selected_backend = (nous_enabled && managed_ready)
                                    ? MODAL_MODE_MANAGED : NULL;
    } else if (strcmp(normalized, MODAL_MODE_DIRECT) == 0) {
        result.selected_backend = has_direct ? MODAL_MODE_DIRECT : NULL;
    } else {
        /* auto: prefer managed, fall back to direct */
        if (nous_enabled && managed_ready)
            result.selected_backend = MODAL_MODE_MANAGED;
        else if (has_direct)
            result.selected_backend = MODAL_MODE_DIRECT;
        else
            result.selected_backend = NULL;
    }

    return result;
}

/* ─── OpenAI audio key ──────────────────────────────────── */

const char *resolve_openai_audio_api_key(void)
{
    if (s_openai_key)
        return s_openai_key;

    const char *voice_key = getenv("VOICE_TOOLS_OPENAI_KEY");
    const char *openai_key = getenv("OPENAI_API_KEY");

    if (voice_key && voice_key[0]) {
        strncpy(s_key_buf, voice_key, sizeof(s_key_buf) - 1);
        s_openai_key = s_key_buf;
    } else if (openai_key && openai_key[0]) {
        strncpy(s_key_buf, openai_key, sizeof(s_key_buf) - 1);
        s_openai_key = s_key_buf;
    } else {
        s_key_buf[0] = '\0';
        s_openai_key = "";
    }

    return s_openai_key;
}

/* ─── Gateway preference ────────────────────────────────── */

bool prefers_gateway(const char *config_section)
{
    if (!config_section || !config_section[0])
        return false;

    /*
     * The Python reads config.yaml <section>.use_gateway.
     * C config system (hermes_config_t) doesn't have a generic
     * section-reader for tool_output-style sections yet.
     *
     * Env-var fallback: <SECTION>_USE_GATEWAY=true
     * (e.g. IMAGE_GEN_USE_GATEWAY=true)
     */
    char env_name[256];
    size_t i;
    for (i = 0; config_section[i] && i < sizeof(env_name) - 6; i++) {
        env_name[i] = (char)toupper((unsigned char)config_section[i]);
    }
    env_name[i] = '\0';
    strcat(env_name, "_USE_GATEWAY");

    const char *val = getenv(env_name);
    if (!val) return false;
    return (strcmp(val, "true") == 0 ||
            strcmp(val, "1") == 0 ||
            strcasecmp(val, "yes") == 0);
}

/* ─── FAL key check ─────────────────────────────────────── */

bool fal_key_configured(void)
{
    /* Check SLERMES_FAL_KEY (C convention) first */
    const char *key = getenv("SLERMES_FAL_KEY");
    if (key && key[0])
        return true;

    /* Fall back to FAL_KEY (Python convention) */
    key = getenv("FAL_KEY");
    if (key && key[0])
        return true;

    return false;
}
