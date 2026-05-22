/*
 * path.c — C path manipulation library implementation (J04: pathlib).
 *
 * Implements path_join, basename, dirname, stem, suffix,
 * glob, resolve, normalize, and queries.
 */

#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <glob.h>
#include <fnmatch.h>

/* ─── Helpers ─────────────────────────────────────────────── */

/* Trim trailing '/' (preserve "/" for root). In-place, returns path. */
static char *trim_trailing_slash(char *p) {
    if (!p || !*p) return p;
    size_t len = strlen(p);
    while (len > 1 && p[len - 1] == '/')
        p[--len] = '\0';
    return p;
}

/* Find last '/' in path (or NULL). */
static const char *last_slash(const char *path) {
    if (!path) return NULL;
    const char *s = strrchr(path, '/');
    return s;
}

/* ─── Joining ─────────────────────────────────────────────── */

char *path_join(const char *a, const char *b) {
    if (!b || !*b) return a ? strdup(a) : strdup("");
    if (path_is_absolute(b)) return strdup(b);
    if (!a || !*a) return strdup(b);

    size_t alen = strlen(a);
    size_t blen = strlen(b);
    size_t need_slash = (a[alen - 1] != '/') ? 1 : 0;
    char *out = (char *)malloc(alen + need_slash + blen + 1);
    if (!out) return NULL;

    memcpy(out, a, alen);
    if (need_slash) out[alen] = '/';
    memcpy(out + alen + need_slash, b, blen + 1);
    return out;
}

char *path_join_n(size_t count, ...) {
    va_list ap;
    va_start(ap, count);

    char *result = NULL;
    for (size_t i = 0; i < count; i++) {
        const char *part = va_arg(ap, const char *);
        if (!result) {
            result = strdup(part ? part : "");
            if (!result) { va_end(ap); return NULL; }
        } else {
            char *joined = path_join(result, part);
            free(result);
            result = joined;
            if (!result) { va_end(ap); return NULL; }
        }
    }
    va_end(ap);
    return result ? result : strdup("");
}

/* ─── Components ──────────────────────────────────────────── */

const char *path_basename(const char *path) {
    if (!path || !*path) return path ? path : "";
    const char *s = last_slash(path);
    return s ? s + 1 : path;
}

char *path_dirname(const char *path) {
    if (!path || !*path) return strdup(".");
    const char *s = last_slash(path);
    if (!s) return strdup(".");
    if (s == path) return strdup("/");  /* root */
    size_t len = (size_t)(s - path);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, path, len);
    out[len] = '\0';
    return trim_trailing_slash(out);
}

char *path_stem(const char *path) {
    if (!path || !*path) return NULL;
    const char *base = path_basename(path);
    if (!base || !*base) return NULL;
    /* Find last dot, excluding leading dot for hidden files */
    const char *dot = NULL;
    if (base[0] != '.' || base[1] == '\0') {
        dot = strrchr(base, '.');
    } else {
        /* ".hidden" or ".hidden.ext" — look for second dot */
        dot = strchr(base + 1, '.');
    }
    if (!dot || dot == base) return strdup(path);
    size_t stem_len = (size_t)(dot - path);
    return strndup(path, stem_len);
}

const char *path_suffix(const char *path) {
    const char *base = path_basename(path);
    if (!base || !*base) return "";
    /* Hidden files (.foo) — dot is part of name, not suffix */
    if (base[0] == '.') {
        const char *dot = strchr(base + 1, '.');
        return dot ? dot : "";
    }
    const char *dot = strrchr(base, '.');
    return dot ? dot : "";
}

int path_suffixes(const char *path, char ***out) {
    *out = NULL;
    const char *base = path_basename(path);
    if (!base || !*base) return 0;

    /* Count dots in basename, excluding leading dot for hidden files */
    size_t start = (base[0] == '.') ? 1 : 0;
    const char *p = base + start;
    size_t count = 0;
    while ((p = strchr(p, '.')) != NULL) {
        count++;
        p++;
    }
    if (count == 0) return 0;

    *out = (char **)calloc(count, sizeof(char *));
    if (!*out) return 0;

    p = base + start;
    size_t idx = 0;
    while ((p = strchr(p, '.')) != NULL) {
        /* Find next dot to determine suffix boundary */
        const char *next = strchr(p + 1, '.');
        size_t slen = next ? (size_t)(next - p) : strlen(p);
        (*out)[idx] = strndup(p, slen);
        if (!(*out)[idx]) {
            for (size_t j = 0; j < idx; j++) free((*out)[j]);
            free(*out);
            *out = NULL;
            return 0;
        }
        idx++;
        if (!next) break;
        p = next;
    }
    return (int)count;
}

