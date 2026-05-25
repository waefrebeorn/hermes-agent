/* test_signal.c — J13: libsignal tests (register, default, common handlers). */
#define _POSIX_C_SOURCE 199309L
#include "hermes_signal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

/* Test handler flag */
static volatile int g_test_flag = 0;
static void test_handler(int signum) {
    (void)signum;
    g_test_flag = 1;
}

int main(void) {
    printf("=== J13: libsignal Tests ===\n\n");

    /* --- signal_on / signal_default --- */
    printf("--- signal_on / signal_default ---\n");
    TEST("register SIGUSR1", signal_on(SIGUSR1, test_handler));
    TEST("register SIGUSR2", signal_on(SIGUSR2, test_handler));

    /* Raise SIGUSR1 and check flag */
    g_test_flag = 0;
    raise(SIGUSR1);
    TEST("handler called for SIGUSR1", g_test_flag == 1);

    /* Restore default and verify no crash on signal_default */
    TEST("restore default SIGUSR1", signal_default(SIGUSR1));
    /* Don't raise SIGUSR1 here — default action terminates */

    /* --- signal_register_common --- */
    printf("\n--- signal_register_common ---\n");
    volatile int stop = 0;
    TEST("register common handlers", signal_register_common(&stop));

    raise(SIGINT);
    TEST("SIGINT sets stop flag", stop == 1);

    stop = 0;
    raise(SIGTERM);
    TEST("SIGTERM sets stop flag", stop == 1);

    /* --- signal_safe_write (just verify no crash) --- */
    printf("\n--- signal_safe_write ---\n");
    signal_safe_write("test write\n");
    signal_safe_write(NULL);
    TEST("safe_write works", 1);

    /* --- signal_default_handler --- */
    printf("\n--- signal_default_handler (won't call -- would exit) ---\n");
    TEST("default handler ptr non-NULL", signal_default_handler != NULL);

    /* Cleanup */
    signal_default(SIGUSR1);
    signal_default(SIGUSR2);
    signal_default(SIGINT);
    signal_default(SIGTERM);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
