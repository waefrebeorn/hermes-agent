/*
 * test_skill_usage.c — Tests for skill usage telemetry library.
 * Port of Python tools/skill_usage.py tests.
 */

#include "skill_usage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

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

/* Helper: create a temp hermes_home with skills dir */
static void create_test_home(const char *home)
{
    mkdir(home, 0755);
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/skills", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/skills/.archive", home);
    mkdir(dir, 0755);
}

/* Helper: remove temp home */
static void cleanup_test_home(const char *home)
{
    /* Simple rm -rf approach */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", home);
    system(cmd);
}

/* ─── Tests ─────────────────────────────────────────────── */

static void test_file_path(void)
{
    char path[SKILL_USAGE_MAX_PATH];
    skill_usage_file_path("/tmp/hermes_test", path);
    TEST("file path ends with .usage.json",
         strstr(path, ".usage.json") != NULL);
    TEST("file path contains skills dir",
         strstr(path, "/skills/") != NULL);
}

static void test_archive_dir_path(void)
{
    char path[SKILL_USAGE_MAX_PATH];
    skill_usage_archive_dir("/tmp/hermes_test", path);
    TEST("archive dir ends with .archive",
         strstr(path, ".archive") != NULL);
}

static void test_now_iso(void)
{
    char buf[32];
    skill_usage_now_iso(buf);
    TEST("now iso not empty", strlen(buf) > 0);
    TEST("now iso starts with year", buf[0] == '2');
    TEST("now iso contains T separator", strchr(buf, 'T') != NULL);
}

static void test_load_empty_home(void)
{
    const char *home = "/tmp/hermes_test_su_empty";
    create_test_home(home);

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    TEST("empty home -> map count 0", map.count == 0);

    cleanup_test_home(home);
}

static void test_save_and_load(void)
{
    const char *home = "/tmp/hermes_test_su_saveload";
    create_test_home(home);

    /* Create a map with one record */
    skill_usage_map_t map;
    memset(&map, 0, sizeof(map));
    skill_usage_record_t *r = &map.records[0];
    strcpy(r->name, "test-skill");
    strcpy(r->state, SKILL_USAGE_STATE_ACTIVE);
    r->use_count = 5;
    r->view_count = 3;
    skill_usage_now_iso(r->created_at);
    map.count = 1;

    int ret = skill_usage_save(home, &map);
    TEST("save returns 0", ret == 0);

    /* Load it back */
    skill_usage_map_t loaded;
    skill_usage_load(home, &loaded);
    TEST("loaded has 1 record", loaded.count == 1);
    TEST("loaded name matches", strcmp(loaded.records[0].name, "test-skill") == 0);
    TEST("loaded use_count matches", loaded.records[0].use_count == 5);
    TEST("loaded view_count matches", loaded.records[0].view_count == 3);
    TEST("loaded state is active",
         strcmp(loaded.records[0].state, SKILL_USAGE_STATE_ACTIVE) == 0);
    TEST("loaded created_at not empty",
         strlen(loaded.records[0].created_at) > 0);
    TEST("loaded pinned is false", loaded.records[0].pinned == false);

    cleanup_test_home(home);
}

static void test_find(void)
{
    skill_usage_map_t map;
    memset(&map, 0, sizeof(map));
    strcpy(map.records[0].name, "alpha");
    strcpy(map.records[1].name, "beta");
    strcpy(map.records[2].name, "gamma");
    map.count = 3;

    TEST("find alpha returns 0", skill_usage_find(&map, "alpha") == 0);
    TEST("find beta returns 1", skill_usage_find(&map, "beta") == 1);
    TEST("find gamma returns 2", skill_usage_find(&map, "gamma") == 2);
    TEST("find nonexistent returns -1", skill_usage_find(&map, "zeta") == -1);
    TEST("find NULL returns -1", skill_usage_find(&map, NULL) == -1);
    TEST("find empty returns -1", skill_usage_find(&map, "") == -1);
}

static void test_get_record(void)
{
    skill_usage_map_t map;
    memset(&map, 0, sizeof(map));
    strcpy(map.records[0].name, "existing");
    strcpy(map.records[0].state, SKILL_USAGE_STATE_STALE);
    map.records[0].use_count = 10;
    map.count = 1;

    skill_usage_record_t rec;
    skill_usage_get_record(&map, "existing", &rec);
    TEST("get existing name", strcmp(rec.name, "existing") == 0);
    TEST("get existing state", strcmp(rec.state, SKILL_USAGE_STATE_STALE) == 0);
    TEST("get existing use_count", rec.use_count == 10);

    skill_usage_get_record(&map, "nonexistent", &rec);
    TEST("get nonexistent uses passed name",
         strcmp(rec.name, "nonexistent") == 0);
    TEST("get nonexistent default state",
         strcmp(rec.state, SKILL_USAGE_STATE_ACTIVE) == 0);
    TEST("get nonexistent use_count 0", rec.use_count == 0);

    skill_usage_get_record(&map, NULL, &rec);
    TEST("get NULL returns empty", rec.use_count == 0);
}

