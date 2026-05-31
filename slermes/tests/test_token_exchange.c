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

    /* Test 7: auth_store_free with one valid entry */
    {
        auth_entry_t *entry = (auth_entry_t *)calloc(1, sizeof(auth_entry_t));
        TEST("auth_store_free single entry allocated", entry != NULL);
        if (entry) {
            strncpy(entry->provider, "xai-oauth", sizeof(entry->provider) - 1);
            entry->token.access_token = strdup("test_at");
            entry->token.token_type = strdup("Bearer");
            entry->token.expires_in = 3600;
            auth_store_free(entry, 1);
            TEST("auth_store_free single entry no crash", 1);
        }
    }

    /* Test 8: auth_store_free with multiple valid entries */
    {
        auth_entry_t *entries = (auth_entry_t *)calloc(3, sizeof(auth_entry_t));
        TEST("auth_store_free multi entries allocated", entries != NULL);
        if (entries) {
            strncpy(entries[0].provider, "xai-oauth", sizeof(entries[0].provider) - 1);
            entries[0].token.access_token = strdup("tok1");
            entries[0].token.refresh_token = strdup("ref1");
            strncpy(entries[1].provider, "minimax", sizeof(entries[1].provider) - 1);
            entries[1].token.access_token = strdup("tok2");
            entries[1].token.token_type = strdup("Bearer");
            strncpy(entries[2].provider, "nous", sizeof(entries[2].provider) - 1);
            entries[2].token.id_token = strdup("id2");
            entries[2].token.expires_at = 1000000.0;
            auth_store_free(entries, 3);
            TEST("auth_store_free multi entries no crash", 1);
        }
    }

    /* Test 9: auth_store_free with count=0 frees pointer but not fields */
    {
        auth_entry_t *entry = (auth_entry_t *)calloc(1, sizeof(auth_entry_t));
        if (entry) {
            entry->token.access_token = strdup("should_not_free");
            /* auth_store_free(count=0) runs 0 iterations of the loop,
             * but still calls free(entries). Token fields leak by design. */
            auth_store_free(entry, 0);
            TEST("auth_store_free count=0 frees pointer no crash", 1);
        } else {
            TEST("auth_store_free count=0 skipped alloc", 1);
        }
    }

    /* Test 10: oauth_token_free with only access_token */
    {
        oauth_token_t *tok = (oauth_token_t *)calloc(1, sizeof(oauth_token_t));
        if (tok) {
            tok->access_token = strdup("only_access");
            oauth_token_free(tok);
            TEST("oauth_token_free only access_token no crash", 1);
        } else {
            TEST("oauth_token_free only access_token skipped", 1);
        }
    }

    /* Test 11: oauth_token_free with only refresh_token */
    {
        oauth_token_t *tok = (oauth_token_t *)calloc(1, sizeof(oauth_token_t));
        if (tok) {
            tok->refresh_token = strdup("only_refresh");
            oauth_token_free(tok);
            TEST("oauth_token_free only refresh_token no crash", 1);
        } else {
            TEST("oauth_token_free only refresh_token skipped", 1);
        }
    }

    /* Test 12: oauth_token_free with all empty strings */
    {
        oauth_token_t *tok = (oauth_token_t *)calloc(1, sizeof(oauth_token_t));
        if (tok) {
            tok->access_token = strdup("");
            tok->refresh_token = strdup("");
            tok->id_token = strdup("");
            tok->token_type = strdup("");
            oauth_token_free(tok);
            TEST("oauth_token_free all empty strings no crash", 1);
        } else {
            TEST("oauth_token_free all empty strings skipped", 1);
        }
    }

    /* Test 13: oauth_token_free with very long access_token */
    {
        oauth_token_t *tok = (oauth_token_t *)calloc(1, sizeof(oauth_token_t));
        if (tok) {
            size_t len = 500;
            char *long_str = (char *)malloc(len + 1);
            if (long_str) {
                memset(long_str, 'A', len);
                long_str[len] = '\0';
                tok->access_token = long_str;
                oauth_token_free(tok);
                TEST("oauth_token_free long access_token no crash", 1);
            } else {
                free(tok);
                TEST("oauth_token_free long access_token skipped", 1);
            }
        } else {
            TEST("oauth_token_free long access_token skipped", 1);
        }
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All token exchange tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
