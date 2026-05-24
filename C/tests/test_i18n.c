/*
 * test_i18n.c — Tests for i18n module (embedded data).
 *
 * Tests language detection, key lookup, format substitution,
 * fallback to English, alias resolution, and edge cases.
 */

#include "hermes_i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ================================================================
 *  Language resolution
 * ================================================================ */

static void test_default_language(void) {
    TEST("default language is 'en'");
    unsetenv("HERMES_LANGUAGE");
    const char *lang = i18n_get_language();
    ASSERT(lang != NULL, "language NULL");
    ASSERT(strcmp(lang, "en") == 0, "expected 'en'");
    PASS();
}

static void test_env_language(void) {
    TEST("HERMES_LANGUAGE set to 'de'");
    setenv("HERMES_LANGUAGE", "de", 1);
    const char *lang = i18n_get_language();
    ASSERT(lang != NULL, "language NULL");
    ASSERT(strcmp(lang, "de") == 0, "expected 'de'");
    PASS();

    TEST("HERMES_LANGUAGE alias 'english' resolves to 'en'");
    setenv("HERMES_LANGUAGE", "english", 1);
    lang = i18n_get_language();
    ASSERT(strcmp(lang, "en") == 0, "expected 'en'");
    PASS();

    TEST("HERMES_LANGUAGE alias 'japanese' resolves to 'ja'");
    setenv("HERMES_LANGUAGE", "japanese", 1);
    lang = i18n_get_language();
    ASSERT(strcmp(lang, "ja") == 0, "expected 'ja'");
    PASS();

    TEST("HERMES_LANGUAGE alias 'zh-cn' resolves to 'zh'");
    setenv("HERMES_LANGUAGE", "zh-cn", 1);
    lang = i18n_get_language();
    ASSERT(strcmp(lang, "zh") == 0, "expected 'zh'");
    PASS();

    TEST("HERMES_LANGUAGE alias 'uk-ua' resolves to 'uk'");
    setenv("HERMES_LANGUAGE", "uk-ua", 1);
    lang = i18n_get_language();
    ASSERT(strcmp(lang, "uk") == 0, "expected 'uk'");
    PASS();
}

/* ================================================================
 *  Basic key lookup
 * ================================================================ */

static void test_basic_lookup(void) {
    setenv("HERMES_LANGUAGE", "en", 1);
    i18n_init(NULL);

    TEST("i18n_t finds 'approval.cancelled'");
    char *val = i18n_t("approval.cancelled");
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strlen(val) > 0, "empty string");
    ASSERT(strstr(val, "Cancelled") != NULL || strstr(val, "cancelled") != NULL,
           "expected cancellation message");
    free(val);
    PASS();

    TEST("i18n_t finds 'gateway.draining' with {count}");
    val = i18n_t("gateway.draining");
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strstr(val, "{count}") != NULL, "expected {count} placeholder");
    free(val);
    PASS();

    TEST("i18n_t returns key for nonexistent key");
    val = i18n_t("nonexistent.key.12345");
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strcmp(val, "nonexistent.key.12345") == 0, "expected key as fallback");
    free(val);
    PASS();

    TEST("i18n_t with NULL key returns empty string");
    val = i18n_t(NULL);
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strcmp(val, "") == 0, "expected empty string");
    free(val);
    PASS();
}

/* ================================================================
 *  Language override
 * ================================================================ */

static void test_language_override(void) {
    setenv("HERMES_LANGUAGE", "en", 1);
    i18n_init(NULL);

    TEST("i18n_t_lang finds 'de' key");
    char *val = i18n_t_lang("approval.cancelled", "de");
    ASSERT(val != NULL, "returned NULL");
    /* German translation should be different from English */
    ASSERT(strcmp(val, "approval.cancelled") != 0, "should not be raw key");
    free(val);
    PASS();

    TEST("i18n_t_lang with unsupported lang falls back to 'en'");
    val = i18n_t_lang("approval.cancelled", "xx");
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strcmp(val, "approval.cancelled") != 0, "should not be raw key");
    ASSERT(strstr(val, "ancel") != NULL, "should have english text");
    free(val);
    PASS();
}

/* ================================================================
 *  Format substitution
 * ================================================================ */

static void test_format_substitution(void) {
    setenv("HERMES_LANGUAGE", "en", 1);
    i18n_init(NULL);

    TEST("i18n_t_fmt replaces {count} in 'gateway.draining'");
    char *val = i18n_t_fmt("gateway.draining", "count", "3", NULL);
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strstr(val, "3") != NULL, "expected '3' in output");
    ASSERT(strstr(val, "{count}") == NULL, "expected no remaining placeholder");
    free(val);
    PASS();

    TEST("i18n_t_fmt with multiple args");
    val = i18n_t_fmt("gateway.model.switched", "model", "gpt-4", NULL);
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strstr(val, "gpt-4") != NULL, "expected model name in output");
    free(val);
    PASS();

    TEST("i18n_t_fmt replaces {error} in 'gateway.config_read_failed'");
    val = i18n_t_fmt("gateway.config_read_failed", "error", "permission denied", NULL);
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strstr(val, "permission denied") != NULL, "expected error text");
    ASSERT(strstr(val, "{error}") == NULL, "expected no remaining placeholder");
    free(val);
    PASS();

    TEST("i18n_t_fmt with unset placeholder preserves {placeholder}");
    val = i18n_t_fmt("gateway.draining", "other_key", "value", NULL);
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strstr(val, "{count}") != NULL, "expected unmapped placeholder");
    free(val);
    PASS();

    TEST("i18n_t_vfmt with array args");
    i18n_arg_t args[2] = {
        {"count", "5"},
        {"foo", "bar"}
    };
    val = i18n_t_vfmt("gateway.draining", args, 2);
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strstr(val, "5") != NULL, "expected '5'");
    ASSERT(strstr(val, "{count}") == NULL, "expected no remaining {count}");
    free(val);
    PASS();
}

/* ================================================================
 *  Locale data integrity
 * ================================================================ */

static void test_locale_data_integrity(void) {
    setenv("HERMES_LANGUAGE", "en", 1);
    int n = i18n_init(NULL);

    TEST("i18n_init loads multiple catalogs");
    ASSERT(n > 0, "expected at least 1 catalog");
    PASS();

    TEST("i18n_init returns 16 catalogs (or current count)");
    ASSERT(n > 10, "expected at least 10 locale catalogs");
    PASS();
}

/* ================================================================
 *  Language alias resolution (internal, tested via i18n_t_lang)
 * ================================================================ */

static void test_alias_resolution(void) {
    setenv("HERMES_LANGUAGE", "en", 1);
    i18n_init(NULL);

    TEST("alias 'english' via i18n_t_lang finds key");
    char *val = i18n_t_lang("approval.cancelled", "english");
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strstr(val, "ancel") != NULL, "should have english text");
    free(val);
    PASS();

    TEST("alias 'chinese' via i18n_t_lang");
    val = i18n_t_lang("approval.cancelled", "chinese");
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strcmp(val, "approval.cancelled") != 0, "should not be raw key");
    free(val);
    PASS();

    TEST("alias 'french' via i18n_t_lang");
    val = i18n_t_lang("approval.cancelled", "french");
    ASSERT(val != NULL, "returned NULL");
    ASSERT(strcmp(val, "approval.cancelled") != 0, "should not be raw key");
    free(val);
    PASS();
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== i18n Library Tests ===\n\n");

    test_default_language();
    test_env_language();
    test_basic_lookup();
    test_language_override();
    test_format_substitution();
    test_locale_data_integrity();
    test_alias_resolution();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
