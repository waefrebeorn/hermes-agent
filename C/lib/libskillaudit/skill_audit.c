/*
 * skill_audit.c — AST-level deep audit for skill Python files (text-pattern scanner).
 * Port of Python tools/skills_ast_audit.py.
 *
 * Uses line-by-line text scanning to detect dynamic import/attribute access
 * patterns in Python skill files. Findings are diagnostic hints for human
 * review, not security verdicts.
 */

#include "skill_audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

/* ── Ignored directory names (matching Python's _IGNORED_DIRS) ── */
static const char *IGNORED_DIRS[] = {
    "__pycache__", ".venv", "venv", "node_modules", NULL
};

static bool _is_ignored_dir(const char *name)
{
    for (int i = 0; IGNORED_DIRS[i]; i++) {
        if (strcmp(name, IGNORED_DIRS[i]) == 0)
            return true;
    }
    return false;
}

static bool _ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) return false;
    size_t slen = strlen(str);
    size_t suflen = strlen(suffix);
    if (suflen > slen) return false;
    return strcmp(str + slen - suflen, suffix) == 0;
}

/* Check if a string looks like a quoted literal (starts with ' or ") */
static bool _is_literal_arg(const char *s)
{
    if (!s) return false;
    while (*s == ' ' || *s == '\t') s++;
    return (*s == '\'' || *s == '"');
}

/* Add a finding to the result set */
static void _add_finding(skill_audit_result_t *r, const char *file,
                          int line, const char *pid, const char *desc)
{
    if (r->count >= SKILLAUDIT_MAX_FINDINGS) return;
    skill_audit_finding_t *f = &r->findings[r->count];
    strncpy(f->file, file, sizeof(f->file) - 1);
    f->file[sizeof(f->file) - 1] = '\0';
    f->line = line;
    strncpy(f->pattern_id, pid, sizeof(f->pattern_id) - 1);
    f->pattern_id[sizeof(f->pattern_id) - 1] = '\0';
    strncpy(f->description, desc, sizeof(f->description) - 1);
    f->description[sizeof(f->description) - 1] = '\0';
    r->count++;
}

/* ── Line scanner ── */

