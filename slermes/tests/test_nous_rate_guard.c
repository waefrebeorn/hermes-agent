/*
 * test_nous_rate_guard.c — Tests for the Nous Portal rate limit guard.
 */
#include "nous_rate_guard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static int tests_pass = 0;
static int tests_fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s)\n", name, #expr); \
        tests_fail++; \
    } else { \
        tests_pass++; \
    } \
} while(0)

int main(void) {
    /* Create temp directory as hermes_home */
    char tmpdir[] = "/tmp/nous_rate_guard_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "FAIL: mkdtemp\n");
        return 1;
    }

    /* Test 1: remaining returns -1 initially (no state file) */
    double rem = nous_rate_guard_remaining(tmpdir);
    TEST("initial_remaining_is_none", rem < 0);

    /* Test 2: record rate limit */
    double now = (double)time(NULL);
    double reset_at = now + 120.0;  /* 2 minutes from now */
    nous_rate_guard_record(tmpdir, reset_at, 300.0);

    /* Test 3: remaining is positive */
    rem = nous_rate_guard_remaining(tmpdir);
    TEST("remaining_after_record", rem > 0);
    TEST("remaining_within_range", rem > 60 && rem < 180);

    /* Test 4: format_remaining */
    char buf[64];
    nous_rate_guard_format_remaining(45.0, buf, sizeof(buf));
    TEST("format_45s", strcmp(buf, "45s") == 0);
    nous_rate_guard_format_remaining(192.0, buf, sizeof(buf));
    TEST("format_3m12s", strcmp(buf, "3m 12s") == 0);
    nous_rate_guard_format_remaining(3600.0, buf, sizeof(buf));
    TEST("format_1h", strcmp(buf, "1h") == 0);
    nous_rate_guard_format_remaining(5400.0, buf, sizeof(buf));
    TEST("format_1h30m", strcmp(buf, "1h 30m") == 0);
    nous_rate_guard_format_remaining(-1.0, buf, sizeof(buf));
    TEST("format_expired", strcmp(buf, "expired") == 0);

    /* Test 5: default cooldown when reset_at is 0 */
    nous_rate_guard_record(tmpdir, 0.0, 60.0);
    rem = nous_rate_guard_remaining(tmpdir);
    TEST("default_cooldown", rem > 0 && rem < 120);

    /* Test 6: clear */
    nous_rate_guard_clear(tmpdir);
    rem = nous_rate_guard_remaining(tmpdir);
    TEST("clear_removes_state", rem < 0);

    /* Test 7: record with expired reset_at */
    nous_rate_guard_record(tmpdir, now - 10.0, 30.0);  /* 10s in the past */
    rem = nous_rate_guard_remaining(tmpdir);
    TEST("expired_reset_uses_cooldown", rem > 0 && rem < 90);

    /* Cleanup */
    unlink(tmpdir);
    rmdir(tmpdir);

    printf("%s: %d/%d passed, %d failed\n",
           tests_fail > 0 ? "FAIL" : "OK",
           tests_pass, tests_pass + tests_fail, tests_fail);
    return tests_fail > 0 ? 1 : 0;
}
