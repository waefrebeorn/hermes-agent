#ifndef HERMES_SKILL_AUDIT_H
#define HERMES_SKILL_AUDIT_H

/*
 * skill_audit.h — AST-level deep audit for skill Python files.
 * Port of Python tools/skills_ast_audit.py.
 *
 * Opt-in diagnostic scanner that flags dynamic import / dynamic attribute
 * access patterns in skill Python files. Findings are hints for human
 * review, not security verdicts.
 *
 * CLI equivalent: hermes skills audit --deep
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ── */

#define SKILLAUDIT_MAX_PATH     1024
#define SKILLAUDIT_MAX_FINDINGS 128
#define SKILLAUDIT_DESC_LEN     256
#define SKILLAUDIT_PID_LEN      64

/* ── Types ── */

/** A single audit finding: (file, line, pattern_id, description) */
typedef struct {
    char file[SKILLAUDIT_MAX_PATH];
    int  line;
    char pattern_id[SKILLAUDIT_PID_LEN];
    char description[SKILLAUDIT_DESC_LEN];
} skill_audit_finding_t;

/** Collection of findings from a scan */
typedef struct {
    skill_audit_finding_t findings[SKILLAUDIT_MAX_FINDINGS];
    int count;
} skill_audit_result_t;

/* ── Scanning API ── */

/**
 * Scan a single .py file for dangerous dynamic import/access patterns.
 * Returns findings for the file. If path is not a .py file, returns empty.
 */
skill_audit_result_t skill_audit_scan_file(const char *path);

/**
 * Recursively scan all .py files under a directory.
 * Skips __pycache__, .venv, venv, node_modules directories.
 * Returns findings from all scanned files.
 */
skill_audit_result_t skill_audit_scan_directory(const char *dir_path);

/**
 * Scan a path (file or directory). Dispatches to scan_file or
 * scan_directory depending on the type of the path.
 */
skill_audit_result_t skill_audit_scan_path(const char *path);

/* ── Reporting API ── */

/**
 * Format findings as a plain-text report (Rich-markup-free) grouped by file.
 * Reports findings grouped by file with line numbers, pattern IDs, and
 * descriptions. Returns a null-terminated string in the output buffer.
 *
 * If skill_name is provided, includes it in the header.
 */
void skill_audit_format_report(const skill_audit_result_t *result,
                                const char *skill_name,
                                char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SKILL_AUDIT_H */
