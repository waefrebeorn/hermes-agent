/*
 * tests/test_auxiliary_client.c — unit tests for auxiliary client
 *
 * Tests: "auto" → main provider resolution, explicit override, task lookup.
 */

#include "auxiliary_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;

#define TEST(name) do { \
    printf("  %s ... ", #name); \
    if (test_##name()) { \
        printf("PASS\n"); pass++; \
    } else { \
        printf("FAIL\n"); fail++; \
    } \
} while(0)

#define CHECK(cond, fmt, ...) do { \
    if (!(cond)) { \
        printf("    FAIL at %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        return 0; \
    } \
} while(0)

static int test_resolve_auto_main_provider(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "openai", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gpt-4o", sizeof(cfg.model) - 1);
    strncpy(cfg.api_key, "sk-test", sizeof(cfg.api_key) - 1);
    strncpy(cfg.base_url, "https://api.openai.com/v1", sizeof(cfg.base_url) - 1);

    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));
    strncpy(task.provider, "auto", sizeof(task.provider) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_llm_config(&cfg, &task, &out);
    CHECK(ok != 0, "resolve should succeed with auto provider");
    CHECK(strcmp(out.provider, "openai") == 0,
          "auto -> main provider 'openai', got '%s'", out.provider);
    CHECK(strcmp(out.model, "gpt-4o") == 0,
          "model should be main 'gpt-4o', got '%s'", out.model);
    CHECK(strcmp(out.api_key, "sk-test") == 0,
          "api_key should be main 'sk-test', got '%s'", out.api_key);
    CHECK(strcmp(out.base_url, "https://api.openai.com/v1") == 0,
          "base_url should be main, got '%s'", out.base_url);

    return 1;
}

static int test_resolve_explicit_provider(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "openai", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gpt-4o", sizeof(cfg.model) - 1);
    strncpy(cfg.api_key, "sk-main", sizeof(cfg.api_key) - 1);

    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));
    strncpy(task.provider, "anthropic", sizeof(task.provider) - 1);
    strncpy(task.model, "claude-sonnet-4", sizeof(task.model) - 1);
    strncpy(task.api_key, "sk-ant-test", sizeof(task.api_key) - 1);
    strncpy(task.base_url, "https://api.anthropic.com/v1", sizeof(task.base_url) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_llm_config(&cfg, &task, &out);
    CHECK(ok != 0, "resolve should succeed");
    CHECK(strcmp(out.provider, "anthropic") == 0,
          "explicit provider 'anthropic', got '%s'", out.provider);
    CHECK(strcmp(out.model, "claude-sonnet-4") == 0,
          "explicit model 'claude-sonnet-4', got '%s'", out.model);
    CHECK(strcmp(out.api_key, "sk-ant-test") == 0,
          "explicit api_key, got '%s'", out.api_key);
    CHECK(strcmp(out.base_url, "https://api.anthropic.com/v1") == 0,
          "explicit base_url, got '%s'", out.base_url);

    return 1;
}

static int test_resolve_blank_provider(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "deepseek", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "deepseek-chat", sizeof(cfg.model) - 1);

    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_llm_config(&cfg, &task, &out);
    CHECK(ok != 0, "blank provider should fallback to main");
    CHECK(strcmp(out.provider, "deepseek") == 0,
          "blank -> main provider 'deepseek', got '%s'", out.provider);

    return 1;
}

static int test_resolve_task_by_name(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "openai", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gpt-4o", sizeof(cfg.model) - 1);
    strncpy(cfg.api_key, "sk-test", sizeof(cfg.api_key) - 1);

    strncpy(cfg.auxiliary.compression.provider, "anthropic", sizeof(cfg.auxiliary.compression.provider) - 1);
    strncpy(cfg.auxiliary.compression.model, "claude-sonnet-4", sizeof(cfg.auxiliary.compression.model) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_task(&cfg, "compression", &out);
    CHECK(ok != 0, "resolve compression should succeed");
    CHECK(strcmp(out.provider, "anthropic") == 0,
          "compression provider 'anthropic', got '%s'", out.provider);
    CHECK(strcmp(out.model, "claude-sonnet-4") == 0,
          "compression model 'claude-sonnet-4', got '%s'", out.model);

    const char *label = auxiliary_task_label("compression");
    CHECK(strcmp(label, "context compression") == 0,
          "label 'context compression', got '%s'", label);

    label = auxiliary_task_label("nonexistent");
    CHECK(strcmp(label, "nonexistent") == 0,
          "unknown task name returned, got '%s'", label);

    return 1;
}

static int test_resolve_null_args(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));
    llm_config_t out;

    int ok = auxiliary_resolve_llm_config(NULL, &task, &out);
    CHECK(ok == 0, "NULL config should fail");

    ok = auxiliary_resolve_llm_config(&cfg, NULL, &out);
    CHECK(ok == 0, "NULL task should fail");

    ok = auxiliary_resolve_llm_config(&cfg, &task, NULL);
    CHECK(ok == 0, "NULL output should fail");

    ok = auxiliary_resolve_task(NULL, "compression", &out);
    CHECK(ok == 0, "NULL config should fail in resolve_task");

    ok = auxiliary_resolve_task(&cfg, NULL, &out);
    CHECK(ok == 0, "NULL task_name should fail");

    ok = auxiliary_resolve_task(&cfg, "compression", NULL);
    CHECK(ok == 0, "NULL output should fail in resolve_task");

    return 1;
}

static int test_resolve_auto_with_override_model(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "openai", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gpt-4o", sizeof(cfg.model) - 1);
    strncpy(cfg.api_key, "sk-main", sizeof(cfg.api_key) - 1);
    strncpy(cfg.base_url, "https://api.openai.com/v1", sizeof(cfg.base_url) - 1);

    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));
    strncpy(task.provider, "auto", sizeof(task.provider) - 1);
    strncpy(task.model, "gpt-4o-mini", sizeof(task.model) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_llm_config(&cfg, &task, &out);
    CHECK(ok != 0, "resolve should succeed");
    CHECK(strcmp(out.provider, "openai") == 0,
          "provider should be main 'openai', got '%s'", out.provider);
    CHECK(strcmp(out.model, "gpt-4o-mini") == 0,
          "model should be task override 'gpt-4o-mini', got '%s'", out.model);
    CHECK(strcmp(out.api_key, "sk-main") == 0,
          "api_key should be main, got '%s'", out.api_key);

    return 1;
}

static int test_resolve_task_vision_auto(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "google", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gemini-2.5-flash", sizeof(cfg.model) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_task(&cfg, "vision", &out);
    CHECK(ok != 0, "resolve vision should succeed");
    CHECK(strcmp(out.provider, "google") == 0,
          "vision (auto) -> main 'google', got '%s'", out.provider);
    CHECK(strcmp(out.model, "gemini-2.5-flash") == 0,
          "vision model should be main, got '%s'", out.model);

    return 1;
}

static int test_resolve_task_unknown(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_task(&cfg, "nonexistent_task", &out);
    CHECK(ok == 0, "unknown task should fail");

    return 1;
}

int main(void) {
    printf("Auxiliary client tests:\n");

    TEST(resolve_auto_main_provider);
    TEST(resolve_explicit_provider);
    TEST(resolve_blank_provider);
    TEST(resolve_task_by_name);
    TEST(resolve_null_args);
    TEST(resolve_auto_with_override_model);
    TEST(resolve_task_vision_auto);
    TEST(resolve_task_unknown);

    printf("  Results: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
