/*
 * nous_rate_guard.h — Cross-session rate limit guard for Nous Portal.
 * Port of Python agent/nous_rate_guard.py (325 lines).
 *
 * Writes rate limit state to a shared JSON file (~/.hermes/rate_limits/nous.json)
 * so all sessions can check whether Nous Portal is currently rate-limited
 * before making requests. Prevents retry amplification when RPH is tapped.
 */
#ifndef NOUS_RATE_GUARD_H
#define NOUS_RATE_GUARD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- State file management --- */

/**
 * Record that Nous Portal is rate-limited.
 * Writes reset_at timestamp to the shared state file.
 * @param hermes_home  Path to HERMES_HOME (e.g. ~/.hermes).
 * @param reset_at     POSIX timestamp when rate limit resets (0 = use default cooldown).
 * @param default_cooldown_secs  Fallback cooldown when reset_at is 0 (default 300s).
 */
void nous_rate_guard_record(const char *hermes_home,
                             double reset_at,
                             double default_cooldown_secs);

/**
 * Check if Nous Portal is currently rate-limited.
 * @param hermes_home  Path to HERMES_HOME.
 * @return Seconds remaining until reset, or -1.0 if not rate-limited or on error.
 */
double nous_rate_guard_remaining(const char *hermes_home);

/**
 * Clear the rate limit state file (e.g., after a successful request).
 */
void nous_rate_guard_clear(const char *hermes_home);

/* --- Formatting --- */

/**
 * Format seconds remaining into human-readable duration.
 * e.g. "45s", "3m 12s", "1h 30m"
 * @param seconds  Seconds to format.
 * @param buf      Output buffer (minimum 32 bytes).
 * @param sz       Buffer size.
 * @return buf.
 */
const char *nous_rate_guard_format_remaining(double seconds, char *buf, size_t sz);

#ifdef __cplusplus
}
#endif

#endif /* NOUS_RATE_GUARD_H */
