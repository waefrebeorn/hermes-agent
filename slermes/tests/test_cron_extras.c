/*
 * test_cron_extras.c — Cron extras unit tests (P172-P175).
 *
 * Tests: retry set/get/reset, chain context/output, template create/instantiate,
 * notification set, NULL safety, edge cases.
 *
 * Avoids cron_job_increment_retry() which calls sleep().
 * Avoids cron_send_notification() which needs gateway.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libcron \
 *       tests/test_cron_extras.c src/cron/cron_extras.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_cron_extras -lm
 *
 * Run:
 *   /tmp/hermes_test_cron_extras
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Functions not exposed in hermes.h */
extern const char *cron_notify_get_channel(void);
extern void cron_job_reset_retry(const char *job_name);

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 * 1. Retry — set / get / reset
 * ================================================================ */
static void test_retry_set_get(void) {
    TEST("set retry for new job");
    assert(cron_job_set_retry("job_r1", 5, 30));
    PASS();

    TEST("get retry count (initial = 0)");
    assert(cron_job_get_retry_count("job_r1") == 0);
    PASS();

    TEST("get max retries");
    assert(cron_job_get_max_retries("job_r1") == 5);
    PASS();

    TEST("get retry for unknown job returns 0");
    assert(cron_job_get_retry_count("nonexistent") == 0);
    PASS();

    TEST("get max retries for unknown job returns 0");
    assert(cron_job_get_max_retries("nonexistent") == 0);
    PASS();

    TEST("set retry with NULL name returns false");
    assert(!cron_job_set_retry(NULL, 3, 10));
    PASS();

    TEST("update existing retry");
    assert(cron_job_set_retry("job_r1", 10, 60));
    assert(cron_job_get_max_retries("job_r1") == 10);
    PASS();
}

static void test_retry_reset(void) {
    TEST("reset void function — no crash on existing");
    cron_job_reset_retry("job_r1");
    assert(cron_job_get_retry_count("job_r1") == 0);
    PASS();

    TEST("reset void function — no crash on unknown");
    cron_job_reset_retry("nonexistent_job");
    PASS();

    TEST("reset void function — no crash on NULL");
    cron_job_reset_retry(NULL);
    PASS();
}

/* ================================================================
 * 2. Notification — set channel
 * ================================================================ */
static void test_notify_channel(void) {
    TEST("set channel with valid ID");
    assert(cron_notify_set_channel("telegram:12345"));
    PASS();

    TEST("get channel returns set value");
    const char *ch = cron_notify_get_channel();
    assert(ch != NULL);
    assert(strcmp(ch, "telegram:12345") == 0);
    PASS();

    TEST("set channel with NULL returns false");
    assert(!cron_notify_set_channel(NULL));
    PASS();

    TEST("update channel");
    assert(cron_notify_set_channel("discord:67890"));
    ch = cron_notify_get_channel();
    assert(ch != NULL);
    assert(strcmp(ch, "discord:67890") == 0);
    PASS();

    /* Reset channel for subsequent tests */
    assert(cron_notify_set_channel(""));
    TEST("notify on complete — no-op returns true");
    assert(cron_notify_on_complete("test_job", true));
    PASS();

    TEST("notify on failure — no-op returns true");
    assert(cron_notify_on_failure("test_job", true));
    PASS();
}

/* ================================================================
 * 3. Chain — context set/get
 * ================================================================ */
static void test_chain_context(void) {
    TEST("set context for job");
    assert(cron_chain_set_context("chain_job", "source_job"));
    PASS();

    TEST("get context returns source");
    const char *ctx = cron_chain_get_context("chain_job");
    assert(ctx != NULL);
    assert(strcmp(ctx, "source_job") == 0);
    PASS();

    TEST("get context for unknown returns NULL");
    assert(cron_chain_get_context("unknown") == NULL);
    PASS();

    TEST("set context with NULL name returns false");
    assert(!cron_chain_set_context(NULL, "source"));
    PASS();

    TEST("get context with NULL name returns NULL");
    assert(cron_chain_get_context(NULL) == NULL);
    PASS();

    TEST("update context");
    assert(cron_chain_set_context("chain_job", "new_source"));
    ctx = cron_chain_get_context("chain_job");
    assert(ctx != NULL);
    assert(strcmp(ctx, "new_source") == 0);
    PASS();
}

/* ================================================================
 * 4. Chain — store/get output
 * ================================================================ */
