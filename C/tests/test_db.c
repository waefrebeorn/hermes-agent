/*
 * test_db.c — Smoke test for file-based JSON session store.
 */
#include "db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    char tmpdir[256];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/db_test_%d", getpid());
    mkdir(tmpdir, 0700);

    /* Test 1: db_open */
    char *err = NULL;
    db_t *db = db_open(tmpdir, &err);
    TEST("db_open success", db != NULL && err == NULL);
    free(err);

    if (db) {
        /* Test 2: db_save */
        bool saved = db_save(db, "session_1", "{\"key\":\"value\"}");
        TEST("db_save session_1", saved == true);

        bool saved2 = db_save(db, "session_2", "{\"data\":[1,2,3]}");
        TEST("db_save session_2", saved2 == true);

        /* Test 3: db_exists */
        bool exists = db_exists(db, "session_1");
        TEST("db_exists session_1", exists == true);

        bool not_exists = db_exists(db, "nonexistent");
        TEST("db_exists nonexistent", not_exists == false);

        /* Test 4: db_load */
        char *data = db_load(db, "session_1", &err);
        TEST("db_load session_1 non-NULL", data != NULL);
        if (data) {
            TEST("db_load content", strcmp(data, "{\"key\":\"value\"}") == 0);
            free(data);
        }
        free(err);
        err = NULL;

        /* Test 5: db_list */
        size_t count = 0;
        char **list = db_list(db, &count);
        TEST("db_list non-NULL", list != NULL);
        if (list) {
            TEST("db_list count == 2", count == 2);
            bool found_1 = false, found_2 = false;
            for (size_t i = 0; i < count; i++) {
                if (strcmp(list[i], "session_1") == 0) found_1 = true;
                if (strcmp(list[i], "session_2") == 0) found_2 = true;
                free(list[i]);
            }
            TEST("db_list contains session_1", found_1);
            TEST("db_list contains session_2", found_2);
            free(list);
        }

        /* Test 6: db_count */
        size_t c = db_count(db);
        TEST("db_count == 2", c == 2);

        /* Test 7: db_delete */
        bool deleted = db_delete(db, "session_1");
        TEST("db_delete session_1", deleted == true);
        TEST("db_count after delete", db_count(db) == 1);

        /* Test 8: db_storage_size */
        long long size = db_storage_size(db);
        TEST("db_storage_size >= 0", size >= 0);

        /* Test 9: db_flush */
        bool flushed = db_flush(db);
        TEST("db_flush success", flushed == true);

        /* Test 10: db_clear */
        bool cleared = db_clear(db);
        TEST("db_clear success", cleared == true);
        TEST("db_count after clear", db_count(db) == 0);

        db_close(db);
    }

    /* Cleanup temp dir */
    rmdir(tmpdir);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All DB tests PASSED");
    return failures ? 1 : 0;
}
