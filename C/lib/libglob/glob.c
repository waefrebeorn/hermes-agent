/*
 * glob.c — C recursive glob matching library (J12: Python glob port).
 *
 * Implements glob matching with ** for recursive directory traversal.
 */

#include "glob.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>

/* ================================================================
 *  Pattern matching (supports **, *, ?)
 * ================================================================ */

bool glob_match(const char *pattern, const char *path) {
    if (!pattern || !path) return false;

    /* Handle ** — match across directory separators */
    const char *starstar = strstr(pattern, "**");
    if (starstar) {
        /* Pattern has **: split on **, verify prefix and suffix match */
        size_t prefix_len = (size_t)(starstar - pattern);
        const char *suffix = starstar + 2; /* skip ** */

        /* Check prefix */
        if (prefix_len > 0) {
            if (strncmp(pattern, path, prefix_len) != 0)
                return false;
        }

        /* Check suffix -- must match at some point in path after prefix */
        const char *path_remain = path + prefix_len;
        if (suffix[0] == '/') {
            /* ** slash: skip the slash */
            suffix++;
            /* ** can match zero or more dirs */
            /* Find suffix in remaining path */
            while (*path_remain) {
                if (fnmatch(suffix, path_remain, 0) == 0)
                    return true;
                /* Skip to next path component */
                path_remain = strchr(path_remain, '/');
                if (!path_remain) break;
                path_remain++;
            }
            return suffix[0] == '\0';
        } else if (suffix[0] == '\0') {
            /* ** at end: matches everything */
            return true;
        } else {
            /* ** followed by non-slash: match suffix anywhere in remaining path */
            while (*path_remain) {
                if (fnmatch(suffix, path_remain, 0) == 0)
                    return true;
                path_remain++;
            }
            return false;
        }
    }

    /* No **: use fnmatch */
    return fnmatch(pattern, path, 0) == 0;
}

/* ================================================================
 *  Recursive file finding
 * ================================================================ */

/* Append a path to the match array, reallocating if needed */
static bool append_match(char ***matches, int *count, int *capacity,
                          const char *path) {
    if (*count >= *capacity) {
        int new_cap = *capacity == 0 ? 64 : *capacity * 2;
        if (new_cap > GLOB_MAX_MATCHES) new_cap = GLOB_MAX_MATCHES;
        char **new_m = (char **)realloc(*matches, (size_t)new_cap * sizeof(char *));
        if (!new_m) return false;
        *matches = new_m;
        *capacity = new_cap;
    }

    (*matches)[*count] = strdup(path);
    if (!(*matches)[*count]) return false;
    (*count)++;
    return true;
}

/* Recursive directory walk */
static void walk_dir(const char *dir, const char *rel_base,
                     const char *pattern,
                     char ***matches, int *count, int *capacity) {
    DIR *dp = opendir(dir);
    if (!dp) return;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* Build full path */
        char full[GLOB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir, entry->d_name);

        /* Build relative path for matching */
        char rel[GLOB_MAX_PATH];
        if (rel_base[0]) {
            snprintf(rel, sizeof(rel), "%s/%s", rel_base, entry->d_name);
        } else {
            snprintf(rel, sizeof(rel), "%s", entry->d_name);
        }

        /* Check stat for directory check */
        struct stat st;
        bool is_dir = false;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
            is_dir = true;

        /* Match this entry */
        if (glob_match(pattern, rel)) {
            if (!append_match(matches, count, capacity, full))
                break;
        }

        /* Recurse into subdirectories */
        if (is_dir && *count < GLOB_MAX_MATCHES) {
            walk_dir(full, rel, pattern, matches, count, capacity);
        }
    }

    closedir(dp);
}

char **glob_find(const char *pattern, const char *base_dir, int *count) {
    if (!count) return NULL;
    *count = 0;

    if (!pattern || !base_dir) return NULL;

    /* Check base_dir exists */
    struct stat st;
    if (stat(base_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return NULL;

    int capacity = 0;
    char **matches = NULL;

    walk_dir(base_dir, "", pattern, &matches, count, &capacity);

    return matches;
}

void glob_free(char **matches, int count) {
    if (!matches) return;
    for (int i = 0; i < count; i++) {
        free(matches[i]);
    }
    free(matches);
}
