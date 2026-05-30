/* test_context_engine.c — Tests for context_engine interface + defaults.
 * Port of Python agent/context_engine.py test concepts.
 */
#include "hermes_context_engine.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test default init */
static void test_default_init(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test-engine");

    assert(strcmp(engine.name, "test-engine") == 0);
    assert(engine.threshold_percent == 0.75f);
    assert(engine.protect_first_n == 3);
    assert(engine.protect_last_n == 6);
    assert(engine.last_prompt_tokens == 0);
    assert(engine.compression_count == 0);
    assert(engine.should_compress_preflight != NULL);
    assert(engine.has_content_to_compress != NULL);
    assert(engine.on_session_reset != NULL);
    assert(engine.get_status != NULL);
    assert(engine.get_tool_schemas != NULL);
    assert(engine.handle_tool_call != NULL);
    assert(engine.update_model != NULL);
    /* Core methods left NULL for caller to set */
    assert(engine.update_from_response == NULL);
    assert(engine.should_compress == NULL);
    assert(engine.compress == NULL);
}

/* Test default init with NULL name */
static void test_default_name(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, NULL);
    assert(strcmp(engine.name, "compressor") == 0);
}

/* Test on_session_reset zeros counters */
static void test_on_session_reset(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");
    engine.last_prompt_tokens = 100;
    engine.last_completion_tokens = 50;
    engine.last_total_tokens = 150;
    engine.compression_count = 3;

    assert(engine.on_session_reset != NULL);
    engine.on_session_reset(&engine);

    assert(engine.last_prompt_tokens == 0);
    assert(engine.last_completion_tokens == 0);
    assert(engine.last_total_tokens == 0);
    assert(engine.compression_count == 0);
}

/* Test get_status returns correct fields */
static void test_get_status(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");
    engine.last_prompt_tokens = 500;
    engine.context_length = 4096;
    engine.threshold_tokens = 3072;

    assert(engine.get_status != NULL);
    json_t *status = engine.get_status(&engine);
    assert(status != NULL);

    int prompt = (int)json_get_num(status, "last_prompt_tokens", 0);
    int threshold = (int)json_get_num(status, "threshold_tokens", 0);
    int ctx_len = (int)json_get_num(status, "context_length", 0);
    int usage = (int)json_get_num(status, "usage_percent", 0);
    int count = (int)json_get_num(status, "compression_count", 0);

    assert(prompt == 500);
    assert(threshold == 3072);
    assert(ctx_len == 4096);
    assert(usage == 12); /* 500/4096*100 = 12.2 → 12 */
    assert(count == 0);

    json_free(status);
}

/* Test get_status: 100% cap when prompt > context */
static void test_get_status_overflow(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");
    engine.last_prompt_tokens = 5000;
    engine.context_length = 4096;

    json_t *status = engine.get_status(&engine);
    assert(status != NULL);
    int usage = (int)json_get_num(status, "usage_percent", 0);
    assert(usage == 100);
    json_free(status);
}

/* Test get_status: 0% when context_length is 0 */
static void test_get_status_no_context(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");
    engine.last_prompt_tokens = 100;
    engine.context_length = 0;

    json_t *status = engine.get_status(&engine);
    assert(status != NULL);
    int usage = (int)json_get_num(status, "usage_percent", 0);
    assert(usage == 0);
    json_free(status);
}

/* Test get_tool_schemas returns empty array */
static void test_get_tool_schemas(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");

    json_t *schemas = engine.get_tool_schemas(&engine);
    assert(schemas != NULL);
    assert(json_len(schemas) == 0);
    json_free(schemas);
}

/* Test handle_tool_call returns error */
static void test_handle_tool_call(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");

    const char *result = engine.handle_tool_call(&engine, "unknown_tool", NULL, NULL);
    assert(result != NULL);
    /* Should be a valid JSON error */
    json_t *parsed = json_parse(result, NULL);
    assert(parsed != NULL);
    const char *err = json_get_str(parsed, "error", "");
    assert(strstr(err, "Unknown") != NULL);
    free((void*)result);
    json_free(parsed);
}

/* Test update_model */
static void test_update_model(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");
    engine.threshold_percent = 0.5f; /* 50% threshold for test */

    engine.update_model(&engine, "gpt-4", 8192, "", "", "");

    assert(engine.context_length == 8192);
    assert(engine.threshold_tokens == 4096); /* 50% of 8192 */
}

/* Test should_compress_preflight returns false */
static void test_should_compress_preflight(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");

    assert(engine.should_compress_preflight(&engine, NULL) == false);
}

/* Test has_content_to_compress returns true */
static void test_has_content_to_compress(void) {
    context_engine_t engine;
    context_engine_init_default(&engine, "test");

    assert(engine.has_content_to_compress(&engine, NULL) == true);
}

int main(void) {
    test_default_init();
    test_default_name();
    test_on_session_reset();
    test_get_status();
    test_get_status_overflow();
    test_get_status_no_context();
    test_get_tool_schemas();
    test_handle_tool_call();
    test_update_model();
    test_should_compress_preflight();
    test_has_content_to_compress();

    printf("context_engine: 11/11 pass\n");
    return 0;
}
