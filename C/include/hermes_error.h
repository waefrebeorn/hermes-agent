/*
 * hermes_error.h — Typed error system for Hermes C.
 *
 * K01-K05: Structured error types with code, message, and context.
 * Replaces bare "return NULL" / "fprintf(stderr, ...)" patterns.
 */
/**
 * @defgroup hermes_error Error Types
 * @brief Typed error system (K01-K05).
 *
 * 
ValueError, TypeError, RuntimeError, OSError,
TimeoutError with structured error info.
 *
 * @{
 */
#ifndef HERMES_ERROR_H
#define HERMES_ERROR_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Error Codes (K01-K05)
 * ================================================================ */
typedef enum {
    HERMES_OK = 0,

    /* K01: Value errors — invalid arguments, config values */
    HERMES_ERR_VALUE,           /* General invalid argument */
    HERMES_ERR_OUT_OF_RANGE,    /* Value outside acceptable range */
    HERMES_ERR_INVALID_CONFIG,  /* Config validation failure */

    /* K02: Type errors — type mismatches */
    HERMES_ERR_TYPE,            /* General type mismatch */
    HERMES_ERR_UNEXPECTED_TYPE, /* Expected different JSON/YAML type */

    /* K03: Runtime errors */
    HERMES_ERR_RUNTIME,         /* General runtime failure */
    HERMES_ERR_NOT_FOUND,       /* Resource/session/entity not found */
    HERMES_ERR_ALREADY_EXISTS,  /* Duplicate creation */
    HERMES_ERR_NOT_IMPLEMENTED, /* Stub/unimplemented feature */
    HERMES_ERR_BUSY,            /* Resource in use */

    /* K04: I/O errors */
    HERMES_ERR_IO,              /* General I/O failure */
    HERMES_ERR_FILE_NOT_FOUND,  /* File does not exist */
    HERMES_ERR_PERMISSION,      /* Permission denied */
    HERMES_ERR_DISK_FULL,       /* No space left */

    /* K05: Timeout errors */
    HERMES_ERR_TIMEOUT,         /* General timeout */
    HERMES_ERR_NETWORK_TIMEOUT, /* Network request timed out */
    HERMES_ERR_LLM_TIMEOUT,     /* LLM response timed out */
} hermes_error_code_t;

/* Human-readable error code names */
const char *hermes_error_name(hermes_error_code_t code);

/* ================================================================
 *  Error Result (K01-K05)
 * ================================================================ */
typedef struct {
    hermes_error_code_t code;  /* Error code */
    char message[1024];        /* Human-readable error description */
    char context[256];         /* Context: function name, file:line, or tag */
} hermes_error_t;

/* Create a simple error */
static inline hermes_error_t hermes_error(hermes_error_code_t code, const char *msg) {
    hermes_error_t e = {code, {0}, {0}};
    if (msg) {
        size_t len = strlen(msg);
        if (len >= sizeof(e.message)) len = sizeof(e.message) - 1;
        memcpy(e.message, msg, len);
        e.message[len] = '\0';
    }
    return e;
}

/* Create an error with context tag */
static inline hermes_error_t hermes_error_ctx(hermes_error_code_t code, const char *msg, const char *ctx) {
    hermes_error_t e = hermes_error(code, msg);
    if (ctx) {
        size_t len = strlen(ctx);
        if (len >= sizeof(e.context)) len = sizeof(e.context) - 1;
        memcpy(e.context, ctx, len);
        e.context[len] = '\0';
    }
    return e;
}

/* Check if error indicates success */
static inline bool hermes_ok(hermes_error_t e) { return e.code == HERMES_OK; }

/* Check if error indicates failure */
static inline bool hermes_failed(hermes_error_t e) { return e.code != HERMES_OK; }

/* Format error as string (truncated to buf) */
void hermes_error_format(const hermes_error_t *e, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of hermes_error group */
#endif /* HERMES_ERROR_H */