static void _scan_line(const char *line, int lineno,
                        const char *rel_path, skill_audit_result_t *r)
{
    /* Trim leading whitespace for matching but keep original for comparison */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;

    /* 1. importlib.import_module(...) */
    if (strstr(p, "importlib.import_module(")) {
        _add_finding(r, rel_path, lineno, "dynamic_import",
                     "importlib.import_module() — loads arbitrary modules at runtime");
    }

    /* 2. __import__(<computed>) — flag all __import__ calls; Python only
     *    flags non-literal args, but without AST we conservatively flag all.
     *    check: __import__( followed by something that's NOT a literal */
    {
        const char *imp = strstr(p, "__import__(");
        if (imp) {
            const char *arg_start = imp + 11; /* after "__import__(" */
            if (!_is_literal_arg(arg_start)) {
                _add_finding(r, rel_path, lineno, "dynamic_import_computed",
                             "__import__ with non-literal module name — possible dynamic import");
            }
        }
    }

    /* 3. getattr(obj, <computed>) — flag getattr where second arg isn't a literal */
    {
        const char *g = strstr(p, "getattr(");
        if (g) {
            /* Walk past first arg (find the comma after first arg) */
            const char *comma = g + 8; /* after "getattr(" */
            int paren_depth = 1;
            while (*comma && paren_depth > 0) {
                if (*comma == '(') paren_depth++;
                else if (*comma == ')') paren_depth--;
                else if (*comma == ',' && paren_depth == 1) break;
                comma++;
            }
            if (*comma == ',') {
                comma++;
                /* Skip whitespace before second arg */
                while (*comma == ' ' || *comma == '\t') comma++;
                if (!_is_literal_arg(comma)) {
                    _add_finding(r, rel_path, lineno, "dynamic_getattr",
                                 "getattr with non-literal attribute name — computed attribute access");
                }
            }
        }
    }

    /* 4. obj.__dict__[<computed>] */
    if (strstr(p, ".__dict__[")) {
        /* Check if the subscript is not a literal */
        const char *dict = strstr(p, ".__dict__[");
        const char *slice = dict + 11; /* after ".__dict__[" */
        if (slice && *slice && !_is_literal_arg(slice)) {
            _add_finding(r, rel_path, lineno, "dict_access",
                         "__dict__[<computed>] — dynamic attribute access");
        } else if (slice && *slice) {
            /* Literal subscript — not flagged */
        }
    }

    /* 5. import importlib / from importlib import ... */
    if ((strncmp(p, "import ", 7) == 0 || strncmp(p, "from ", 5) == 0)) {
        const char *mod_start;
        if (strncmp(p, "import ", 7) == 0)
            mod_start = p + 7;
        else
            mod_start = p + 5;

        /* Check if module starts with "importlib" */
        if (strncmp(mod_start, "importlib", 9) == 0) {
            /* Check word boundary: importlib must be followed by \0, space, or . */
            char after = mod_start[9];
            if (after == '\0' || after == ' ' || after == '.' || after == '\n') {
                const char *end = p;
                while (*end && *end != '\n') end++;
                int stmt_len = (int)(end - p);
                if (stmt_len > 200) stmt_len = 200;
                char stmt[201];
                memcpy(stmt, p, stmt_len);
                stmt[stmt_len] = '\0';

                char desc[SKILLAUDIT_DESC_LEN];
                snprintf(desc, sizeof(desc), "%s", stmt);
                /* Truncate description to fit with suffix */
                size_t dlen = strlen(desc);
                const char *suffix = " - enables dynamic module loading";
                size_t needed = dlen + strlen(suffix) + 1;
                if (needed > SKILLAUDIT_DESC_LEN) {
                    /* Truncate the statement part */
                    size_t max_stmt = SKILLAUDIT_DESC_LEN - strlen(suffix) - 4;
                    if (max_stmt < 10) max_stmt = 10;
                    desc[max_stmt] = '\0';
                    strncat(desc, "...", 3);
                }
                strncat(desc, suffix, SKILLAUDIT_DESC_LEN - strlen(desc) - 1);

                _add_finding(r, rel_path, lineno, "importlib_import", desc);
            }
        }
    }
}

/* ── Scan a single file ── */

skill_audit_result_t skill_audit_scan_file(const char *path)
{
    skill_audit_result_t result = { .count = 0 };

    if (!path || !*path) return result;

    /* Only scan .py files */
    if (!_ends_with(path, ".py"))
        return result;

    FILE *f = fopen(path, "r");
    if (!f) return result;

    /* Extract relative filename from path */
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;

    char buf[8192];
    int lineno = 0;

    while (fgets(buf, sizeof(buf), f)) {
        lineno++;
        _scan_line(buf, lineno, name, &result);
    }

    fclose(f);
    return result;
}

/* ── Scan directory recursively ── */

typedef struct {
    char path[SKILLAUDIT_MAX_PATH];
} path_entry_t;

/* Simple stack-based directory traversal to avoid recursion depth issues */
static int _collect_py_files(const char *dir, path_entry_t *files, int max_files)
{
    int count = 0;
    path_entry_t *dirs = malloc(sizeof(path_entry_t) * 256);
    if (!dirs) return 0;
    int dir_count = 0;

    strncpy(dirs[dir_count].path, dir, sizeof(dirs[0].path) - 1);
    dirs[dir_count].path[sizeof(dirs[0].path) - 1] = '\0';
    dir_count = 1;

    while (dir_count > 0 && count < max_files) {
        dir_count--;
        const char *current_dir = dirs[dir_count].path;

        DIR *d = opendir(current_dir);
        if (!d) continue;

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL && count < max_files) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            if (_is_ignored_dir(entry->d_name))
                continue;

            char full[SKILLAUDIT_MAX_PATH];
            snprintf(full, sizeof(full), "%s/%s", current_dir, entry->d_name);

            struct stat st;
            if (stat(full, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (dir_count < 256) {
                    strncpy(dirs[dir_count].path, full, sizeof(dirs[0].path) - 1);
                    dirs[dir_count].path[sizeof(dirs[0].path) - 1] = '\0';
                    dir_count++;
                }
            } else if (S_ISREG(st.st_mode) && _ends_with(entry->d_name, ".py")) {
                strncpy(files[count].path, full, sizeof(files[0].path) - 1);
                files[count].path[sizeof(files[0].path) - 1] = '\0';
                count++;
            }
        }
        closedir(d);
    }

    free(dirs);
    return count;
}

