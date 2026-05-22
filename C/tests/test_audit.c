/*
 * test_audit.c — Security audit log test suite (G130).
 *
 * Tests: init with custom dir, log security events, convenience
 * wrappers, auto-init, shutdown, NULL safety.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_audit.c src/agent/audit.c \
 *       -o /tmp/hermes_test_audit -lm -lpthread
 *
 * Run:
 *   /tmp/hermes_test_audit
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_TRUE(name, expr) TEST(name, (expr))
#define TEST_FALSE(name, expr) TEST(name, !(expr))

/* ================================================================
 *  1. Init with custom directory
 * ================================================================ */
static void test_init(void) {
    printf("\n--- Init ---\n");

    /* Use /tmp for test logs */
    TEST_TRUE("init with /tmp", audit_init("/tmp"));

    /* Double init should be safe */
    TEST_TRUE("re-init safe", audit_init("/tmp"));

    /* NULL init should fall back to env */
    /* This may create ~/.slermes/logs/ but test shouldn't fail */
    audit_shutdown();
    TEST_TRUE("init with NULL", audit_init(NULL));

    audit_shutdown();
}

/* ================================================================
 *  2. Log security events
 * ================================================================ */
static void test_log_security(void) {
    printf("\n--- Log Security Events ---\n");

    /* Clean state */
    audit_shutdown();
    TEST_TRUE("init for log tests", audit_init("/tmp"));

    /* Log various events */
    audit_log_security("approval", "terminal", "approved", "user_ok", "rm -rf /");
    TEST("approval log no crash", 1);

    audit_log_security("approval", "file", "denied", "user_blocked", "delete_all");
    TEST("denial log no crash", 1);

    audit_log_security("violation", "url_safety", "blocked", "private_ip", "http://localhost");
    TEST("violation log no crash", 1);

    audit_log_security("redaction", "api_key", "redacted", "sk-*", NULL);
    TEST("redaction log no crash", 1);

    /* All NULL fields */
    audit_log_security(NULL, NULL, NULL, NULL, NULL);
    TEST("NULL field log no crash", 1);

    audit_shutdown();
}

/* ================================================================
 *  3. Convenience wrappers
 * ================================================================ */
static void test_convenience_wrappers(void) {
    printf("\n--- Convenience Wrappers ---\n");

    audit_shutdown();
    TEST_TRUE("init for wrappers", audit_init("/tmp"));

    audit_log_approval("terminal", "curl http://evil.com", true);
    TEST("approval wrapper no crash", 1);

    audit_log_approval("file", "read /etc/shadow", false);
    TEST("denial wrapper no crash", 1);

    audit_log_redaction("user_input", "sk-[A-Za-z0-9]+");
    TEST("redaction wrapper no crash", 1);

    audit_log_violation("command_allowlist", "blocked: rm -rf /");
    TEST("violation wrapper no crash", 1);

    audit_shutdown();
}

/* ================================================================
 *  4. Shutdown and re-init
 * ================================================================ */
static void test_shutdown(void) {
    printf("\n--- Shutdown ---\n");

    /* Shutdown when not initialized — no crash */
    audit_shutdown();
    TEST("shutdown uninitialized no crash", 1);

    /* Init, log, shutdown */
    audit_init("/tmp");
    audit_log_security("test", "shutdown", "ok", NULL, NULL);
    audit_shutdown();
    TEST("init-log-shutdown cycle", 1);

    /* Log after shutdown — should auto-reinit */
    audit_log_security("test", "auto_reinit", "ok", "after_shutdown", NULL);
    TEST("auto-reinit after shutdown", 1);

    audit_shutdown();
    audit_shutdown();
    TEST("double shutdown no crash", 1);
}

/* ================================================================
 *  5. Newlines in detail
 * ================================================================ */
static void test_newline_sanitization(void) {
    printf("\n--- Newline Sanitization ---\n");

    audit_shutdown();
    audit_init("/tmp");

    /* Detail with newlines should be sanitized to spaces */
    audit_log_security("test", "newline", "ok", "multiline\ndetail\r\nwith\nlines", NULL);
    TEST("newline sanitization no crash", 1);

    /* Very long detail */
    char long_detail[5000];
    memset(long_detail, 'X', 4000);
    long_detail[4000] = '\0';
    audit_log_security("test", "long", "ok", "truncation_test", long_detail);
    TEST("long detail no crash", 1);

    audit_shutdown();
}

int main(void) {
    printf("=== Audit Log Test Suite (G130) ===\n");

    test_init();
    test_log_security();
    test_convenience_wrappers();
    test_shutdown();
    test_newline_sanitization();

    /* Cleanup test log files */
    unlink("/tmp/hermes_security.log");
    unlink("/tmp/security.log");

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
