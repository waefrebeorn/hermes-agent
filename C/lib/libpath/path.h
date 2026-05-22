/*
 * path.h — C path manipulation library (J04: pathlib port).
 *
 * Public API for platform-independent path operations.
 * All functions use '/' as separator (forward-slash only).
 * Memory-owning functions return malloc'd strings — caller must free().
 * Non-owning functions return pointers into the input string.
 */

#ifndef HERMES_PATHLIB_H
#define HERMES_PATHLIB_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Joining ─────────────────────────────────────────────── */

/** path_join(a, b) — Join two path components with '/'.
 *  If b is absolute, returns strdup(b).
 *  If a is NULL/empty, returns strdup(b).
 *  Caller must free(). */
char *path_join(const char *a, const char *b);

/** path_join_n(count, ...) — Join N path components.
 *  Caller must free(). */
char *path_join_n(size_t count, ...);

/* ─── Components (non-owning — ptr into input) ───────────── */

/** path_basename(path) — Last component (filename or dir).
 *  Returns "path" if no '/'. Never returns NULL (returns "" for ""). */
const char *path_basename(const char *path);

/** path_dirname(path) — Parent directory. malloc'd. Caller free().
 *  Returns "." for single-component paths, "/" for root. */
char *path_dirname(const char *path);

/** path_stem(path) — Filename without last suffix. malloc'd. Caller free().
 *  Returns strdup of basename if no '.' or filename starts with '.'. */
char *path_stem(const char *path);

/** path_suffix(path) — Last extension including dot.
 *  Points into input. Returns "" if no suffix. */
const char *path_suffix(const char *path);

/** path_suffixes(path, out) — All extensions as string array.
 *  Returns count (0 if none). Caller must free() each string and the array. */
int path_suffixes(const char *path, char ***out);

/** path_ext_swap(path, new_ext) — Replace last extension.
 *  new_ext may include leading '.' or not. Caller free(). */
char *path_ext_swap(const char *path, const char *new_ext);

/* ─── Queries ─────────────────────────────────────────────── */

/** path_exists(path) — True if path exists (any type). */
bool path_exists(const char *path);

/** path_is_dir(path) — True if path is a directory. */
bool path_is_dir(const char *path);

/** path_is_file(path) — True if path is a regular file. */
bool path_is_file(const char *path);

/* ─── Resolution ──────────────────────────────────────────── */

/** path_resolve(path) — Canonical absolute path (realpath).
 *  Returns NULL if path doesn't exist. Caller free(). */
char *path_resolve(const char *path);

/** path_abs(path) — Absolute path without resolving symlinks.
 *  If path is relative, prepends cwd. Caller free(). */
char *path_abs(const char *path);

/** path_is_absolute(path) — True if path starts with '/'. */
bool path_is_absolute(const char *path);

/* ─── Pattern matching ───────────────────────────────────── */

/** path_glob(pattern, out) — Glob matching via POSIX glob().
 *  Returns match count (<0 on error). Caller must free() each string and array. */
int path_glob(const char *pattern, char ***out);

/** path_fnmatch(pattern, path) — Shell wildcard match (fnmatch).
 *  Returns true if path matches pattern. */
bool path_fnmatch(const char *pattern, const char *path);

/* ─── Normalization ──────────────────────────────────────── */

/** path_normalize(path) — Collapse "..", ".", double slashes.
 *  Caller free(). */
char *path_normalize(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PATHLIB_H */
