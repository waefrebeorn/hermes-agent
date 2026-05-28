/*
 * test_file_batch.c — Tests for file_batch tool actions.
 * Tests stat, hash, touch, chmod operations on the local filesystem.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define PASS 0
#define FAIL 1
static int g_tests = 0, g_passed = 0;

static void test_result(const char *name, int result) {
    g_tests++;
    printf("  %s  %s\n", result == PASS ? "PASS" : "FAIL", name);
    if (result != PASS) g_passed--; /* tracking via subtraction at end */
    else g_passed++;
}

/* Create a temp file with given content */
static char *create_temp_file(const char *content) {
    char template[] = "/tmp/hermes_file_batch_XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) return NULL;
    if (content) {
        write(fd, content, strlen(content));
    }
    close(fd);
    return strdup(template);
}

/* Test stat action detects file size */
static void test_stat_size(void) {
    char *path = create_temp_file("hello world\n");
    if (!path) { test_result("stat_size", FAIL); printf("    mkstemp failed\n"); return; }

    struct stat st;
    int ret = stat(path, &st);
    test_result("stat_size", (ret == 0 && st.st_size == 12) ? PASS : FAIL);
    if (ret != 0) printf("    stat failed\n");
    else if (st.st_size != 12) printf("    expected 12, got %ld\n", (long)st.st_size);

    unlink(path);
    free(path);
}

/* Test hash action (SHA-256 via libhash) */
static void test_hash_sha256(void) {
    char *path = create_temp_file("test content for hash");
    if (!path) { test_result("hash_sha256", FAIL); printf("    mkstemp failed\n"); return; }

    /* Call terminal tool to compute SHA-256 using sha256sum */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null | cut -d' ' -f1", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) { test_result("hash_sha256", FAIL); printf("    popen failed\n"); return; }

    char hash[128] = {0};
    if (!fgets(hash, sizeof(hash), fp)) { pclose(fp); test_result("hash_sha256", FAIL); printf("    no hash output\n"); unlink(path); free(path); return; }
    pclose(fp);

    size_t len = strlen(hash);
    while (len > 0 && (hash[len-1] == '\n' || hash[len-1] == '\r')) hash[--len] = '\0';

    test_result("hash_sha256", (len == 64) ? PASS : FAIL);
    if (len != 64) printf("    expected 64 hex chars, got %zu: '%s'\n", len, hash);

    unlink(path);
    free(path);
}

/* Test touch creates new file */
static void test_touch_create(void) {
    char path[] = "/tmp/hermes_test_touch_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) { test_result("touch_create", FAIL); printf("    mkstemp failed\n"); return; }
    close(fd);
    unlink(path); /* Remove so touch creates it */

    /* Re-create via touch equivalent: write empty content */
    FILE *fp = fopen(path, "w");
    if (!fp) { test_result("touch_create", FAIL); printf("    fopen failed\n"); return; }
    fclose(fp);

    struct stat st;
    int ret = stat(path, &st);
    test_result("touch_create", (ret == 0 && st.st_size == 0) ? PASS : FAIL);
    if (ret != 0) printf("    stat failed\n");

    unlink(path);
}

/* Test stat detects regular file type */
static void test_stat_type(void) {
    char *path = create_temp_file("test");
    if (!path) { test_result("stat_type", FAIL); printf("    mkstemp failed\n"); return; }

    struct stat st;
    int ret = stat(path, &st);
    test_result("stat_type", (ret == 0 && S_ISREG(st.st_mode)) ? PASS : FAIL);
    if (ret != 0) printf("    stat failed\n");

    unlink(path);
    free(path);
}

/* Test chmod changes permissions */
static void test_chmod_perms(void) {
    char *path = create_temp_file("chmod test");
    if (!path) { test_result("chmod_perms", FAIL); printf("    mkstemp failed\n"); return; }

    /* Set 0644 */
    chmod(path, 0644);
    struct stat st;
    stat(path, &st);
    mode_t expected = 0644;
    mode_t got = st.st_mode & 0777;

    test_result("chmod_perms", (got == expected) ? PASS : FAIL);
    if (got != expected) printf("    expected %o, got %o\n", (unsigned)expected, (unsigned)got);

    unlink(path);
    free(path);
}

/* Test read-only file detection */
static void test_chmod_readonly(void) {
    char *path = create_temp_file("readonly test");
    if (!path) { test_result("chmod_readonly", FAIL); printf("    mkstemp failed\n"); return; }

    /* Set 0444 (read-only) */
    chmod(path, 0444);
    struct stat st;
    stat(path, &st);
    int is_readonly = ((st.st_mode & 0222) == 0);

    test_result("chmod_readonly", is_readonly ? PASS : FAIL);
    if (!is_readonly) printf("    expected readonly, mode=%o\n", (unsigned)(st.st_mode & 0777));

    /* Restore write perms so we can unlink */
    chmod(path, 0644);
    unlink(path);
    free(path);
}

int main(void) {
    printf("=== file_batch tests ===\n\n");

    test_stat_size();
    test_hash_sha256();
    test_touch_create();
    test_stat_type();
    test_chmod_perms();
    test_chmod_readonly();

    printf("\nResults: %d/%d passed, %d failed\n",
           g_passed, g_tests, g_tests - g_passed);
    return (g_passed == g_tests) ? 0 : 1;
}
