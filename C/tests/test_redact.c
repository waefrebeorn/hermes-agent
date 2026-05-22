/* Test redact subsystem */
#include "hermes.h"
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

int main(void) {
    printf("=== Redact Tests ===\n");

    char *result;

    /* NULL/empty/plain text */
    TEST("NULL input returns NULL", hermes_redact(NULL) == NULL);
    result = hermes_redact("");
    TEST("empty string returns empty", result && result[0] == '\0'); free(result);
    result = hermes_redact("Hello, this is a normal message.");
    TEST("plain text unchanged", result && strcmp(result, "Hello, this is a normal message.") == 0); free(result);

    /* api_key in key=value format */
    result = hermes_redact("api_key=sk-abc123def456ghi");
    TEST("api_key redacted", result && strstr(result, "***REDACTED***") != NULL); free(result);

    /* Short value below min_len not redacted */
    result = hermes_redact("api_key=ab");
    TEST("short api_key not redacted", result && strstr(result, "ab") != NULL); free(result);

    /* sk- OpenAI key in key=value context */
    result = hermes_redact("api_key=sk-proj-abcdefghijklmnopqrstuvwxyz0123456789");
    TEST("sk- key in key=value redacted", result && strstr(result, "***REDACTED***") != NULL); free(result);

    /* GitHub PAT in key=value */
    result = hermes_redact("token=ghp_abc123def456ghi789jkl012mno345pqr678stu");
    TEST("ghp_ in key=value redacted", result && strstr(result, "***REDACTED***") != NULL); free(result);

    /* token= pattern */
    result = hermes_redact("token=super-secret-token-value-here");
    TEST("token= value redacted", result && strstr(result, "***REDACTED***") != NULL); free(result);

    /* password= pattern */
    result = hermes_redact("password=mypassword123");
    TEST("password= redacted", result && strstr(result, "***REDACTED***") != NULL); free(result);

    /* Authorization: Bearer + JWT */
    result = hermes_redact("Authorization: Bearer eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.signature");
    TEST("Bearer JWT redacted", result && strstr(result, "REDACTED") != NULL); free(result);

    /* Multiple secrets */
    result = hermes_redact("api_key=abc123456789 and token=def456789012 and normal text");
    TEST("multiple secrets: api_key value redacted", result && strstr(result, "abc123") == NULL); free(result);

    /* Large input safely handled */
    char big[70000];
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    memcpy(big, "api_key=sk-test", 15);
    result = hermes_redact(big);
    TEST("large input no crash", result != NULL);
    TEST("large input truncates", result && strlen(result) <= 65536); free(result);

    /* Custom pattern lifecycle */
    hermes_redact_clear_patterns();

    bool added = hermes_redact_add_pattern("mycustom:4:2");
    TEST("add custom pattern succeeds", added == true);

    result = hermes_redact("mycustom=abcdefgh");
    TEST("custom pattern redacts", result && strstr(result, "***REDACTED***") != NULL); free(result);

    /* Clear custom, built-in api_key still works */
    hermes_redact_clear_patterns();
    result = hermes_redact("api_key=abcdefghijklmnop");
    TEST("built-in api_key works after custom clear", result && strstr(result, "***REDACTED***") != NULL); free(result);

    /* Error handling */
    TEST("NULL pattern returns false", hermes_redact_add_pattern(NULL) == false);

    printf("\n=== %d/%d passed, %d failed ===\n", pass, pass + fail, fail);
    return fail > 0 ? 1 : 0;
}
