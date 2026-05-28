/*
 * context_engine.c — Pluggable context engine default implementations.
 * Port of Python agent/context_engine.py (211 lines).
 *
 * Provides default method implementations matching Python ContextEngine ABC
 * defaults, plus an init function that wires them up.
 */
#include "hermes_context_engine.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Default implementations
 * ================================================================ */

/* Default on_session_reset: zero counters */
void context_engine_default_on_session_reset(context_engine_t *engine) {
    if (!engine) return;
    engine->last_prompt_tokens = 0;
    engine->last_completion_tokens = 0;
    engine->last_total_tokens = 0;
    engine->compression_count = 0;
}

/* Default get_status: standard fields */
json_t *context_engine_default_get_status(context_engine_t *engine) {
    json_t *status = json_new_object();
    if (!status) return NULL;

    json_object_set(status, "last_prompt_tokens",
        json_new_number(engine->last_prompt_tokens));
    json_object_set(status, "threshold_tokens",
        json_new_number(engine->threshold_tokens));
    json_object_set(status, "context_length",
        json_new_number(engine->context_length));

    int usage_pct = 0;
    if (engine->context_length > 0 && engine->last_prompt_tokens > 0) {
        int val = (engine->last_prompt_tokens * 100) / engine->context_length;
        usage_pct = val < 100 ? val : 100;
    }
    json_object_set(status, "usage_percent", json_new_number(usage_pct));
    json_object_set(status, "compression_count",
        json_new_number(engine->compression_count));

    return status;
}

/* Default get_tool_schemas: empty list */
json_t *context_engine_default_get_tool_schemas(
    context_engine_t *engine)
{
    (void)engine;
    return json_new_array();
}

/* Default update_model: update context_length, recalculate threshold */
void context_engine_default_update_model(context_engine_t *engine,
    const char *model, int context_length,
    const char *base_url, const char *api_key, const char *provider)
{
    (void)model;
    (void)base_url;
    (void)api_key;
    (void)provider;
    if (!engine) return;
    engine->context_length = context_length;
    engine->threshold_tokens = (int)(context_length * engine->threshold_percent);
}

/* Default should_compress_preflight: return false */
static bool default_should_compress_preflight(
    context_engine_t *engine, json_t *messages)
{
    (void)engine;
    (void)messages;
    return false;
}

/* Default has_content_to_compress: return true */
static bool default_has_content_to_compress(
    context_engine_t *engine, json_t *messages)
{
    (void)engine;
    (void)messages;
    return true;
}

/* Default on_session_start: noop */
static void default_on_session_start(context_engine_t *engine,
    const char *session_id, json_t *kwargs)
{
    (void)engine;
    (void)session_id;
    (void)kwargs;
}

/* Default on_session_end: noop */
static void default_on_session_end(context_engine_t *engine,
    const char *session_id, json_t *messages)
{
    (void)engine;
    (void)session_id;
    (void)messages;
}

/* Default handle_tool_call: error response */
static const char *default_handle_tool_call(context_engine_t *engine,
    const char *name, json_t *args, json_t *kwargs)
{
    (void)engine;
    (void)name;
    (void)args;
    (void)kwargs;
    json_t *err = json_new_object();
    if (!err) return strdup("{\"error\":\"unknown tool\"}");
    json_object_set(err, "error", json_new_string("Unknown context engine tool"));
    char *s = json_serialize(err);
    json_free(err);
    return s;
}

/* ================================================================
 *  Init: wire defaults into a context_engine_t
 * ================================================================ */

void context_engine_init_default(context_engine_t *engine, const char *name) {
    if (!engine) return;
    memset(engine, 0, sizeof(*engine));

    if (name)
        snprintf(engine->name, sizeof(engine->name), "%s", name);
    else
        snprintf(engine->name, sizeof(engine->name), "%s", "compressor");

    /* Default parameters (match Python) */
    engine->threshold_percent = 0.75f;
    engine->protect_first_n = 3;
    engine->protect_last_n = 6;

    /* Default token counts */
    engine->last_prompt_tokens = 0;
    engine->last_completion_tokens = 0;
    engine->last_total_tokens = 0;
    engine->threshold_tokens = 0;
    engine->context_length = 0;
    engine->compression_count = 0;

    /* Wire default vtable entries */
    engine->should_compress_preflight = default_should_compress_preflight;
    engine->has_content_to_compress = default_has_content_to_compress;
    engine->on_session_start = default_on_session_start;
    engine->on_session_end = default_on_session_end;
    engine->on_session_reset = context_engine_default_on_session_reset;
    engine->get_tool_schemas = context_engine_default_get_tool_schemas;
    engine->handle_tool_call = default_handle_tool_call;
    engine->get_status = context_engine_default_get_status;
    engine->update_model = context_engine_default_update_model;

    /* Core methods MUST be set by caller after init:
     *   update_from_response
     *   should_compress
     *   compress
     * These are left NULL — caller must provide them. */
}
