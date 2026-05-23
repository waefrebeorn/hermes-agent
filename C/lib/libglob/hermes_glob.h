/*
 * glob.h — C recursive glob matching library (J12: Python glob port).
 *
 * Provides recursive ** glob pattern matching for file discovery.
 * Thread-safe — no global state.
 */

#ifndef HERMES_GLOB_H
#define HERMES_GLOB_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max matches returned */
#define GLOB_MAX_MATCHES 4096

/** Max path length */
#define GLOB_MAX_PATH 4096

/**
 * glob_match(pattern, path) — Check if path matches glob pattern.
 * Supports *, **, ? wildcards. ** matches across directory separators.
 * Returns true if match, false otherwise.
 */
bool glob_match(const char *pattern, const char *path);

/**
 * glob_find(pattern, base_dir) — Find all files matching pattern under base_dir.
 * Returns array of matching paths (malloc'd strings). Sets *count.
 * Pattern may include ** for recursive matching.
 * Caller must free() each string and free() the array.
 */
char **glob_find(const char *pattern, const char *base_dir, int *count);

/**
 * Free glob_find result. Frees all strings and array.
 */
void glob_free(char **matches, int count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GLOB_H */