static void test_bump_view(void)
{
    const char *home = "/tmp/hermes_test_su_bview";
    create_test_home(home);

    int ret = skill_usage_bump_view(home, "view-skill");
    TEST("bump_view returns 0", ret == 0);

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    TEST("bump_view created record", map.count > 0);
    TEST("bump_view view_count=1", map.records[0].view_count == 1);
    TEST("bump_view last_viewed_at set",
         strlen(map.records[0].last_viewed_at) > 0);

    /* Second bump */
    skill_usage_bump_view(home, "view-skill");
    skill_usage_load(home, &map);
    TEST("bump_view view_count=2 after second bump",
         map.records[0].view_count == 2);

    cleanup_test_home(home);
}

static void test_bump_use(void)
{
    const char *home = "/tmp/hermes_test_su_buse";
    create_test_home(home);

    int ret = skill_usage_bump_use(home, "use-skill");
    TEST("bump_use returns 0", ret == 0);

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    TEST("bump_use use_count=1", map.records[0].use_count == 1);
    TEST("bump_use last_used_at set",
         strlen(map.records[0].last_used_at) > 0);

    skill_usage_bump_use(home, "use-skill");
    skill_usage_load(home, &map);
    TEST("bump_use use_count=2", map.records[0].use_count == 2);

    cleanup_test_home(home);
}

static void test_bump_patch(void)
{
    const char *home = "/tmp/hermes_test_su_bpatch";
    create_test_home(home);

    int ret = skill_usage_bump_patch(home, "patch-skill");
    TEST("bump_patch returns 0", ret == 0);

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    TEST("bump_patch patch_count=1", map.records[0].patch_count == 1);
    TEST("bump_patch last_patched_at set",
         strlen(map.records[0].last_patched_at) > 0);

    skill_usage_bump_patch(home, "patch-skill");
    skill_usage_load(home, &map);
    TEST("bump_patch patch_count=2", map.records[0].patch_count == 2);

    cleanup_test_home(home);
}

static void test_mark_agent_created(void)
{
    const char *home = "/tmp/hermes_test_su_mark";
    create_test_home(home);

    int ret = skill_usage_mark_agent_created(home, "agent-skill");
    TEST("mark_agent_created returns 0", ret == 0);

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    int idx = skill_usage_find(&map, "agent-skill");
    TEST("mark_agent_created record exists", idx >= 0);
    if (idx >= 0) {
        TEST("created_by is agent",
             strcmp(map.records[idx].created_by, "agent") == 0);
    }

    cleanup_test_home(home);
}

static void test_set_state(void)
{
    const char *home = "/tmp/hermes_test_su_state";
    create_test_home(home);

    /* Create a record first */
    skill_usage_bump_use(home, "state-skill");

    int ret = skill_usage_set_state(home, "state-skill", SKILL_USAGE_STATE_STALE);
    TEST("set_state stale returns 0", ret == 0);

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    int idx = skill_usage_find(&map, "state-skill");
    TEST("set_state stale matches", idx >= 0);
    if (idx >= 0) {
        TEST("state is stale",
             strcmp(map.records[idx].state, SKILL_USAGE_STATE_STALE) == 0);
    }

    /* Set to archived */
    ret = skill_usage_set_state(home, "state-skill", SKILL_USAGE_STATE_ARCHIVED);
    TEST("set_state archived returns 0", ret == 0);

    skill_usage_load(home, &map);
    idx = skill_usage_find(&map, "state-skill");
    if (idx >= 0) {
        TEST("state is archived",
             strcmp(map.records[idx].state, SKILL_USAGE_STATE_ARCHIVED) == 0);
        TEST("archived_at is set",
             strlen(map.records[idx].archived_at) > 0);
    }

    /* Invalid state */
    ret = skill_usage_set_state(home, "state-skill", "invalid");
    TEST("set_state invalid returns -1", ret == -1);

    cleanup_test_home(home);
}

static void test_set_pinned(void)
{
    const char *home = "/tmp/hermes_test_su_pin";
    create_test_home(home);

    skill_usage_bump_use(home, "pin-skill");

    int ret = skill_usage_set_pinned(home, "pin-skill", true);
    TEST("set_pinned true returns 0", ret == 0);

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    int idx = skill_usage_find(&map, "pin-skill");
    if (idx >= 0) {
        TEST("pinned is true", map.records[idx].pinned == true);
    }

    ret = skill_usage_set_pinned(home, "pin-skill", false);
    TEST("set_pinned false returns 0", ret == 0);

    skill_usage_load(home, &map);
    idx = skill_usage_find(&map, "pin-skill");
    if (idx >= 0) {
        TEST("pinned is false", map.records[idx].pinned == false);
    }

    cleanup_test_home(home);
}

