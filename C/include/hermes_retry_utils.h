#ifndef HERMES_RETRY_UTILS_H
#define HERMES_RETRY_UTILS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compute jittered exponential backoff delay.
 *
 * Replaces fixed exponential backoff with jittered delays to prevent
 * thundering-herd retry spikes when multiple sessions hit the same
 * rate-limited provider concurrently.
 *
 * @param attempt      1-based retry attempt number.
 * @param base_delay   Base delay in seconds for attempt 1.
 * @param max_delay    Maximum delay cap in seconds.
 * @param jitter_ratio Fraction of computed delay to use as random jitter
 *                     range. 0.5 means jitter is uniform in [0, 0.5 * delay].
 * @return Delay in seconds: min(base * 2^(attempt-1), max_delay) + jitter.
 */
double hermes_jittered_backoff(int attempt,
                               double base_delay,
                               double max_delay,
                               double jitter_ratio);

/**
 * Reset the jitter seed counter (for testing).
 */
void hermes_jittered_backoff_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_RETRY_UTILS_H */
