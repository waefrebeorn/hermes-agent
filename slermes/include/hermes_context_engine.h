/*
 * hermes_context_engine.h — Pluggable context engine interface for Hermes C.
 * Port of Python agent/context_engine.py (211 lines).
 *
 * A context engine controls how conversation context is managed when
 * approaching the model's token limit. The built-in compressor is the
 * default implementation. Third-party engines can replace it via the
 * plugin system.
 *
 * Lifecycle:
 *   1. Engine struct is populated and registered
 *   2. on_session_start() called when a conversation begins
 *   3. update_from_response() called after each API response
 *   4. should_compress() checked after each turn
 *   5. compress() called when should_compress() returns true
 *   6. on_session_end() called at real session boundaries
 */
#ifndef HERMES_CONTEXT_ENGINE_H
#define HERMES_CONTEXT_ENGINE_H

#include "hermes_json.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Engine name max length */
#define CONTEXT_ENGINE_NAME_MAX 64

/* Forward declaration */
typedef struct context_engine_t context_engine_t;

/* Function pointer types for context engine vtable */
typedef void (*ce_on_session_start_fn)(context_engine_t *engine,
    const char *session_id, json_t *kwargs);

typedef void (*ce_on_session_end_fn)(context_engine_t *engine,
    const char *session_id, json_t *messages);

typedef void (*ce_update_from_response_fn)(context_engine_t *engine,
    json_t *usage);

typedef bool (*ce_should_compress_fn)(context_engine_t *engine,
    int prompt_tokens);

typedef json_t *(*ce_compress_fn)(context_engine_t *engine,
    json_t *messages, int current_tokens, const char *focus_topic);

typedef bool (*ce_should_compress_preflight_fn)(context_engine_t *engine,
    json_t *messages);

typedef bool (*ce_has_content_to_compress_fn)(context_engine_t *engine,
    json_t *messages);

typedef void (*ce_on_session_reset_fn)(context_engine_t *engine);

typedef json_t *(*ce_get_tool_schemas_fn)(context_engine_t *engine);

typedef const char *(*ce_handle_tool_call_fn)(context_engine_t *engine,
    const char *name, json_t *args, json_t *kwargs);

typedef json_t *(*ce_get_status_fn)(context_engine_t *engine);

typedef void (*ce_update_model_fn)(context_engine_t *engine,
    const char *model, int context_length,
    const char *base_url, const char *api_key, const char *provider);

/* Context engine struct — vtable pattern matching Python ABC */
struct context_engine_t {
    /* Identity */
    char name[CONTEXT_ENGINE_NAME_MAX];

    /* Token state (read by agent loop for display/logging) */
    int last_prompt_tokens;
    int last_completion_tokens;
    int last_total_tokens;
    int threshold_tokens;
    int context_length;
    int compression_count;

    /* Compaction parameters */
    float threshold_percent;
    int protect_first_n;
    int protect_last_n;

    /* --- Virtual methods --- */

    /* Core interface — must be non-NULL */
    ce_update_from_response_fn       update_from_response;
    ce_should_compress_fn             should_compress;
    ce_compress_fn                    compress;

    /* Optional: name is set directly in struct, not via vtable */

    /* Optional — set to NULL for defaults */

    /* Pre-flight check before API call (default: false) */
    ce_should_compress_preflight_fn   should_compress_preflight;

    /* Quick check if there's anything to compress (default: true) */
    ce_has_content_to_compress_fn     has_content_to_compress;

    /* Session lifecycle */
    ce_on_session_start_fn            on_session_start;
    ce_on_session_end_fn              on_session_end;

    /* Reset per-session state */
    ce_on_session_reset_fn            on_session_reset;

    /* Tool schemas this engine provides */
    ce_get_tool_schemas_fn            get_tool_schemas;

    /* Handle a tool call from the agent */
    ce_handle_tool_call_fn            handle_tool_call;

    /* Status dict for display/logging */
    ce_get_status_fn                  get_status;

    /* Called when user switches models */
    ce_update_model_fn                update_model;
};

/* ================================================================
 *  Default implementations (match Python ContextEngine defaults)
 * ================================================================ */

/* Initialize engine with default values. Sets name and default vtables. */
void context_engine_init_default(context_engine_t *engine, const char *name);

/* Default on_session_reset: zeroes counters */
void context_engine_default_on_session_reset(context_engine_t *engine);

/* Default get_status: returns standard fields */
json_t *context_engine_default_get_status(context_engine_t *engine);

/* Default get_tool_schemas: returns empty list */
json_t *context_engine_default_get_tool_schemas(context_engine_t *engine);

/* Default update_model: updates context_length, recalculates threshold */
void context_engine_default_update_model(context_engine_t *engine,
    const char *model, int context_length,
    const char *base_url, const char *api_key, const char *provider);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CONTEXT_ENGINE_H */