static void test_forget(void)
{
    const char *home = "/tmp/hermes_test_su_forget";
    create_test_home(home);

    skill_usage_bump_use(home, "forget-me");

    int ret = skill_usage_forget(home, "forget-me");
    TEST("forget returns 0", ret == 0);

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    int idx = skill_usage_find(&map, "forget-me");
    TEST("forget removes record", idx < 0);

    /* Forget nonexistent */
    ret = skill_usage_forget(home, "never-existed");
    TEST("forget nonexistent returns 0", ret == 0);

    cleanup_test_home(home);
}

static void test_latest_activity(void)
{
    skill_usage_record_t r;
    memset(&r, 0, sizeof(r));
    char buf[SKILL_USAGE_MAX_VALUE];

    /* No activity */
    const char *ret = skill_usage_latest_activity(&r, buf);
    TEST("no activity returns NULL", ret == NULL);

    /* Only use */
    strcpy(r.last_used_at, "2026-01-01T12:00:00");
    ret = skill_usage_latest_activity(&r, buf);
    TEST("use activity returns timestamp", ret != NULL);
    if (ret) TEST("use activity matches", strcmp(ret, "2026-01-01T12:00:00") == 0);

    /* View is newer */
    strcpy(r.last_viewed_at, "2026-03-15T08:00:00");
    ret = skill_usage_latest_activity(&r, buf);
    if (ret) TEST("view newer than use", strcmp(ret, "2026-03-15T08:00:00") == 0);

    /* Patch is newest */
    strcpy(r.last_patched_at, "2026-05-01T22:00:00");
    ret = skill_usage_latest_activity(&r, buf);
    if (ret) TEST("patch newest", strcmp(ret, "2026-05-01T22:00:00") == 0);
}

static void test_activity_count(void)
{
    skill_usage_record_t r;
    memset(&r, 0, sizeof(r));

    TEST("zero count", skill_usage_activity_count(&r) == 0);

    r.use_count = 3;
    r.view_count = 5;
    r.patch_count = 2;
    TEST("total count 10", skill_usage_activity_count(&r) == 10);
}

static void test_multiple_skills(void)
{
    const char *home = "/tmp/hermes_test_su_multi";
    create_test_home(home);

    skill_usage_bump_use(home, "skill-a");
    skill_usage_bump_view(home, "skill-b");
    skill_usage_bump_patch(home, "skill-c");

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    TEST("multiple skills: 3 records", map.count == 3);

    int a = skill_usage_find(&map, "skill-a");
    int b = skill_usage_find(&map, "skill-b");
    int c = skill_usage_find(&map, "skill-c");
    TEST("skill-a exists", a >= 0);
    TEST("skill-b exists", b >= 0);
    TEST("skill-c exists", c >= 0);

    if (a >= 0) TEST("skill-a use=1", map.records[a].use_count == 1);
    if (b >= 0) TEST("skill-b view=1", map.records[b].view_count == 1);
    if (c >= 0) TEST("skill-c patch=1", map.records[c].patch_count == 1);

    cleanup_test_home(home);
}

static void test_save_loaded_counts(void)
{
    /* Bump multiple times and verify counts */
    const char *home = "/tmp/hermes_test_su_counts";
    create_test_home(home);

    skill_usage_bump_view(home, "counter");
    skill_usage_bump_view(home, "counter");
    skill_usage_bump_view(home, "counter");
    skill_usage_bump_use(home, "counter");
    skill_usage_bump_use(home, "counter");
    skill_usage_bump_patch(home, "counter");

    skill_usage_map_t map;
    skill_usage_load(home, &map);
    int idx = skill_usage_find(&map, "counter");
    if (idx >= 0) {
        TEST("counter view=3", map.records[idx].view_count == 3);
        TEST("counter use=2", map.records[idx].use_count == 2);
        TEST("counter patch=1", map.records[idx].patch_count == 1);
        TEST("counter total=6", skill_usage_activity_count(&map.records[idx]) == 6);
    }

    cleanup_test_home(home);
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    test_file_path();
    test_archive_dir_path();
    test_now_iso();
    test_load_empty_home();
    test_save_and_load();
    test_find();
    test_get_record();
    test_bump_view();
    test_bump_use();
    test_bump_patch();
    test_mark_agent_created();
    test_set_state();
    test_set_pinned();
    test_forget();
    test_latest_activity();
    test_activity_count();
    test_multiple_skills();
    test_save_loaded_counts();

    printf("skill_usage: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
