#ifndef HERMES_TOOL_BACKEND_H
#define HERMES_TOOL_BACKEND_H

/*
 * tool_backend.h — Shared helpers for tool backend selection.
 * Port of Python tools/tool_backend_helpers.py.
 *
 * Provides backend-selection helpers for browser, Modal, gateway,
 * and image-gen tools.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ─── Nous managed-tool gateway ─────────────────────────── */

/**
 * Check whether managed Nous tools are enabled.
 * Returns true when the user has a non-free-tier Nous subscription.
 * Never raises — returns false on any error.
 */
bool managed_nous_tools_enabled(void);

/* ─── Browser provider helpers ───────────────────────────── */

#define BROWSER_PROVIDER_LOCAL  "local"

/**
 * Normalize a browser cloud provider string.
 * Returns "local" when value is NULL, empty, or unrecognized.
 */
const char *normalize_browser_cloud_provider(const char *value);

/* ─── Modal backend helpers ──────────────────────────────── */

#define MODAL_MODE_AUTO    "auto"
#define MODAL_MODE_DIRECT  "direct"
#define MODAL_MODE_MANAGED "managed"

/**
 * Coerce a modal mode string to a valid value.
 * Returns "auto" on NULL, empty, or invalid input.
 */
const char *coerce_modal_mode(const char *value);

/** Alias for coerce_modal_mode */
const char *normalize_modal_mode(const char *value);

/**
 * Check whether direct Modal credentials are available.
 * Checks MODAL_TOKEN_ID + MODAL_TOKEN_SECRET env vars
 * and ~/.modal.toml existence.
 */
bool has_direct_modal_credentials(void);

/** Result of resolve_modal_backend_state() */
typedef struct {
    const char *requested_mode;   /**< Raw requested mode string */
    const char *mode;             /**< Normalized mode */
    bool        has_direct;       /**< Direct credentials exist */
    bool        managed_ready;    /**< Managed gateway is ready */
    bool        managed_mode_blocked; /**< Requested managed but not available */
    const char *selected_backend; /**< "direct", "managed", or NULL */
} modal_backend_state_t;

/**
 * Resolve direct vs managed Modal backend selection.
 * Semantics match Python:
 *   "direct"  → direct-only
 *   "managed" → managed-only
 *   "auto"    → prefers managed, falls back to direct
 */
modal_backend_state_t resolve_modal_backend_state(
    const char *modal_mode,
    bool has_direct,
    bool managed_ready
);

/* ─── OpenAI audio key ──────────────────────────────────── */

/**
 * Resolve the OpenAI API key for audio/voice tools.
 * Prefers VOICE_TOOLS_OPENAI_KEY, falls back to OPENAI_API_KEY.
 * Returns empty string when neither is set.
 */
const char *resolve_openai_audio_api_key(void);

/* ─── Gateway preference ────────────────────────────────── */

/**
 * Check whether the user opted into the Tool Gateway for a
 * given config section.
 * Reads <section>.use_gateway from config. Never raises.
 * Returns false on any error.
 */
bool prefers_gateway(const char *config_section);

/* ─── FAL key check ─────────────────────────────────────── */

/**
 * Check whether FAL_KEY is configured.
 * Consults SLERMES_FAL_KEY env var (preferred) and FAL_KEY.
 * Returns true when set to a non-empty value.
 */
bool fal_key_configured(void);

void tool_backend_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_BACKEND_H */
