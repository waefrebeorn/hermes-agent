/*
 * test_file_sync.c — Tests for file_sync library
 * Tests collection, directory commands, and upload callback
 */

#include "file_sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Track uploads for verification */
static int g_upload_count = 0;
static char g_last_upload_host[1024] = "";
static char g_last_upload_remote[1024] = "";

static bool test_upload_fn(const char *host, const char *remote, void *ctx) {
    (void)ctx;
    g_upload_count++;
    snprintf(g_last_upload_host, sizeof(g_last_upload_host), "%s", host);
    snprintf(g_last_upload_remote, sizeof(g_last_upload_remote), "%s", remote);
    return true;
}

int main(void) {
    printf("=== File Sync Tests ===\n");

    /* Test 1: file_sync_collect with default path */
    {
        file_sync_list_t *files = file_sync_collect("/root/.hermes");
        TEST("collect returns non-NULL", files != NULL);
        if (files) {
            printf("  (found %d files to sync)\n", files->count);
            TEST("collect returns non-negative count", files->count >= 0);
            file_sync_list_free(files);
        }
    }

    /* Test 2: collect with empty state (no files is OK) */
    {
        file_sync_set_home("/nonexistent/path");
        file_sync_list_t *files = file_sync_collect("/root/.hermes");
        TEST("collect with invalid home returns non-NULL", files != NULL);
        if (files) {
            printf("  (found %d files to sync)\n", files->count);
            TEST("empty dir returns 0-count list", files->count == 0);
            file_sync_list_free(files);
        }
    }

    /* Test 3: file_sync_list_free with NULL */
    {
        file_sync_list_free(NULL);
        TEST("free NULL doesn't crash", 1);
    }

    /* Test 4: mkdir_cmd with NULL */
    {
        char *cmd = file_sync_mkdir_cmd(NULL);
        TEST("mkdir_cmd with NULL returns empty string", cmd != NULL && cmd[0] == '\0');
        free(cmd);
    }

    /* Test 5: mkdir_cmd with empty list */
    {
        file_sync_list_t *list = calloc(1, sizeof(file_sync_list_t));
        list->entries = NULL;
        list->count = 0;
        list->capacity = 0;
        char *cmd = file_sync_mkdir_cmd(list);
        TEST("mkdir_cmd with empty list", cmd != NULL && cmd[0] == '\0');
        free(cmd);
        free(list);
    }

    /* Test 6: upload_all with NULL */
    {
        bool r = file_sync_upload_all(NULL, test_upload_fn, NULL);
        TEST("upload_all with NULL files", r == false);
    }

    /* Test 7: upload_all with NULL callback */
    {
        file_sync_list_t *list = calloc(1, sizeof(file_sync_list_t));
        list->entries = NULL;
        list->count = 0;
        list->capacity = 0;
        bool r = file_sync_upload_all(list, NULL, NULL);
        TEST("upload_all with NULL callback", r == false);
        free(list);
    }

    /* Test 8: upload_all with real callback */
    {
        g_upload_count = 0;
        file_sync_list_t *list = calloc(1, sizeof(file_sync_list_t));
        list->capacity = 2;
        list->entries = calloc(2, sizeof(file_sync_entry_t));
        snprintf(list->entries[0].host_path, sizeof(list->entries[0].host_path), "/host/a.txt");
        snprintf(list->entries[0].remote_path, sizeof(list->entries[0].remote_path), "/remote/a.txt");
        snprintf(list->entries[1].host_path, sizeof(list->entries[1].host_path), "/host/b.txt");
        snprintf(list->entries[1].remote_path, sizeof(list->entries[1].remote_path), "/remote/b.txt");
        list->count = 2;

        bool r = file_sync_upload_all(list, test_upload_fn, NULL);
        TEST("upload_all returns true", r == true);
        TEST("upload_all calls callback 2x", g_upload_count == 2);
        TEST("last upload host is /host/b.txt",
             strcmp(g_last_upload_host, "/host/b.txt") == 0);
        TEST("last upload remote is /remote/b.txt",
             strcmp(g_last_upload_remote, "/remote/b.txt") == 0);
        file_sync_list_free(list);
    }

    /* Test 9: set_home and verify it works (doesn't crash) */
    {
        file_sync_set_home("/tmp/test_hermes_home");
        file_sync_set_home(NULL); /* should be no-op */
        TEST("set_home doesn't crash", 1);
    }

    /* Results */
    printf("\n=== File Sync Test Summary: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
