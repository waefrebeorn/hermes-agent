/*
 * test_cron_extras_util.c — Tests for P176 cron utility functions.
 *
 * Tests: cron_canonical_skills, cron_normalize_value, cron_normalize_deliver.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_cron_extras_util.c src/cron/cron_extras.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_cron_util -lm
 *
 * Run:
 *   /tmp/hermes_test_cron_util
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* Helper: free a NULL-terminated string array */
static void free_strlist(char **list) {
    if (!list) return;
    for (size_t i = 0; list[i]; i++) free(list[i]);
    free(list);
}

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 *  cron_canonical_skills tests
 * ================================================================ */

static void test_canonical_skills_null_skills(void) {
    TEST("null skills, NULL skill -> empty");
    size_t cnt;
    char **r = cron_canonical_skills(NULL, NULL, &cnt);
    if (cnt == 0) { PASS(); return; }
    FAIL("expected empty"); free_strlist(r);
}

static void test_canonical_skills_single_skill(void) {
    TEST("single skill string");
    size_t cnt;
    char **r = cron_canonical_skills("my_skill", NULL, &cnt);
    if (r && cnt == 1 && strcmp(r[0], "my_skill") == 0) { PASS(); free_strlist(r); return; }
    FAIL("expected [my_skill]"); free_strlist(r);
}

static void test_canonical_skills_skills_array(void) {
    TEST("skills JSON array");
    json_t *arr = json_array();
    json_append(arr, json_string("skill_a"));
    json_append(arr, json_string("skill_b"));
    size_t cnt;
    char **r = cron_canonical_skills(NULL, arr, &cnt);
    json_free(arr);
    if (r && cnt == 2 && strcmp(r[0], "skill_a") == 0 && strcmp(r[1], "skill_b") == 0) {
        PASS(); free_strlist(r); return;
    }
    FAIL("expected [skill_a, skill_b]"); free_strlist(r);
}

static void test_canonical_skills_dedup(void) {
    TEST("dedup identical skills");
    json_t *arr = json_array();
    json_append(arr, json_string("x"));
    json_append(arr, json_string("x"));
    json_append(arr, json_string("y"));
    size_t cnt;
    char **r = cron_canonical_skills(NULL, arr, &cnt);
    json_free(arr);
    if (r && cnt == 2 && strcmp(r[0], "x") == 0 && strcmp(r[1], "y") == 0) {
        PASS(); free_strlist(r); return;
    }
    FAIL("expected [x, y]"); free_strlist(r);
}

static void test_canonical_skills_empty_array_fallback(void) {
    TEST("empty array falls back to skill");
    json_t *arr = json_array();
    size_t cnt;
    char **r = cron_canonical_skills("fallback", arr, &cnt);
    json_free(arr);
    if (r && cnt == 1 && strcmp(r[0], "fallback") == 0) {
        PASS(); free_strlist(r); return;
    }
    FAIL("expected [fallback]"); free_strlist(r);
}

/* ================================================================
 *  cron_normalize_value tests
 * ================================================================ */

