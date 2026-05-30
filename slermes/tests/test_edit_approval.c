/*
 * test_edit_approval.c — Tests for acp/edit_approval.c
 *
 * Tests: sensitive path detection, edit proposal building,
 * auto-approval policies, and approval request lifecycle.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_edit_approval.c src/acp/edit_approval.c \
 *       lib/libjson/json.c -o /tmp/hermes_test_edit_approval -lm
 *
 * Run:
 *   /tmp/hermes_test_edit_approval
 */

#include "acp/edit_approval.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  TEST: %s\n", name); } while(0)
#define PASS() do { printf("    PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("    FAIL: %s\n", msg); fail++; } while(0)

/* ================================================================
 *  Helper: create a JSON object from a string
 * ================================================================ */
static json_node_t *make_json(const char *str) {
    return json_parse(str, NULL);
}

/* ================================================================
 *  acp_is_sensitive_path tests
 * ================================================================ */
static void test_sensitive_path_null(void) {
    TEST("is_sensitive_path: NULL path");
    if (!acp_is_sensitive_path(NULL)) PASS();
    else FAIL("expected false for NULL");
}

static void test_sensitive_path_empty(void) {
    TEST("is_sensitive_path: empty string");
    if (!acp_is_sensitive_path("")) PASS();
    else FAIL("expected false for empty");
}

static void test_sensitive_path_env_file(void) {
    TEST("is_sensitive_path: .env file");
    if (acp_is_sensitive_path("/home/user/.env")) PASS();
    else FAIL("expected true for .env");
}

static void test_sensitive_path_env_local(void) {
    TEST("is_sensitive_path: .env.local");
    if (acp_is_sensitive_path("/project/.env.local")) PASS();
    else FAIL("expected true for .env.local");
}

static void test_sensitive_path_git_dir(void) {
    TEST("is_sensitive_path: /.git/ path");
    if (acp_is_sensitive_path("/repo/.git/config")) PASS();
    else FAIL("expected true for .git dir");
}

static void test_sensitive_path_ssh_dir(void) {
    TEST("is_sensitive_path: /.ssh/ path");
    if (acp_is_sensitive_path("/home/user/.ssh/authorized_keys")) PASS();
    else FAIL("expected true for .ssh dir");
}

static void test_sensitive_path_rsa_key(void) {
    TEST("is_sensitive_path: id_rsa");
    if (acp_is_sensitive_path("/home/user/.ssh/id_rsa")) PASS();
    else FAIL("expected true for id_rsa");
}

static void test_sensitive_path_ed25519_key(void) {
    TEST("is_sensitive_path: id_ed25519");
    if (acp_is_sensitive_path("/home/user/.ssh/id_ed25519")) PASS();
    else FAIL("expected true for id_ed25519");
}

static void test_sensitive_path_normal_file(void) {
    TEST("is_sensitive_path: normal file");
    if (!acp_is_sensitive_path("/home/user/project/main.c")) PASS();
    else FAIL("expected false for normal file");
}

static void test_sensitive_path_src_dir(void) {
    TEST("is_sensitive_path: src directory");
    if (!acp_is_sensitive_path("/project/src/main.c")) PASS();
    else FAIL("expected false for src dir");
}

static void test_sensitive_path_git_starts_with(void) {
    TEST("is_sensitive_path: starts with .git/");
    if (acp_is_sensitive_path(".git/HEAD")) PASS();
    else FAIL("expected true for .git/ prefix");
}

static void test_sensitive_path_ssh_starts_with(void) {
    TEST("is_sensitive_path: starts with .ssh/");
    if (acp_is_sensitive_path(".ssh/id_rsa")) PASS();
    else FAIL("expected true for .ssh/ prefix");
}

/* ================================================================
 *  acp_build_edit_proposal tests
 * ================================================================ */
static void test_build_proposal_null_tool(void) {
    TEST("build_proposal: NULL tool_name");
    acp_edit_proposal_t prop;
    char *err = NULL;
    if (!acp_build_edit_proposal(NULL, NULL, &prop, &err)) {
        PASS();
        free(err);
    } else FAIL("expected false for NULL args");
}

static void test_build_proposal_null_args(void) {
    TEST("build_proposal: NULL args_node");
    acp_edit_proposal_t prop;
    char *err = NULL;
    if (!acp_build_edit_proposal("write_file", NULL, &prop, &err)) {
        PASS();
        free(err);
    } else FAIL("expected false for NULL args");
}

