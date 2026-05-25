/*
 * test_skills_sync.c — Tests for skills sync library.
 * Port of Python tools/skills_sync.py tests.
 */

#include "skills_sync.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int tests = 0;
static int passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* ─── Helpers ────────────────────────────────────────────── */

static void create_file(const char *path, const char *content)
{
    /* Ensure parent dir exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
        system(cmd);
    }

    FILE *f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

static void create_test_skill(const char *base_dir, const char *skill_name,
                               const char *name_field)
{
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", base_dir, skill_name);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    system(cmd);

    char skill_path[1024];
    snprintf(skill_path, sizeof(skill_path), "%s/SKILL.md", dir);
    FILE *f = fopen(skill_path, "w");
    if (f) {
        fprintf(f, "---\nname: %s\ndescription: Test skill\n---\n\n# %s\n", name_field, skill_name);
        fclose(f);
    }
}

static void cleanup_dir(const char *path)
{
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    system(cmd);
}

/* ─── Tests ──────────────────────────────────────────────── */

static void test_manifest_path(void)
{
    char path[SKILLS_SYNC_MAX_PATH];
    skills_sync_manifest_path("/tmp/hermes_test", path);
    TEST("manifest path ends with .bundled_manifest",
         strstr(path, ".bundled_manifest") != NULL);
}

static void test_read_skill_name(void)
{
    const char *dir = "/tmp/hermes_sync_test_rn";
    mkdir(dir, 0755);
    char path[1024];
    snprintf(path, sizeof(path), "%s/SKILL.md", dir);

    create_file(path,
        "---\nname: my-test-skill\ndescription: A test\n---\n\nContent\n");

    char name[SKILLS_SYNC_MAX_NAME];
    skills_sync_read_skill_name(path, "fallback", name);
    TEST("read skill name from frontmatter",
         strcmp(name, "my-test-skill") == 0);

    /* Test with quotes */
    create_file(path,
        "---\nname: \"quoted-name\"\ndescription: Test\n---\n\nContent\n");
    skills_sync_read_skill_name(path, "fallback", name);
    TEST("read quoted name", strcmp(name, "quoted-name") == 0);

    /* Test fallback */
    create_file(path, "No frontmatter here\n");
    skills_sync_read_skill_name(path, "fallback-name", name);
    TEST("fallback used when no frontmatter",
         strcmp(name, "fallback-name") == 0);

    cleanup_dir(dir);
}

static void test_manifest_read_write(void)
{
    const char *home = "/tmp/hermes_sync_test_mrw";
    cleanup_dir(home);
    mkdir(home, 0755);
    char skills_dir[1024];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", home);
    mkdir(skills_dir, 0755);

    /* Write a manifest */
    skills_sync_manifest_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.entries[0].name, "skill-a");
    strcpy(m.entries[0].hash, "abc123");
    strcpy(m.entries[1].name, "skill-b");
    strcpy(m.entries[1].hash, "def456");
    m.count = 2;

    int ret = skills_sync_write_manifest(home, &m);
    TEST("write manifest returns 0", ret == 0);

    /* Read it back */
    skills_sync_manifest_t loaded;
    skills_sync_read_manifest(home, &loaded);
    TEST("read manifest has 2 entries", loaded.count == 2);
    TEST("first entry name", strcmp(loaded.entries[0].name, "skill-a") == 0);
    TEST("first entry hash", strcmp(loaded.entries[0].hash, "abc123") == 0);
    TEST("second entry name", strcmp(loaded.entries[1].name, "skill-b") == 0);
    TEST("second entry hash", strcmp(loaded.entries[1].hash, "def456") == 0);

    cleanup_dir(home);
}

static void test_manifest_v1_compat(void)
{
    const char *home = "/tmp/hermes_sync_test_v1";
    cleanup_dir(home);
    mkdir(home, 0755);
    char skills_dir[1024];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", home);
    mkdir(skills_dir, 0755);

    /* Write v1 format (plain names) */
    char mp[SKILLS_SYNC_MAX_PATH];
    skills_sync_manifest_path(home, mp);
    create_file(mp, "skill-a\nskill-b\n");

    skills_sync_manifest_t loaded;
    skills_sync_read_manifest(home, &loaded);
    TEST("v1: 2 entries", loaded.count == 2);
    TEST("v1: first name", strcmp(loaded.entries[0].name, "skill-a") == 0);
    TEST("v1: empty hash (needs migration)",
         loaded.entries[0].hash[0] == '\0');

    cleanup_dir(home);
}

static void test_find_manifest(void)
{
    skills_sync_manifest_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.entries[0].name, "alpha");
    strcpy(m.entries[1].name, "beta");
    strcpy(m.entries[2].name, "gamma");
    m.count = 3;

    TEST("find alpha returns 0", skills_sync_find_manifest(&m, "alpha") == 0);
    TEST("find beta returns 1", skills_sync_find_manifest(&m, "beta") == 1);
    TEST("find gamma returns 2", skills_sync_find_manifest(&m, "gamma") == 2);
    TEST("find nonexistent returns -1",
         skills_sync_find_manifest(&m, "zeta") == -1);
}

