/*
 * hermes_error.c — Typed error system implementation.
 *
 * K01-K05: Error code names, formatting, and utilities.
 */
#include "hermes_error.h"
#include <string.h>
#include <stdio.h>

const char *hermes_error_name(hermes_error_code_t code) {
    switch (code) {
        case HERMES_OK:               return "OK";
        case HERMES_ERR_VALUE:        return "ValueError";
        case HERMES_ERR_OUT_OF_RANGE: return "OutOfRangeError";
        case HERMES_ERR_INVALID_CONFIG: return "InvalidConfigError";
        case HERMES_ERR_TYPE:         return "TypeError";
        case HERMES_ERR_UNEXPECTED_TYPE: return "UnexpectedTypeError";
        case HERMES_ERR_RUNTIME:      return "RuntimeError";
        case HERMES_ERR_NOT_FOUND:    return "NotFoundError";
        case HERMES_ERR_ALREADY_EXISTS: return "AlreadyExistsError";
        case HERMES_ERR_NOT_IMPLEMENTED: return "NotImplementedError";
        case HERMES_ERR_BUSY:         return "BusyError";
        case HERMES_ERR_IO:           return "IOError";
        case HERMES_ERR_FILE_NOT_FOUND: return "FileNotFoundError";
        case HERMES_ERR_PERMISSION:   return "PermissionError";
        case HERMES_ERR_DISK_FULL:    return "DiskFullError";
        case HERMES_ERR_TIMEOUT:      return "TimeoutError";
        case HERMES_ERR_NETWORK_TIMEOUT: return "NetworkTimeoutError";
        case HERMES_ERR_LLM_TIMEOUT:  return "LLMTimeoutError";
        default:                      return "UnknownError";
    }
}

void hermes_error_format(const hermes_error_t *e, char *buf, size_t buf_size) {
    if (!e || !buf || buf_size == 0) return;
    if (e->code == HERMES_OK) {
        snprintf(buf, buf_size, "OK");
        return;
    }
    if (e->context[0])
        snprintf(buf, buf_size, "[%s] %s: %s", hermes_error_name(e->code), e->context, e->message);
    else
        snprintf(buf, buf_size, "%s: %s", hermes_error_name(e->code), e->message);
}