skill_audit_result_t skill_audit_scan_directory(const char *dir_path)
{
    skill_audit_result_t result = { .count = 0 };

    if (!dir_path || !*dir_path) return result;

    struct stat st;
    if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode))
        return result;

    path_entry_t *py_files = malloc(sizeof(path_entry_t) * 512);
    if (!py_files) return result;
    int n_files = _collect_py_files(dir_path, py_files, 512);

    /* Get the base directory name for relative paths */
    const char *base_name = strrchr(dir_path, '/');
    base_name = base_name ? base_name + 1 : dir_path;

    for (int i = 0; i < n_files; i++) {
        FILE *f = fopen(py_files[i].path, "r");
        if (!f) continue;

        /* Compute relative path from base directory */
        const char *rel = py_files[i].path + strlen(dir_path) + 1;
        /* Ensure rel starts from the base name */
        if (strncmp(py_files[i].path, dir_path, strlen(dir_path)) == 0) {
            rel = py_files[i].path + strlen(dir_path);
            if (*rel == '/') rel++;
        }

        char buf[8192];
        int lineno = 0;
        while (fgets(buf, sizeof(buf), f)) {
            lineno++;
            _scan_line(buf, lineno, rel, &result);
        }
        fclose(f);
    }

    free(py_files);
    return result;
}

/* ── Scan path (file or directory) ── */

skill_audit_result_t skill_audit_scan_path(const char *path)
{
    skill_audit_result_t result = { .count = 0 };
    if (!path || !*path) return result;

    struct stat st;
    if (stat(path, &st) != 0) return result;

    if (S_ISREG(st.st_mode))
        return skill_audit_scan_file(path);
    else if (S_ISDIR(st.st_mode))
        return skill_audit_scan_directory(path);

    return result;
}

/* ── Report formatting ── */

void skill_audit_format_report(const skill_audit_result_t *result,
                                const char *skill_name,
                                char *out, size_t out_size)
{
    if (!result || !out || out_size == 0) return;

    char *pos = out;
    size_t remaining = out_size;

    /* Header */
    if (skill_name && *skill_name) {
        int n = snprintf(pos, remaining, "AST deep scan: %s\n", skill_name);
        if (n < 0) return;
        if ((size_t)n < remaining) { pos += n; remaining -= n; } else remaining = 0;
    } else {
        int n = snprintf(pos, remaining, "AST deep scan\n");
        if (n < 0) return;
        if ((size_t)n < remaining) { pos += n; remaining -= n; } else remaining = 0;
    }

    if (result->count == 0) {
        snprintf(pos, remaining, "  No dynamic import/access patterns detected.\n");
        return;
    }

    /* Summary */
    {
        int n = snprintf(pos, remaining, "  %d finding(s):\n", result->count);
        if (n < 0) return;
        if ((size_t)n < remaining) { pos += n; remaining -= n; } else remaining = 0;
    }

    /* Group findings by file */
    char current_file[SKILLAUDIT_MAX_PATH] = "";
    for (int i = 0; i < result->count; i++) {
        const skill_audit_finding_t *f = &result->findings[i];

        /* Print file header when file changes */
        if (strcmp(f->file, current_file) != 0) {
            strncpy(current_file, f->file, sizeof(current_file) - 1);
            current_file[sizeof(current_file) - 1] = '\0';
            int n = snprintf(pos, remaining, "  %s\n", f->file);
            if (n < 0) return;
            if ((size_t)n < remaining) { pos += n; remaining -= n; } else remaining = 0;
        }

        /* Print finding line */
        int n = snprintf(pos, remaining, "    L%d  %s  — %s\n",
                         f->line, f->pattern_id, f->description);
        if (n < 0) return;
        if ((size_t)n < remaining) { pos += n; remaining -= n; } else remaining = 0;
    }

    /* Footer note */
    snprintf(pos, remaining,
             "\n  Note: diagnostic hints for human review, not security verdicts.\n");
}
