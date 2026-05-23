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
        case HERMES_ERR_CONFIG:       return "ConfigError";
        case HERMES_ERR_MISSING_KEY:  return "MissingKeyError";
        case HERMES_ERR_CONNECTION:   return "ConnectionError";
        case HERMES_ERR_CONN_REFUSED: return "ConnectionRefusedError";
        case HERMES_ERR_CONN_RESET:   return "ConnectionResetError";
        case HERMES_ERR_AUTH:         return "AuthenticationError";
        case HERMES_ERR_INVALID_KEY:  return "InvalidKeyError";
        case HERMES_ERR_EXPIRED_KEY:  return "ExpiredKeyError";
        case HERMES_ERR_FORBIDDEN:    return "ForbiddenError";
        case HERMES_ERR_RATE_LIMITED: return "RateLimitedError";
        case HERMES_ERR_VALIDATION:   return "ValidationError";
        case HERMES_ERR_INVALID_FORMAT: return "InvalidFormatError";
        case HERMES_ERR_QUOTA:        return "QuotaError";
        case HERMES_ERR_QUOTA_TOKENS: return "TokenQuotaError";
        case HERMES_ERR_QUOTA_REQUESTS: return "RequestQuotaError";
        case HERMES_ERR_MODEL:        return "ModelError";
        case HERMES_ERR_MODEL_UNAVAIL: return "ModelUnavailableError";
        case HERMES_ERR_CONTEXT_LEN:  return "ContextLengthError";
        case HERMES_ERR_TOOL:         return "ToolError";
        case HERMES_ERR_TOOL_NOT_FOUND: return "ToolNotFoundError";
        case HERMES_ERR_TOOL_TIMEOUT: return "ToolTimeoutError";
        case HERMES_ERR_PLUGIN:       return "PluginError";
        case HERMES_ERR_PLUGIN_LOAD:  return "PluginLoadError";
        case HERMES_ERR_PLUGIN_INIT:  return "PluginInitError";
        case HERMES_ERR_GATEWAY:      return "GatewayError";
        case HERMES_ERR_GATEWAY_SEND: return "GatewaySendError";
        case HERMES_ERR_PLATFORM:     return "PlatformError";
        case HERMES_ERR_SESSION:      return "SessionError";
        case HERMES_ERR_SESSION_LOST: return "SessionLostError";
        case HERMES_ERR_SERIALIZE:    return "SerializationError";
        case HERMES_ERR_DESERIALIZE:  return "DeserializationError";
        case HERMES_ERR_ENCODE:       return "EncodingError";
        case HERMES_ERR_INTERNAL:     return "InternalError";
        case HERMES_ERR_ASSERTION:    return "AssertionError";
        case HERMES_ERR_MEMORY:       return "MemoryError";
        case HERMES_ERR_ABORT:        return "AbortError";
        case HERMES_ERR_CANCELLED:    return "CancelledError";
        case HERMES_ERR_INTERRUPTED:  return "InterruptedError";
        case HERMES_ERR_STREAM:       return "StreamError";
        case HERMES_ERR_PIPE:         return "PipeError";
        case HERMES_ERR_PROTOCOL:     return "ProtocolError";
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
