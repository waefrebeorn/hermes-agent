/*
 * test_usage_pricing.c — Token cost estimation tests.
 *
 * Tests the usage_pricing module: known model lookup, cost estimation,
 * fallback to family rates, formatting helpers, and edge cases.
 *
 * Build (from C/):
 *   gcc -O2 -g -Wall -Werror -I include \
 *       tests/test_usage_pricing.c src/agent/usage_pricing.c src/hermes_tokenizer.c \
 *       -o /tmp/hermes_test_usage_pricing -lm
 *
 * Run:
 *   /tmp/hermes_test_usage_pricing
 */

#include "usage_pricing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

#define ASSERT_STR_EQ(label, actual, expected) do { \
    const char *_a = (actual); \
    const char *_e = (expected); \
    TEST(label, strcmp(_a, _e) == 0); \
    if (strcmp(_a, _e) != 0) fprintf(stderr, "    got: \"%s\" expected: \"%s\"\n", _a, _e); \
} while(0)

/* Helper: check that a cost label contains a substring */
static int label_contains(const usage_cost_t *c, const char *sub) {
    return strstr(c->label, sub) != NULL;
}

int main(void)
{
    /* ================================================================== */
    /* 1. usage_pricing_known() — known models                             */
    /* ================================================================== */

    /* Known per-model entries */
    TEST("known: anthropic claude-sonnet-4",
         usage_pricing_known("anthropic/claude-sonnet-4-6"));
    TEST("known: anthropic claude-opus-4",
         usage_pricing_known("anthropic/claude-opus-4-latest"));
    TEST("known: anthropic claude-haiku-4",
         usage_pricing_known("anthropic/claude-haiku-4-someversion"));
    TEST("known: anthropic claude-3.5",
         usage_pricing_known("anthropic/claude-3.5-sonnet"));
    TEST("known: anthropic claude-3",
         usage_pricing_known("anthropic/claude-3-opus"));
    TEST("known: openai gpt-4o",
         usage_pricing_known("openai/gpt-4o-2026-05-14"));
    TEST("known: openai gpt-4o-mini",
         usage_pricing_known("openai/gpt-4o-mini"));
    TEST("known: openai gpt-4",
         usage_pricing_known("openai/gpt-4"));
    TEST("known: openai gpt-3.5-turbo",
         usage_pricing_known("openai/gpt-3.5-turbo"));
    TEST("known: deepseek deepseek-chat",
         usage_pricing_known("deepseek/deepseek-chat"));
    TEST("known: deepseek deepseek-reasoner",
         usage_pricing_known("deepseek/deepseek-reasoner"));
    TEST("known: deepseek deepseek-r1",
         usage_pricing_known("deepseek/deepseek-r1"));
    TEST("known: grok-3",
         usage_pricing_known("xai/grok-3"));
    TEST("known: grok-2",
         usage_pricing_known("xai/grok-2"));
    TEST("known: gemini-2.0",
         usage_pricing_known("google/gemini-2.0-flash"));
    TEST("known: gemini-1.5-pro",
         usage_pricing_known("google/gemini-1.5-pro"));
    TEST("known: azure gpt-4o",
         usage_pricing_known("azure/gpt-4o"));
    TEST("known: bedrock claude",
         usage_pricing_known("bedrock/claude-sonnet-4"));

    /* Provider catch-all matches */
    TEST("known: nouprovider catch-all",
         usage_pricing_known("openai/gpt-4-unknown-version"));

    /* ================================================================== */
    /* 2. usage_pricing_known() — unknown / NULL                           */
    /* ================================================================== */

    TEST("known: unknown model",
         !usage_pricing_known("totally-fake-model-that-doesnt-exist"));
    TEST("known: empty model",
         !usage_pricing_known(""));
    TEST("known: NULL model",
         !usage_pricing_known(NULL));

    /* ================================================================== */
    /* 3. usage_pricing_known() — family fallback                          */
    /* ================================================================== */

    /* Models like "gpt-4-fake" should fall through to family_rates */
    TEST("known: gpt-4 fallout family",
         usage_pricing_known("openai/gpt-4-fake"));
    TEST("known: claude fallout family",
         usage_pricing_known("anthropic/claude-fake-model"));

    /* ================================================================== */
    /* 4. usage_pricing_estimate() — basic cost computation                */
    /* ================================================================== */

    usage_counts_t usage = {1000, 500, 100, 0, 0, 1};

    /* cost = 1000/1M * rate */
    usage_cost_t cost = usage_pricing_estimate("openai/gpt-4o", &usage);
    TEST("estimate: has_pricing", cost.has_pricing);
    TEST("estimate: status estimated", cost.status == COST_STATUS_ESTIMATED);
    TEST("estimate: source", strcmp(cost.source, "docs_snapshot") == 0);
    /* gpt-4o: $10/$30 per 1M. 1000 input = $0.01, 500 output = $0.015, cache = 100/1M*5 = $0.0005 */
    TEST("estimate: input cost ~0.01",
         fabs(cost.input_cost - 0.01) < 0.0001);
    TEST("estimate: output cost ~0.015",
         fabs(cost.output_cost - 0.015) < 0.0001);
    TEST("estimate: cache read cost ~0.0005",
         fabs(cost.cache_read_cost - 0.0005) < 0.0001);
    TEST("estimate: total ~0.0255",
         fabs(cost.total_cost - 0.0255) < 0.0001);
    TEST("estimate: label contains input rate",
         label_contains(&cost, "$10.00"));
    TEST("estimate: label contains output rate",
         label_contains(&cost, "$30.00"));

    /* ================================================================== */
    /* 5. Per-model vs catch-all — deepseek-chat should match exact, not   */
    /*    catch-all $0.27/$1.10                                            */
    /* ================================================================== */

    usage_counts_t deepseek_usage = {1000000, 500000, 0, 0, 0, 1};
    cost = usage_pricing_estimate("deepseek/deepseek-chat", &deepseek_usage);
    TEST("deepseek-chat: has_pricing", cost.has_pricing);
    /* deepseek-chat: $0.27/$1.10 per 1M. 1M input = $0.27, 500K output = $0.55 */
    TEST("deepseek-chat: input ~0.27",
         fabs(cost.input_cost - 0.27) < 0.001);
    TEST("deepseek-chat: output ~0.55",
         fabs(cost.output_cost - 0.55) < 0.001);

    /* ================================================================== */
    /* 6. Anthropic with cache write rates                                */
    /* ================================================================== */

    usage_counts_t anthropic_usage = {100000, 50000, 10000, 5000, 0, 1};
    cost = usage_pricing_estimate("anthropic/claude-sonnet-4-20260514", &anthropic_usage);
    TEST("anthropic: has_pricing", cost.has_pricing);
    /* claude-sonnet-4: $3/$15 per 1M, cache read=$0.30, write=$3.75 */
    /* 100K/1M*3 = 0.30, 50K/1M*15 = 0.75, 10K/1M*0.30 = 0.003, 5K/1M*3.75 = 0.01875 */
    TEST("anthropic: input ~0.30",
         fabs(cost.input_cost - 0.30) < 0.001);
    TEST("anthropic: output ~0.75",
         fabs(cost.output_cost - 0.75) < 0.001);
    TEST("anthropic: cache write ~0.01875",
         fabs(cost.cache_write_cost - 0.01875) < 0.0001);

    /* ================================================================== */
    /* 7. Edge cases: NULL model, NULL usage, zero usage                   */
    /* ================================================================== */

    cost = usage_pricing_estimate(NULL, &usage);
    /* NULL model — should return COST_STATUS_UNKNOWN */
    TEST("null model: status unknown", cost.status == COST_STATUS_UNKNOWN);
    TEST("null model: no pricing", !cost.has_pricing);

    cost = usage_pricing_estimate("openai/gpt-4o", NULL);
    TEST("null usage: status unknown", cost.status == COST_STATUS_UNKNOWN);
    TEST("null usage: no pricing", !cost.has_pricing);

    usage_counts_t zero_usage = {0, 0, 0, 0, 0, 0};
    cost = usage_pricing_estimate("openai/gpt-4o", &zero_usage);
    TEST("zero usage: has_pricing", cost.has_pricing);
    TEST("zero usage: cost zero", cost.total_cost == 0.0);

    /* ================================================================== */
    /* 8. Unknown model — should return unknown status                     */
    /* ================================================================== */

    usage_counts_t some_usage = {1000, 500, 0, 0, 0, 1};
    cost = usage_pricing_estimate("fake/model-name", &some_usage);
    TEST("unknown model: status unknown", cost.status == COST_STATUS_UNKNOWN);
    TEST("unknown model: no pricing", !cost.has_pricing);

    /* ================================================================== */
    /* 9. usage_pricing_format_cost()                                      */
    /* ================================================================== */

    ASSERT_STR_EQ("cost 0",                usage_pricing_format_cost(0.0),         "$0.00");
    ASSERT_STR_EQ("cost 0.00005",          usage_pricing_format_cost(0.00005),    "$0.00");
    ASSERT_STR_EQ("cost 0.0001",           usage_pricing_format_cost(0.0001),     "$0.0001");
    ASSERT_STR_EQ("cost 0.001",            usage_pricing_format_cost(0.001),      "$0.0010");
    ASSERT_STR_EQ("cost 0.005",            usage_pricing_format_cost(0.005),      "$0.0050");
    ASSERT_STR_EQ("cost 0.12",             usage_pricing_format_cost(0.12),       "$0.12");
    ASSERT_STR_EQ("cost 1.50",             usage_pricing_format_cost(1.50),       "$1.50");
    ASSERT_STR_EQ("cost 12.3456",          usage_pricing_format_cost(12.3456),    "$12.35");
    ASSERT_STR_EQ("cost 100.0",            usage_pricing_format_cost(100.0),      "$100");
    ASSERT_STR_EQ("cost 1234.56",          usage_pricing_format_cost(1234.56),    "$1235");

    /* ================================================================== */
    /* 10. usage_pricing_format_duration()                                  */
    /* ================================================================== */

    ASSERT_STR_EQ("duration 0s",           usage_pricing_format_duration(0),      "0s");
    ASSERT_STR_EQ("duration 5s",           usage_pricing_format_duration(5),      "5s");
    ASSERT_STR_EQ("duration 59s",          usage_pricing_format_duration(59),     "59s");
    ASSERT_STR_EQ("duration 60s = 1m",     usage_pricing_format_duration(60),     "1m");
    ASSERT_STR_EQ("duration 120s = 2m",    usage_pricing_format_duration(120),    "2m");
    ASSERT_STR_EQ("duration 3599s = 59m",  usage_pricing_format_duration(3599),   "59m");
    ASSERT_STR_EQ("duration 3600s = 1h",   usage_pricing_format_duration(3600),   "1.0h");
    ASSERT_STR_EQ("duration 5400s = 1.5h", usage_pricing_format_duration(5400),   "1.5h");
    ASSERT_STR_EQ("duration 86400s = 1d",  usage_pricing_format_duration(86400),  "1.0d");
    ASSERT_STR_EQ("duration 172800s = 2d", usage_pricing_format_duration(172800), "2.0d");
    TEST("negative duration clamped",
         strcmp(usage_pricing_format_duration(-1), "0s") == 0);

    /* ================================================================== */
    /* 11. Provider prefix parsing                                          */
    /* ================================================================== */

    /* Model without provider prefix — should still find gpt-4o via provider "" catch-all */
    usage_counts_t no_prefix_usage = {1000, 500, 0, 0, 0, 1};
    cost = usage_pricing_estimate("gpt-4o", &no_prefix_usage);
    /* "gpt-4o" has no "/" so provider="" and model_name="gpt-4o" */
    /* provider="" won't match any table entries (all have explicit provider) */
    /* It will fall through to family_rates → TOKEN_FAMILY_GPT4 */
    TEST("no-prefix: falls to family_rates",
         strcmp(cost.source, "family_rates") == 0);

    /* ================================================================== */
    /* 12. Family rates fallback — model known to tokenizer but not table  */
    /* ================================================================== */

    /* "gpt-4-turbo-2025-04-16" is TOKEN_FAMILY_GPT4 in tokenizer */
    cost = usage_pricing_estimate("gpt-4-turbo-2025-04-16", &no_prefix_usage);
    TEST("family rates: has_pricing", cost.has_pricing);
    TEST("family rates: source",
         strcmp(cost.source, "family_rates") == 0);

    /* ================================================================== */
    /* Summary                                                             */
    /* ================================================================== */

    printf("\n");
    printf("  Results: %d passed, %d failed, 0 skipped\n", passed, tests - passed);

    return (passed == tests) ? 0 : 1;
}