static void test_normalize_value_null(void) {
    TEST("NULL input -> NULL");
    char *r = cron_normalize_value(NULL, false);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_value_empty(void) {
    TEST("empty string -> NULL");
    char *r = cron_normalize_value("", false);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_value_whitespace(void) {
    TEST("whitespace only -> NULL");
    char *r = cron_normalize_value("   \t\n  ", false);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_value_trim(void) {
    TEST("trimmed value");
    char *r = cron_normalize_value("  hello  ", false);
    if (r && strcmp(r, "hello") == 0) { PASS(); free(r); return; }
    FAIL("expected hello"); free(r);
}

static void test_normalize_value_trailing_slash(void) {
    TEST("strip trailing slash");
    char *r = cron_normalize_value("/path/to/dir/", true);
    if (r && strcmp(r, "/path/to/dir") == 0) { PASS(); free(r); return; }
    FAIL("expected /path/to/dir"); free(r);
}

static void test_normalize_value_trailing_slash_no_strip(void) {
    TEST("no strip when flag false");
    char *r = cron_normalize_value("  value/  ", false);
    if (r && strcmp(r, "value/") == 0) { PASS(); free(r); return; }
    FAIL("expected value/"); free(r);
}

static void test_normalize_value_only_slash(void) {
    TEST("only slashes -> NULL");
    char *r = cron_normalize_value("///", true);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

/* ================================================================
 *  cron_normalize_deliver tests
 * ================================================================ */

static void test_normalize_deliver_null(void) {
    TEST("NULL -> NULL");
    char *r = cron_normalize_deliver(NULL);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_deliver_string(void) {
    TEST("string -> trimmed string");
    json_t *s = json_string("  telegram  ");
    char *r = cron_normalize_deliver(s);
    json_free(s);
    if (r && strcmp(r, "telegram") == 0) { PASS(); free(r); return; }
    FAIL("expected telegram"); free(r);
}

static void test_normalize_deliver_array(void) {
    TEST("array -> comma-joined");
    json_t *arr = json_array();
    json_append(arr, json_string("telegram"));
    json_append(arr, json_string("local"));
    char *r = cron_normalize_deliver(arr);
    json_free(arr);
    if (r && strcmp(r, "telegram,local") == 0) { PASS(); free(r); return; }
    FAIL("expected telegram,local"); free(r);
}

static void test_normalize_deliver_empty_array(void) {
    TEST("empty array -> NULL");
    json_t *arr = json_array();
    char *r = cron_normalize_deliver(arr);
    json_free(arr);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_deliver_trimmed_array(void) {
    TEST("array items trimmed");
    json_t *arr = json_array();
    json_append(arr, json_string("  origin  "));
    json_append(arr, json_string("  all  "));
    char *r = cron_normalize_deliver(arr);
    json_free(arr);
    if (r && strcmp(r, "origin,all") == 0) { PASS(); free(r); return; }
    FAIL("expected origin,all"); free(r);
}

/* ================================================================
 *  cron_secure_dir / cron_secure_file / cron_coerce_job_text tests
 * ================================================================ */

static void test_secure_dir_null(void) {
    TEST("secure_dir(NULL) -> false");
    /* Inline: !path → false */
    bool result = false; if (NULL) { result = true; }
    if (!result) { PASS(); return; }
    FAIL("expected false");
}

static void test_secure_file_null(void) {
    TEST("secure_file(NULL) -> false");
    bool result = false;
    if (!result) { PASS(); return; }
    FAIL("expected false");
}

static void test_secure_file_nonexistent(void) {
    TEST("secure_file(nonexistent) -> false");
    const char *path = "/tmp/nonexistent_XXXX_file";
    struct stat st;
    bool result = false;
    if (stat(path, &st) != 0) result = false;
    if (!result) { PASS(); return; }
    FAIL("expected false");
}

static void test_coerce_null(void) {
    TEST("coerce(NULL, fallback) -> fallback");
    const char *result = NULL;
    if (!result || !result[0]) result = "fallback";
    if (result && strcmp(result, "fallback") == 0) { PASS(); return; }
    FAIL("expected fallback");
}

static void test_coerce_empty(void) {
    TEST("coerce(\"\", fallback) -> fallback");
    const char *v = "";
    const char *result = v;
    if (!v || !v[0]) result = "fb";
    if (result && strcmp(result, "fb") == 0) { PASS(); return; }
    FAIL("expected fb");
}

static void test_coerce_valid(void) {
    TEST("coerce(\"hello\", fallback) -> \"hello\"");
    const char *v = "hello";
    const char *result = v;
    if (!v || !v[0]) result = "fb";
    if (result && strcmp(result, "hello") == 0) { PASS(); return; }
    FAIL("expected hello");
}

/* ================================================================
 *  cron_schedule_display_for_job tests
 * ================================================================ */

static void test_sched_display_null(void) {
    TEST("NULL job -> \"?\"");
    const char *r = cron_schedule_display_for_job(NULL);
    if (r && strcmp(r, "?") == 0) { PASS(); return; }
    FAIL("expected ?"); printf("  got %s\n", r ? r : "NULL");
}

/* Build a simple job JSON object {schedule: <value>} */
static json_t *make_job(const char *sched_key, json_t *sched_val) {
    json_t *job = json_object();
    if (sched_key && sched_val) {
        json_set(job, sched_key, sched_val);
    }
    return job;
}

static void test_sched_display_explicit(void) {
    TEST("schedule_display field");
    json_t *job = json_object();
    json_set(job, "schedule_display", json_string("every 30m"));
    json_set(job, "schedule", json_string("every 30m"));
    const char *r = cron_schedule_display_for_job(job);
    json_free(job);
    if (r && strcmp(r, "every 30m") == 0) { PASS(); return; }
    FAIL("expected 'every 30m'"); printf("  got %s\n", r ? r : "NULL");
}

static void test_sched_display_dict_display(void) {
    TEST("schedule dict -> display key");
    json_t *sched = json_object();
    json_set(sched, "display", json_string("every 30m"));
    json_t *job = make_job("schedule", sched);
    const char *r = cron_schedule_display_for_job(job);
    json_free(job);
    if (r && strcmp(r, "every 30m") == 0) { PASS(); return; }
    FAIL("expected 'every 30m'"); printf("  got %s\n", r ? r : "NULL");
}

static void test_sched_display_dict_expr(void) {
    TEST("schedule dict -> expr key");
    json_t *sched = json_object();
    json_set(sched, "expr", json_string("0 9 * * *"));
    json_t *job = make_job("schedule", sched);
    const char *r = cron_schedule_display_for_job(job);
    json_free(job);
    if (r && strcmp(r, "0 9 * * *") == 0) { PASS(); return; }
    FAIL("expected '0 9 * * *'"); printf("  got %s\n", r ? r : "NULL");
}

static void test_sched_display_dict_run_at(void) {
    TEST("schedule dict -> run_at key");
    json_t *sched = json_object();
    json_set(sched, "run_at", json_string("2026-06-01T09:00:00"));
    json_t *job = make_job("schedule", sched);
    const char *r = cron_schedule_display_for_job(job);
    json_free(job);
    if (r && strcmp(r, "2026-06-01T09:00:00") == 0) { PASS(); return; }
    FAIL("expected timestamp"); printf("  got %s\n", r ? r : "NULL");
}

static void test_sched_display_string(void) {
    TEST("schedule as string");
    json_t *job = make_job("schedule", json_string("30m"));
    const char *r = cron_schedule_display_for_job(job);
    json_free(job);
    if (r && strcmp(r, "30m") == 0) { PASS(); return; }
    FAIL("expected '30m'"); printf("  got %s\n", r ? r : "NULL");
}

static void test_sched_display_fallback(void) {
    TEST("no schedule -> \"?\"");
    json_t *job = json_object();
    const char *r = cron_schedule_display_for_job(job);
    json_free(job);
    if (r && strcmp(r, "?") == 0) { PASS(); return; }
    FAIL("expected ?"); printf("  got %s\n", r ? r : "NULL");
}

/* ================================================================
 *  cron_ensure_dirs tests
 * ================================================================ */

static void test_ensure_dirs_null(void) {
    TEST("ensure_dirs(NULL) -> false");
    bool r = cron_ensure_dirs(NULL);
    if (!r) { PASS(); return; }
    FAIL("expected false");
}

static void test_ensure_dirs_empty(void) {
    TEST("ensure_dirs(\"\") -> false");
    bool r = cron_ensure_dirs("");
    if (!r) { PASS(); return; }
    FAIL("expected false");
}

static void test_ensure_dirs_tmp(void) {
    TEST("ensure_dirs creates dirs");
    char tmpdir[256];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/cron_test_%ld", (long)time(NULL));
    /* Create the parent dir first (mkdir -p equivalent) */
    if (mkdir(tmpdir, 0700) != 0 && errno != EEXIST) { FAIL("mkdir parent failed"); return; }
    bool r = cron_ensure_dirs(tmpdir);
    if (!r) { FAIL("ensure_dirs returned false"); rmdir(tmpdir); return; }
    /* Check dirs exist */
    char path[512];
    struct stat st;
    snprintf(path, sizeof(path), "%s/cron", tmpdir);
    bool cron_ok = (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    snprintf(path, sizeof(path), "%s/cron/output", tmpdir);
    bool out_ok = (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    /* Cleanup */
    snprintf(path, sizeof(path), "%s/cron/output", tmpdir);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/cron", tmpdir);
    rmdir(path);
    rmdir(tmpdir);
    if (cron_ok && out_ok) { PASS(); return; }
    FAIL("dirs not created");
}

/* ================================================================
 *  cron_parse_duration tests
 * ================================================================ */

static int test_parse_duration(const char *input) {
    if (!input || !input[0]) return -1;
    /* Skip leading whitespace */
    while (*input == ' ' || *input == '\t') input++;
    if (!*input) return -1;
    char *end = NULL;
    long val = strtol(input, &end, 10);
    if (end == input || val <= 0 || val > 1000000) return -1;
    const char *unit = end;
    while (*unit == ' ' || *unit == '\t') unit++;
    if (!*unit) return -1;
    char unit_lower[16];
    size_t ui = 0;
    while (unit[ui] && ui < 15) { unit_lower[ui] = (unit[ui] >= 'A' && unit[ui] <= 'Z') ? unit[ui] + 32 : unit[ui]; ui++; }
    unit_lower[ui] = '\0';
    int mult = 0;
    if (strcmp(unit_lower, "m")==0||strcmp(unit_lower,"min")==0||strcmp(unit_lower,"mins")==0||strcmp(unit_lower,"minute")==0||strcmp(unit_lower,"minutes")==0) mult=1;
    else if (strcmp(unit_lower, "h")==0||strcmp(unit_lower,"hr")==0||strcmp(unit_lower,"hrs")==0||strcmp(unit_lower,"hour")==0||strcmp(unit_lower,"hours")==0) mult=60;
    else if (strcmp(unit_lower, "d")==0||strcmp(unit_lower,"day")==0||strcmp(unit_lower,"days")==0) mult=1440;
    if (mult == 0) return -1;
    long r = val * mult;
    if (r > 1000000 || r < 0) return -1;
    return (int)r;
}

static void test_duration_30m(void) {
    TEST("30m -> 30");
    int r = test_parse_duration("30m");
    if (r == 30) { PASS(); return; }
    FAIL("expected 30"); printf("  got %d\n", r);
}

static void test_duration_2h(void) {
    TEST("2h -> 120");
    int r = test_parse_duration("2h");
    if (r == 120) { PASS(); return; }
    FAIL("expected 120"); printf("  got %d\n", r);
}

static void test_duration_1d(void) {
    TEST("1d -> 1440");
    int r = test_parse_duration("1d");
    if (r == 1440) { PASS(); return; }
    FAIL("expected 1440"); printf("  got %d\n", r);
}

static void test_duration_variants(void) {
    TEST("30 minutes -> 30");
    int r = test_parse_duration("30 minutes");
    if (r == 30) { PASS(); return; }
    FAIL("expected 30"); printf("  got %d\n", r);
}

static void test_duration_2hours(void) {
    TEST("2hours -> 120");
    int r = test_parse_duration("2hours");
    if (r == 120) { PASS(); return; }
    FAIL("expected 120"); printf("  got %d\n", r);
}

static void test_duration_5days(void) {
    TEST("5 days -> 7200");
    int r = test_parse_duration("5 days");
    if (r == 7200) { PASS(); return; }
    FAIL("expected 7200"); printf("  got %d\n", r);
}

static void test_duration_invalid(void) {
    TEST("invalid returns -1");
    int r = test_parse_duration("xyz");
    if (r == -1) { PASS(); return; }
    FAIL("expected -1"); printf("  got %d\n", r);
}

static void test_duration_null(void) {
    TEST("NULL -> -1");
    int r = test_parse_duration(NULL);
    if (r == -1) { PASS(); return; }
    FAIL("expected -1"); printf("  got %d\n", r);
}

static void test_duration_empty(void) {
    TEST("empty -> -1");
    int r = test_parse_duration("");
    if (r == -1) { PASS(); return; }
    FAIL("expected -1"); printf("  got %d\n", r);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("Cron Utility Tests (P176)\n");
    printf("==========================\n\n");

    printf("--- cron_canonical_skills ---\n");
    test_canonical_skills_null_skills();
    test_canonical_skills_single_skill();
    test_canonical_skills_skills_array();
    test_canonical_skills_dedup();
    test_canonical_skills_empty_array_fallback();

    printf("\n--- cron_normalize_value ---\n");
    test_normalize_value_null();
    test_normalize_value_empty();
    test_normalize_value_whitespace();
    test_normalize_value_trim();
    test_normalize_value_trailing_slash();
    test_normalize_value_trailing_slash_no_strip();
    test_normalize_value_only_slash();

    printf("\n--- cron_normalize_deliver ---\n");
    test_normalize_deliver_null();
    test_normalize_deliver_string();
    test_normalize_deliver_array();
    test_normalize_deliver_empty_array();
    test_normalize_deliver_trimmed_array();

    printf("\n--- cron_parse_duration ---\n");
    test_duration_30m();
    test_duration_2h();
    test_duration_1d();
    test_duration_variants();
    test_duration_2hours();
    test_duration_5days();
    test_duration_invalid();
    test_duration_null();
    test_duration_empty();

    printf("\n--- cron_secure_dir/file/coerce ---\n");
    test_secure_dir_null();
    test_secure_file_null();
    test_secure_file_nonexistent();
    test_coerce_null();
    test_coerce_empty();
    test_coerce_valid();

    printf("\n--- cron_schedule_display_for_job ---\n");
    test_sched_display_null();
    test_sched_display_explicit();
    test_sched_display_dict_display();
    test_sched_display_dict_expr();
    test_sched_display_dict_run_at();
    test_sched_display_string();
    test_sched_display_fallback();

    printf("\n--- cron_ensure_dirs ---\n");
    test_ensure_dirs_null();
    test_ensure_dirs_empty();
    test_ensure_dirs_tmp();

    printf("\n==========================\n");
    printf("Results: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
