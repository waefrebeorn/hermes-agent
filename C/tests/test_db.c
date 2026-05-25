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

        /* L19: Tag CRUD tests */
        {
            int count = 0;
            /* Re-init session_1 for tag tests */
            db_save(db, "session_1", "{}");

            TEST("db_tag_add adds tag", db_tag_add(db, "session_1", "important"));
            TEST("db_tag_add duplicate returns true",
                 db_tag_add(db, "session_1", "important") == true);

            char **tags = db_tag_list(db, "session_1", &count);
            TEST("db_tag_list non-NULL", tags != NULL);
            if (tags) {
                TEST("db_tag_list has 1 tag", count == 1);
                TEST("db_tag_list correct tag", strcmp(tags[0], "important") == 0);
                for (int i = 0; i < count; i++) free(tags[i]);
                free(tags);
            }

            TEST("db_tag_add second tag", db_tag_add(db, "session_1", "work"));
            tags = db_tag_list(db, "session_1", &count);
            TEST("db_tag_list has 2 tags", tags && count == 2);
            if (tags) {
                for (int i = 0; i < count; i++) free(tags[i]);
                free(tags);
            }

            TEST("db_tag_remove removes tag",
                 db_tag_remove(db, "session_1", "important"));
            tags = db_tag_list(db, "session_1", &count);
            TEST("db_tag_list has 1 tag after remove", tags && count == 1);
            if (tags) {
                TEST("remaining tag is 'work'", strcmp(tags[0], "work") == 0);
                for (int i = 0; i < count; i++) free(tags[i]);
                free(tags);
            }

            TEST("db_tag_remove nonexistent returns false",
                 db_tag_remove(db, "session_1", "nonexistent") == false);

            /* db_tag_find */
            db_save(db, "session_tag_find", "{}");
            db_tag_add(db, "session_tag_find", "important");
            db_tag_add(db, "session_1", "important"); /* re-add after earlier remove */
            size_t find_count = 0;
            char **found = db_tag_find(db, "important", &find_count);
            TEST("db_tag_find non-NULL", found != NULL);
            if (found) {
                TEST("db_tag_find found sessions", find_count > 0);
                bool found_s1 = false, found_stf = false;
                for (size_t i = 0; found[i]; i++) {
                    if (strcmp(found[i], "session_1") == 0) found_s1 = true;
                    if (strcmp(found[i], "session_tag_find") == 0) found_stf = true;
                }
                TEST("db_tag_find found session_1", found_s1);
                TEST("db_tag_find found session_tag_find", found_stf);
                for (size_t i = 0; found[i]; i++) free(found[i]);
                free(found);
            }
            db_delete(db, "session_tag_find");
        }

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