static void test_build_proposal_write_file(void) {
    TEST("build_proposal: write_file with valid args");
    json_node_t *args = make_json(
        "{\"path\":\"/tmp/test.txt\",\"content\":\"hello world\"}"
    );
    acp_edit_proposal_t prop;
    char *err = NULL;
    bool ok = acp_build_edit_proposal("write_file", args, &prop, &err);
    json_free(args);

    if (ok) {
        if (strcmp(prop.tool_name, "write_file") == 0 &&
            strcmp(prop.path, "/tmp/test.txt") == 0 &&
            prop.new_text && strcmp(prop.new_text, "hello world") == 0) {
            PASS();
        } else {
            FAIL("proposal fields incorrect");
        }
        acp_edit_proposal_free(&prop);
    } else {
        FAIL(err ? err : "proposal build failed");
        free(err);
    }
}

static void test_build_proposal_write_file_no_path(void) {
    TEST("build_proposal: write_file missing path");
    json_node_t *args = make_json("{\"content\":\"hello\"}");
    acp_edit_proposal_t prop;
    char *err = NULL;
    bool ok = acp_build_edit_proposal("write_file", args, &prop, &err);
    json_free(args);

    if (!ok && err) {
        if (strstr(err, "path required")) {
            PASS();
        } else {
            FAIL(err);
        }
        free(err);
    } else {
        FAIL("expected error for missing path");
        acp_edit_proposal_free(&prop);
    }
}

static void test_build_proposal_patch(void) {
    TEST("build_proposal: patch with valid args");
    /* Create a test file so read_file_if_exists succeeds */
    FILE *f = fopen("/tmp/test_edit_approval_patch.txt", "w");
    assert(f);
    fprintf(f, "hello world\n");
    fclose(f);

    json_node_t *args = make_json(
        "{\"mode\":\"replace\",\"path\":\"/tmp/test_edit_approval_patch.txt\","
        "\"old_string\":\"hello\",\"new_string\":\"world\"}"
    );
    acp_edit_proposal_t prop;
    char *err = NULL;
    bool ok = acp_build_edit_proposal("patch", args, &prop, &err);
    json_free(args);

    if (ok) {
        if (strcmp(prop.tool_name, "patch") == 0 &&
            strcmp(prop.path, "/tmp/test_edit_approval_patch.txt") == 0 &&
            prop.old_text && strstr(prop.old_text, "hello") &&
            prop.new_text && strcmp(prop.new_text, "world") == 0) {
            PASS();
        } else {
            FAIL("proposal fields incorrect");
        }
        acp_edit_proposal_free(&prop);
    } else {
        FAIL(err ? err : "proposal build failed");
        free(err);
    }

    remove("/tmp/test_edit_approval_patch.txt");
}

static void test_build_proposal_non_mutation_tool(void) {
    TEST("build_proposal: non-mutation tool returns false");
    json_node_t *args = make_json("{\"path\":\"/tmp/test.txt\"}");
    acp_edit_proposal_t prop;
    char *err = NULL;
    bool ok = acp_build_edit_proposal("read_file", args, &prop, &err);
    json_free(args);

    if (!ok) {
        PASS();
    } else {
        FAIL("expected false for read_file");
        acp_edit_proposal_free(&prop);
    }
}

static void test_build_proposal_patch_replace_only(void) {
    TEST("build_proposal: patch with mode=patch returns false");
    json_node_t *args = make_json(
        "{\"mode\":\"patch\",\"path\":\"/tmp/test.txt\"}"
    );
    acp_edit_proposal_t prop;
    char *err = NULL;
    bool ok = acp_build_edit_proposal("patch", args, &prop, &err);
    json_free(args);

    if (!ok) {
        PASS();
    } else {
        FAIL("expected false for patch mode=patch");
        acp_edit_proposal_free(&prop);
    }
}

static void test_build_proposal_patch_missing_old_string(void) {
    TEST("build_proposal: patch replace missing old_string");
    json_node_t *args = make_json(
        "{\"mode\":\"replace\",\"path\":\"/tmp/test.txt\","
        "\"new_string\":\"world\"}"
    );
    acp_edit_proposal_t prop;
    char *err = NULL;
    bool ok = acp_build_edit_proposal("patch", args, &prop, &err);
    json_free(args);

    /* Patch replace without old_string will fail at read_file_if_exists
     * since the file doesn't exist */
    if (!ok) {
        PASS();
        free(err);
    } else {
        FAIL("expected false for missing old_string (file doesn't exist)");
        acp_edit_proposal_free(&prop);
    }
}

/* ================================================================
 *  acp_should_auto_approve_edit tests
 * ================================================================ */
static void test_auto_approve_null_proposal(void) {
    TEST("auto_approve: NULL proposal");
    if (!acp_should_auto_approve_edit(NULL, "session", NULL)) PASS();
    else FAIL("expected false for NULL proposal");
}