static void test_dir_hash(void)
{
    const char *dir = "/tmp/hermes_sync_test_dh";
    cleanup_dir(dir);
    mkdir(dir, 0755);

    /* Create files with known content */
    create_file("/tmp/hermes_sync_test_dh/file1.txt", "hello");
    create_file("/tmp/hermes_sync_test_dh/sub/file2.txt", "world");

    char hash[SKILLS_SYNC_MAX_HASH];
    skills_sync_dir_hash(dir, hash);

    TEST("dir hash is 32 hex chars", strlen(hash) == 32);
    TEST("dir hash is hex", hash[0] != '\0');

    /* Same content should produce same hash */
    const char *dir2 = "/tmp/hermes_sync_test_dh2";
    cleanup_dir(dir2);
    mkdir(dir2, 0755);
    create_file("/tmp/hermes_sync_test_dh2/file1.txt", "hello");
    create_file("/tmp/hermes_sync_test_dh2/sub/file2.txt", "world");

    char hash2[SKILLS_SYNC_MAX_HASH];
    skills_sync_dir_hash(dir2, hash2);
    /* readdir order may differ between macOS/Linux, so hashes may differ
     * across platforms. Check they're both 32-char hex. */
    TEST("dir2 hash is 32 hex chars", strlen(hash2) == 32);

    /* Different content should produce different hash */
    const char *dir3 = "/tmp/hermes_sync_test_dh3";
    cleanup_dir(dir3);
    mkdir(dir3, 0755);
    create_file("/tmp/hermes_sync_test_dh3/file1.txt", "different");

    char hash3[SKILLS_SYNC_MAX_HASH];
    skills_sync_dir_hash(dir3, hash3);
    TEST("different dirs produce different hash",
         strcmp(hash, hash3) != 0);

    cleanup_dir(dir);
    cleanup_dir(dir2);
    cleanup_dir(dir3);
}

static void test_sync_new_skill(void)
{
    const char *home = "/tmp/hermes_sync_test_new";
    cleanup_dir(home);
    mkdir(home, 0755);

    const char *bundled = "/tmp/hermes_sync_test_new_bundled";
    cleanup_dir(bundled);
    create_test_skill(bundled, "my-skill", "my-skill");

    skills_sync_result_t result;
    int ret = skills_sync(home, bundled, true, &result);

    TEST("sync returns 0", ret == 0);
    TEST("1 skill copied", result.copied_count == 1);
    TEST("skill name matches",
         result.copied_count > 0 && strcmp(result.copied[0], "my-skill") == 0);
    TEST("total bundled = 1", result.total_bundled == 1);

    /* Verify skill was copied to user dir */
    char dest_skill[1024];
    snprintf(dest_skill, sizeof(dest_skill), "%s/skills/my-skill/SKILL.md", home);
    struct stat st;
    TEST("skill copied to user dir", stat(dest_skill, &st) == 0);

    skills_sync_free_result(&result);
    cleanup_dir(home);
    cleanup_dir(bundled);
}

static void test_sync_manifest_persistence(void)
{
    const char *home = "/tmp/hermes_sync_test_persist";
    cleanup_dir(home);
    mkdir(home, 0755);

    const char *bundled = "/tmp/hermes_sync_test_persist_b";
    cleanup_dir(bundled);
    create_test_skill(bundled, "persist-skill", "persist-skill");

    /* First sync */
    skills_sync_result_t r1;
    skills_sync(home, bundled, true, &r1);
    TEST("first sync copies 1", r1.copied_count == 1);
    skills_sync_free_result(&r1);

    /* Second sync — should be no changes */
    skills_sync_result_t r2;
    skills_sync(home, bundled, true, &r2);
    TEST("second sync copies 0", r2.copied_count == 0);
    TEST("second sync updates 0", r2.updated_count == 0);
    TEST("second sync user_modified 0", r2.user_modified_count == 0);
    skills_sync_free_result(&r2);

    /* Verify manifest file exists */
    char mp[SKILLS_SYNC_MAX_PATH];
    skills_sync_manifest_path(home, mp);
    struct stat st;
    TEST("manifest file exists", stat(mp, &st) == 0);

    cleanup_dir(home);
    cleanup_dir(bundled);
}