char *path_ext_swap(const char *path, const char *new_ext) {
    if (!path) return strdup("");
    const char *base = path_basename(path);
    /* Find last dot in basename, excluding leading dot for hidden */
    const char *dot = NULL;
    if (base[0] != '.' || base[1] == '\0') {
        dot = strrchr(base, '.');
    } else {
        /* .hidden or .. */
        dot = strrchr(base + 1, '.');
    }

    size_t stem_len;
    if (dot && dot > base && dot[1] != '\0') {
        stem_len = (size_t)(dot - path);
    } else {
        stem_len = strlen(path);
    }

    size_t ext_len = new_ext ? strlen(new_ext) : 0;
    /* Ensure new_ext starts with '.' */
    size_t add_dot = (ext_len > 0 && new_ext[0] != '.') ? 1 : 0;
    size_t total = stem_len + add_dot + ext_len + 1;

    char *out = (char *)malloc(total);
    if (!out) return NULL;
    memcpy(out, path, stem_len);
    if (add_dot) out[stem_len] = '.';
    if (ext_len > 0)
        memcpy(out + stem_len + add_dot, new_ext, ext_len);
    out[total - 1] = '\0';
    return out;
}

/* ─── Queries ─────────────────────────────────────────────── */

bool path_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0;
}

bool path_is_dir(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool path_is_file(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* ─── Resolution ──────────────────────────────────────────── */

char *path_resolve(const char *path) {
    if (!path || !*path) return NULL;
    char *resolved = realpath(path, NULL);
    return resolved;  /* NULL if path doesn't exist */
}

char *path_abs(const char *path) {
    if (!path || !*path) return NULL;
    if (path_is_absolute(path)) return strdup(path);

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) return NULL;
    return path_join(cwd, path);
}

bool path_is_absolute(const char *path) {
    return path && path[0] == '/';
}

/* ─── Pattern matching ───────────────────────────────────── */

int path_glob(const char *pattern, char ***out) {
    *out = NULL;
    if (!pattern || !*pattern) return -1;

    glob_t g;
    memset(&g, 0, sizeof(g));
    int ret = glob(pattern, GLOB_MARK | GLOB_NOSORT, NULL, &g);
    if (ret != 0) {
        if (ret == GLOB_NOMATCH) return 0;
        return -1;  /* error */
    }

    size_t count = g.gl_pathc;
    if (count == 0) {
        globfree(&g);
        return 0;
    }

    *out = (char **)calloc(count, sizeof(char *));
    if (!*out) { globfree(&g); return -1; }

    for (size_t i = 0; i < count; i++) {
        (*out)[i] = strdup(g.gl_pathv[i]);
        if (!(*out)[i]) {
            for (size_t j = 0; j < i; j++) free((*out)[j]);
            free(*out);
            *out = NULL;
            globfree(&g);
            return -1;
        }
    }
    globfree(&g);
    return (int)count;
}

bool path_fnmatch(const char *pattern, const char *path) {
    if (!pattern || !path) return false;
    return fnmatch(pattern, path, FNM_PATHNAME) == 0;
}

/* ─── Normalization ──────────────────────────────────────── */

char *path_normalize(const char *path) {
    if (!path || !*path) return strdup(".");

    size_t len = strlen(path);
    char *copy = (char *)malloc(len + 2);
    if (!copy) return NULL;
    strcpy(copy, path);

    /* Collapse double slashes */
    {
        size_t w = 0;
        for (size_t r = 0; r <= len; r++) {
            if (r > 0 && copy[r] == '/' && copy[r - 1] == '/') continue;
            copy[w++] = copy[r];
        }
    }

    /* Resolve "." and ".." components */
    /* Split into tokens */
    char *parts[1024];
    size_t depth = 0;

    char *save = NULL;
    char *tok = strtok_r(copy, "/", &save);
    while (tok && depth < 1024) {
        if (strcmp(tok, ".") == 0) {
            /* skip */
        } else if (strcmp(tok, "..") == 0) {
            if (depth > 0) depth--;
        } else {
            parts[depth++] = tok;
        }
        tok = strtok_r(NULL, "/", &save);
    }

    /* Reconstruct */
    /* Calculate total length */
    size_t total = 1;  /* at least '\0' */
    for (size_t i = 0; i < depth; i++)
        total += strlen(parts[i]) + 1;

    char *result = (char *)malloc(total);
    if (!result) { free(copy); return NULL; }
    result[0] = '\0';

    for (size_t i = 0; i < depth; i++) {
        if (i > 0) strcat(result, "/");
        strcat(result, parts[i]);
    }

    /* Preserve leading '/' for absolute paths */
    if (path[0] == '/' && result[0] != '/') {
        char *abs_result = (char *)malloc(strlen(result) + 2);
        if (!abs_result) { free(result); free(copy); return NULL; }
        abs_result[0] = '/';
        strcpy(abs_result + 1, result);
        free(result);
        result = abs_result;
    }

    if (depth == 0 && path[0] != '/') {
        free(result);
        result = strdup(".");
    }

    free(copy);
    return result;
}
