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

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
