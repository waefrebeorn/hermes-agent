/*
 * test_hermes_signal.c — Tests for C signal handling helpers (J13).
 *
 * Tests: signal_on, signal_default, signal_register_common,
 * signal_safe_write, and edge cases.
 *
 * Compile:
 *   gcc -O2 -Wall -Wextra -I../include -I../lib/libsignal \
 *       tests/test_hermes_signal.c lib/libsignal/hermes_signal.c \
 *       -o /tmp/hermes_test_signal -lm
 *
 * Run: /tmp/hermes_test_signal
 */
#include "hermes_signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static int passed = 0, failed = 0;
static int test_count = 0;

#define TEST(name, expr) do { \
    test_count++; \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_NULL(name, p) TEST(name, p == NULL)

/* Flag set by test handler */
static volatile int g_test_signal_caught = 0;

static void test_sig_handler(int signum) {
    (void)signum;
    g_test_signal_caught = 1;
}

static void test_signal_on(void) {
    printf("\n--- signal_on ---\n");

    /* Register handler for SIGUSR1 (safe to test — no system impact) */
    bool ok = signal_on(SIGUSR1, test_sig_handler);
    TEST("signal_on SIGUSR1 returns true", ok);

    /* Restore default */
    bool ok2 = signal_default(SIGUSR1);
    TEST("signal_default SIGUSR1 returns true", ok2);

    /* Register handler for SIGUSR2 */
    bool ok3 = signal_on(SIGUSR2, test_sig_handler);
    TEST("signal_on SIGUSR2 returns true", ok3);

    /* Restore default */
    signal_default(SIGUSR2);
}

static void test_signal_register_common(void) {
    printf("\n--- signal_register_common ---\n");

    volatile int stop_flag = 0;

    /* Register common handlers */
    bool ok = signal_register_common(&stop_flag);
    TEST("signal_register_common returns true", ok);

    /* Flag should still be 0 */
    TEST("stop_flag still 0", stop_flag == 0);

    /* Restore defaults */
    signal_default(SIGINT);
    signal_default(SIGTERM);
}

static void test_signal_safe_write(void) {
    printf("\n--- signal_safe_write ---\n");

    /* Write a short message to stderr — verify no crash */
    signal_safe_write("test message\n");
    TEST("signal_safe_write doesn't crash", 1);

    /* NULL message */
    signal_safe_write(NULL);
    TEST("signal_safe_write(NULL) doesn't crash", 1);

    /* Empty message */
    signal_safe_write("");
    TEST("signal_safe_write('') doesn't crash", 1);

    /* Very long message */
    {
        char long_msg[4096];
        memset(long_msg, 'x', 4000);
        long_msg[4000] = '\0';
        signal_safe_write(long_msg);
        TEST("signal_safe_write(4000 chars) doesn't crash", 1);
    }
}

static void test_edge_cases(void) {
    printf("\n--- Edge cases ---\n");

    /* Uncatchable signals */
    bool r = signal_on(SIGKILL, test_sig_handler);
    TEST("signal_on(SIGKILL) returns false", !r);

    r = signal_on(SIGSTOP, test_sig_handler);
    TEST("signal_on(SIGSTOP) returns false", !r);

    /* Invalid signum */
    r = signal_on(0, test_sig_handler);
    TEST("signal_on(sig=0) returns false", !r);

    r = signal_on(-1, test_sig_handler);
    TEST("signal_on(sig=-1) returns false", !r);

    /* signal_default with uncatchable/invalid */
    r = signal_default(SIGKILL);
    TEST("signal_default(SIGKILL) returns false", !r);

    r = signal_default(0);
    TEST("signal_default(sig=0) returns false", !r);

    /* Double-register same signal */
    r = signal_on(SIGUSR1, test_sig_handler);
    TEST("signal_on SIGUSR1 (first) returns true", r);

    r = signal_on(SIGUSR1, test_sig_handler);
    TEST("signal_on SIGUSR1 (second) returns true (replaces)", r);

    r = signal_default(SIGUSR1);
    TEST("signal_default SIGUSR1 restores default", r);

    /* Toggle: register, restore, register again */
    r = signal_on(SIGUSR2, test_sig_handler);
    TEST("signal_on SIGUSR2 returns true", r);
    r = signal_default(SIGUSR2);
    TEST("signal_default SIGUSR2 returns true", r);
    r = signal_on(SIGUSR2, test_sig_handler);
    TEST("signal_on SIGUSR2 (re-register) returns true", r);
    signal_default(SIGUSR2);

    /* signal_register_common with NULL */
    r = signal_register_common(NULL);
    TEST("signal_register_common(NULL) returns true", r);
    signal_default(SIGINT);
    signal_default(SIGTERM);
}

int main(void) {
    printf("=== Signal Helper Test Suite (J13) ===\n");

    test_signal_on();
    test_signal_register_common();
    test_signal_safe_write();
    test_edge_cases();

    printf("\n=== Results: %d passed, %d failed (%d assertions) ===\n",
           passed, failed, test_count);
    return failed > 0 ? 1 : 0;
}
