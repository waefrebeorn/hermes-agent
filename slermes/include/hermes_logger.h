/**
 * @file hermes_logger.h
 * @brief Structured logger for C Hermes agent.
 *
 * Writes agent.log and errors.log in Python-compatible format:
 *   2026-05-26 12:34:56,789 LEVEL module: message
 *
 * Log directory: <SLERMES_HOME>/logs/   (same as cmd_logs reads)
 * File: agent.log (INFO+), errors.log (WARNING+)
 *
 * Thread-safe via mutex.
 */

#ifndef HERMES_LOGGER_H
#define HERMES_LOGGER_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log severity levels — mirrors Python logging */
typedef enum {
    LOG_DEBUG    = 10,
    LOG_INFO     = 20,
    LOG_WARNING  = 30,
    LOG_ERROR    = 40,
    LOG_CRITICAL = 50,
} log_level_t;

/* Level name strings for output */
extern const char *hermes_log_level_name(log_level_t level);

/**
 * Initialize the logger subsystem.
 * Opens agent.log and errors.log in <home>/logs/.
 * Creates the logs/ directory if it doesn't exist.
 * Safe to call multiple times — subsequent calls are no-ops.
 */
void hermes_log_init(void);

/**
 * Write a log entry at the given severity level.
 *
 * Format: "YYYY-MM-DD HH:MM:SS,mmm LEVEL module: message\n"
 * Writes to agent.log for INFO+, errors.log for WARNING+.
 * Thread-safe.
 *
 * @param level   Severity (LOG_DEBUG .. LOG_CRITICAL)
 * @param module  Source module name (e.g. "agent_loop", "cli")
 * @param fmt     printf-style format string
 * @param ...     Format arguments
 */
void hermes_log(log_level_t level, const char *module, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * Flush and close log files.
 * Safe to call from atexit or shutdown.
 */
void hermes_log_shutdown(void);

/**
 * Set per-turn session context for log tagging.
 * Call at the start of each turn to tag subsequent log entries.
 * Pass NULL for any field that hasn't changed.
 *
 * @param session_id  Active session ID (or NULL to keep current)
 * @param model       Active model name (or NULL to keep current)
 * @param provider    Active provider name (or NULL to keep current)
 * @param iteration   Current iteration number (-1 to skip)
 */
void hermes_log_set_context(const char *session_id, const char *model,
                             const char *provider, int iteration);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_LOGGER_H */
