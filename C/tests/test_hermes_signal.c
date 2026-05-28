/*
 * test_hermes_signal.c — Tests for signal handling helpers (lib/libsignal).
 */
#define _POSIX_C_SOURCE 199309L
#include "hermes_signal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

/* Test flag for custom handler */
static volatile int g_caught = 0;
static void test_handler(int signum) {
    (void)signum;
    g_caught = 1;
}

static void test_on_and_default(void) {
    /* Register custom handler for SIGUSR1 (safe to use in test) */
    TEST("signal_on succeeds", signal_on(SIGUSR1, test_handler));

    /* Verify handler is registered by triggering */
    g_caught = 0;
    raise(SIGUSR1);
    TEST("custom handler caught signal", g_caught == 1);

    /* Restore default and confirm no crash */
    TEST("signal_default succeeds", signal_default(SIGUSR1));

    /* Register again to verify re-registration works */
    TEST("signal_on again succeeds", signal_on(SIGUSR1, test_handler));
    g_caught = 0;
    raise(SIGUSR1);
    TEST("handler still works after re-register", g_caught == 1);
    signal_default(SIGUSR1);
}

static void test_register_common(void) {
    volatile int stop_flag = 0;
    TEST("signal_register_common succeeds", signal_register_common(&stop_flag));
    TEST("stop_flag initially 0", stop_flag == 0);

    /* Trigger SIGINT via raise */
    raise(SIGINT);
    signal_default(SIGINT);
    TEST("stop_flag set by SIGINT", stop_flag == 1);

    /* Reset and test SIGTERM */
    stop_flag = 0;
    TEST("signal_register_common again", signal_register_common(&stop_flag));
    raise(SIGTERM);
    signal_default(SIGTERM);
    TEST("stop_flag set by SIGTERM", stop_flag == 1);
}

static void test_safe_write(void) {
    /* Should not crash */
    signal_safe_write(NULL);
    TEST("safe_write(NULL) no crash", 1);

    signal_safe_write("test_msg");
    TEST("safe_write(msg) no crash", 1);
}

static void test_default_handler(void) {
    /* Fork to test default_handler without crashing test process */
    pid_t pid = fork();
    if (pid == -1) {
        TEST("fork for default_handler", 0);
        return;
    }
    if (pid == 0) {
        /* Child: register handler and trigger */
        signal_on(SIGUSR1, signal_default_handler);
        raise(SIGUSR1);
        _exit(0);  /* Should not reach here */
    }
    int status;
    waitpid(pid, &status, 0);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) :
                    (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1);
    TEST("default_handler exits with 128+signum", exit_code == 128 + SIGUSR1);
}

int main(void) {
    printf("=== Signal Library Tests ===\n");

    test_on_and_default();
    test_register_common();
    test_safe_write();
    test_default_handler();

    printf("\n%d failures\n", failures);
    return failures > 0 ? 1 : 0;
}
