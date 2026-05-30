/* test_secrets.c — Secrets resolution: resolve_string parsing (no bws dependency). */
#include "hermes_secrets.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

/* Forward decl of the function we're testing */
extern char *hermes_secrets_resolve_string(const char *input, bool strict);

int main(void) {
    printf("=== Secrets Resolution Tests ===\n\n");

    /* --- resolve_string with no BSM refs --- */
    printf("--- Plain text passthrough ---\n");
    char *result = hermes_secrets_resolve_string("hello world", false);
    printf("  result='%s'\n", result ? result : "NULL");
    TEST("plain text passthrough", result && strcmp(result, "hello world") == 0);
    free(result);

    result = hermes_secrets_resolve_string("", false);
    TEST("empty string", result && strcmp(result, "") == 0);
    free(result);

    result = hermes_secrets_resolve_string(NULL, false);
    TEST("NULL input", result && strcmp(result, "") == 0);
    free(result);

    /* --- resolve_string with BSM refs (unresolvable, non-strict) --- */
    printf("\n--- BSM refs passthrough (non-strict, no bws) ---\n");
    result = hermes_secrets_resolve_string("${BSM:api-key}", false);
    TEST("BSM ref passthrough", result && strstr(result, "${BSM:api-key}") != NULL);
    free(result);

    result = hermes_secrets_resolve_string("prefix ${BSM:key} suffix", false);
    TEST("BSM ref with context", result && strstr(result, "prefix ") != NULL && strstr(result, " suffix") != NULL);
    free(result);

    result = hermes_secrets_resolve_string("${BSM:a}${BSM:b}", false);
    TEST("multiple BSM refs", result && strstr(result, "${BSM:a}") != NULL && strstr(result, "${BSM:b}") != NULL);
    free(result);

    /* --- resolve_string with BSM refs (strict mode) --- */
    printf("\n--- BSM refs strict mode (no bws → NULL) ---\n");
    result = hermes_secrets_resolve_string("${BSM:api-key}", true);
    TEST("strict mode returns NULL on unresolvable", result == NULL);

    result = hermes_secrets_resolve_string("prefix ${BSM:key} suffix", true);
    TEST("strict mode with context returns NULL", result == NULL);

    /* Mixed: non-strict when no refs should work in strict too */
    result = hermes_secrets_resolve_string("no refs here", true);
    TEST("strict mode no refs passthrough", result && strcmp(result, "no refs here") == 0);
    free(result);

    /* --- Malformed refs --- */
    printf("\n--- Malformed refs ---\n");
    result = hermes_secrets_resolve_string("${BSM:unclosed", false);
    TEST("unclosed brace passthrough", result && strstr(result, "${BSM:unclosed") != NULL);
    free(result);

    result = hermes_secrets_resolve_string("${NOT_BSM:key}", false);
    TEST("non-BSM ref passthrough", result && strcmp(result, "${NOT_BSM:key}") == 0);
    free(result);

    /* --- Additional edge cases --- */
    printf("\n--- Edge cases ---\n");

    /* Empty BSM key */
    result = hermes_secrets_resolve_string("${BSM:}", false);
    TEST("empty BSM key passthrough", result && strcmp(result, "${BSM:}") == 0);
    free(result);

    /* BSM key with spaces */
    result = hermes_secrets_resolve_string("${BSM:my key}", false);
    TEST("BSM key with space passthrough", result && strstr(result, "${BSM:my key}") != NULL);
    free(result);

    /* BSM ref at very start of string */
    result = hermes_secrets_resolve_string("${BSM:start} tail", false);
    TEST("BSM ref at start", result && strstr(result, "${BSM:start}") != NULL && strstr(result, " tail") != NULL);
    free(result);

    /* BSM ref at very end of string */
    result = hermes_secrets_resolve_string("head ${BSM:end}", false);
    TEST("BSM ref at end", result && strstr(result, "head ") != NULL && strstr(result, "${BSM:end}") != NULL);
    free(result);

    /* Only BSM prefix, no colon */
    result = hermes_secrets_resolve_string("${BSM}", false);
    TEST("BSM without colon passthrough", result && strcmp(result, "${BSM}") == 0);
    free(result);

    /* Very long normal input */
    {
        char long_input[2048];
        memset(long_input, 'x', 2000);
        long_input[2000] = '\0';
        result = hermes_secrets_resolve_string(long_input, false);
        TEST("very long input passthrough", result && strlen(result) == 2000);
        free(result);
    }

    /* Very long input with BSM ref */
    {
        char long_input[2048];
        memset(long_input, 'x', 1000);
        memcpy(long_input + 1000, "${BSM:secret}", 12);
        memset(long_input + 1012, 'y', 988);
        long_input[2000] = '\0';
        result = hermes_secrets_resolve_string(long_input, false);
        TEST("long input with BSM ref preserves length", result && strlen(result) >= 2000);
        free(result);
    }

    /* Null in strict mode returns empty string (same as non-strict) */
    result = hermes_secrets_resolve_string(NULL, true);
    TEST("NULL strict mode returns empty", result && strcmp(result, "") == 0);
    free(result);

    /* Empty string strict mode */
    result = hermes_secrets_resolve_string("", true);
    TEST("empty strict mode passthrough", result && strcmp(result, "") == 0);
    free(result);

    /* BSM key with special characters */
    result = hermes_secrets_resolve_string("${BSM:my-key_1.0}", false);
    TEST("BSM special chars key passthrough", result && strstr(result, "${BSM:my-key_1.0}") != NULL);
    free(result);

    /* Multiple non-adjacent BSM refs with varying spacing */
    result = hermes_secrets_resolve_string("a${BSM:x}b${BSM:y}c", false);
    TEST("BSM refs separated by single chars",
         result && strstr(result, "${BSM:x}") != NULL && strstr(result, "${BSM:y}") != NULL);
    free(result);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