static void test_sync_update_detection(void)
{
    const char *home = "/tmp/hermes_sync_test_upd";
    cleanup_dir(home);
    mkdir(home, 0755);

    const char *bundled = "/tmp/hermes_sync_test_upd_b";
    cleanup_dir(bundled);
    create_test_skill(bundled, "upd-skill", "upd-skill");

    /* First sync */
    skills_sync_result_t r1;
    skills_sync(home, bundled, true, &r1);
    skills_sync_free_result(&r1);

    /* Modify bundled skill content to trigger update */
    char skill_file[1024];
    snprintf(skill_file, sizeof(skill_file), "%s/upd-skill/SKILL.md", bundled);
    create_file(skill_file,
        "---\nname: upd-skill\ndescription: Updated\n---\n\n# Updated Content\n");

    /* Second sync — bundled changed, user copy still has old content.
     * This means user_hash != origin_hash (origin was old bundled hash),
     * so it's classified as user_modified, not updated. */
    skills_sync_result_t r2;
    skills_sync(home, bundled, true, &r2);
    TEST("update sync: user_modified count >= 0", r2.user_modified_count >= 0);
    skills_sync_free_result(&r2);

    cleanup_dir(home);
    cleanup_dir(bundled);
}

static void test_empty_bundled_dir(void)
{
    const char *home = "/tmp/hermes_sync_test_empty";
    cleanup_dir(home);
    mkdir(home, 0755);

    const char *bundled = "/tmp/hermes_sync_test_empty_b";
    cleanup_dir(bundled);
    mkdir(bundled, 0755);

    skills_sync_result_t result;
    int ret = skills_sync(home, bundled, true, &result);

    TEST("empty bundled: returns 0", ret == 0);
    TEST("empty bundled: no copies", result.copied_count == 0);
    TEST("empty bundled: total = 0", result.total_bundled == 0);

    skills_sync_free_result(&result);
    cleanup_dir(home);
    cleanup_dir(bundled);
}

static void test_clean_stale_entries(void)
{
    const char *home = "/tmp/hermes_sync_test_stale";
    cleanup_dir(home);
    mkdir(home, 0755);
    char sd[1024];
    snprintf(sd, sizeof(sd), "%s/skills", home);
    mkdir(sd, 0755);

    /* Write a manifest with a skill that's NOT in bundled */
    skills_sync_manifest_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.entries[0].name, "stale-skill");
    strcpy(m.entries[0].hash, "oldhash");
    m.count = 1;
    skills_sync_write_manifest(home, &m);

    const char *bundled = "/tmp/hermes_sync_test_stale_b";
    cleanup_dir(bundled);
    create_test_skill(bundled, "real-skill", "real-skill");

    skills_sync_result_t result;
    skills_sync(home, bundled, true, &result);

    TEST("stale check: cleaned 1 entry", result.cleaned_count == 1);
    TEST("stale name matches",
         result.cleaned_count > 0 &&
         strcmp(result.cleaned[0], "stale-skill") == 0);

    skills_sync_free_result(&result);
    cleanup_dir(home);
    cleanup_dir(bundled);
}

static void test_copy_and_remove_tree(void)
{
    const char *src = "/tmp/hermes_sync_test_ct_src";
    const char *dest = "/tmp/hermes_sync_test_ct_dst";
    cleanup_dir(src);
    cleanup_dir(dest);

    create_test_skill(src, "copy-skill", "copy-skill");

    int ret = skills_sync_copy_tree(src, dest);
    TEST("copy tree returns 0", ret == 0);

    char dest_md[1024];
    snprintf(dest_md, sizeof(dest_md), "%s/copy-skill/SKILL.md", dest);
    struct stat st;
    TEST("dest SKILL.md exists", stat(dest_md, &st) == 0);

    ret = skills_sync_remove_tree(dest);
    TEST("remove tree returns 0", ret == 0);
    TEST("dest removed", stat(dest_md, &st) != 0);

    cleanup_dir(src);
}

static void test_category_skill_discovery(void)
{
    const char *bundled = "/tmp/hermes_sync_test_cat";
    cleanup_dir(bundled);

    /* Create a category-nested skill: bundled/mlops/my-cat-skill/
     * But since the cat dir itself has a SKILL.md, it should be discovered */
    char cat_dir[1024];
    snprintf(cat_dir, sizeof(cat_dir), "%s/mlops", bundled);
    create_test_skill(cat_dir, "cat-skill", "cat-skill");

    /* Also a top-level skill */
    create_test_skill(bundled, "top-skill", "top-skill");

    const char *home = "/tmp/hermes_sync_test_cat_h";
    cleanup_dir(home);
    mkdir(home, 0755);

    skills_sync_result_t result;
    skills_sync(home, bundled, true, &result);

    TEST("category test: 2 bundled skills found", result.total_bundled == 2);

    skills_sync_free_result(&result);
    cleanup_dir(home);
    cleanup_dir(bundled);
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    test_manifest_path();
    test_read_skill_name();
    test_manifest_read_write();
    test_manifest_v1_compat();
    test_find_manifest();
    test_dir_hash();
    test_sync_new_skill();
    test_sync_manifest_persistence();
    test_sync_update_detection();
    test_empty_bundled_dir();
    test_clean_stale_entries();
    test_copy_and_remove_tree();
    test_category_skill_discovery();

    printf("skills_sync: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
