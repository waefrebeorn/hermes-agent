/*
 * test_subdir_hints.c — Tests for hermes_subdir_hints
 * Verifies subdirectory hint discovery and loading.
 */

#include "hermes_subdir_hints.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int tests_passed = 0;
static int tests_failed = 0;

/* Forward declarations */
static void cleanup(void);

#define TEST(name) do { \
    printf("  TEST: %s\n", name); \
} while(0)

#define PASS() do { \
    printf("    PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("    FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

/* Helper: create a temp directory */
static char *make_temp_dir(void) {
    char template[] = "/tmp/hermes_test_XXXXXX";
    char *dir = mkdtemp(template);
    return dir ? strdup(dir) : NULL;
}

/* Helper: write content to a file */
static int write_test_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    size_t n = fwrite(content, 1, strlen(content), f);
    fclose(f);
    return (int)n;
}

/* ================================================================
 *  Test: init and cleanup cycle
 * ================================================================ */

static void test_init_cleanup(void) {
    TEST("init and cleanup cycle");
    subdir_hints_init("/tmp");
    subdir_hints_cleanup();
    PASS();
}

static void test_init_null_defaults_to_cwd(void) {
    TEST("init with NULL defaults to CWD");
    subdir_hints_init(NULL);
    subdir_hints_cleanup();
    PASS();
}

/* ================================================================
 *  Test: path-based hint discovery (read_file, patch, etc.)
 * ================================================================ */

static void test_read_file_discovers_agents_md(void) {
    TEST("read_file discovers AGENTS.md in subdirectory");
    char *tmpdir = make_temp_dir();
    if (!tmpdir) { FAIL("cannot create tmpdir"); return; }

    /* Create a subdirectory with AGENTS.md */
    char subdir[4096];
    snprintf(subdir, sizeof(subdir), "%s/subproj", tmpdir);
    mkdir(subdir, 0755);

    char agents_path[4096];
    snprintf(agents_path, sizeof(agents_path), "%s/AGENTS.md", subdir);
    write_test_file(agents_path, "# Subproject Guide\n\nUse caution.");

    /* Init with tmpdir */
    subdir_hints_init(tmpdir);

    /* Simulate read_file tool call pointing into subproj */
    char tool_args[4096];
    snprintf(tool_args, sizeof(tool_args),
        "{\"path\":\"%s/src/main.c\"}", subdir);

    char *result = subdir_hints_check("read_file", tool_args);
    if (!result) { FAIL("expected hint but got NULL"); cleanup(); free(tmpdir); return; }
    if (!strstr(result, "AGENTS.md")) { FAIL("missing AGENTS.md in hint"); free(result); cleanup(); free(tmpdir); return; }
    if (!strstr(result, "Subproject Guide")) { FAIL("missing content"); free(result); cleanup(); free(tmpdir); return; }
    free(result);

    /* Second call should return NULL (already loaded) */
    char *result2 = subdir_hints_check("read_file", tool_args);
    if (result2 != NULL) { FAIL("second call should return NULL"); free(result2); }

    /* Cleanup */
    unlink(agents_path);
    rmdir(subdir);
    rmdir(tmpdir);
    free(tmpdir);
    cleanup();
    PASS();
}

static void test_patch_discovers_claude_md(void) {
    TEST("patch discovers CLAUDE.md in subdirectory");
    char *tmpdir = make_temp_dir();
    if (!tmpdir) { FAIL("cannot create tmpdir"); return; }

    char subdir[4096];
    snprintf(subdir, sizeof(subdir), "%s/lib", tmpdir);
    mkdir(subdir, 0755);

    char claude_path[4096];
    snprintf(claude_path, sizeof(claude_path), "%s/CLAUDE.md", subdir);
    write_test_file(claude_path, "## Library conventions\n\nUse snake_case.");

    subdir_hints_init(tmpdir);

    char tool_args[4096];
    snprintf(tool_args, sizeof(tool_args),
        "{\"path\":\"%s/utils.c\"}", subdir);

    char *result = subdir_hints_check("patch", tool_args);
    if (!result) { FAIL("expected hint but got NULL"); cleanup(); free(tmpdir); return; }
    if (!strstr(result, "CLAUDE.md")) { FAIL("missing CLAUDE.md"); free(result); cleanup(); free(tmpdir); return; }
    free(result);

    unlink(claude_path);
    rmdir(subdir);
    rmdir(tmpdir);
    free(tmpdir);
    cleanup();
    PASS();
}

/* ================================================================
 *  Test: terminal commands — extract path from cd + command
 * ================================================================ */

static void test_terminal_cd_discovers_hints(void) {
    TEST("terminal 'cd' discovers hints in target dir");
    char *tmpdir = make_temp_dir();
    if (!tmpdir) { FAIL("cannot create tmpdir"); return; }

    char subdir[4096];
    snprintf(subdir, sizeof(subdir), "%s/module_a", tmpdir);
    mkdir(subdir, 0755);

    char hints_path[4096];
    snprintf(hints_path, sizeof(hints_path), "%s/.cursorrules", subdir);
    write_test_file(hints_path, "Always use tabs for indentation.");

    subdir_hints_init(tmpdir);

    /* Simulate a terminal call with cd */
    char tool_args[4096];
    snprintf(tool_args, sizeof(tool_args),
        "{\"command\":\"cd %s && make build\"}", subdir);

    char *result = subdir_hints_check("terminal", tool_args);
    if (!result) { FAIL("expected hint from terminal cd"); cleanup(); free(tmpdir); return; }
    if (!strstr(result, ".cursorrules")) { FAIL("missing .cursorrules"); free(result); cleanup(); free(tmpdir); return; }
    free(result);

    unlink(hints_path);
    rmdir(subdir);
    rmdir(tmpdir);
    free(tmpdir);
    cleanup();
    PASS();
}

