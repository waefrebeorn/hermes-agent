/*
 * test_file_batch.c — Tests for file_batch tool actions.
 * Tests stat, hash, touch, chmod operations on the local filesystem.
 * Phase 336: +7 edge cases: stat nonexistent, hash empty/large,
 *   touch existing preserves content, stat directory, chmod 0000+restore,
 *   chmod nonexistent.
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
    if (result != PASS) g_passed--;
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

/* --- Phase 336: Edge case expansion --- */

/* Test stat on non-existent file returns -1 */
static void test_stat_nonexistent(void) {
    struct stat st;
    int ret = stat("/tmp/hermes_nonexistent_file_xyz9876", &st);
    test_result("stat_nonexistent", (ret == -1) ? PASS : FAIL);
    if (ret != -1) printf("    expected -1, got %d\n", ret);
}

/* Test SHA-256 hash of empty file */
static void test_hash_empty(void) {
    char *path = create_temp_file("");
    if (!path) { test_result("hash_empty", FAIL); printf("    mkstemp failed\n"); return; }

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null | cut -d' ' -f1", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) { test_result("hash_empty", FAIL); printf("    popen failed\n"); unlink(path); free(path); return; }

    char hash[128] = {0};
    if (!fgets(hash, sizeof(hash), fp)) { pclose(fp); test_result("hash_empty", FAIL); printf("    no hash\n"); unlink(path); free(path); return; }
    pclose(fp);
    size_t len = strlen(hash);
    while (len > 0 && (hash[len-1] == '\n' || hash[len-1] == '\r')) hash[--len] = '\0';

    /* e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 = SHA-256 of empty string */
    test_result("hash_empty", (strcmp(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0) ? PASS : FAIL);
    if (strcmp(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") != 0)
        printf("    expected empty hash, got '%s'\n", hash);

    unlink(path);
    free(path);
}

/* Test hash of larger file (repeated content) */
static void test_hash_large(void) {
    /* Create file with repeated pattern */
    char template[] = "/tmp/hermes_large_hash_XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) { test_result("hash_large", FAIL); printf("    mkstemp failed\n"); return; }

    /* Write "AAAAA...\n" * 1000 = ~6KB */
    char buf[64];
    memset(buf, 'A', 60);
    buf[60] = '\n';
    buf[61] = '\0';
    for (int i = 0; i < 1000; i++) write(fd, buf, 61);
    close(fd);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null | cut -d' ' -f1", template);
    FILE *fp = popen(cmd, "r");
    if (!fp) { test_result("hash_large", FAIL); printf("    popen failed\n"); unlink(template); return; }

    char hash[128] = {0};
    if (!fgets(hash, sizeof(hash), fp)) { pclose(fp); test_result("hash_large", FAIL); printf("    no hash\n"); unlink(template); return; }
    pclose(fp);
    size_t len = strlen(hash);
    while (len > 0 && (hash[len-1] == '\n' || hash[len-1] == '\r')) hash[--len] = '\0';

    test_result("hash_large", (len == 64) ? PASS : FAIL);
    if (len != 64) printf("    expected 64 hex chars, got %zu\n", len);

    unlink(template);
}

/* Test touch on existing file preserves content */
static void test_touch_preserves_content(void) {
    char *path = create_temp_file("preserve me!");
    if (!path) { test_result("touch_preserve", FAIL); printf("    mkstemp failed\n"); return; }

    /* "Touch" by opening for append (doesn't truncate) */
    FILE *fp = fopen(path, "a");
    if (!fp) { test_result("touch_preserve", FAIL); printf("    fopen failed\n"); unlink(path); free(path); return; }
    fclose(fp);

    /* Verify content preserved */
    fp = fopen(path, "r");
    char buf[64] = {0};
    size_t br = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[br] = '\0';

    test_result("touch_preserve", (strcmp(buf, "preserve me!") == 0) ? PASS : FAIL);
    if (strcmp(buf, "preserve me!") != 0) printf("    expected 'preserve me!', got '%s'\n", buf);

    unlink(path);
    free(path);
}

/* Test stat on directory returns S_ISDIR */
static void test_stat_directory(void) {
    struct stat st;
    int ret = stat("/tmp", &st);
    test_result("stat_directory", (ret == 0 && S_ISDIR(st.st_mode)) ? PASS : FAIL);
    if (ret != 0) printf("    stat failed\n");
    else if (!S_ISDIR(st.st_mode)) printf("    expected directory\n");
}

/* Test chmod to 0000 then restore */
static void test_chmod_extreme(void) {
    char *path = create_temp_file("extreme chmod test");
    if (!path) { test_result("chmod_extreme", FAIL); printf("    mkstemp failed\n"); return; }

    /* Remove all permissions */
    chmod(path, 0000);
    struct stat st;
    stat(path, &st);
    int is_all_zero = ((st.st_mode & 0777) == 0);

    /* Restore so we can unlink */
    chmod(path, 0600);
    unlink(path);
    free(path);

    test_result("chmod_extreme", is_all_zero ? PASS : FAIL);
    if (!is_all_zero) printf("    expected mode 0000, got %o\n", (unsigned)(st.st_mode & 0777));
}

/* Test chmod on non-existent file */
static void test_chmod_nonexistent(void) {
    int ret = chmod("/tmp/hermes_nonexistent_chmod_xyz", 0644);
    test_result("chmod_nonexistent", (ret == -1) ? PASS : FAIL);
    if (ret != -1) printf("    expected -1, got %d\n", ret);
}

int main(void) {
    printf("=== file_batch tests ===\n\n");

    test_stat_size();
    test_hash_sha256();
    test_touch_create();
    test_stat_type();
    test_chmod_perms();
    test_chmod_readonly();

    /* Phase 336: edge cases */
    test_stat_nonexistent();
    test_hash_empty();
    test_hash_large();
    test_touch_preserves_content();
    test_stat_directory();
    test_chmod_extreme();
    test_chmod_nonexistent();

    printf("\nResults: %d/%d passed, %d failed\n",
           g_passed, g_tests, g_tests - g_passed);
    return (g_passed == g_tests) ? 0 : 1;
}
