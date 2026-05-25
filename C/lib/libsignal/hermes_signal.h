/*
 * signal.h — C signal handling helpers (J13: Python signal port).
 *
 * Provides safe signal registration and default handlers.
 * Uses standard C signal() — no POSIX sigaction dependency.
 */

#ifndef HERMES_SIGNAL_H
#define HERMES_SIGNAL_H

/* Must define before any includes for POSIX signal constants */
#define _POSIX_C_SOURCE 199309L

#include <stdbool.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max signal handlers we track */
#define SIGNAL_MAX_HANDLERS 16

/** Signal handler function type */
typedef void (*signal_handler_t)(int signum);

/**
 * Register a handler for the given signal using standard signal().
 * Returns true on success.
 */
bool signal_on(int signum, signal_handler_t handler);

/**
 * Restore the default handler (SIG_DFL) for the given signal.
 */
bool signal_default(int signum);

/**
 * Register common cleanup handlers: SIGINT, SIGTERM.
 * When caught, sets *stop_flag to 1 via the callback.
 * Returns true on success.
 */
bool signal_register_common(volatile int *stop_flag);

/**
 * Signal-safe write to stderr (for use in signal handlers).
 * Uses write() directly — no malloc, no printf.
 */
void signal_safe_write(const char *msg);

/**
 * Default handler: prints signal name to stderr, calls _exit(128+signum).
 * Safe to use as signal handler.
 */
void signal_default_handler(int signum);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SIGNAL_H */