static void test_chain_output(void) {
    /* setup: chain_job_A -> chain_job_B */
    cron_chain_set_context("chain_job_B", "chain_job_A");

    TEST("store output for source job");
    cron_chain_store_output("chain_job_A", "result_data_123");
    PASS();

    TEST("get output for chained job");
    char *out = cron_chain_get_output("chain_job_B");
    assert(out != NULL);
    assert(strcmp(out, "result_data_123") == 0);
    free(out);
    PASS();

    TEST("get output for standalone job returns NULL");
    cron_chain_store_output("standalone", "data");
    out = cron_chain_get_output("standalone");
    assert(out == NULL);
    free(out);
    PASS();

    TEST("get output for unknown returns NULL");
    out = cron_chain_get_output("unknown");
    assert(out == NULL);
    PASS();

    TEST("get output with NULL returns NULL");
    out = cron_chain_get_output(NULL);
    assert(out == NULL);
    PASS();

    TEST("store output with NULL name — no crash");
    cron_chain_store_output(NULL, "data");
    PASS();

    TEST("store output over existing");
    cron_chain_store_output("chain_job_A", "updated_data");
    out = cron_chain_get_output("chain_job_B");
    assert(out != NULL);
    assert(strcmp(out, "updated_data") == 0);
    free(out);
    PASS();
}

/* ================================================================
 * 5. Template — create
 * ================================================================ */
static void test_template_create(void) {
    TEST("create template with all params");
    assert(cron_template_create("daily_backup", "0 2 * * *",
                                 "/usr/bin/backup {{dir}}",
                                 "{\"dir\": \"/data\"}"));
    PASS();

    TEST("create template without params");
    assert(cron_template_create("simple", "*/5 * * * *", "echo hi", NULL));
    PASS();

    TEST("create duplicate name returns false");
    assert(!cron_template_create("daily_backup", "0 3 * * *", "other", NULL));
    PASS();

    TEST("create with NULL name returns false");
    assert(!cron_template_create(NULL, "*/5 * * * *", "cmd", NULL));
    PASS();

    TEST("create with NULL schedule returns false");
    assert(!cron_template_create("t_nosched", NULL, "cmd", NULL));
    PASS();
}

/* ================================================================
 * 6. Template — instantiate
 * ================================================================ */
static void test_template_instantiate(void) {
    char name[256], sched[256], cmd[4096];

    TEST("instantiate with params — placeholder replaced");
    assert(cron_template_instantiate("daily_backup", "{\"dir\": \"/home/backup\"}",
                                      name, sizeof(name),
                                      sched, sizeof(sched),
                                      cmd, sizeof(cmd)));
    assert(strcmp(sched, "0 2 * * *") == 0);
    assert(strstr(cmd, "/home/backup") != NULL);
    assert(strstr(cmd, "{{dir}}") == NULL); /* placeholder gone */
    PASS();

    TEST("instantiate without params — raw command");
    assert(cron_template_instantiate("simple", NULL,
                                      name, sizeof(name),
                                      sched, sizeof(sched),
                                      cmd, sizeof(cmd)));
    assert(strcmp(sched, "*/5 * * * *") == 0);
    assert(strcmp(cmd, "echo hi") == 0);
    PASS();

    TEST("instantiate unknown template returns false");
    assert(!cron_template_instantiate("nonexistent", NULL,
                                       name, sizeof(name),
                                       sched, sizeof(sched),
                                       cmd, sizeof(cmd)));
    PASS();

    TEST("instantiate with NULL name returns false");
    assert(!cron_template_instantiate(NULL, NULL,
                                       name, sizeof(name),
                                       sched, sizeof(sched),
                                       cmd, sizeof(cmd)));
    PASS();

    TEST("instantiate with NULL output buffers returns false");
    assert(!cron_template_instantiate("daily_backup", NULL,
                                       NULL, sizeof(name),
                                       sched, sizeof(sched),
                                       cmd, sizeof(cmd)));
    PASS();
}

/* ================================================================
 * 7. Multiple placeholders in template
 * ================================================================ */
static void test_template_multi_placeholder(void) {
    TEST("create template with multiple placeholders");
    assert(cron_template_create("multi_pl", "0 * * * *",
                                 "process {{input}} --output {{output}} --mode {{mode}}",
                                 NULL));
    PASS();

    char name[256], sched[256], cmd[4096];
    TEST("instantiate with multiple params");
    assert(cron_template_instantiate("multi_pl",
                                      "{\"input\": \"/data/in\", \"output\": \"/data/out\", \"mode\": \"fast\"}",
                                      name, sizeof(name),
                                      sched, sizeof(sched),
                                      cmd, sizeof(cmd)));
    assert(strstr(cmd, "/data/in") != NULL);
    assert(strstr(cmd, "/data/out") != NULL);
    assert(strstr(cmd, "fast") != NULL);
    assert(strstr(cmd, "{{input}}") == NULL);
    assert(strstr(cmd, "{{output}}") == NULL);
    assert(strstr(cmd, "{{mode}}") == NULL);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void) {
    printf("=== Cron Extras Tests (P172-P175) ===\n");

    test_retry_set_get();
    test_retry_reset();
    test_notify_channel();
    test_chain_context();
    test_chain_output();
    test_template_create();
    test_template_instantiate();
    test_template_multi_placeholder();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
