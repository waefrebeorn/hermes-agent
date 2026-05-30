/*
 * test_token_exchange.c — Token exchange unit tests.
 *
 * Tests: error state getter/setter, token free, auth store free.
 * These functions are pure logic with no network dependency.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_token_exchange.c src/provider/token_exchange.c \
 *       lib/libjson/json.c -o /tmp/hermes_test_token_exchange \
 *       -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_token_exchange
 */
#include "hermes.h"
#include "hermes_auth.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Functions under test */
const char *oauth_last_error(void);
void oauth_token_free(oauth_token_t *tok);
void auth_store_free(auth_entry_t *entries, int count);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Token Exchange Tests ===\n");

    /* Test 1: Initial error state is empty */
    {
        const char *err = oauth_last_error();
        TEST("initial error is NULL or empty", err == NULL || err[0] == '\0');
    }

    /* Test 2: oauth_token_free handles NULL gracefully */
    {
        oauth_token_free(NULL);
        TEST("oauth_token_free(NULL) no crash", 1);
    }

    /* Test 3: oauth_token_free handles allocated token */
    {
        oauth_token_t *tok = (oauth_token_t *)calloc(1, sizeof(oauth_token_t));
        TEST("allocated token not NULL", tok != NULL);
        oauth_token_free(tok);
        TEST("oauth_token_free(allocated) no crash", 1);
    }

    /* Test 4: oauth_token_free with populated fields */
    {
        oauth_token_t *tok = (oauth_token_t *)calloc(1, sizeof(oauth_token_t));
        if (tok) {
            tok->access_token = strdup("test_access_token");
            tok->refresh_token = strdup("test_refresh_token");
            tok->id_token = strdup("test_id_token");
            tok->token_type = strdup("Bearer");
            oauth_token_free(tok);
            TEST("oauth_token_free(populated) no crash", 1);
        } else {
            TEST("oauth_token_free(populated) skipped", 1);
        }
    }

    /* Test 5: auth_store_free handles NULL gracefully */
    {
        auth_store_free(NULL, 0);
        TEST("auth_store_free(NULL, 0) no crash", 1);
    }

    /* Test 6: auth_store_free handles empty array */
    {
        auth_store_free(NULL, 5);
        TEST("auth_store_free(NULL, 5) no crash (entries param NULL)", 1);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All token exchange tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
