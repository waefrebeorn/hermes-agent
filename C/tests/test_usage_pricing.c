/* test_usage_pricing.c — Tests for usage_pricing module.
 * Tests: estimate, known, format_cost, format_duration for known models.
 * Compile with: gcc ... usage_pricing.c -Wl,--unresolved-symbols=ignore-all
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "usage_pricing.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

static int double_eq(double a, double b) {
    return fabs(a - b) < 0.0001;
}

/* ================================================================
 *  usage_pricing_known
 * ================================================================ */
static void test_known_models(void) {
    TEST("gpt-4o is known",         usage_pricing_known("gpt-4o"));
    TEST("gpt-4o-mini is known",    usage_pricing_known("gpt-4o-mini"));
    TEST("claude-sonnet-4 is known", usage_pricing_known("claude-sonnet-4"));
    TEST("deepseek-chat is known",  usage_pricing_known("deepseek-chat"));
    TEST("gpt-3.5-turbo is known",  usage_pricing_known("gpt-3.5-turbo"));
    TEST("gemini-pro is known",     usage_pricing_known("gemini-pro"));
    TEST("unknown-model is NOT known", !usage_pricing_known("unknown-model-xyz"));
}

/* ================================================================
 *  usage_pricing_format_cost
 * ================================================================ */
static void test_format_cost(void) {
    const char *r;
    r = usage_pricing_format_cost(0.0);
    TEST("zero formats as $0.00", strcmp(r, "$0.00") == 0);

    r = usage_pricing_format_cost(0.1234);
    TEST("$0.12 formats correctly", strcmp(r, "$0.12") == 0);

    r = usage_pricing_format_cost(1.50);
    TEST("$1.50 formats correctly", strcmp(r, "$1.50") == 0);

    r = usage_pricing_format_cost(12.34);
    TEST("$12.34 formats correctly", strcmp(r, "$12.34") == 0);

    r = usage_pricing_format_cost(123.45);
    TEST("$123 formats without decimals for large", strcmp(r, "$123") == 0);

    r = usage_pricing_format_cost(0.004);
    TEST("$0.0040 formats with 4 decimals", strcmp(r, "$0.0040") == 0);
}

/* ================================================================
 *  usage_pricing_format_duration
 * ================================================================ */
static void test_format_duration(void) {
    const char *r;

    r = usage_pricing_format_duration(0.0);
    TEST("0s formats as 0s", strcmp(r, "0s") == 0);

    r = usage_pricing_format_duration(15.0);
    TEST("15s formats as 15s", strcmp(r, "15s") == 0);

    r = usage_pricing_format_duration(120.0);
    TEST("120s (2m) formats as 2m", strcmp(r, "2m") == 0);

    r = usage_pricing_format_duration(3661.0);
    TEST("3661s (1h) formats as 1.0h", strcmp(r, "1.0h") >= 0);
}

/* ================================================================
 *  usage_pricing_estimate — basic sanity
 * ================================================================ */
static void test_estimate_known_model(void) {
    usage_counts_t usage = {0};
    usage.input_tokens = 1000;
    usage.output_tokens = 500;

    usage_cost_t cost = usage_pricing_estimate("gpt-4o", &usage);
    TEST("gpt-4o estimate has pricing info", cost.has_pricing);
    TEST("gpt-4o input cost > 0", cost.input_cost > 0);
    TEST("gpt-4o output cost > 0", cost.output_cost > 0);
    TEST("gpt-4o total cost > 0", cost.total_cost > 0);
    TEST("gpt-4o total = input + output",
         double_eq(cost.total_cost, cost.input_cost + cost.output_cost));
}

static void test_estimate_unknown_model(void) {
    usage_counts_t usage = {0};
    usage.input_tokens = 1000;
    usage.output_tokens = 500;

    usage_cost_t cost = usage_pricing_estimate("nonexistent-model-v99", &usage);
    TEST("unknown model has_pricing is false", !cost.has_pricing);
    TEST("unknown model total cost is 0", double_eq(cost.total_cost, 0.0));
}

static void test_estimate_zero_usage(void) {
    usage_counts_t usage = {0};
    usage_cost_t cost = usage_pricing_estimate("gpt-4o", &usage);
    TEST("zero usage has pricing", cost.has_pricing);
    TEST("zero usage total cost is 0", double_eq(cost.total_cost, 0.0));
}

static void test_estimate_cache_usage(void) {
    usage_counts_t usage = {0};
    usage.input_tokens = 1000;
    usage.cache_read_tokens = 5000;

    usage_cost_t cost = usage_pricing_estimate("gpt-4o", &usage);
    TEST("cache usage total = input + cache",
         double_eq(cost.total_cost, cost.input_cost + cost.cache_read_cost + cost.cache_write_cost));
}

static void test_estimate_provider_prefixed(void) {
    usage_counts_t usage = {0};
    usage.input_tokens = 500;
    usage.output_tokens = 200;

    usage_cost_t cost = usage_pricing_estimate("openai/gpt-4o", &usage);
    TEST("provider-prefixed model has pricing", cost.has_pricing);
    TEST("provider-prefixed total > 0", cost.total_cost > 0);
}

static void test_estimate_deepseek(void) {
    usage_counts_t usage = {0};
    usage.input_tokens = 1000;
    usage.output_tokens = 500;

    usage_cost_t cost = usage_pricing_estimate("deepseek-chat", &usage);
    TEST("deepseek-chat has pricing", cost.has_pricing);
    TEST("deepseek total > 0", cost.total_cost > 0);
}

static void test_estimate_claude(void) {
    usage_counts_t usage = {0};
    usage.input_tokens = 1000;
    usage.output_tokens = 500;

    usage_cost_t cost = usage_pricing_estimate("claude-sonnet-4", &usage);
    TEST("claude-sonnet-4 has pricing", cost.has_pricing);
    TEST("claude total > 0", cost.total_cost > 0);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Usage Pricing Tests ===\n\n");

    test_known_models();
    printf("\n--- Cost Formatting ---\n");
    test_format_cost();

    printf("\n--- Duration Formatting ---\n");
    test_format_duration();

    printf("\n--- Estimate Sanity ---\n");
    test_estimate_known_model();
    test_estimate_unknown_model();
    test_estimate_zero_usage();
    test_estimate_cache_usage();
    test_estimate_provider_prefixed();
    test_estimate_deepseek();
    test_estimate_claude();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
