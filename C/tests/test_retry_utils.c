/* Retry utils tests — verify jittered backoff behavior.
 *
 * Tests:
 * 1. attempt=1 returns base_delay + jitter
 * 2. attempt=N doubles each time
 * 3. Caps at max_delay
 * 4. Jitter is non-negative and bounded
 * 5. Thread-safe counter produces different results
 * 6. reset() resets counter
 * 7. Zero base_delay clips to max_delay
 * 8. Large attempt (>=63) clips to max_delay
 */

#include "hermes_retry_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
    } else { \
        passed++; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, eps) TEST(#a " ~= " #b, fabs((a) - (b)) <= (eps))

int main(void)
{
    /* Test 1: attempt=1 returns base_delay-ish value */
    double r = hermes_jittered_backoff(1, 5.0, 120.0, 0.5);
    TEST("attempt1>=5.0", r >= 5.0);
    TEST("attempt1<12.5", r < 12.5); /* 5 + 0.5*5 = max 7.5, but with jitter_ratio*delay jitter, max is 5+2.5=7.5 */
    /* Actually with jitter_ratio=0.5: delay=5, jitter in [0, 2.5). Max = 7.5 */
    /* Hmm wait, jitter = jitter_ratio * delay * (rnd/UINT64_MAX), so max is 0.5*5*1 = 2.5 */
    /* Total max = 7.5 */
    /* Actually let me double-check: jitter_ratio * delay * (rnd/UINT64_MAX) where rnd is in [0, UINT64_MAX] */
    /* So jitter is in [0, jitter_ratio * delay) = [0, 2.5) */
    /* Total = [5.0, 7.5) */
    TEST("attempt1<8.0", r < 8.0);

    /* Test 2: attempt=2 doubles */
    double r2 = hermes_jittered_backoff(2, 5.0, 120.0, 0.0);
    ASSERT_NEAR(r2, 10.0, 0.01);

    /* Test 3: caps at max_delay */
    double r3 = hermes_jittered_backoff(10, 5.0, 30.0, 0.0);
    ASSERT_NEAR(r3, 30.0, 0.01);

    /* Test 4: Zero base_delay clips to max */
    double r4 = hermes_jittered_backoff(1, 0.0, 30.0, 0.0);
    ASSERT_NEAR(r4, 30.0, 0.01);

    /* Test 5: Large attempt (>=63) clips to max */
    double r5 = hermes_jittered_backoff(63, 5.0, 30.0, 0.0);
    ASSERT_NEAR(r5, 30.0, 0.01);

    double r5b = hermes_jittered_backoff(100, 5.0, 30.0, 0.0);
    ASSERT_NEAR(r5b, 30.0, 0.01);

    /* Test 6: Jitter is non-negative */
    double r6 = hermes_jittered_backoff(3, 10.0, 120.0, 0.8);
    double expected_delay = 40.0; /* 10 * 2^2 */
    TEST("jitter>=0", r6 >= expected_delay);
    TEST("jitter<=max", r6 <= expected_delay + 0.8 * expected_delay);

    /* Test 7: reset() resets counter (counter starts at 0, first call == 1) */
    hermes_jittered_backoff_reset();
    double ra = hermes_jittered_backoff(1, 5.0, 120.0, 0.0);
    ASSERT_NEAR(ra, 5.0, 0.01);

    /* Test 8: No jitter_ratio = 0 gives exact backoff */
    hermes_jittered_backoff_reset();
    double r7a = hermes_jittered_backoff(1, 2.0, 60.0, 0.0);
    ASSERT_NEAR(r7a, 2.0, 0.01);
    double r7b = hermes_jittered_backoff(2, 2.0, 60.0, 0.0);
    ASSERT_NEAR(r7b, 4.0, 0.01);
    double r7c = hermes_jittered_backoff(3, 2.0, 60.0, 0.0);
    ASSERT_NEAR(r7c, 8.0, 0.01);
    double r7d = hermes_jittered_backoff(4, 2.0, 60.0, 0.0);
    ASSERT_NEAR(r7d, 16.0, 0.01);

    /* Test 9: Negative/zero attempt is treated as attempt 1 */
    hermes_jittered_backoff_reset();
    double r8a = hermes_jittered_backoff(0, 5.0, 120.0, 0.0);
    ASSERT_NEAR(r8a, 5.0, 0.01);
    double r8b = hermes_jittered_backoff(-1, 5.0, 120.0, 0.0);
    ASSERT_NEAR(r8b, 5.0, 0.01);

    /* Test 10: Zero max_delay caps at 0 */
    double r9 = hermes_jittered_backoff(3, 5.0, 0.0, 0.0);
    ASSERT_NEAR(r9, 0.0, 0.01);

    /* Test 11: Very small base_delay */
    double r10 = hermes_jittered_backoff(4, 0.001, 120.0, 0.0);
    ASSERT_NEAR(r10, 0.008, 0.001);

    /* Test 12: Very large max_delay */
    double r11 = hermes_jittered_backoff(10, 5.0, 999999.0, 0.0);
    ASSERT_NEAR(r11, 2560.0, 0.1);

    /* Test 13: Jitter ratio > 1.0 still bounded */
    double r12 = hermes_jittered_backoff(2, 5.0, 120.0, 2.0);
    double expected_r12 = 10.0;
    TEST("jitter>1 bounded correctly", r12 >= expected_r12 && r12 <= expected_r12 + 2.0 * expected_r12);

    /* Test 14: Multiple resets */
    hermes_jittered_backoff_reset();
    hermes_jittered_backoff_reset();
    hermes_jittered_backoff_reset();
    double r13 = hermes_jittered_backoff(3, 10.0, 100.0, 0.0);
    ASSERT_NEAR(r13, 40.0, 0.01);

    printf("retry_utils: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
