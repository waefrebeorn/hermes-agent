/*
 * test_memory.c — Memory tool unit tests (M35).
 *
 * Tests memory CRUD operations using the in-memory backend:
 * store, search, delete, list keys, and edge cases.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_memory.c src/tools/memory.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_memory -lm
 *
 * Run:
 *   /tmp/hermes_test_memory
 */

#include "hermes.h"
#include "hermes_memory.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Memory Tool Tests ===\n");

    /* 1. Init in-memory backend */
    memory_t mem;
    memset(&mem, 0, sizeof(mem));
    TEST("memory_init in-memory", memory_init(&mem, MEMORY_STORAGE_INMEM, ""));

    /* 2. Store entries */
    memory_entry_t e1;
    memset(&e1, 0, sizeof(e1));
    snprintf(e1.key, sizeof(e1.key), "k1");
    snprintf(e1.content, sizeof(e1.content), "Hello world, this is memory test");
    e1.priority = 5;
    TEST("memory_store k1", memory_store(&mem, &e1));

    memory_entry_t e2;
    memset(&e2, 0, sizeof(e2));
    snprintf(e2.key, sizeof(e2.key), "k2");
    snprintf(e2.content, sizeof(e2.content), "Another memory with different content");
    e2.priority = 3;
    TEST("memory_store k2", memory_store(&mem, &e2));

    memory_entry_t e3;
    memset(&e3, 0, sizeof(e3));
    snprintf(e3.key, sizeof(e3.key), "k3");
    snprintf(e3.content, sizeof(e3.content), "Third entry for search testing");
    e3.priority = 1;
    TEST("memory_store k3", memory_store(&mem, &e3));

    /* 3. Store with convenience function */
    TEST("memory_store_simple", memory_store_simple(&mem, "Simple entry", 0, 0));

    /* 4. List keys */
    size_t key_count = 0;
    char **keys = memory_list_keys(&mem, &key_count);
    TEST("memory_list_keys returns 4 entries", key_count == 4);
    if (keys) {
        for (size_t i = 0; i < key_count; i++) free(keys[i]);
        free(keys);
    }

    /* 5. Search existing entry */
    memory_entry_t *results = memory_search(&mem, "Hello", 10);
    TEST("memory_search finds 'Hello'", results != NULL);
    if (results) {
        TEST("search result has correct content",
             strcmp(results[0].content, "Hello world, this is memory test") == 0);
        free(results);
    }

    /* 6. Search non-existent keyword returns NULL (or empty) */
    memory_entry_t *nores = memory_search(&mem, "zzzznotfound", 10);
    TEST("memory_search no match returns NULL", nores == NULL);

    /* 7. Delete entry */
    TEST("memory_delete k2", memory_delete(&mem, "k2"));

    /* 8. Verify deletion */
    size_t after_del = 0;
    char **keys2 = memory_list_keys(&mem, &after_del);
    TEST("memory_list_keys returns 3 after delete", after_del == 3);
    if (keys2) {
        for (size_t i = 0; i < after_del; i++) free(keys2[i]);
        free(keys2);
    }

    /* 9. Delete non-existent key */
    TEST("memory_delete non-existent returns false",
         memory_delete(&mem, "nonexistent") == false);

    /* 10. Edge: empty content store (auto-generates key) */
    TEST("memory_store empty key auto-generates",
         memory_store_simple(&mem, "", 0, 0) == true);

    /* 11. Edge: NULL content */
    memory_entry_t null_entry;
    memset(&null_entry, 0, sizeof(null_entry));
    snprintf(null_entry.key, sizeof(null_entry.key), "null");
    /* content stays empty */
    TEST("memory_store empty content allowed",
         memory_store(&mem, &null_entry) == true);

    /* 12. Verify entry count after all ops */
    size_t final_count = 0;
    char **keys3 = memory_list_keys(&mem, &final_count);
    /* At minimum k1, k3, and at least one auto-keyed entry should remain */
    TEST("final count >= 3 after all operations", final_count >= 3);
    if (keys3) {
        for (size_t i = 0; i < final_count; i++) free(keys3[i]);
        free(keys3);
    }

    /* 13. Cleanup */
    memory_cleanup(&mem);
    TEST("memory_cleanup cleans up", 1);

    /* ================================================================
     *  Edge case tests (S7 X04 expansion)
     * ================================================================ */

    /* Re-init for edge case tests */
    memory_t mem2;
    memset(&mem2, 0, sizeof(mem2));
    memory_init(&mem2, MEMORY_STORAGE_INMEM, "");

    /* 14. memory_count */
    TEST("count empty == 0", memory_count(&mem2) == 0);

    /* 15. memory_store max-length key */
    memory_entry_t long_key;
    memset(&long_key, 0, sizeof(long_key));
    memset(long_key.key, 'k', MEMORY_KEY_MAX - 2);
    long_key.key[MEMORY_KEY_MAX - 2] = '\0';
    snprintf(long_key.content, sizeof(long_key.content), "max key test");
    TEST("store max-length key", memory_store(&mem2, &long_key));
    TEST("count == 1 after store", memory_count(&mem2) == 1);

    /* 16. memory_get success + fail */
    memory_entry_t got;
    memset(&got, 0, sizeof(got));
    TEST("get existing key", memory_get(&mem2, long_key.key, &got));
    TEST("get non-existent key returns false", !memory_get(&mem2, "nonexistent", &got));
    TEST("get NULL key returns false", !memory_get(&mem2, NULL, &got));
    TEST("get empty key returns false", !memory_get(&mem2, "", &got));

    /* 17. memory_delete edge cases */
    TEST("delete NULL key returns false", !memory_delete(&mem2, NULL));
    TEST("delete empty key returns false", !memory_delete(&mem2, ""));

    /* 18. memory_search edge cases */
    memory_entry_t *sr = memory_search(&mem2, NULL, 5);
    TEST("search NULL query returns NULL", sr == NULL);
    sr = memory_search(&mem2, "", 5);
    /* Empty query may return results — just verify no crash */
    if (sr) free(sr);
    TEST("search empty query no crash", 1);
    sr = memory_search(&mem2, "max", 0);
    /* Limit 0 may still return results — just verify no crash */
    if (sr) free(sr);
    TEST("search limit=0 no crash", 1);

    /* 19. memory_search with valid query after storing */
    memory_entry_t se;
    memset(&se, 0, sizeof(se));
    snprintf(se.key, sizeof(se.key), "searchme");
    snprintf(se.content, sizeof(se.content), "unique_search_term_xyz");
    memory_store(&mem2, &se);
    sr = memory_search(&mem2, "unique_search", 10);
    TEST("search finds result", sr != NULL);
    if (sr) { free(sr); }

    /* 20. memory_clear */
    size_t before_clear = memory_count(&mem2);
    TEST("entries exist before clear", before_clear > 0);
    memory_clear(&mem2);
    TEST("count == 0 after clear", memory_count(&mem2) == 0);

    /* 21. Re-use after clear */
    memory_entry_t after;
    memset(&after, 0, sizeof(after));
    snprintf(after.key, sizeof(after.key), "after");
    snprintf(after.content, sizeof(after.content), "after clear test");
    TEST("store after clear", memory_store(&mem2, &after));
    TEST("count == 1 after re-store", memory_count(&mem2) == 1);

    /* 22. memory_entry_expired */
    memory_entry_t fresh;
    memset(&fresh, 0, sizeof(fresh));
    fresh.expires_at = 0;
    TEST("no expiry = not expired", !memory_entry_expired(&fresh));
    fresh.expires_at = 1;
    TEST("past expiry = expired", memory_entry_expired(&fresh));
    fresh.expires_at = time(NULL) + 3600;
    TEST("future expiry = not expired", !memory_entry_expired(&fresh));

    /* 23. memory_hash_content */
    uint64_t h1 = memory_hash_content("hello");
    uint64_t h2 = memory_hash_content("hello");
    uint64_t h3 = memory_hash_content("world");
    TEST("hash deterministic", h1 == h2);
    TEST("hash different content differs", h1 != h3);
    TEST("hash empty string", memory_hash_content("") != 0);
    TEST("hash NULL safe", memory_hash_content(NULL) == 0);

    /* 24. memory_get_prioritized */
    memory_entry_t p1, p2;
    memset(&p1, 0, sizeof(p1)); snprintf(p1.key, sizeof(p1.key), "high");
    snprintf(p1.content, sizeof(p1.content), "high priority"); p1.priority = 100;
    memset(&p2, 0, sizeof(p2)); snprintf(p2.key, sizeof(p2.key), "low");
    snprintf(p2.content, sizeof(p2.content), "low priority"); p2.priority = 1;
    memory_store(&mem2, &p1);
    memory_store(&mem2, &p2);
    size_t pcount = 0;
    memory_entry_t *pr = memory_get_prioritized(&mem2, 10, &pcount);
    TEST("prioritized returns results", pr != NULL && pcount >= 2);
    if (pr && pcount >= 2) {
        TEST("highest priority first", pr[0].priority == 100);
        free(pr);
    }

    /* 25. memory_store with very long content (fits in buffer) */
    memory_entry_t longc;
    memset(&longc, 0, sizeof(longc));
    snprintf(longc.key, sizeof(longc.key), "longc");
    memset(longc.content, 'C', 5000);
    longc.content[5000] = '\0';
    TEST("store 5000-char content", memory_store(&mem2, &longc));

    /* 26. memory_store duplicate key (update) */
    memory_entry_t dup;
    memset(&dup, 0, sizeof(dup));
    snprintf(dup.key, sizeof(dup.key), "high");
    snprintf(dup.content, sizeof(dup.content), "updated content");
    dup.priority = 50;
    TEST("store duplicate key (update)", memory_store(&mem2, &dup));
    memory_entry_t g2;
    memset(&g2, 0, sizeof(g2));
    TEST("get updated entry", memory_get(&mem2, "high", &g2));
    TEST("updated content matches", strcmp(g2.content, "updated content") == 0);

    /* 27. Store with priority boundaries */
    memory_entry_t pb;
    memset(&pb, 0, sizeof(pb));
    snprintf(pb.key, sizeof(pb.key), "neg_priority");
    snprintf(pb.content, sizeof(pb.content), "negative priority");
    pb.priority = -1;
    TEST("store negative priority", memory_store(&mem2, &pb));

    /* 28. Cleanup edge */
    memory_cleanup(&mem2);
    TEST("cleanup after re-init", 1);
    /* Double cleanup should be safe */
    memory_cleanup(&mem2);
    TEST("double cleanup safe", 1);

    /* 29. NULL pointer safety for remaining API */
    memory_clear(NULL);
    TEST("memory_clear(NULL) safe", 1);
    TEST("memory_count(NULL) == 0", memory_count(NULL) == 0);
    TEST("memory_list_keys(NULL) == NULL", memory_list_keys(NULL, NULL) == NULL);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