static void test_auto_approve_null_policy(void) {
    TEST("auto_approve: NULL policy");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/tmp/test.txt");

    if (!acp_should_auto_approve_edit(&prop, NULL, NULL)) PASS();
    else FAIL("expected false for NULL policy");
}

static void test_auto_approve_ask_policy(void) {
    TEST("auto_approve: ask policy always returns false");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/tmp/test.txt");

    if (!acp_should_auto_approve_edit(&prop, "ask", NULL)) PASS();
    else FAIL("expected false for ask policy");
}

static void test_auto_approve_session_policy(void) {
    TEST("auto_approve: session policy auto-approves");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/tmp/test.txt");

    if (acp_should_auto_approve_edit(&prop, "session", NULL)) PASS();
    else FAIL("expected true for session policy");
}

static void test_auto_approve_session_sensitive_path(void) {
    TEST("auto_approve: session policy blocked by sensitive path");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/home/user/.env");

    if (!acp_should_auto_approve_edit(&prop, "session", NULL)) PASS();
    else FAIL("expected false for .env even with session policy");
}

static void test_auto_approve_workspace_tmp(void) {
    TEST("auto_approve: workspace_session approves /tmp/ paths");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/tmp/test.txt");

    if (acp_should_auto_approve_edit(&prop, "workspace_session", "/home/user/proj")) PASS();
    else FAIL("expected true for /tmp path with workspace_session");
}

static void test_auto_approve_workspace_cwd(void) {
    TEST("auto_approve: workspace_session approves cwd paths");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/home/user/proj/src/main.c");

    if (acp_should_auto_approve_edit(&prop, "workspace_session", "/home/user/proj")) PASS();
    else FAIL("expected true for cwd path");
}

static void test_auto_approve_workspace_outside(void) {
    TEST("auto_approve: workspace_session denies outside paths");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/etc/passwd");

    if (!acp_should_auto_approve_edit(&prop, "workspace_session", "/home/user/proj")) PASS();
    else FAIL("expected false for path outside cwd");
}

static void test_auto_approve_workspace_tmp_macos(void) {
    TEST("auto_approve: workspace_session approves /private/tmp/ paths");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/private/tmp/test.txt");

    if (acp_should_auto_approve_edit(&prop, "workspace_session", "/home/user/proj")) PASS();
    else FAIL("expected true for /private/tmp path");
}

/* ================================================================
 *  Approval request lifecycle tests
 * ================================================================ */
static void test_approval_request_init(void) {
    TEST("approval: init sets status to PENDING");
    acp_approval_request_t req;
    memset(&req, 0xFF, sizeof(req));  /* poison */
    acp_approval_request_init(&req);

    if (req.status == ACP_APPROVAL_PENDING) PASS();
    else FAIL("status not PENDING after init");
}

static void test_approval_request_init_null(void) {
    TEST("approval: init NULL is safe");
    acp_approval_request_init(NULL);
    PASS();
}

static void test_approval_request_begin(void) {
    TEST("approval: begin sets proposal and pending");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    snprintf(prop.path, sizeof(prop.path), "/tmp/test.txt");
    prop.old_text = strdup("old");
    prop.new_text = strdup("new");

    acp_approval_request_t req;
    acp_approval_request_init(&req);
    acp_approval_request_begin(&req, &prop, 30.0);

    if (req.status == ACP_APPROVAL_PENDING &&
        req.timeout_sec == 30.0 &&
        strlen(req.proposal_id) > 0 &&
        strcmp(req.proposal.path, "/tmp/test.txt") == 0) {
        PASS();
    } else {
        FAIL("begin did not set proposal correctly");
    }

    acp_edit_proposal_free(&prop);
    acp_edit_proposal_free(&req.proposal);
}

static void test_approval_request_begin_null(void) {
    TEST("approval: begin NULL is safe");
    acp_approval_request_begin(NULL, NULL, 0);
    PASS();
}

static void test_approval_request_resolve_grant(void) {
    TEST("approval: resolve granted");
    acp_approval_request_t req;
    acp_approval_request_init(&req);
    acp_approval_request_resolve(&req, true);

    if (req.status == ACP_APPROVAL_GRANTED) PASS();
    else FAIL("status not GRANTED");
}

static void test_approval_request_resolve_deny(void) {
    TEST("approval: resolve denied");
    acp_approval_request_t req;
    acp_approval_request_init(&req);
    acp_approval_request_resolve(&req, false);

    if (req.status == ACP_APPROVAL_DENIED) PASS();
    else FAIL("status not DENIED");
}

static void test_approval_request_resolve_null(void) {
    TEST("approval: resolve NULL is safe");
    acp_approval_request_resolve(NULL, true);
    PASS();
}

