/*
 * test_hermes_error.c — K01-K05: Typed error system tests.
 *
 * Tests: error codes, names, creation, formatting, ok/failed helpers.
 */
#include "hermes_error.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

int main(void) {
    printf("=== K01-K05: Typed error system ===\n\n");

    /* HERMES_OK */
    {
        hermes_error_t e = hermes_error(HERMES_OK, NULL);
        TEST("ok returns success", hermes_ok(e));
        TEST("ok not failed", !hermes_failed(e));
        char buf[128];
        hermes_error_format(&e, buf, sizeof(buf));
        TEST("ok formats as OK", strcmp(buf, "OK") == 0);
    }

    /* K01: ValueError */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_VALUE, "max_tokens must be positive");
        TEST("value error code", e.code == HERMES_ERR_VALUE);
        TEST("value error message", strcmp(e.message, "max_tokens must be positive") == 0);
        TEST("value error failed", hermes_failed(e));
        TEST("value error not ok", !hermes_ok(e));
        char buf[128];
        hermes_error_format(&e, buf, sizeof(buf));
        TEST("value error format", strstr(buf, "ValueError") && strstr(buf, "max_tokens"));
    }

    /* K02: TypeError */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_TYPE, "expected string, got number");
        TEST("type error code", e.code == HERMES_ERR_TYPE);
        TEST("type error name", strcmp(hermes_error_name(HERMES_ERR_TYPE), "TypeError") == 0);
    }

    /* K03: RuntimeError */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_RUNTIME, "provider connection failed");
        TEST("runtime error name", strcmp(hermes_error_name(HERMES_ERR_RUNTIME), "RuntimeError") == 0);
    }

    /* K03: NotFound */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_NOT_FOUND, "session not found");
        TEST("not found name", strcmp(hermes_error_name(HERMES_ERR_NOT_FOUND), "NotFoundError") == 0);
    }

    /* K04: FileNotFoundError */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_FILE_NOT_FOUND, "/etc/config.yaml");
        TEST("file not found name", strcmp(hermes_error_name(HERMES_ERR_FILE_NOT_FOUND), "FileNotFoundError") == 0);
        TEST("file not found message", strcmp(e.message, "/etc/config.yaml") == 0);
    }

    /* K04: PermissionError */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_PERMISSION, "access denied");
        TEST("permission name", strcmp(hermes_error_name(HERMES_ERR_PERMISSION), "PermissionError") == 0);
    }

    /* K05: TimeoutError */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_TIMEOUT, "request timed out");
        TEST("timeout name", strcmp(hermes_error_name(HERMES_ERR_TIMEOUT), "TimeoutError") == 0);
    }

    /* K05: LLMTimeout */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_LLM_TIMEOUT, "LLM did not respond within 30s");
        TEST("K05 LLMTimeoutError name", strcmp(hermes_error_name(HERMES_ERR_LLM_TIMEOUT), "LLMTimeoutError") == 0);
    }

    /* K06-K20: Extended error types */
    TEST("K06 ConfigError name", strcmp(hermes_error_name(HERMES_ERR_CONFIG), "ConfigError") == 0);
    TEST("K06 MissingKeyError name", strcmp(hermes_error_name(HERMES_ERR_MISSING_KEY), "MissingKeyError") == 0);
    TEST("K07 ConnectionError name", strcmp(hermes_error_name(HERMES_ERR_CONNECTION), "ConnectionError") == 0);
    TEST("K07 ConnectionRefusedError name", strcmp(hermes_error_name(HERMES_ERR_CONN_REFUSED), "ConnectionRefusedError") == 0);
    TEST("K07 ConnectionResetError name", strcmp(hermes_error_name(HERMES_ERR_CONN_RESET), "ConnectionResetError") == 0);
    TEST("K08 AuthenticationError name", strcmp(hermes_error_name(HERMES_ERR_AUTH), "AuthenticationError") == 0);
    TEST("K08 InvalidKeyError name", strcmp(hermes_error_name(HERMES_ERR_INVALID_KEY), "InvalidKeyError") == 0);
    TEST("K08 ExpiredKeyError name", strcmp(hermes_error_name(HERMES_ERR_EXPIRED_KEY), "ExpiredKeyError") == 0);
    TEST("K09 ForbiddenError name", strcmp(hermes_error_name(HERMES_ERR_FORBIDDEN), "ForbiddenError") == 0);
    TEST("K09 RateLimitedError name", strcmp(hermes_error_name(HERMES_ERR_RATE_LIMITED), "RateLimitedError") == 0);
    TEST("K10 ValidationError name", strcmp(hermes_error_name(HERMES_ERR_VALIDATION), "ValidationError") == 0);
    TEST("K11 QuotaError name", strcmp(hermes_error_name(HERMES_ERR_QUOTA), "QuotaError") == 0);
    TEST("K12 ModelError name", strcmp(hermes_error_name(HERMES_ERR_MODEL), "ModelError") == 0);
    TEST("K13 ToolError name", strcmp(hermes_error_name(HERMES_ERR_TOOL), "ToolError") == 0);
    TEST("K14 PluginError name", strcmp(hermes_error_name(HERMES_ERR_PLUGIN), "PluginError") == 0);
    TEST("K15 GatewayError name", strcmp(hermes_error_name(HERMES_ERR_GATEWAY), "GatewayError") == 0);
    TEST("K16 SessionError name", strcmp(hermes_error_name(HERMES_ERR_SESSION), "SessionError") == 0);
    TEST("K17 SerializationError name", strcmp(hermes_error_name(HERMES_ERR_SERIALIZE), "SerializationError") == 0);
    TEST("K18 InternalError name", strcmp(hermes_error_name(HERMES_ERR_INTERNAL), "InternalError") == 0);
    TEST("K19 AbortError name", strcmp(hermes_error_name(HERMES_ERR_ABORT), "AbortError") == 0);
    TEST("K20 ProtocolError name", strcmp(hermes_error_name(HERMES_ERR_PROTOCOL), "ProtocolError") == 0);

    /* Error creation with context */
    {
        hermes_error_t e = hermes_error_ctx(HERMES_ERR_OUT_OF_RANGE, "temperature 2.5", "config.c:847");
        TEST("context stored", strcmp(e.context, "config.c:847") == 0);
        char buf[128];
        hermes_error_format(&e, buf, sizeof(buf));
        TEST("context in format", strstr(buf, "config.c:847") != NULL);
    }

    /* NotImplemented */
    {
        hermes_error_t e = hermes_error(HERMES_ERR_NOT_IMPLEMENTED, "CDP auto-launch not implemented");
        TEST("not impl name", strcmp(hermes_error_name(HERMES_ERR_NOT_IMPLEMENTED), "NotImplementedError") == 0);
    }

    /* Null safety */
    {
        hermes_error_t e = hermes_error(HERMES_OK, NULL);
        TEST("null message ok", hermes_ok(e));
        e = hermes_error_ctx(HERMES_ERR_RUNTIME, NULL, NULL);
        TEST("null msg+ctx ok", e.message[0] == '\0' && e.context[0] == '\0');
        /* format with NULL/empty — should not crash */
        char buf[4];
        hermes_error_format(NULL, buf, sizeof(buf));
        hermes_error_format(&e, buf, 0);
        TEST("null/zero format no crash", 1);
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
