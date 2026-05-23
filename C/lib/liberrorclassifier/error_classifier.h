#ifndef HERMES_ERROR_CLASSIFIER_H
#define HERMES_ERROR_CLASSIFIER_H

/*
 * error_classifier.h — API error classification for smart failover/recovery.
 * Port of Python agent/error_classifier.py.
 *
 * Taxonomy of API errors with a priority-ordered classification pipeline
 * that determines the correct recovery action (retry, rotate credential,
 * fallback to another provider, compress context, or abort).
 *
 * MIT License — WuBu Hermes Project
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ── Error taxonomy ──────────────────────────────────────────────────── */

typedef enum {
    /* Authentication / authorization */
    FAILOVER_AUTH,                    /* Transient auth (401/403) — refresh/rotate */
    FAILOVER_AUTH_PERMANENT,          /* Auth failed after refresh — abort */

    /* Billing / quota */
    FAILOVER_BILLING,                 /* 402 or confirmed credit exhaustion */
    FAILOVER_RATE_LIMIT,              /* 429 or quota-based throttling */

    /* Server-side */
    FAILOVER_OVERLOADED,              /* 503/529 — provider overloaded */
    FAILOVER_SERVER_ERROR,            /* 500/502 — internal server error */

    /* Transport */
    FAILOVER_TIMEOUT,                 /* Connection/read timeout */

    /* Context / payload */
    FAILOVER_CONTEXT_OVERFLOW,        /* Context too large */
    FAILOVER_PAYLOAD_TOO_LARGE,       /* 413 — compress payload */
    FAILOVER_IMAGE_TOO_LARGE,         /* Image exceeds per-image limit */

    /* Model */
    FAILOVER_MODEL_NOT_FOUND,         /* 404 or invalid model */
    FAILOVER_PROVIDER_POLICY_BLOCKED, /* Aggregator blocked by data policy */

    /* Request format */
    FAILOVER_FORMAT_ERROR,            /* 400 bad request */
    FAILOVER_MULTIMODAL_TOOL_UNSUPPORTED, /* Provider rejected list-type tool content */

    /* Provider-specific */
    FAILOVER_THINKING_SIGNATURE,      /* Anthropic thinking block sig invalid */
    FAILOVER_LONG_CONTEXT_TIER,       /* Anthropic extra usage tier gate */
    FAILOVER_OAUTH_LONG_CONTEXT_BETA_FORBIDDEN, /* Anthropic OAuth 1M ctx beta forbidden */
    FAILOVER_LLAMA_CPP_GRAMMAR,       /* llama.cpp json-schema-to-grammar rejection */

    /* Catch-all */
    FAILOVER_UNKNOWN
} failover_reason_t;

/* ── Classification result ──────────────────────────────────────────── */

typedef struct {
    failover_reason_t reason;
    int status_code;                  /* -1 if unknown */
    char provider[64];
    char model[128];
    char message[1024];

    /* Recovery action hints */
    bool retryable;
    bool should_compress;
    bool should_rotate_credential;
    bool should_fallback;
} classified_error_t;

/* ── Public API ─────────────────────────────────────────────────────── */

/*
 * Classify an API error from status code, error body JSON, provider, model.
 *
 * @param status_code  HTTP status code (0 or negative if unknown)
 * @param error_body   JSON error body string (may be NULL or empty)
 * @param provider     Provider name (e.g. "openrouter", "anthropic")
 * @param model        Model slug
 * @param approx_tokens Approximate token count of current context (0 if unknown)
 * @param context_length Maximum context length for the model (0 if unknown)
 * @param[out] result  Filled with classification result
 */
void error_classify(int status_code, const char *error_body,
                    const char *provider, const char *model,
                    int approx_tokens, int context_length,
                    classified_error_t *result);

/*
 * Format a classified error into a human-readable string.
 * Returns the number of chars written (excluding NUL).
 */
int error_format(const classified_error_t *err, char *buf, size_t bufsz);

/*
 * Check if reason is auth-related.
 */
static inline bool error_is_auth(const classified_error_t *err) {
    return err->reason == FAILOVER_AUTH ||
           err->reason == FAILOVER_AUTH_PERMANENT;
}

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ERROR_CLASSIFIER_H */
