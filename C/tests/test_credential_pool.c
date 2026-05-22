/*
 * test_credential_pool.c — Credential pool test suite (G157).
 *
 * Tests: create, add keys, round-robin rotation, rate limiting,
 * consecutive failures, cooloff, reset, stats JSON.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_credential_pool.c \
 *       src/agent/credential_pool.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_credpool -lm
 *
 * Run:
 *   /tmp/hermes_test_credpool
 */

#include "credential_pool.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

int main(void) {
    printf("=== Credential Pool Test Suite (G157) ===\n");

    /* ================================================================
     *  1. Create and basic operations
     * ================================================================ */
    printf("\n--- Create and add keys ---\n");

    credential_pool_t *pool = credential_pool_create("openai");
    TEST_NOT_NULL("pool created", pool);
    if (!pool) return 1;

    TEST_STR_EQ("provider name", pool->provider_name, "openai");
    TEST_INT_EQ("entry count starts 0", pool->entry_count, 0);
    TEST_INT_EQ("current index 0", pool->current_index, 0);
    TEST("no available keys", !credential_pool_has_available(pool));

    /* Add keys */
    int idx0 = credential_pool_add_key(pool, "sk-prod-key-1", "prod-1");
    TEST_INT_EQ("first key at index 0", idx0, 0);
    TEST_INT_EQ("entry count 1", pool->entry_count, 1);

    int idx1 = credential_pool_add_key(pool, "sk-backup-key-2", "backup");
    TEST_INT_EQ("second key at index 1", idx1, 1);
    TEST_INT_EQ("entry count 2", pool->entry_count, 2);

    int idx2 = credential_pool_add_key(pool, "sk-free-tier-3", NULL);
    TEST_INT_EQ("third key at index 2", idx2, 2);
    TEST_INT_EQ("entry count 3", pool->entry_count, 3);

    TEST("has available keys", credential_pool_has_available(pool));

    /* Verify key values */
    TEST_STR_EQ("key 0", pool->entries[0].api_key, "sk-prod-key-1");
    TEST_STR_EQ("key 0 label", pool->entries[0].label, "prod-1");
    TEST_STR_EQ("key 1 label", pool->entries[1].label, "backup");
    /* NULL label defaults to "key-N" */
    TEST_STR_EQ("key 2 label defaults to key-2", pool->entries[2].label, "key-2");

    /* Add beyond max should fail */
    for (int i = 3; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "extra-key-%d", i);
        credential_pool_add_key(pool, buf, "");
    }
    TEST_INT_EQ("entry count capped at MAX_KEYS", pool->entry_count, CREDENTIAL_POOL_MAX_KEYS);

    /* ================================================================
     *  2. Round-robin rotation
     * ================================================================ */
    printf("\n--- Round-robin ---\n");

    /* Re-create with 3 keys for clean state */
    credential_pool_free(pool);
    pool = credential_pool_create("openai");
    credential_pool_add_key(pool, "key-A", "A");
    credential_pool_add_key(pool, "key-B", "B");
    credential_pool_add_key(pool, "key-C", "C");

    /* Round-robin through keys */
    int out_idx = -1;
    const char *key;

    key = credential_pool_next_key(pool, &out_idx);
    TEST_NOT_NULL("first key is non-null", key);
    TEST_INT_EQ("first key index 0", out_idx, 0);
    TEST_STR_EQ("first key = key-A", key, "key-A");

    key = credential_pool_next_key(pool, &out_idx);
    TEST_INT_EQ("second key index 1", out_idx, 1);
    TEST_STR_EQ("second key = key-B", key, "key-B");

    key = credential_pool_next_key(pool, &out_idx);
    TEST_INT_EQ("third key index 2", out_idx, 2);
    TEST_STR_EQ("third key = key-C", key, "key-C");

    /* Should wrap around */
    key = credential_pool_next_key(pool, &out_idx);
    TEST_INT_EQ("wrap to index 0", out_idx, 0);
    TEST_STR_EQ("wrap key = key-A", key, "key-A");

    /* Without out_index */
    key = credential_pool_next_key(pool, NULL);
    TEST_NOT_NULL("key with NULL out_index", key);
    /* Index still advances internally */

    key = credential_pool_next_key(pool, &out_idx);
    TEST_NOT_NULL("key after NULL out_index", key);

    /* ================================================================
     *  3. Rate limit handling
     * ================================================================ */
    printf("\n--- Rate limiting ---\n");

    /* Re-create with 2 keys */
    credential_pool_free(pool);
    pool = credential_pool_create("openai");
    credential_pool_add_key(pool, "primary", "P");
    credential_pool_add_key(pool, "backup", "B");

    /* Report rate limit on key 0 */
    credential_status_t st = credential_pool_report(pool, 0, 429, 0, 0, time(NULL) + 60);
    TEST_INT_EQ("rate limited status", st, CRED_RATE_LIMITED);
    TEST_INT_EQ("entry 0 status after 429", pool->entries[0].status, CRED_RATE_LIMITED);

    /* Next key should skip rate-limited, return backup */
    out_idx = -1;
    key = credential_pool_next_key(pool, &out_idx);
    TEST_INT_EQ("skipped to backup", out_idx, 1);
    TEST_STR_EQ("backup key returned", key, "backup");

    /* After retrying backup, primary is still rate-limited */
    key = credential_pool_next_key(pool, &out_idx);
    TEST_INT_EQ("still skips to backup", out_idx, 1);

    /* Reset the rate-limited key */
    TEST("reset rate-limited key", credential_pool_reset(pool, 0));
    TEST_INT_EQ("entry 0 back to OK", pool->entries[0].status, CRED_OK);

    /* Now primary should be available */
    key = credential_pool_next_key(pool, &out_idx);
    TEST_STR_EQ("primary restored", key, "primary");

    /* ================================================================
     *  4. Consecutive failure handling
     * ================================================================ */
    printf("\n--- Consecutive failures ---\n");

    /* Re-create with 1 key */
    credential_pool_free(pool);
    pool = credential_pool_create("openai");
    credential_pool_add_key(pool, "fragile", "F");

    /* Report HTTP 500 (server error) — should increment consecutive failures */
    st = credential_pool_report(pool, 0, 500, 0, -1, 0);
    TEST_INT_EQ("500 → still OK", pool->entries[0].status, CRED_OK);
    TEST_INT_EQ("1 consecutive failure", pool->entries[0].consecutive_failures, 1);

    /* Two more failures should trigger FAILED status */
    st = credential_pool_report(pool, 0, 500, 0, -1, 0);
    st = credential_pool_report(pool, 0, 500, 0, -1, 0);
    TEST_INT_EQ("3rd 500 → FAILED", pool->entries[0].status, CRED_FAILED);
    TEST_INT_EQ("3 consecutive failures", pool->entries[0].consecutive_failures, 3);

    /* No available keys — but next_key auto-resurrects failed entries */
    TEST("no available after all failed", !credential_pool_has_available(pool));
    /* next_key returns the failed key (auto-resurrect behavior) */
    key = credential_pool_next_key(pool, NULL);
    TEST_NOT_NULL("next_key auto-resurrects failed key", key);

    /* Successful call (HTTP 200) should reset failure count */
    st = credential_pool_report(pool, 0, 200, 100, -1, 0);
    TEST_INT_EQ("200 → OK status", pool->entries[0].status, CRED_OK);
    TEST_INT_EQ("failures reset to 0", pool->entries[0].consecutive_failures, 0);

    /* ================================================================
     *  5. Cooloff (tick) — resurrect FAILED entries after timeout
     * ================================================================ */
    printf("\n--- Cooloff ---\n");

    credential_pool_free(pool);
    pool = credential_pool_create("openai");
    credential_pool_add_key(pool, "cool-me", "C");

    /* Fail the key */
    credential_pool_report(pool, 0, 500, 0, -1, 0);
    credential_pool_report(pool, 0, 500, 0, -1, 0);
    credential_pool_report(pool, 0, 500, 0, -1, 0);
    TEST_INT_EQ("entry FAILED via consecutive", pool->entries[0].status, CRED_FAILED);

    /* Tick should NOT resurrect yet (last_used is now, cooloff=300s) */
    int resurrected = credential_pool_tick(pool);
    TEST_INT_EQ("no resurrect before cooloff", resurrected, 0);
    TEST_INT_EQ("still FAILED", pool->entries[0].status, CRED_FAILED);

    /* Travel back in time 1 second — still too early */
    pool->entries[0].last_used = time(NULL) - 1;
    resurrected = credential_pool_tick(pool);
    TEST_INT_EQ("no resurrect at 1s", resurrected, 0);

    /* Travel back past the cooloff (default 300s) */
    pool->entries[0].last_used = time(NULL) - 301;
    pool->cooloff_seconds = 300;
    resurrected = credential_pool_tick(pool);
    TEST_INT_EQ("resurrected after cooloff", resurrected, 1);
    TEST_INT_EQ("entry back to OK", pool->entries[0].status, CRED_OK);

    /* ================================================================
     *  6. Stats JSON
     * ================================================================ */
    printf("\n--- Stats JSON ---\n");

    char *stats = credential_pool_stats_json(pool);
    TEST_NOT_NULL("stats JSON non-null", stats);
    if (stats) {
        TEST("stats contains provider name", strstr(stats, "openai") != NULL);
        TEST("stats contains entries array", strstr(stats, "entries") != NULL);
        TEST("stats contains status", strstr(stats, "ok") != NULL);
        free(stats);
    }

    /* Stats for NULL pool */
    stats = credential_pool_stats_json(NULL);
    TEST_NULL("stats for NULL pool", stats);

    /* Stats for empty pool */
    credential_pool_t *empty = credential_pool_create("empty");
    stats = credential_pool_stats_json(empty);
    TEST_NOT_NULL("stats for empty pool", stats);
    if (stats) {
        TEST("empty stats contains provider", strstr(stats, "empty") != NULL);
        free(stats);
    }
    credential_pool_free(empty);

    /* ================================================================
     *  7. Edge cases
     * ================================================================ */
    printf("\n--- Edge cases ---\n");

    /* NULL pool operations should not crash */
    credential_pool_add_key(NULL, "key", "label");
    TEST("add_key NULL pool no crash", 1);

    credential_pool_next_key(NULL, NULL);
    TEST("next_key NULL pool no crash", 1);

    credential_pool_report(NULL, 0, 200, 0, -1, 0);
    TEST("report NULL pool no crash", 1);

    credential_pool_reset(NULL, 0);
    TEST("reset NULL pool no crash", 1);

    credential_pool_has_available(NULL);
    TEST("has_available NULL pool no crash", 1);

    credential_pool_tick(NULL);
    TEST("tick NULL pool no crash", 1);

    /* Report with bad index */
    st = credential_pool_report(pool, 999, 200, 0, -1, 0);
    TEST_INT_EQ("report bad index returns FAILED", st, CRED_FAILED);

    /* Reset with bad index */
    TEST("reset bad index returns false", !credential_pool_reset(pool, 999));

    /* ================================================================
     *  Summary
     * ================================================================ */
    credential_pool_free(pool);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