static void test_terminal_cd_relative_parent(void) {
    TEST("terminal 'cd ..' resolves relative to working dir");
    char *tmpdir = make_temp_dir();
    if (!tmpdir) { FAIL("cannot create tmpdir"); return; }

    /* Create CWD-like subdir and a target subdir */
    char cwd_dir[4096];
    snprintf(cwd_dir, sizeof(cwd_dir), "%s/workspace", tmpdir);
    mkdir(cwd_dir, 0755);

    char subdir[4096];
    snprintf(subdir, sizeof(subdir), "%s/target", tmpdir);
    mkdir(subdir, 0755);

    char agents_path[4096];
    snprintf(agents_path, sizeof(agents_path), "%s/AGENTS.md", subdir);
    write_test_file(agents_path, "# Target Project\n\nDocs here.");

    subdir_hints_init(cwd_dir);

    /* Simulate read_file into target using relative path */
    char tool_args[4096];
    snprintf(tool_args, sizeof(tool_args),
        "{\"path\":\"../target/file.c\"}");

    char *result = subdir_hints_check("read_file", tool_args);
    if (!result) { FAIL("expected hint for relative path"); cleanup(); free(tmpdir); return; }
    if (!strstr(result, "AGENTS.md")) { FAIL("missing AGENTS.md"); free(result); cleanup(); free(tmpdir); return; }
    free(result);

    unlink(agents_path);
    rmdir(subdir);
    rmdir(cwd_dir);
    rmdir(tmpdir);
    free(tmpdir);
    cleanup();
    PASS();
}

/* ================================================================
 *  Test: unknown tools return NULL
 * ================================================================ */

static void test_unknown_tool_returns_null(void) {
    TEST("unknown tool returns NULL");
    char *tmpdir = make_temp_dir();
    if (!tmpdir) { FAIL("cannot create tmpdir"); return; }

    subdir_hints_init(tmpdir);

    char *result = subdir_hints_check("unknown_tool_name",
        "{\"path\":\"/some/path\"}");
    if (result != NULL) { FAIL("expected NULL for unknown tool"); free(result); }

    rmdir(tmpdir);
    free(tmpdir);
    cleanup();
    PASS();
}

static void test_no_path_arg_returns_null(void) {
    TEST("tool without path arg returns NULL");
    char *tmpdir = make_temp_dir();
    if (!tmpdir) { FAIL("cannot create tmpdir"); return; }

    subdir_hints_init(tmpdir);

    char *result = subdir_hints_check("read_file",
        "{\"content\":\"just text\"}");
    if (result != NULL) { FAIL("expected NULL for no path arg"); free(result); }

    rmdir(tmpdir);
    free(tmpdir);
    cleanup();
    PASS();
}

/* ================================================================
 *  Test: workdir arg (for terminal, process tools)
 * ================================================================ */

static void test_workdir_arg_discovers_hints(void) {
    TEST("workdir arg discovers hints");
    char *tmpdir = make_temp_dir();
    if (!tmpdir) { FAIL("cannot create tmpdir"); return; }

    char subdir[4096];
    snprintf(subdir, sizeof(subdir), "%s/deploy", tmpdir);
    mkdir(subdir, 0755);

    char agents_path[4096];
    snprintf(agents_path, sizeof(agents_path), "%s/AGENTS.md", subdir);
    write_test_file(agents_path, "# Deploy Guide\n\nRun deploy.sh");

    subdir_hints_init(tmpdir);

    char tool_args[4096];
    snprintf(tool_args, sizeof(tool_args),
        "{\"code\":\"make deploy\",\"workdir\":\"%s\"}", subdir);

    char *result = subdir_hints_check("execute_code", tool_args);
    if (!result) { FAIL("expected hint from workdir arg"); cleanup(); free(tmpdir); return; }
    if (!strstr(result, "Deploy Guide")) { FAIL("missing content"); free(result); cleanup(); free(tmpdir); return; }
    free(result);

    unlink(agents_path);
    rmdir(subdir);
    rmdir(tmpdir);
    free(tmpdir);
    cleanup();
    PASS();
}

/* ================================================================
 *  Runner helpers
 * ================================================================ */

static void cleanup(void) {
    subdir_hints_cleanup();
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== Subdirectory Hints Tests ===\n\n");

    printf("--- Init/Cleanup ---\n\n");
    test_init_cleanup();
    test_init_null_defaults_to_cwd();

    printf("\n--- Path-based discovery ---\n\n");
    test_read_file_discovers_agents_md();
    test_patch_discovers_claude_md();

    printf("\n--- Terminal cd discovery ---\n\n");
    test_terminal_cd_discovers_hints();
    test_terminal_cd_relative_parent();

    printf("\n--- Workdir arg discovery ---\n\n");
    test_workdir_arg_discovers_hints();

    printf("\n--- Edge cases ---\n\n");
    test_unknown_tool_returns_null();
    test_no_path_arg_returns_null();

    printf("\n==============================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
