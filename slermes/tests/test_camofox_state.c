/*
 * test_camofox_state.c — Tests for Camofox browser state helpers.
 */

#include "camofox_state.h"
#include "uuid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

static void test_state_dir(void)
{
    char buf[CAMOFOX_PATH_MAX];
    const char *result = camofox_state_dir("/home/test", buf);
    TEST("state dir non-null", result != NULL);
    TEST("state dir ends with browser_auth/camofox",
         strstr(result, "browser_auth/camofox") != NULL);
    TEST("state dir starts with home",
         strncmp(result, "/home/test", 10) == 0);

    TEST("null home returns NULL",
         camofox_state_dir(NULL, buf) == NULL);
}

static void test_gen_identity_basic(void)
{
    char user_id[CAMOFOX_USER_ID_MAX];
    char session_key[CAMOFOX_SESSION_KEY_MAX];

    bool ok = camofox_gen_identity("/tmp/hermes_test", "task-123",
                                    user_id, session_key);
    TEST("identity generation succeeds", ok);
    TEST("user_id starts with hermes_",
         strncmp(user_id, "hermes_", 7) == 0);
    TEST("user_id has 10 hex chars after prefix",
         strlen(user_id) == 17);  /* "hermes_" + 10 hex */
    TEST("session_key starts with task_",
         strncmp(session_key, "task_", 5) == 0);
    TEST("session_key has 16 hex chars after prefix",
         strlen(session_key) == 21);  /* "task_" + 16 hex */
}

static void test_gen_identity_deterministic(void)
{
    char uid1[CAMOFOX_USER_ID_MAX], sk1[CAMOFOX_SESSION_KEY_MAX];
    char uid2[CAMOFOX_USER_ID_MAX], sk2[CAMOFOX_SESSION_KEY_MAX];

    camofox_gen_identity("/tmp/hermes_test", "same-task", uid1, sk1);
    camofox_gen_identity("/tmp/hermes_test", "same-task", uid2, sk2);

    TEST("user_id is deterministic", strcmp(uid1, uid2) == 0);
    TEST("session_key is deterministic", strcmp(sk1, sk2) == 0);
}

static void test_gen_identity_different_home(void)
{
    char uid1[CAMOFOX_USER_ID_MAX], sk1[CAMOFOX_SESSION_KEY_MAX];
    char uid2[CAMOFOX_USER_ID_MAX], sk2[CAMOFOX_SESSION_KEY_MAX];

    camofox_gen_identity("/home/user1", "task", uid1, sk1);
    camofox_gen_identity("/home/user2", "task", uid2, sk2);

    TEST("different home = different user_id", strcmp(uid1, uid2) != 0);
    TEST("different home = different session_key", strcmp(sk1, sk2) != 0);
}

static void test_gen_identity_different_task(void)
{
    char uid1[CAMOFOX_USER_ID_MAX], sk1[CAMOFOX_SESSION_KEY_MAX];
    char uid2[CAMOFOX_USER_ID_MAX], sk2[CAMOFOX_SESSION_KEY_MAX];

    camofox_gen_identity("/tmp/hermes_test", "task-a", uid1, sk1);
    camofox_gen_identity("/tmp/hermes_test", "task-b", uid2, sk2);

    TEST("same home = same user_id", strcmp(uid1, uid2) == 0);
    TEST("different task = different session_key",
         strcmp(sk1, sk2) != 0);
}

static void test_gen_identity_default_task(void)
{
    char uid[CAMOFOX_USER_ID_MAX];
    char sk_default[CAMOFOX_SESSION_KEY_MAX];
    char sk_explicit[CAMOFOX_SESSION_KEY_MAX];

    camofox_gen_identity("/tmp/hermes_test", NULL, uid, sk_default);
    camofox_gen_identity("/tmp/hermes_test", "default", uid, sk_explicit);

    TEST("NULL task_id uses 'default'", strcmp(sk_default, sk_explicit) == 0);
}

static void test_gen_identity_null_output_handling(void)
{
    bool ok = camofox_gen_identity("/tmp/hermes_test", "task", NULL, NULL);
    TEST("NULL outputs don't crash", ok);
}

static void test_gen_identity_null_home(void)
{
    char uid[CAMOFOX_USER_ID_MAX];
    char sk[CAMOFOX_SESSION_KEY_MAX];
    bool ok = camofox_gen_identity(NULL, "task", uid, sk);
    TEST("NULL home returns false", !ok);
}

static void test_gen_identity_all_null(void)
{
    bool ok = camofox_gen_identity(NULL, NULL, NULL, NULL);
    TEST("all NULL returns false", !ok);
}

static void test_save_load_session(void)
{
    char url[CAMOFOX_PATH_MAX];
    bool ok;

    /* Save session */
    ok = camofox_save_session("/tmp/hermes_test_sessions", "test-browser",
                               "ws://127.0.0.1:9222/devtools/browser/test");
    TEST("save session succeeds", ok);

    /* Load session */
    ok = camofox_load_session("/tmp/hermes_test_sessions", "test-browser", url);
    TEST("load session succeeds", ok);
    TEST("cdp_url matches",
         strcmp(url, "ws://127.0.0.1:9222/devtools/browser/test") == 0);

    /* Delete session */
    ok = camofox_delete_session("/tmp/hermes_test_sessions", "test-browser");
    TEST("delete session succeeds", ok);

    /* After delete, load should fail */
    ok = camofox_load_session("/tmp/hermes_test_sessions", "test-browser", url);
    TEST("load after delete fails", !ok);
}

static void test_save_session_null_params(void)
{
    bool ok = camofox_save_session(NULL, "task", "url");
    TEST("save null home fails", !ok);

    ok = camofox_save_session("/tmp/t", NULL, "url");
    TEST("save null task fails", !ok);

    ok = camofox_save_session("/tmp/t", "task", NULL);
    TEST("save null url fails", !ok);
}

static void test_load_session_nonexistent(void)
{
    char url[CAMOFOX_PATH_MAX] = "dirty";
    bool ok = camofox_load_session("/tmp/nonexistent_XXXX", "ghost", url);
    TEST("load nonexistent fails", !ok);
    TEST("load nonexistent nulls output", url[0] == '\0');
}

static void test_delete_session_null(void)
{
    bool ok = camofox_delete_session(NULL, "task");
    TEST("delete null home fails", !ok);

    ok = camofox_delete_session("/tmp/t", NULL);
    TEST("delete null task fails", !ok);
}

static void test_delete_session_nonexistent(void)
{
    /* Deleting a nonexistent file should succeed (idempotent) */
    bool ok = camofox_delete_session("/tmp", "no-such-session-12345");
    TEST("delete nonexistent returns true", ok);
}

int main(void)
{
    printf("=== Camofox State Library Tests ===\n");

    test_state_dir();
    test_gen_identity_basic();
    test_gen_identity_deterministic();
    test_gen_identity_different_home();
    test_gen_identity_different_task();
    test_gen_identity_default_task();
    test_gen_identity_null_output_handling();
    test_gen_identity_null_home();
    test_gen_identity_all_null();
    test_save_load_session();
    test_save_session_null_params();
    test_load_session_nonexistent();
    test_delete_session_null();
    test_delete_session_nonexistent();

    printf("\nResults: %d passed, %d failed, %d total\n",
           passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
