/* test_plugin_image_gen.c — Tests for image generation plugin. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/plugins/plugin_image_gen.c"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) do { tests_run++; fprintf(stderr, "  TEST: %s ... ", name); } while (0)
#define PASS() do { tests_passed++; fprintf(stderr, "PASS\n"); } while (0)
#define FAIL(msg) do { tests_failed++; fprintf(stderr, "FAIL: %s\n", msg); } while (0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while (0)

static void reset_state(void) {
    memset(g_history, 0, sizeof(g_history));
    g_history_count = 0;
    g_history_wrap = 0;
    snprintf(g_provider, sizeof(g_provider), "openai");
    snprintf(g_default_size, sizeof(g_default_size), "%s", DEFAULT_SIZE);
}

static void test_generate_image(void) {
    TEST("generate_image returns result");
    reset_state();
    char *result = impl_image_gen("a cute cat", NULL);
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "url") != NULL, "should contain url");
    ASSERT(strstr(result, "cute") != NULL || strstr(result, "cat") != NULL,
           "should contain prompt info");
    ASSERT(g_history_count == 1, "history should increment");
    free(result);
    PASS();
}

static void test_generate_empty_prompt(void) {
    TEST("generate_image with empty prompt returns error");
    reset_state();
    char *result = impl_image_gen("", NULL);
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "error") != NULL, "should contain error");
    free(result);
    PASS();
}

static void test_generate_with_size(void) {
    TEST("generate_image with custom size");
    reset_state();
    char *result = impl_image_gen("landscape", "{\"size\":\"1792x1024\"}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "1792") != NULL, "should contain custom width");
    free(result);
    PASS();
}

static void test_history_tracking(void) {
    TEST("multiple generations are tracked");
    reset_state();
    impl_image_gen("first", NULL);
    impl_image_gen("second", NULL);
    ASSERT(g_history_count == 2, "should have 2 generations");
    ASSERT(strcmp(g_history[0].prompt, "first") == 0, "first prompt preserved");
    ASSERT(strcmp(g_history[1].prompt, "second") == 0, "second prompt preserved");
    PASS();
}

static void test_list_generations(void) {
    TEST("list_generations returns history");
    reset_state();
    impl_image_gen("test", NULL);
    char *result = tool_list_generations("{}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "count") != NULL, "should contain count");
    ASSERT(strstr(result, "test") != NULL, "should contain prompt");
    free(result);
    PASS();
}

static void test_configure(void) {
    TEST("configure changes provider");
    reset_state();
    char *result = tool_configure("{\"provider\":\"xai\",\"size\":\"1024x1792\"}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strcmp(g_provider, "xai") == 0, "provider should be xai");
    ASSERT(strcmp(g_default_size, "1024x1792") == 0, "size should be updated");
    free(result);
    PASS();
}

static void test_image_gen_interface(void) {
    TEST("image_gen interface fn pointer works");
    /* Need to init first */
    memset(&g_interface, 0, sizeof(g_interface));
    plugin_init();
    ASSERT(g_interface.image_gen != NULL, "image_gen ptr should be set");
    char *result = g_interface.image_gen("via iface", NULL);
    ASSERT(result != NULL, "should return non-NULL");
    free(result);
    PASS();
}

static void test_plugin_init(void) {
    TEST("plugin_init sets PLUGIN_IMAGE_GEN");
    memset(&g_interface, 0, sizeof(g_interface));
    int ret = plugin_init();
    ASSERT(ret == 0, "init should return 0");
    ASSERT(g_interface.type == PLUGIN_IMAGE_GEN, "type should be PLUGIN_IMAGE_GEN");
    PASS();
}

static void test_ring_buffer_wrap(void) {
    TEST("history wraps at MAX_GENERATIONS");
    reset_state();
    for (int i = 0; i < MAX_GENERATIONS + 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "gen_%d", i);
        impl_image_gen(buf, NULL);
    }
    ASSERT(g_history_count == MAX_GENERATIONS + 5, "count should exceed max");
    /* Check oldest entries were overwritten */
    ASSERT(strcmp(g_history[5].prompt, "gen_5") == 0, "index 5 should be gen_5");
    ASSERT(strcmp(g_history[9].prompt, "gen_9") == 0, "index 9 should be gen_9");
    PASS();
}

int main(void) {
    fprintf(stderr, "Image Gen Plugin Tests\n");
    fprintf(stderr, "======================\n");

    snprintf(g_state_path, sizeof(g_state_path), "/tmp/test_image_gen_state.json");

    test_generate_image();
    test_generate_empty_prompt();
    test_generate_with_size();
    test_history_tracking();
    test_list_generations();
    test_configure();
    test_image_gen_interface();
    test_plugin_init();
    test_ring_buffer_wrap();

    fprintf(stderr, "\nResults: %d passed, %d failed, %d total\n",
            tests_passed, tests_failed, tests_run);

    unlink("/tmp/test_image_gen_state.json");
    if (tests_failed > 0) return 1;
    return 0;
}
