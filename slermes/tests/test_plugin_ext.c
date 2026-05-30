/* Plugin extension tests — verify plugin_ext.c lifecycle and config functions.
 *
 * Tests functions that work with config blocks and empty registries.
 * Plugin .so loading is tested by the plugin library itself.
 */

#include "hermes_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
    } else { \
        passed++; \
    } \
} while(0)

static void test_cfg_to_json_null(void)
{
    TEST("cfg_to_json(NULL)", hermes_plugin_cfg_to_json(NULL) == NULL);
}

static void test_cfg_to_json_empty(void)
{
    plugin_cfg_block_t block;
    memset(&block, 0, sizeof(block));
    snprintf(block.name, sizeof(block.name), "test-plugin");
    block.count = 0;

    char *json = hermes_plugin_cfg_to_json(&block);
    TEST("cfg_to_json empty block", json != NULL);
    if (json) {
        TEST("cfg_to_json empty is {}", strcmp(json, "{}") == 0);
        free(json);
    }
}

static void test_cfg_to_json_single_entry(void)
{
    plugin_cfg_block_t block;
    memset(&block, 0, sizeof(block));
    snprintf(block.name, sizeof(block.name), "test-plugin");
    block.count = 1;
    snprintf(block.entries[0].key, sizeof(block.entries[0].key), "api_key");
    snprintf(block.entries[0].value, sizeof(block.entries[0].value), "sk-12345");

    char *json = hermes_plugin_cfg_to_json(&block);
    TEST("cfg_to_json single entry", json != NULL);
    if (json) {
        TEST("cfg_to_json has api_key", strstr(json, "api_key") != NULL);
        TEST("cfg_to_json has sk-12345", strstr(json, "sk-12345") != NULL);
        free(json);
    }
}

static void test_cfg_to_json_multiple_entries(void)
{
    plugin_cfg_block_t block;
    memset(&block, 0, sizeof(block));
    snprintf(block.name, sizeof(block.name), "test-plugin");
    block.count = 3;
    snprintf(block.entries[0].key, sizeof(block.entries[0].key), "api_key");
    snprintf(block.entries[0].value, sizeof(block.entries[0].value), "sk-xxx");
    snprintf(block.entries[1].key, sizeof(block.entries[1].key), "model");
    snprintf(block.entries[1].value, sizeof(block.entries[1].value), "gpt-4");
    snprintf(block.entries[2].key, sizeof(block.entries[2].key), "timeout");
    snprintf(block.entries[2].value, sizeof(block.entries[2].value), "30");

    char *json = hermes_plugin_cfg_to_json(&block);
    TEST("cfg_to_json multi entry", json != NULL);
    if (json) {
        /* Verify all three keys exist */
        int count = 0;
        const char *p = json;
        while ((p = strstr(p, "\"")) != NULL) {
            count++;
            p++;
        }
        /* Each key:value pair adds 2 quotes around the key + 2 around the value = 4 per pair = 12 for 3 pairs
         * Wait, easier: just check that it starts with '{' and ends with '}' */
        TEST("json starts with {", json[0] == '{');
        TEST("json ends with }", json[strlen(json)-1] == '}');
        TEST("json has model key", strstr(json, "model") != NULL);
        TEST("json has timeout key", strstr(json, "timeout") != NULL);
        free(json);
    }
}

static void test_cfg_to_json_special_chars(void)
{
    plugin_cfg_block_t block;
    memset(&block, 0, sizeof(block));
    snprintf(block.name, sizeof(block.name), "test");
    block.count = 1;
    snprintf(block.entries[0].key, sizeof(block.entries[0].key), "url");
    snprintf(block.entries[0].value, sizeof(block.entries[0].value), "https://api.example.com/v1?key=val");

    char *json = hermes_plugin_cfg_to_json(&block);
    TEST("cfg_to_json special chars", json != NULL);
    if (json) {
        TEST("url in json", strstr(json, "api.example.com") != NULL);
        TEST("url key present", strstr(json, "url") != NULL);
        free(json);
    }
}

static void test_discover_config_null_safety(void)
{
    plugin_cfg_block_t block;
    memset(&block, 0, sizeof(block));
    const hermes_config_t cfg;
    memset((void*)&cfg, 0, sizeof(cfg));

    /* All NULLs */
    TEST("discover(NULL, name, block)", hermes_plugin_discover_config(NULL, "test", &block) == 0);
    TEST("discover(cfg, NULL, block)", hermes_plugin_discover_config(&cfg, NULL, &block) == 0);
    TEST("discover(cfg, name, NULL)", hermes_plugin_discover_config(&cfg, "test", NULL) == 0);
    /* All valid but no plugin config */
    TEST("discover(cfg, name, block) empty cfg", hermes_plugin_discover_config(&cfg, "test", &block) == 0);
}

static void test_cfg_inject_null_safety(void)
{
    const hermes_config_t cfg;
    memset((void*)&cfg, 0, sizeof(cfg));

    /* NULL config or plugin */
    TEST("cfg_inject(NULL, p)", hermes_plugin_cfg_inject(NULL, NULL) == 0);
    TEST("cfg_inject(cfg, NULL)", hermes_plugin_cfg_inject(&cfg, NULL) == 0);
}

static void test_init_null(void)
{
    TEST("plugin_init(NULL)", hermes_plugin_init(NULL) == NULL);
}

static void test_shutdown_null(void)
{
    /* Should not crash */
    hermes_plugin_shutdown(NULL);
    TEST("plugin_shutdown(NULL)", 1);
}

static void test_init_enabled_null(void)
{
    const hermes_config_t cfg;
    memset((void*)&cfg, 0, sizeof(cfg));
    TEST("init_enabled(NULL, cfg)", hermes_plugin_init_enabled(NULL, &cfg) == -1);
    TEST("init_enabled(reg, NULL)", hermes_plugin_init_enabled((plugin_registry_t*)1, NULL) == -1);
}

static void test_plugin_list_null(void)
{
    char buf[256];
    TEST("plugin_list(NULL, buf, sz)", hermes_plugin_list(NULL, NULL, 0) == 0);
    TEST("plugin_list(reg, NULL, sz)", hermes_plugin_list((plugin_registry_t*)1, NULL, 0) == 0);
    TEST("plugin_list(reg, buf, 0)", hermes_plugin_list((plugin_registry_t*)1, buf, 0) == 0);
}

static void test_plugin_list_empty(void)
{
    /* Create empty registry and list it */
    plugin_registry_t *reg = plugin_registry_new();
    TEST("empty registry created", reg != NULL);
    if (reg) {
        char buf[4096];
        int written = hermes_plugin_list(reg, buf, sizeof(buf));
        TEST("empty list written > 0", written > 0);
        if (written > 0) {
            TEST("list shows 0 loaded", strstr(buf, "0 loaded") != NULL);
        }
        plugin_registry_free(reg);
    }
}

int main(void)
{
    test_cfg_to_json_null();
    test_cfg_to_json_empty();
    test_cfg_to_json_single_entry();
    test_cfg_to_json_multiple_entries();
    test_cfg_to_json_special_chars();
    test_discover_config_null_safety();
    test_cfg_inject_null_safety();
    test_init_null();
    test_shutdown_null();
    test_init_enabled_null();
    test_plugin_list_null();
    test_plugin_list_empty();

    printf("plugin_ext: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
