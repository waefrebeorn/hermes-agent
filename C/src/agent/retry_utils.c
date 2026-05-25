/* Retry utilities — jittered exponential backoff for decorrelated retries.
 *
 * Python equivalent: agent/retry_utils.py
 */

#include "hermes_retry_utils.h"
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Monotonic counter for jitter seed uniqueness within the same process.
 * Protected by a mutex to avoid race conditions in concurrent retry paths. */
static uint64_t g_jitter_counter = 0;
static pthread_mutex_t g_jitter_lock = PTHREAD_MUTEX_INITIALIZER;

/* Simple xorshift64 PRNG — stateful, seeded per-call so concurrent callers
 * each get independent sequences without shared state contention. */
static inline uint64_t xorshift64(uint64_t seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

double hermes_jittered_backoff(int attempt,
                                double base_delay,
                                double max_delay,
                                double jitter_ratio)
{
    uint64_t tick;

    /* Thread-safe counter increment */
    pthread_mutex_lock(&g_jitter_lock);
    g_jitter_counter++;
    tick = g_jitter_counter;
    pthread_mutex_unlock(&g_jitter_lock);

    /* Compute exponential delay */
    int exponent = (attempt > 1) ? attempt - 1 : 0;
    double delay;
    if (exponent >= 63 || base_delay <= 0.0) {
        delay = max_delay;
    } else {
        delay = base_delay * pow(2.0, exponent);
        if (delay > max_delay)
            delay = max_delay;
    }

    /* Seed from time + counter for decorrelation even with coarse clocks */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t seed = ((uint64_t)ts.tv_nsec ^ (tick * 0x9E3779B97F4A7C15ULL)) & 0xFFFFFFFFULL;
    uint64_t rnd = xorshift64(seed);

    /* Uniform jitter in [0, jitter_ratio * delay) */
    double jitter = jitter_ratio * delay * ((double)rnd / (double)UINT64_MAX);

    return delay + jitter;
}

void hermes_jittered_backoff_reset(void)
{
    pthread_mutex_lock(&g_jitter_lock);
    g_jitter_counter = 0;
    pthread_mutex_unlock(&g_jitter_lock);
}