static void test_approval_request_timed_out_pending(void) {
    TEST("approval: pending request not timed out");
    acp_approval_request_t req;
    acp_approval_request_init(&req);
    req.timeout_sec = 60.0;
    req.started_at = time(NULL);

    if (!acp_approval_request_timed_out(&req)) PASS();
    else FAIL("new request should not be timed out");
}

static void test_approval_request_timed_out_after_timeout(void) {
    TEST("approval: resolved request not timed out");
    acp_approval_request_t req;
    acp_approval_request_init(&req);
    req.timeout_sec = 60.0;
    req.started_at = 0;  /* far in the past */
    acp_approval_request_resolve(&req, true);

    if (!acp_approval_request_timed_out(&req)) PASS();
    else FAIL("resolved request should not show timed out");
}

static void test_approval_request_timed_out_zero_timeout(void) {
    TEST("approval: zero timeout never times out");
    acp_approval_request_t req;
    acp_approval_request_init(&req);
    req.timeout_sec = 0;
    req.started_at = 0;

    if (!acp_approval_request_timed_out(&req)) PASS();
    else FAIL("zero timeout should not time out");
}

static void test_approval_request_timed_out_negative_timeout(void) {
    TEST("approval: negative timeout never times out");
    acp_approval_request_t req;
    acp_approval_request_init(&req);
    req.timeout_sec = -1.0;
    req.started_at = 0;

    if (!acp_approval_request_timed_out(&req)) PASS();
    else FAIL("negative timeout should not time out");
}

static void test_approval_request_timed_out_past(void) {
    TEST("approval: request with past start times out");
    acp_approval_request_t req;
    acp_approval_request_init(&req);
    req.timeout_sec = 1.0;
    req.started_at = 0;  /* epoch = way in the past */

    if (acp_approval_request_timed_out(&req)) PASS();
    else FAIL("request with epoch start should be timed out");
}

static void test_approval_request_timed_out_null(void) {
    TEST("approval: timed_out NULL is safe");
    if (!acp_approval_request_timed_out(NULL)) PASS();
    else FAIL("NULL should return false");
}

/* ================================================================
 *  acp_edit_proposal_free tests
 * ================================================================ */
static void test_proposal_free_null(void) {
    TEST("proposal_free: NULL is safe");
    acp_edit_proposal_free(NULL);
    PASS();
}

static void test_proposal_free_normal(void) {
    TEST("proposal_free: frees allocations");
    acp_edit_proposal_t prop;
    memset(&prop, 0, sizeof(prop));
    prop.old_text = strdup("old content");
    prop.new_text = strdup("new content");

    acp_edit_proposal_free(&prop);

    if (prop.old_text == NULL && prop.new_text == NULL) PASS();
    else FAIL("pointers not NULL after free");
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Edit Approval Tests ===\n\n");

    /* Sensitive path detection */
    test_sensitive_path_null();
    test_sensitive_path_empty();
    test_sensitive_path_env_file();
    test_sensitive_path_env_local();
    test_sensitive_path_git_dir();
    test_sensitive_path_ssh_dir();
    test_sensitive_path_rsa_key();
    test_sensitive_path_ed25519_key();
    test_sensitive_path_normal_file();
    test_sensitive_path_src_dir();
    test_sensitive_path_git_starts_with();
    test_sensitive_path_ssh_starts_with();

    /* Edit proposal building */
    test_build_proposal_null_tool();
    test_build_proposal_null_args();
    test_build_proposal_write_file();
    test_build_proposal_write_file_no_path();
    test_build_proposal_patch();
    test_build_proposal_non_mutation_tool();
    test_build_proposal_patch_replace_only();
    test_build_proposal_patch_missing_old_string();

    /* Auto-approval policy */
    test_auto_approve_null_proposal();
    test_auto_approve_null_policy();
    test_auto_approve_ask_policy();
    test_auto_approve_session_policy();
    test_auto_approve_session_sensitive_path();
    test_auto_approve_workspace_tmp();
    test_auto_approve_workspace_cwd();
    test_auto_approve_workspace_outside();
    test_auto_approve_workspace_tmp_macos();

    /* Approval request lifecycle */
    test_approval_request_init();
    test_approval_request_init_null();
    test_approval_request_begin();
    test_approval_request_begin_null();
    test_approval_request_resolve_grant();
    test_approval_request_resolve_deny();
    test_approval_request_resolve_null();
    test_approval_request_timed_out_pending();
    test_approval_request_timed_out_after_timeout();
    test_approval_request_timed_out_zero_timeout();
    test_approval_request_timed_out_negative_timeout();
    test_approval_request_timed_out_past();
    test_approval_request_timed_out_null();

    /* Free */
    test_proposal_free_null();
    test_proposal_free_normal();

    printf("\n=== Results: %d pass, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
