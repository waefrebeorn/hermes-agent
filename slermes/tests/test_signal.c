/*
 * test_signal.c — Signal handling test suite (J13).
 *
 * Tests: signal_on, signal_default, signal_register_common,
 *        signal_default_handler, signal_safe_write.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libsignal \
 *       tests/test_signal.c lib/libsignal/hermes_signal.c \
 *       -o /tmp/hermes_test_signal -lm
 *
 * Run:
 *   /tmp/hermes_test_signal
 */

#include "hermes_signal.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))

static volatile int g_sig_caught = 0;

static void test_handler(int signum) {
    (void)signum;
    g_sig_caught = 1;
}

/* ================================================================
 *  1. signal_on / signal_default
 * ================================================================ */
static void test_signal_on_default(void) {
    printf("\n--- signal_on / signal_default ---\n");

    TEST("signal_on returns true for SIGUSR1",
         signal_on(SIGUSR1, test_handler));

    /* Send SIGUSR1 to self and verify handler runs */
    g_sig_caught = 0;
    raise(SIGUSR1);
    TEST_INT_EQ("handler caught SIGUSR1", g_sig_caught, 1);

    /* Restore default */
    TEST("signal_default returns true for SIGUSR1",
         signal_default(SIGUSR1));

    /* SIGUSR2 — verify round-trip */
    TEST("signal_on returns true for SIGUSR2",
         signal_on(SIGUSR2, test_handler));
    g_sig_caught = 0;
    raise(SIGUSR2);
    TEST_INT_EQ("handler caught SIGUSR2", g_sig_caught, 1);
    TEST("signal_default returns true for SIGUSR2",
         signal_default(SIGUSR2));
}

/* ================================================================
 *  2. signal_safe_write (no crash test)
 * ================================================================ */
static void test_safe_write(void) {
    printf("\n--- signal_safe_write ---\n");

    signal_safe_write("test message\n");
    TEST("safe write with valid message", 1);

    signal_safe_write(NULL);
    TEST("safe write with NULL message", 1);

    signal_safe_write("");
    TEST("safe write with empty message", 1);
}

/* ================================================================
 *  3. signal_register_common
 * ================================================================ */
static void test_register_common(void) {
    printf("\n--- signal_register_common ---\n");

    volatile int stop = 0;
    TEST("register_common returns true",
         signal_register_common(&stop));

    TEST("stop flag still 0 before signal", stop == 0);
}

/* ================================================================
 *  4. Fork-based tests (handler invocation, _exit in child)
 * ================================================================ */

/* Fork: verify signal_on catches SIGUSR1 in child */
static void test_fork_signal_caught(void) {
    printf("\n--- fork: signal_on catches SIGUSR1 ---\n");

    pid_t pid = fork();
    if (pid == 0) {
        /* Child */
        signal_on(SIGUSR1, test_handler);
        g_sig_caught = 0;
        raise(SIGUSR1);
        _exit(g_sig_caught ? 42 : 1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        TEST("child caught SIGUSR1 (status 42)",
             WIFEXITED(status) && WEXITSTATUS(status) == 42);
    } else {
        TEST("fork failed", 0);
    }
}

/* Fork: verify signal_default_handler exits with 128+signum */
static void test_fork_default_handler(void) {
    printf("\n--- fork: default_handler exits with 128+SIGINT ---\n");

    pid_t pid = fork();
    if (pid == 0) {
        /* Redirect stderr to /dev/null to suppress signal name output */
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) dup2(fd, STDERR_FILENO);
        signal_default_handler(SIGINT);
        _exit(0); /* Should not reach here */
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        TEST("default_handler SIGINT exits 130 (128+2)",
             WIFEXITED(status) && WEXITSTATUS(status) == 130);
    } else {
        TEST("fork failed", 0);
    }
}

/* Fork: verify SIGPIPE handler exits with 128+13=141 */
static void test_fork_default_handler_sigpipe(void) {
    printf("\n--- fork: default_handler exits with 128+SIGPIPE ---\n");

    pid_t pid = fork();
    if (pid == 0) {
        signal_default_handler(SIGPIPE);
        _exit(0);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        TEST("default_handler SIGPIPE exits 141 (128+13)",
             WIFEXITED(status) && WEXITSTATUS(status) == 141);
    } else {
        TEST("fork failed", 0);
    }
}

/* Fork: verify unknown signal exits with 128+999=1127 */
static void test_fork_default_handler_unknown(void) {
    printf("\n--- fork: default_handler unknown signal ---\n");

    pid_t pid = fork();
    if (pid == 0) {
        signal_default_handler(999);
        _exit(0);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        /* _exit(128+999) truncated to least significant byte: (1127 & 0xFF) = 103 */
        TEST("default_handler unknown exits status 103 (128+999 & 0xFF)",
             WIFEXITED(status) && WEXITSTATUS(status) == 103);
    } else {
        TEST("fork failed", 0);
    }
}

/* Fork: verify signal_register_common catches SIGINT in child */
static void test_fork_register_common(void) {
    printf("\n--- fork: register_common catches SIGINT ---\n");

    pid_t pid = fork();
    if (pid == 0) {
        volatile int stop_flag = 0;
        signal_register_common(&stop_flag);
        /* Raise SIGINT — should set stop_flag to 1 */
        raise(SIGINT);
        _exit(stop_flag ? 42 : 1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        TEST("register_common SIGINT sets stop flag in child (status 42)",
             WIFEXITED(status) && WEXITSTATUS(status) == 42);
    } else {
        TEST("fork failed", 0);
    }
}

int main(void) {
    printf("=== Signal Handling Test Suite (J13) ===\n");

    test_signal_on_default();
    test_safe_write();
    test_register_common();
    test_fork_signal_caught();
    test_fork_default_handler();
    test_fork_default_handler_sigpipe();
    test_fork_default_handler_unknown();
    test_fork_register_common();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
