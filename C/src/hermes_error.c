/*
 * hermes_error.c — Typed error hierarchy for Hermes C.
 *
 * K01-K05: Implements ValueError, TypeError, RuntimeError,
 * OSError/FileNotFoundError, TimeoutError with source location
 * tracking and thread-local storage for last-error access.
 */

#include "hermes_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>

/* ================================================================
 *  Type name table
 * ================================================================ */

static const char * const ERROR_TYPE_NAMES[] = {
    [HERMES_OK]          = "Ok",
    [HERMES_ERR_VALUE]   = "ValueError",
    [HERMES_ERR_TYPE]    = "TypeError",
    [HERMES_ERR_RUNTIME] = "RuntimeError",
    [HERMES_ERR_OS]      = "OSError",
    [HERMES_ERR_TIMEOUT] = "TimeoutError",
    [HERMES_ERR_CUSTOM]  = "CustomError"
};

#define ERROR_TYPE_COUNT (sizeof(ERROR_TYPE_NAMES) / sizeof(ERROR_TYPE_NAMES[0]))

const char *hermes_error_type_name(hermes_error_type_t type) {
    if (type < 0 || (size_t)type >= ERROR_TYPE_COUNT)
        return "UnknownError";
    return ERROR_TYPE_NAMES[type];
}

/* ================================================================
 *  Construction
 * ================================================================ */

/* Static zero-initialized OK error for default returns */
static const hermes_error_t HERMES_ERROR_OK = { HERMES_OK, 0, "", "", 0 };

hermes_error_t *hermes_error_set(hermes_error_t *err,
                                  hermes_error_type_t type,
                                  const char *file, int line,
                                  const char *fmt, ...)
{
    if (!err) {
        /* Return pointer to static OK on NULL input */
        static hermes_error_t fallback;
        if (type == HERMES_OK) {
            fallback = HERMES_ERROR_OK;
            return &fallback;
        }
        /* For non-NULL err with NULL err pointer, use static */
        err = &fallback;
    }

    err->type = type;
    err->code = 0;

    if (file) {
        snprintf(err->file, sizeof(err->file), "%s", file);
    } else {
        err->file[0] = '\0';
    }
    err->line = line;

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(err->message, sizeof(err->message), fmt, args);
        va_end(args);
    } else {
        err->message[0] = '\0';
    }

    return err;
}

hermes_error_t *hermes_error_from_errno(hermes_error_t *err,
                                         const char *file, int line,
                                         const char *context)
{
    int saved_errno = errno;

    if (!err) {
        static hermes_error_t fallback;
        err = &fallback;
    }

    err->type = HERMES_ERR_OS;
    err->code = saved_errno;
    err->line = line;

    if (file) {
        snprintf(err->file, sizeof(err->file), "%s", file);
    } else {
        err->file[0] = '\0';
    }

    if (context && context[0]) {
        snprintf(err->message, sizeof(err->message),
                 "%s: %s", context, strerror(saved_errno));
    } else {
        snprintf(err->message, sizeof(err->message),
                 "%s", strerror(saved_errno));
    }

    return err;
}

/* ================================================================
 *  Formatting
 * ================================================================ */

char *hermes_error_string(const hermes_error_t *err,
                           char *buf, size_t buf_size)
{
    if (!err || !buf || buf_size == 0)
        return buf;

    if (err->type == HERMES_OK) {
        snprintf(buf, buf_size, "Ok");
        return buf;
    }

    if (err->file[0] && err->line > 0) {
        if (err->code != 0) {
            snprintf(buf, buf_size, "[%s] in %s:%d: %s (code=%d)",
                     hermes_error_type_name(err->type),
                     err->file, err->line,
                     err->message, err->code);
        } else {
            snprintf(buf, buf_size, "[%s] in %s:%d: %s",
                     hermes_error_type_name(err->type),
                     err->file, err->line,
                     err->message);
        }
    } else {
        if (err->code != 0) {
            snprintf(buf, buf_size, "[%s]: %s (code=%d)",
                     hermes_error_type_name(err->type),
                     err->message, err->code);
        } else {
            snprintf(buf, buf_size, "[%s]: %s",
                     hermes_error_type_name(err->type),
                     err->message);
        }
    }

    return buf;
}

/* ================================================================
 *  Thread-local last error
 * ================================================================ */

static pthread_key_t   g_err_key;
static pthread_once_t  g_err_key_once = PTHREAD_ONCE_INIT;

static void err_key_destroy(void *data) {
    hermes_error_t *err = (hermes_error_t *)data;
    free(err);
}

static void err_key_init(void) {
    pthread_key_create(&g_err_key, err_key_destroy);
}

hermes_error_t *hermes_get_last_error(void) {
    pthread_once(&g_err_key_once, err_key_init);
    hermes_error_t *err = (hermes_error_t *)pthread_getspecific(g_err_key);
    if (!err) return NULL;
    if (err->type == HERMES_OK) return NULL;
    return err;
}

hermes_error_t *hermes_set_last_error(const hermes_error_t *src) {
    pthread_once(&g_err_key_once, err_key_init);

    hermes_error_t *err = (hermes_error_t *)pthread_getspecific(g_err_key);
    if (!err) {
        err = (hermes_error_t *)calloc(1, sizeof(hermes_error_t));
        if (!err) return NULL;
        pthread_setspecific(g_err_key, err);
    }

    if (src) {
        *err = *src;
    } else {
        err->type = HERMES_OK;
        err->code = 0;
        err->message[0] = '\0';
        err->file[0] = '\0';
        err->line = 0;
    }

    return err;
}

void hermes_clear_last_error(void) {
    hermes_error_t zero = HERMES_ERROR_OK;
    hermes_set_last_error(&zero);
}
