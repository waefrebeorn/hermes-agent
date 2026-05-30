/*
 * test_skill_audit.c — Tests for skill_audit module.
 * Tests: pattern detection, file scanning, directory scanning, format.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "skill_audit.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

/* Helper: check if result has a pattern ID */
static bool has_pid(skill_audit_result_t *r, const char *pid)
{
    for (int i = 0; i < r->count; i++)
        if (strcmp(r->findings[i].pattern_id, pid) == 0)
            return true;
    return false;
}

/* Helper: write a file */
static void write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%s", content); fclose(f); }
}

int main(void)
{
    printf("=== Skill Audit Tests ===\n\n");

    /* ── Non-Python file returns empty ── */
    printf("-- Non-Python file --\n");
    {
        const char *tmp = "/tmp/hermes_audit1.py";
        write_file(tmp, "import importlib\n");
        skill_audit_result_t r = skill_audit_scan_file("/tmp/hermes_audit1.sh");
        TEST("non-.py returns empty", r.count == 0);
        unlink(tmp);
    }

    /* ── importlib.import_module detected ── */
    printf("\n-- importlib.import_module --\n");
    {
        write_file("/tmp/hermes_audit_im.py",
                   "import importlib\n"
                   "parts = ['o', 's']\n"
                   "m = importlib.import_module(''.join(parts))\n"
                   "e = 1\n");
        skill_audit_result_t r = skill_audit_scan_file("/tmp/hermes_audit_im.py");
        TEST("dynamic_import found", has_pid(&r, "dynamic_import"));
        TEST("importlib_import found", has_pid(&r, "importlib_import"));
        unlink("/tmp/hermes_audit_im.py");
    }

    /* ── Syntax error doesn't crash ── */
    printf("\n-- Syntax error --\n");
    {
        write_file("/tmp/hermes_audit_bad.py", "def broken(\n");
        skill_audit_result_t r = skill_audit_scan_file("/tmp/hermes_audit_bad.py");
        /* Should just return empty or whatever it finds (it's text-based so it
         * won't crash on syntax errors — just may not match patterns) */
        TEST("syntax error doesn't crash", r.count >= 0);
        unlink("/tmp/hermes_audit_bad.py");
    }

    /* ── Lookalike not flagged ── */
    printf("\n-- Import lookalike --\n");
    {
        write_file("/tmp/hermes_audit_ok.py",
                   "import importer\nfrom importer import x\n");
        skill_audit_result_t r = skill_audit_scan_file("/tmp/hermes_audit_ok.py");
        TEST("importer not flagged", !has_pid(&r, "importlib_import"));
        unlink("/tmp/hermes_audit_ok.py");
    }

    /* ── Literal __import__ not flagged ── */
    printf("\n-- Literal __import__ --\n");
    {
        write_file("/tmp/hermes_audit_lit.py",
                   "m = __import__('os')\n");
        skill_audit_result_t r = skill_audit_scan_file("/tmp/hermes_audit_lit.py");
        TEST("literal __import__ NOT flagged as computed",
             !has_pid(&r, "dynamic_import_computed"));
        unlink("/tmp/hermes_audit_lit.py");
    }

    /* ── Directory scans recursively, skips ignored ── */
    printf("\n-- Directory scan --\n");
    {
        mkdir("/tmp/hermes_audit_dir", 0755);
        write_file("/tmp/hermes_audit_dir/main.py", "import importlib\n");
        mkdir("/tmp/hermes_audit_dir/sub", 0755);
        write_file("/tmp/hermes_audit_dir/sub/u.py",
                   "from importlib.util import find_spec\n");
        mkdir("/tmp/hermes_audit_dir/__pycache__", 0755);
        write_file("/tmp/hermes_audit_dir/__pycache__/junk.py",
                   "import importlib\n");
        mkdir("/tmp/hermes_audit_dir/.venv", 0755);
        write_file("/tmp/hermes_audit_dir/.venv/x.py", "import importlib\n");

        skill_audit_result_t r = skill_audit_scan_directory("/tmp/hermes_audit_dir");
        int importlib_count = 0;
        for (int i = 0; i < r.count; i++)
            if (strcmp(r.findings[i].pattern_id, "importlib_import") == 0)
                importlib_count++;
        TEST("exactly 2 importlib_import (skipped cache/venv)",
             importlib_count == 2);

        /* Cleanup */
        unlink("/tmp/hermes_audit_dir/__pycache__/junk.py");
        rmdir("/tmp/hermes_audit_dir/__pycache__");
        unlink("/tmp/hermes_audit_dir/.venv/x.py");
        rmdir("/tmp/hermes_audit_dir/.venv");
        unlink("/tmp/hermes_audit_dir/sub/u.py");
        rmdir("/tmp/hermes_audit_dir/sub");
        unlink("/tmp/hermes_audit_dir/main.py");
        rmdir("/tmp/hermes_audit_dir");
    }

    /* ── Missing path returns empty ── */
    printf("\n-- Missing path --\n");
    {
        skill_audit_result_t r = skill_audit_scan_path("/tmp/does_not_exist_xyzzy");
        TEST("missing path returns empty", r.count == 0);
    }

    /* ── getattr and __dict__ detected ── */
    printf("\n-- getattr and __dict__ --\n");
    {
        write_file("/tmp/hermes_audit_g.py",
                   "name = 'x'\nv = getattr(o, name)\nv = o.__dict__[name]\n");
        skill_audit_result_t r = skill_audit_scan_file("/tmp/hermes_audit_g.py");
        TEST("dynamic_getattr found", has_pid(&r, "dynamic_getattr"));
        TEST("dict_access found", has_pid(&r, "dict_access"));
        unlink("/tmp/hermes_audit_g.py");
    }

    /* ── Format report empty ── */
    printf("\n-- Format report (empty) --\n");
    {
        skill_audit_result_t r = { .count = 0 };
        char buf[4096];
        skill_audit_format_report(&r, NULL, buf, sizeof(buf));
        TEST("empty report contains 'No dynamic'", strstr(buf, "No dynamic") != NULL);
    }

    /* ── Format report with findings ── */
    printf("\n-- Format report (with findings) --\n");
    {
        skill_audit_result_t r = { .count = 2 };
        strcpy(r.findings[0].file, "a.py");
        r.findings[0].line = 1;
        strcpy(r.findings[0].pattern_id, "importlib_import");
        strcpy(r.findings[0].description, "import importlib — test");

        strcpy(r.findings[1].file, "a.py");
        r.findings[1].line = 3;
        strcpy(r.findings[1].pattern_id, "dynamic_import");
        strcpy(r.findings[1].description, "importlib.import_module() — test");

        char buf[4096];
        skill_audit_format_report(&r, "test", buf, sizeof(buf));
        TEST("report contains skill name", strstr(buf, "test") != NULL);
        TEST("report contains a.py", strstr(buf, "a.py") != NULL);
        TEST("report contains L1", strstr(buf, "L1") != NULL);
        TEST("report contains L3", strstr(buf, "L3") != NULL);
        TEST("report contains diagnostic hints",
             strstr(buf, "diagnostic hints") != NULL);
    }

    /* ── Summary ── */
    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
