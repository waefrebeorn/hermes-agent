#ifndef HERMES_ERROR_H
#define HERMES_ERROR_H

/*
 * hermes_error.h — Typed error hierarchy for Hermes C.
 *
 * K01-K05: Provides structured error types replacing bare bool returns
 * and fprintf(stderr) with typed, capturable errors that carry type,
 * message, source location, and optional system errno.
 *
 * Python equivalent: ValueError, TypeError, RuntimeError,
 * OSError/FileNotFoundError, TimeoutError with tracebacks.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  K01-K05: Error types
 * ================================================================ */

typedef enum {
    HERMES_OK          = 0,    /* Not an error — success indicator */
    HERMES_ERR_VALUE   = 1,    /* K01: ValueError — invalid argument */
    HERMES_ERR_TYPE    = 2,    /* K02: TypeError — type mismatch */
    HERMES_ERR_RUNTIME = 3,    /* K03: RuntimeError — general failure */
    HERMES_ERR_OS      = 4,    /* K04: OSError — filesystem/OS failure */
    HERMES_ERR_TIMEOUT = 5,    /* K05: TimeoutError — operation timed out */
    HERMES_ERR_CUSTOM  = 6     /* Extension point for plugin-specific errors */
} hermes_error_type_t;

/* ================================================================
 *  Error struct — compact (stack-friendly), ~1152 bytes
 * ================================================================ */

#define HERMES_ERR_MSG_MAX  768
#define HERMES_ERR_FILE_MAX 256

typedef struct {
    hermes_error_type_t type;
    int                 code;          /* errno or HTTP status code */
    char                message[HERMES_ERR_MSG_MAX];
    char                file[HERMES_ERR_FILE_MAX];
    int                 line;
} hermes_error_t;

/* ================================================================
 *  Construction helpers
 * ================================================================ */

/* Create a typed error with printf-style format message.
 * Returns err (or a static singleton if err is NULL) for chaining.
 * Stack-allocated: pass &local_err to fill an existing struct. */
hermes_error_t *hermes_error_set(hermes_error_t *err,
                                  hermes_error_type_t type,
                                  const char *file, int line,
                                  const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

/* Convenience: create a ValueError */
#define hermes_err_value(err, ...) \
    hermes_error_set((err), HERMES_ERR_VALUE, __FILE__, __LINE__, __VA_ARGS__)

/* Convenience: create a TypeError */
#define hermes_err_type(err, ...) \
    hermes_error_set((err), HERMES_ERR_TYPE, __FILE__, __LINE__, __VA_ARGS__)

/* Convenience: create a RuntimeError */
#define hermes_err_runtime(err, ...) \
    hermes_error_set((err), HERMES_ERR_RUNTIME, __FILE__, __LINE__, __VA_ARGS__)

/* Convenience: create an OSError capturing errno */
#define hermes_err_os(err, ...) \
    hermes_error_set((err), HERMES_ERR_OS, __FILE__, __LINE__, __VA_ARGS__)

/* Convenience: create a TimeoutError */
#define hermes_err_timeout(err, ...) \
    hermes_error_set((err), HERMES_ERR_TIMEOUT, __FILE__, __LINE__, __VA_ARGS__)

/* Create an OSError from the current errno value.
 * Sets code = errno and appends strerror(errno) to message. */
hermes_error_t *hermes_error_from_errno(hermes_error_t *err,
                                         const char *file, int line,
                                         const char *context);

#define hermes_errno(err, ctx) \
    hermes_error_from_errno((err), __FILE__, __LINE__, (ctx))

/* ================================================================
 *  Query / formatting
 * ================================================================ */

/* Human-readable type name (static string, no free needed) */
const char *hermes_error_type_name(hermes_error_type_t type);

/* Format error as string into buffer. Returns buf.
 * Format: "[ValueError] in file.c:42: message (code=22)" */
char *hermes_error_string(const hermes_error_t *err,
                           char *buf, size_t buf_size);

/* Quick check: is this an error? (type != HERMES_OK) */
static inline bool hermes_is_error(const hermes_error_t *err) {
    return err && err->type != HERMES_OK;
}

/* Quick check: is this particular type of error? */
static inline bool hermes_error_is_type(const hermes_error_t *err,
                                         hermes_error_type_t type) {
    return err && err->type == type;
}

/* ================================================================
 *  Thread-local last error (Python-style errno for hermes)
 * ================================================================ */

/* Get the thread-local last error. Returns NULL if no error set. */
hermes_error_t *hermes_get_last_error(void);

/* Set the thread-local last error. Returns err. */
hermes_error_t *hermes_set_last_error(const hermes_error_t *err);

/* Clear the thread-local last error. */
void hermes_clear_last_error(void);

/* Convenience: set last error from type + message */
#define hermes_set_error(type, ...) do { \
    hermes_error_t _e; \
    hermes_error_set(&_e, (type), __FILE__, __LINE__, __VA_ARGS__); \
    hermes_set_last_error(&_e); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ERROR_H */
