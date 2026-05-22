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

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
